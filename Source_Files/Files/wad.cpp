/*
	WAD.C

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html
*/

// TODO: of all the things that could do with migrating to CPP class, wad.cpp is the mostest (file is opened, header is read, file is closed -- this is done so many times instead of reading header once and providing methods for accessing its checksums and other useful values)

#include "cseries.h"

#include "wad.h"
#include "tags.h"
#include "crc.h"
#include "interface.h" // for strERRORS

#include "DataFile.hpp"
#include "Packing.h"


static int32 calculate_directory_offset(wad_header_t *header, short index);

static short get_directory_base_length(wad_header_t *header);

static short get_entry_header_length(wad_header_t *header);

static ao_err read_indexed_directory_data(DataFile& file, wad_header_t* header, int16_t index, directory_entry *entry);

static int32_t calculate_raw_wad_length(wad_header_t* file_header, uint8_t* wad);

static ao_err read_indexed_wad_from_file_into_buffer(DataFile& OFile, wad_header_t *header, short index, void *buffer, int32 *length);

static short count_raw_tags(uint8 *raw_wad);

static struct wad_data *convert_wad_from_raw(struct wad_header_t *header, uint8 *data,	int32 wad_start_offset, int32 raw_length);

static struct wad_data *convert_wad_from_raw_modifiable(struct wad_header_t *header, uint8 *raw_wad, int32 raw_length);
//static void patch_wad_from_raw(struct wad_header *header, uint8 *raw_wad, struct wad_data *read_wad);

static ao_err size_of_indexed_wad(DataFile& OFile, struct wad_header_t *header, short index, int32 *length);




// LP: routines for packing and unpacking the data from streams of bytes
static uint8 *unpack_wad_header(uint8 *Stream, wad_header_t *Objects, size_t Count);
static uint8 *pack_wad_header(uint8 *Stream, wad_header_t *Objects, size_t Count);
static uint8 *unpack_old_directory_entry(uint8 *Stream, old_directory_entry *Objects, size_t Count);
static uint8 *pack_old_directory_entry(uint8 *Stream, old_directory_entry *Objects, size_t Count);
static uint8 *unpack_directory_entry(uint8 *Stream, directory_entry *Objects, size_t Count);
static uint8 *pack_directory_entry(uint8 *Stream, directory_entry *Objects, size_t Count);
//static uint8 *unpack_old_entry_header(uint8 *Stream, old_entry_header *Objects, size_t Count);
static uint8 *pack_old_entry_header(uint8 *Stream, old_entry_header *Objects, size_t Count);
static uint8 *unpack_entry_header(uint8 *Stream, entry_header *Objects, size_t Count);
static uint8 *pack_entry_header(uint8 *Stream, entry_header *Objects, size_t Count);

/* ------------------ Code Begins */

ao_err read_wad_header(DataFile& file, wad_header_t* header)
{
    if (!file.is_open()) return STRID(strERRORS, fileIsNotOpen);
    file.set_position(0);
    
    uint8 buffer[SIZEOF_wad_header];
    file.read(SIZEOF_wad_header, buffer);
    unpack_wad_header(buffer, header, 1);
    
    if (!header->is_supported_map_wad() || header->wad_count < 1) { return STRID(gameError, errUnknownWadVersion); }
    return no_err;
}




// This could be improved.  Under the current implementation, it requires 2X sizeof level worth
// of memory to load... (This makes writing wads easier, but isn't really useful for loading.)
// Note that this does the correct thing for union wadfiles. // EESE: still true? cos what's the commented out line below mean? an explanation of what "union wadfile" means would help
ao_err read_indexed_wad_from_file(DataFile& file, wad_header_t* header, int16_t index, bool read_only, wad_data*& read_wad)
{
    ao_err err = 0;
    
    directory_entry entry;
    err = read_indexed_directory_data(file, header, index, &entry);
    if (err) return err;
    
    int32_t length = entry.length;
    
    err = size_of_indexed_wad(file, header, index, &length);
    if (err) return err;
    
    // The padding is so that one can use later-Marathon entry-header reading
    // on Marathon 1 wadfiles, which have a shorter entry header
    int32 padded_length = length + (SIZEOF_entry_header-SIZEOF_old_entry_header);
    
    // Read into the buffer
    uint8_t* raw_wad = (uint8*)ao_malloc(padded_length);
    err = read_indexed_wad_from_file_into_buffer(file, header, index, raw_wad, &length);
    if (!err)
    {
        // Got the raw wad. Convert it into our internal representation...
        if (read_only)
        {
            read_wad = convert_wad_from_raw(header, raw_wad, 0, length);
            raw_wad = nullptr; // read_wad takes ownership of raw_wad
        }
        else
        {
            read_wad = convert_wad_from_raw_modifiable(header, raw_wad, length);
        }
    }
    free(raw_wad);
	
	return err;
}


// given the 4-char code (e.g. 'text') that identifies a specific resource in the WAD, this returns a pointer to that resource's data // EES: TODO: rename this `get_resource[_of_type]` and make it a method on the wad_data struct. On success, it should return a const'd pointer to the found tag_data struct, else nullptr. Not gonna change it today; just figuring out where the level name should be converted between MacRoman and UTF8.
uint8_t* get_wad_resource_for_tag(wad_data* wad, WadDataType type, size_t* length)
{
    assert_fail(wad, "WAD cannot be nullptr here");
    
    for (int16_t i = 0; i < wad->tag_count; i++)
    {
        tag_data tagged_resource = wad->tag_data[i];
        if (tagged_resource.tag == type)
        {
            assert_fail(tagged_resource.length >= 0, "WAD unpack");
            *length = tagged_resource.length;
            return tagged_resource.data;
        }
    }
    
    *length = 0;
    return nullptr;
}



// called by map_wad.cpp, preferences.cpp, network_dialogs.cpp
uint32 read_wad_file_checksum(const ao_path& path) // caution: this also returns 0 on failure
{
    DataFile file;
    wad_header_t header;
    return file.open(path) == no_err && read_wad_header(file, &header) ? header.checksum : 0;
}


// called by map_wad.cpp
uint32 read_wad_file_parent_checksum(const ao_path& path)
{
    DataFile file;
    wad_header_t header;
    return file.open(path) == no_err && read_wad_header(file, &header) ? header.parent_checksum : 0;
}






void fill_default_wad_header(const ao_path& File, short wadfile_version, short data_version,
                             short wad_count, short application_directory_data_size, wad_header_t *header)
{
	obj_clear(*header);
	header->version= wadfile_version;
	header->data_version= data_version;
    copy_utf8_string_to_buffer(File.filename(), header->file_name, MAXIMUM_WADFILE_NAME_LENGTH);
	header->wad_count= wad_count;
	header->application_specific_directory_data_size= application_directory_data_size;					

	header->entry_header_size= get_entry_header_length(header);
	if(!header->entry_header_size) 
	{
		/* Default.. */
		header->entry_header_size = SIZEOF_entry_header;
	}

	header->directory_entry_base_size= get_directory_base_length(header);
	if(!header->directory_entry_base_size)
	{
		header->directory_entry_base_size = SIZEOF_directory_entry;
	}

	/* Things left for caller to fill in: */
	/* uint32 checksum, int32 directory_offset, uint32 parent_checksum */
}

void write_wad_header(DataFile& file, wad_header_t* header)
{
    ao_err err = 0;
	uint8_t buffer[SIZEOF_wad_header];
    memset(buffer, 0, sizeof(buffer));
	pack_wad_header(buffer, header, 1);
    file.set_position(0);
    file.write(sizeof(buffer), buffer);
}


// Takes raw, unswapped directory data
bool write_directorys(DataFile& file, wad_header_t* header, void* entries)
{
    if (!file.is_open()) return false;
    
	int32_t size_to_write = get_size_of_directory_data(header);
	
	assert_fail(header->version >= WADFILE_HAS_DIRECTORY_ENTRY, "");
    
    file.set_position(header->directory_offset);
    file.write(size_to_write, entries);
    return true;
}


/* Note wad_count better be correct! */
int32 get_size_of_directory_data(
	struct wad_header_t *header)
{
	short base_entry_size= get_directory_base_length(header);

	assert_fail(header->wad_count, "");
	assert_fail(header->version>=WADFILE_HAS_DIRECTORY_ENTRY || header->application_specific_directory_data_size==0, "");

	return (header->wad_count*
		(header->application_specific_directory_data_size+base_entry_size));
}

// Takes raw, unswapped directory data
void *get_indexed_directory_data(
	struct wad_header_t *header,
	short index,
	void *directories)
{
	// LP: changed "char *" to "uint8 *"
	uint8 *data_ptr= (uint8 *)directories;
	short base_entry_size= get_directory_base_length(header);

	assert_fail(header->version>=WADFILE_HAS_DIRECTORY_ENTRY, "WAD ");
	assert_fail(index>=0 && index<header->wad_count, "WAD ");
	data_ptr += index*(header->application_specific_directory_data_size+base_entry_size);
	data_ptr += base_entry_size; /* Because the application specific junk follows the standard entries */

	return ((void *) data_ptr);
}

void set_indexed_directory_offset_and_length(
	struct wad_header_t *header,
	void *entries,
	short index,
	int32 offset,
	int32 length,
	short wad_index)
{
	uint8 *data_ptr= (uint8 *)entries;
	int32 data_offset;
	
	assert_fail(header->version>=WADFILE_HAS_DIRECTORY_ENTRY, "WAD ");
	
	/* calculate_directory_offset is for the file, by subtracting the base, we get the actual offset.. */
	data_offset= calculate_directory_offset(header, index) - header->directory_offset;

	data_ptr+= data_offset;
	
	// LP: eliminating this dangerous sort of casting;
	// should work correctly for wadfiles with size more than 1
	/*
	entry= (struct directory_entry *) data_ptr;
	
	entry->length= length;
	entry->offset_to_start= offset;
	
	if(header->version>=WADFILE_SUPPORTS_OVERLAYS)
	{
		entry->index= wad_index;
	}
	*/
	
	// LP: should be correct for packing also
	if (header->version>=WADFILE_SUPPORTS_OVERLAYS)
	{
		directory_entry entry;
		
		entry.length = length;
		entry.offset_to_start = offset;
		entry.index = wad_index;
		
		pack_directory_entry(data_ptr, &entry, 1);
	}
	else
	{
		old_directory_entry entry;
		
		entry.length = length;
		entry.offset_to_start = offset;
		
		pack_old_directory_entry(data_ptr, &entry, 1);
	}
}

// Returns raw, unswapped directory data
uint8_t* read_directory_data(DataFile& file, wad_header_t* header)
{
    assert_fail(file.is_open(), "");
	assert_fail(header->version >= WADFILE_HAS_DIRECTORY_ENTRY, "");
	
    int32_t size = get_size_of_directory_data(header);
    uint8_t* data= (uint8_t*)ao_malloc(size);
    
    file.set_position(header->directory_offset);
    file.read(size, data);
	return data;
}


// Allows for inplace creation of wadfiles
wad_data* append_data_to_wad(wad_data* wad, WadDataType type, const void* data, size_t size, size_t offset)
{
    assert_fail(wad, "WAD can't be null!");
	assert_fail(size, "You can't append zero length WAD data anymore!");
	assert_fail(!wad->read_only_data, "can't modify a read-only WAD");

	// Find the index to replace
    int16_t index = 0;
	for (; index < wad->tag_count; index++)
	{
		if (wad->tag_data[index].tag == type)
		{
			free(wad->tag_data[index].data);
			break;
		}
	}
	
	// If we are appending
	if (index == wad->tag_count)
    {
        tag_data* old_data = wad->tag_data;
        
        // TODO: realloc would make more sense (but the in-memory WAD data structure may need reworked anyway to support 2D map editing)
        wad->tag_count++;
        wad->tag_data = (tag_data*)ao_calloc(wad->tag_count, sizeof(tag_data));

		if (old_data)
		{
			memcpy(wad->tag_data, old_data, (wad->tag_count - 1) * sizeof(tag_data));
			free(old_data);
		}
	}

	// Copy it in
	assert_fail(index >= 0 && index < wad->tag_count, "WAD index out of range");
    
	wad->tag_data[index].data = (uint8_t*)ao_malloc(size);
	memcpy(wad->tag_data[index].data, data, size);

	// Setup the tag data
	wad->tag_data[index].tag    = type;
	wad->tag_data[index].length = size;
	wad->tag_data[index].offset = offset;

	return wad;
}

/*
void remove_tag_from_wad(
	struct wad_data *wad, 
	WadDataType type)
{
	short index;

	assert_fail(wad, "WAD ");
	assert_fail(!wad->read_only_data, "WAD ");

	// Find the index to replace
	for(index= 0; index<wad->tag_count; ++index)
	{
		if(wad->tag_data[index].tag==type) break;
	}
	
	// If we are appending...
	if(index!=wad->tag_count)
	{
		struct tag_data *old_data= wad->tag_data;

		wad->tag_count-= 1;
		wad->tag_data= (struct tag_data *) malloc(wad->tag_count*sizeof(struct tag_data)); 
		
		if(!wad->tag_data) { exit(outOfMemory); }

		assert_fail(wad->tag_data, "WAD ");
		objlist_clear(wad->tag_data, wad->tag_count);
		if(old_data)
		{
			// Copy the stuff below it.
			objlist_copy(wad->tag_data, old_data, index);
			
			// Copy the stuff above it.
			objlist_copy(&wad->tag_data[index], &old_data[index+1], (wad->tag_count-index));
			
			free(old_data);
		}
	}
}
*/


/* Now uses CRC to checksum.. */
void calculate_and_store_wadfile_checksum(DataFile& OFile)
{
	struct wad_header_t header;
	
	/* read the header */
	read_wad_header(OFile, &header);

	/* Make sure we don't checksum the checksum value.. */
	header.checksum= 0l;
	write_wad_header(OFile, &header);
	
	/* Unused bytes better ALWAYS be initialized to zero.. */
	header.checksum= calculate_crc_for_opened_file(OFile);

	/* Save it.. */
	write_wad_header(OFile, &header);
}

void write_wad(DataFile& OFile, wad_header_t *file_header, wad_data *wad, int32 offset)
{
    assert_fail(OFile.is_open(), "File isn't open");

    short entry_header_length = get_entry_header_length(file_header);

	assert_fail(wad, "WAD ");
	assert_fail(!wad->read_only_data, "WAD ");


    int32_t running_offset= 0;
    entry_header header;
	for (int32_t index = 0; index < wad->tag_count; index++)
	{
		header.tag = wad->tag_data[index].tag;
		header.length = wad->tag_data[index].length;
		// On older versions, this will get overwritten by the copy
		header.offset = wad->tag_data[index].offset;
		if (index == wad->tag_count - 1)
		{
			header.next_offset = 0; // Last entry's next offset is zero.
		} else {
			running_offset += header.length + entry_header_length;
			header.next_offset = running_offset;
		}

		// Write this to the file...
		uint8 buffer[MAX(SIZEOF_old_entry_header,SIZEOF_entry_header)];
		switch (entry_header_length)
        {
            case SIZEOF_old_entry_header:
                pack_old_entry_header(buffer, (old_entry_header *)&header, 1);
                break;
            case SIZEOF_entry_header:
                pack_entry_header(buffer, &header, 1);
                break;
            default:
                throw_bug_report_f("Unrecognized entry-header length: %d", entry_header_length);
        }
        OFile.set_position(offset);
        OFile.read(entry_header_length, buffer);
        
        offset += entry_header_length;
        OFile.set_position(offset);
        OFile.read(wad->tag_data[index].length, wad->tag_data[index].data);
        
        offset += wad->tag_data[index].length;
	}
}


/*
short number_of_wads_in_file(const ao_path& File)
{
	short count = NONE;
	
	DataFile OFile;
	if (OFile.open(File) == no_err)
	{
		wad_header_t header;
		read_wad_header(OFile, &header);
		count = header.wad_count;
	}
	
	return count;
}
*/

void free_wad(wad_data *wad)
{
	short ii;
	
	assert_fail(wad, "WAD ");
	
	/* Free all of the tags */
	if(wad->read_only_data)
	{
		/* Read only wad.. */
		free(wad->read_only_data);
		free(wad->tag_data);
	} else {
		/* Modifiable */
		for(ii=0; ii<wad->tag_count; ++ii)
		{
			assert_fail(wad->tag_data[ii].data, "WAD ");
			free(wad->tag_data[ii].data);
		}
		free(wad->tag_data);
	}
	
	/* And free the total data.. */
	free(wad);
}

int32 calculate_wad_length(wad_header_t *file_header, wad_data *wad)
{
	short ii;
	short header_length= get_entry_header_length(file_header);
	int32 running_length= 0l;

	for(ii= 0; ii<wad->tag_count; ++ii)
	{
		running_length += wad->tag_data[ii].length + header_length;
	}
	
	return running_length;
}

/* ------------ Transfer type functions */
#define CURRENT_FLAT_MAGIC_COOKIE (0xDEADDEAD)

/*
	LP: ought not to use such a struct directly, because this is supposed to be packed data
	Format:
	4 bytes -- magic cookie
	4 bytes -- length
	SIZEOF_wad_header -- packed wad header
*/
const int SIZEOF_encapsulated_wad_data = 2*4 + SIZEOF_wad_header;
	

ao_err get_flat_data(const ao_path& File, short wad_index, uint8_t*& data)
{
    data = nullptr;
    ao_err err = no_err;
	
	DataFile OFile;
    err = OFile.open(File);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(OFile, &header);
    if (err) return err;
    
    // Allocate the conglomerate data
    int32_t length;
    err = size_of_indexed_wad(OFile, &header, wad_index, &length);
    if (err) return err;
    
    data = (uint8_t*)ao_malloc(length+SIZEOF_encapsulated_wad_data);
    
    uint8_t* buffer = data + SIZEOF_encapsulated_wad_data;
    
    // Pack the encapsulated header
    uint8_t* S = data;
    ValueToStream(S, uint32_t(CURRENT_FLAT_MAGIC_COOKIE));
    ValueToStream(S, int32_t(length + SIZEOF_encapsulated_wad_data));
    S = pack_wad_header(S, &header, 1);
    assert_fail((S - data) == SIZEOF_encapsulated_wad_data, "WAD ");
    
    err = read_indexed_wad_from_file_into_buffer(OFile, &header, wad_index, buffer, &length);
    if (err)
    {
        free(data);
        data = nullptr;
    }
    
    assert_fail_f(data, "Failed to extract flat data at index %d from WAD file '%s'", wad_index, File.string().c_str());
	return err;
}

int32_t get_flat_data_length(uint8_t* data)
{
    int32 size = 0;
    if (data)
    {
        uint8_t* S = data;
        S += 4;
        StreamToValue(S, size);
    }
    return size;
}


// this has no failure conditions (if you don't count the debug-only asserts, which should be runtime guards if there's any question of data's integrity/supported version)
/* This is how you dispose of it-> you inflate it, then use free_wad() */
wad_data* inflate_flat_data(uint8_t* flat_data, wad_header_t* header)
{
	uint8* buffer = flat_data + SIZEOF_encapsulated_wad_data;
	int32 raw_length;

	assert_fail(flat_data, "WAD ");
	assert_fail(header, "WAD ");
	
	uint32 MagicCookie;
	uint8 *S = flat_data;
	StreamToValue(S, MagicCookie);
	assert_fail(MagicCookie == CURRENT_FLAT_MAGIC_COOKIE, "WAD ");
	
	// Get the length here, where it's convenient
	int32 Length;
	StreamToValue(S,Length);
	
	S = unpack_wad_header(S,header,1);
	assert_fail((S - flat_data) == SIZEOF_encapsulated_wad_data, "WAD ");

	raw_length = calculate_raw_wad_length(header, buffer);
	assert_fail(raw_length == Length-SIZEOF_encapsulated_wad_data, "WAD ");
	
	// Now inflate.
	return convert_wad_from_raw(header, flat_data, SIZEOF_encapsulated_wad_data, raw_length);
}


/* ---------- debugging routines. */

/*
void dump_wad(
	struct wad_data *wad)
{
	short index;
	struct tag_data *tag= wad->tag_data;
	ao__dprintf__("---Dumping---");
	ao__dprintf__("Tag Count: %d", wad->tag_count);
	for(index= 0; index<wad->tag_count; ++index)
	{
		assert_fail(tag, "");
		ao__dprintf__("Tag: %x data: %p length: %d offset: %d", tag->tag, tag->data, tag->length,
			tag->offset);
		tag++;
	}
	ao__dprintf__("---End of Dump---");
}
 */


/* ------------------------------ Private Code --------------- */


static ao_err size_of_indexed_wad(DataFile& OFile, wad_header_t* header, int16_t index, int32_t* length)
{
    directory_entry entry;
    ao_err err = read_indexed_directory_data(OFile, header, index, &entry);
    if (err) return err;
    
	*length = entry.length;
    return no_err;
}


static int32 calculate_directory_offset(
	struct wad_header_t *header, 
	short index)
{
	int32 offset;
	int32 unit_size;

	switch(header->version)
	{
		case PRE_ENTRY_POINT_WADFILE_VERSION:
			assert_fail(header->application_specific_directory_data_size==0, "WAD ");
			// OK for Marathon 1
		case WADFILE_HAS_DIRECTORY_ENTRY:
		case WADFILE_SUPPORTS_OVERLAYS:
		// LP addition:
		case WADFILE_HAS_INFINITY_STUFF:
			assert_fail(header->application_specific_directory_data_size>=0, "WAD ");
			unit_size= header->application_specific_directory_data_size+get_directory_base_length(header);
			break;
			
		default:
            throw_ao_exception("Unknown WADFILE version: %d", errDataFileTooNew, header->version);
			break;
	}

	/* Now actually calculate it (Note that the directory_entry data is first) */
	offset= header->directory_offset+(index*unit_size);
	
	return offset;
}

static short get_entry_header_length(
	struct wad_header_t *header)
{
	short size;

	assert_fail(header, "WAD ");
	
	switch(header->version)
	{
		case PRE_ENTRY_POINT_WADFILE_VERSION:
		case WADFILE_HAS_DIRECTORY_ENTRY:
			size = SIZEOF_old_entry_header;
			break;

		default:
			/* After this point, I stored it. */
			size = header->entry_header_size;
			break;
	}
	
	return size;
}

static short get_directory_base_length(
	struct wad_header_t *header)
{
	short size;
	
	assert_fail(header, "WAD ");
	assert_fail(header->version <= CURRENT_WADFILE_VERSION, "WAD ");

	switch(header->version)
	{
		case PRE_ENTRY_POINT_WADFILE_VERSION:
		case WADFILE_HAS_DIRECTORY_ENTRY:
			size = SIZEOF_old_directory_entry;
			break;

		default:
			/* After this point, I stored it. */
			size = header->directory_entry_base_size;
			break;
	}
		
	return size;
}

/* This searches the directories for the given index, to allow for special replacements. */
static ao_err read_indexed_directory_data(DataFile& OFile, wad_header_t *header, int16_t index, directory_entry *entry)
{
    if (!OFile.is_open()) return STRID(strERRORS, fileIsNotOpen);
    
    // Get the sizes of the data structures
    short base_entry_size = get_directory_base_length(header);
    
    // For old files, the index==the actual index
    if (header->version <= WADFILE_HAS_DIRECTORY_ENTRY)
    {
        assert_fail(base_entry_size <= SIZEOF_directory_entry, "WAD ");
        
        OFile.set_position(calculate_directory_offset(header, index));
        
        uint8 buffer[MAX(SIZEOF_old_directory_entry,SIZEOF_directory_entry)];
        OFile.read(base_entry_size, buffer);
        
        switch (base_entry_size)
        {
            case SIZEOF_old_directory_entry:
                unpack_old_directory_entry(buffer,(old_directory_entry *)entry,1);
                break;
                
            case SIZEOF_directory_entry:
                unpack_directory_entry(buffer,entry,1);
                break;
                
            default:
                throw_bug_report_f("Unrecognized base-entry length: %d", base_entry_size);
        }
        return no_err;
    }
    else
    {
        // Pin it, so we can try to read future file formats
        if (base_entry_size > SIZEOF_directory_entry) { base_entry_size = SIZEOF_directory_entry; }
        
        for (short directory_index = 0; directory_index < header->wad_count; directory_index++)
        {
            // We use a hint, that the index is the real index, to help make this have a "hit" on the first try
            short test_index = (index + directory_index) % header->wad_count;
            OFile.set_position(calculate_directory_offset(header, test_index));
            
            uint8 buffer[MAX(SIZEOF_old_directory_entry,SIZEOF_directory_entry)];
            OFile.read(base_entry_size, buffer);
            
            switch (base_entry_size)
            {
                case SIZEOF_old_directory_entry:
                    unpack_old_directory_entry(buffer, (old_directory_entry*)entry, 1);
                    break;
                    
                case SIZEOF_directory_entry:
                    unpack_directory_entry(buffer, entry, 1);
                    break;
                    
                default:
                    throw_bug_report_f("Unrecognized base-entry length: %d", base_entry_size);
            }
            if(entry->index == index) { return no_err; }
        }
        return 5; // TODO: what error code for not found?
    }
}


/* Internal function.. */
static ao_err read_indexed_wad_from_file_into_buffer(DataFile& OFile, wad_header_t *header, short index, void *buffer, int32 *length) // Length of maximum buffer on entry, actual length on return */
{
    directory_entry entry;
	bool success = false;

	// Read the directory entry first
    ao_err err = read_indexed_directory_data(OFile, header, index, &entry);
    if (err) return false;
    
    assert_fail(*length <= entry.length, "WAD ");
    assert_fail(buffer, "WAD ");
    
    *length= entry.length;

    if (entry.length > 0)
    {
        if (!OFile.is_open()) return false;
        OFile.set_position(entry.offset_to_start);
        OFile.read(entry.length, buffer);

        
        /* Veracity Check */
        /* ! an error, it has a length non-zero and calculated != actual */
        assert_fail(entry.length==calculate_raw_wad_length(header, (uint8 *)buffer), "WAD ");
    }
	
	return success;
}


// this should never fail and will always return wad_data*
// This MUST be a base wad
static wad_data* convert_wad_from_raw(wad_header_t* header, uint8_t* data, int32_t wad_start_offset, int32_t raw_length)
{
	// In case we are somewhere else, like, for example, in a net transferred level
    uint8_t* raw_wad = data + wad_start_offset;
    wad_data* wad = (wad_data*)ao_calloc(1, sizeof(wad_data));
    
    // If the wad is of non-zero length
    if (raw_length > 0)
    {
        // allocate the tags
        int16_t tag_count = count_raw_tags(raw_wad);
        wad->tag_count = tag_count;
        wad->tag_data = (tag_data*)ao_calloc(tag_count, sizeof(tag_data));
        
        int16_t entry_header_size = get_entry_header_length(header);
        
        uint8_t* raw_wad_entry_header = raw_wad;
        entry_header wad_entry_header;
        unpack_entry_header(raw_wad_entry_header, &wad_entry_header, 1); // Will work OK for Marathon 1
        
        wad->read_only_data = data; // Note that this is a read only wad

        for (int16_t index = 0; index < tag_count; index++)
        {
            assert_fail(header->version < WADFILE_SUPPORTS_OVERLAYS || wad_entry_header.offset == 0, "this must be a base WAD");
            wad->tag_data[index].tag    = wad_entry_header.tag;
            wad->tag_data[index].length = wad_entry_header.length;
            wad->tag_data[index].offset = 0;
            wad->tag_data[index].data   = raw_wad_entry_header + entry_header_size;

            raw_wad_entry_header = raw_wad + wad_entry_header.next_offset;
            unpack_entry_header(raw_wad_entry_header, &wad_entry_header, 1); // Will work OK for Marathon 1
        }
    }
	return wad;
}


// This MUST be a base wad
static wad_data* convert_wad_from_raw_modifiable(wad_header_t* header, uint8_t* raw_wad, int32_t raw_length)
{
    wad_data* wad = (wad_data*)ao_calloc(1, sizeof(wad_data));

    // If the wad is of non-zero length
    if (raw_length)
    {
        // allocate the tags
        int16_t tag_count = count_raw_tags(raw_wad);
        wad->tag_count = tag_count;
        wad->tag_data = (tag_data*)ao_calloc(tag_count, sizeof(tag_data));
        
        int16_t entry_header_size = get_entry_header_length(header);
        
        uint8_t* raw_wad_entry_header = raw_wad;
        entry_header wad_entry_header;
        unpack_entry_header(raw_wad_entry_header, &wad_entry_header, 1); // Will work OK for Marathon 1
        
        for (int16_t index = 0; index < tag_count; index++)
        {
            wad->tag_data[index].tag    = wad_entry_header.tag;
            wad->tag_data[index].length = wad_entry_header.length;
            wad->tag_data[index].data   = (uint8 *) malloc(wad->tag_data[index].length);
            if (!wad->tag_data[index].data) { exit(outOfMemory); }
            
            wad->tag_data[index].offset = 0;
            
            assert_fail(header->version < WADFILE_SUPPORTS_OVERLAYS || wad_entry_header.offset == 0, "this must be a base WAD");

            /* Copy the data.. */
            memcpy(wad->tag_data[index].data, raw_wad_entry_header + entry_header_size, wad->tag_data[index].length);
            raw_wad_entry_header = raw_wad + wad_entry_header.next_offset;
           
            unpack_entry_header(raw_wad_entry_header, &wad_entry_header, 1); // Will work OK for Marathon 1
        }
    }
	return wad;
}


// Will work OK for Marathon 1
static short count_raw_tags(
	uint8 *raw_wad)
{
	int tag_count = 0;

	entry_header header;
	unpack_entry_header(raw_wad, &header, 1);
	while (true) {
		tag_count++;
		uint32 next_offset = header.next_offset;
		if (next_offset == 0)
			break;
		unpack_entry_header(raw_wad + next_offset, &header, 1);
	}

	return tag_count;
}

// Will work OK for Marathon 1
static int32 calculate_raw_wad_length(
	struct wad_header_t *file_header,
	uint8 *wad)
{
	int entry_header_size = get_entry_header_length(file_header);
	int32 length = 0;

	entry_header header;
	unpack_entry_header(wad, &header, 1);
	while (true) {
		length += header.length + entry_header_size;
		uint32 next_offset = header.next_offset;
		if (next_offset == 0)
			break;
		unpack_entry_header(wad + next_offset, &header, 1);
	}

	return length;
}



static uint8 *unpack_wad_header(uint8 *Stream, wad_header_t *Objects, size_t Count)
{
	uint8* S = Stream;
	wad_header_t* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->version);
		StreamToValue(S,ObjPtr->data_version);
		StreamToBytes(S,ObjPtr->file_name,MAXIMUM_WADFILE_NAME_LENGTH);
		StreamToValue(S,ObjPtr->checksum);
		StreamToValue(S,ObjPtr->directory_offset);
		StreamToValue(S,ObjPtr->wad_count);
		StreamToValue(S,ObjPtr->application_specific_directory_data_size);
		StreamToValue(S,ObjPtr->entry_header_size);
		StreamToValue(S,ObjPtr->directory_entry_base_size);
		StreamToValue(S,ObjPtr->parent_checksum);
		S += 2*20;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_wad_header), "WAD ");
	return S;
}

static uint8 *pack_wad_header(uint8 *Stream, wad_header_t *Objects, size_t Count)
{
	uint8* S = Stream;
	wad_header_t* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->version);
		ValueToStream(S,ObjPtr->data_version);
		BytesToStream(S,ObjPtr->file_name,MAXIMUM_WADFILE_NAME_LENGTH);
		ValueToStream(S,ObjPtr->checksum);
		ValueToStream(S,ObjPtr->directory_offset);
		ValueToStream(S,ObjPtr->wad_count);
		ValueToStream(S,ObjPtr->application_specific_directory_data_size);
		ValueToStream(S,ObjPtr->entry_header_size);
		ValueToStream(S,ObjPtr->directory_entry_base_size);
		ValueToStream(S,ObjPtr->parent_checksum);
		S += 2*20;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_wad_header), "WAD ");
	return S;
}


static uint8 *unpack_old_directory_entry(uint8 *Stream, old_directory_entry *Objects, size_t Count)
{
	uint8* S = Stream;
	old_directory_entry* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->offset_to_start);
		StreamToValue(S,ObjPtr->length);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_old_directory_entry), "WAD ");
	return S;
}

static uint8 *pack_old_directory_entry(uint8 *Stream, old_directory_entry *Objects, size_t Count)
{
	uint8* S = Stream;
	old_directory_entry* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->offset_to_start);
		ValueToStream(S,ObjPtr->length);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_old_directory_entry), "WAD ");
	return S;
}


static uint8 *unpack_directory_entry(uint8 *Stream, directory_entry *Objects, size_t Count)
{
	uint8* S = Stream;
	directory_entry* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->offset_to_start);
		StreamToValue(S,ObjPtr->length);
		StreamToValue(S,ObjPtr->index);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_directory_entry), "WAD ");
	return S;
}

static uint8 *pack_directory_entry(uint8 *Stream, directory_entry *Objects, size_t Count)
{
	uint8* S = Stream;
	directory_entry* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->offset_to_start);
		ValueToStream(S,ObjPtr->length);
		ValueToStream(S,ObjPtr->index);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_directory_entry), "WAD ");
	return S;
}


#if 0
static uint8 *unpack_old_entry_header(uint8 *Stream, old_entry_header *Objects, size_t Count)
{
	uint8* S = Stream;
	old_entry_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->tag);
		StreamToValue(S,ObjPtr->next_offset);
		StreamToValue(S,ObjPtr->length);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_old_entry_header), "");
	return S;
}
#endif

static uint8 *pack_old_entry_header(uint8 *Stream, old_entry_header *Objects, size_t Count)
{
	uint8* S = Stream;
	old_entry_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->tag);
		ValueToStream(S,ObjPtr->next_offset);
		ValueToStream(S,ObjPtr->length);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_old_entry_header), "WAD ");
	return S;
}


static uint8 *unpack_entry_header(uint8 *Stream, entry_header *Objects, size_t Count)
{
	uint8* S = Stream;
	entry_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->tag);
		StreamToValue(S,ObjPtr->next_offset);
		StreamToValue(S,ObjPtr->length);
		StreamToValue(S,ObjPtr->offset);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_entry_header), "WAD ");
	return S;
}

static uint8 *pack_entry_header(uint8 *Stream, entry_header *Objects, size_t Count)
{
	uint8* S = Stream;
	entry_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->tag);
		ValueToStream(S,ObjPtr->next_offset);
		ValueToStream(S,ObjPtr->length);
		ValueToStream(S,ObjPtr->offset);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_entry_header), "WAD ");
	return S;
}

