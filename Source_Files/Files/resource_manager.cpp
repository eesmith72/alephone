/*
 resource_manager.cpp - MacOS resource handling for non-Mac platforms
 
 Written in 2000 by Christian Bauer
 
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


#include "resource_manager.h"
#include "DataFile.hpp"



/*
 *  Loaded resource
 */


bool LoadedResource::IsLoaded()
{
    return p != NULL;
}

void LoadedResource::Unload()
{
    if (p) {
        free(p);
        p = NULL;
        size = 0;
    }
}

int64_t LoadedResource::get_length()
{
    return size;
}

void *LoadedResource::GetPointer(bool DoDetach)
{
    void *ret = p;
    if (DoDetach)
        Detach();
    return ret;
}

void LoadedResource::SetData(void *data, size_t length)
{
    Unload();
    p = data;
    size = length;
}

void LoadedResource::Detach()
{
    p = NULL;
    size = 0;
}


/*
 *  Opened resource file
 */

ResourceFile::ResourceFile() : fh(NULL), saved_f(NULL), err(0) {}

bool ResourceFile::Push()
{
    saved_f = get_current_resource_file();
    if (saved_f != fh)
        use_file_resource(fh);
    err = 0;
    return true;
}

bool ResourceFile::Pop()
{
    if (fh != saved_f)
        use_file_resource(saved_f);
    err = 0;
    return true;
}

bool ResourceFile::Check(uint32 Type, int16 ID)
{
    Push();
    bool result = has_1_resource(Type, ID);
    err = result ? 0 : ENOENT;
    Pop();
    return result;
}

bool ResourceFile::Get(uint32 Type, int16 ID, LoadedResource &Rsrc)
{
    Push();
    bool success = get_1_resource(Type, ID, Rsrc);
    err = success ? 0 : ENOENT;
    Pop();
    return success;
}

bool ResourceFile::IsOpen()
{
    return fh != NULL;
}

bool ResourceFile::Close()
{
    if (fh) {
        close_file_resource(fh);
        fh = NULL;
        err = 0;
    }
    return true;
}








/*
 *  Utility functions
 */

bool is_applesingle(SDL_RWops *f, bool rsrc_fork, int32 &offset, int32 &length)
{
	// Check header
	SDL_RWseek(f, 0, SEEK_SET);
	uint32 id = SDL_ReadBE32(f);
	uint32 version = SDL_ReadBE32(f);
	if (id != 0x00051600 || version != 0x00020000)
		return false;
	
	// Find fork
	uint32 req_id = rsrc_fork ? 2 : 1;
	SDL_RWseek(f, 0x18, SEEK_SET);
	int num_entries = SDL_ReadBE16(f);
	while (num_entries--) {
		uint32 id = SDL_ReadBE32(f);
		int32 ofs = SDL_ReadBE32(f);
		int32 len = SDL_ReadBE32(f);
		//printf(" entry id %d, offset %d, length %d\n", id, ofs, len);
		if (id == req_id) {
			offset = ofs;
			length = len;
			return true;
		}
	}
	return false;
}


bool is_macbinary(SDL_RWops *f, int32 &data_length, int32 &rsrc_length)
{
	// This recognizes up to macbinary III (0x81)
	SDL_RWseek(f, 0, SEEK_SET);
	uint8 header[128];
	if (SDL_RWread(f, header, 1, 128) != 128)
	{
		return false;
	}
	
	if (header[0] || header[1] > 63 || header[74]  || header[123] > 0x81)
		return false;
	
	// Check CRC
	uint16 crc = 0;
	for (int i=0; i<124; i++) {
		uint16 data = header[i] << 8;
		for (int j=0; j<8; j++) {
			if ((data ^ crc) & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
			data <<= 1;
		}
	}
	//printf("crc %02x\n", crc);
	if (crc != ((header[124] << 8) | header[125]))
		return false;
	
	// CRC valid, extract fork sizes
	data_length = (header[83] << 24) | (header[84] << 16) | (header[85] << 8) | header[86];
	rsrc_length = (header[87] << 24) | (header[88] << 16) | (header[89] << 8) | header[90];
	return true;
}


// Structure for open resource file
struct file_resource_t
{
	file_resource_t() : fh(nullptr) {}
	file_resource_t(SDL_RWops *file) : fh(file) {}
	file_resource_t(const file_resource_t &other) { fh = other.fh; }
	~file_resource_t() {}

	const file_resource_t &operator=(const file_resource_t &other)
	{
		if (this != &other) fh = other.fh;
		return *this;
	}

	bool read_map();
	size_t count_resources(uint32 type) const;
	void get_resource_id_list(uint32 type, std::vector<int> &ids) const;
	bool get_resource(uint32 type, int id, LoadedResource &rsrc) const;
	bool get_ind_resource(uint32 type, int index, LoadedResource &rsrc) const;
	bool has_resource(uint32 type, int id) const;

	SDL_RWops *fh;		// Opened resource file

	typedef std::map<int, uint32> id_map_t;			// Maps resource ID to offset to resource data
	typedef std::map<uint32, id_map_t> type_map_t;	// Maps resource type to ID map

	type_map_t types;	// Map of all resource types found in file
};


// List of open resource files
static std::list<file_resource_t*> opened_resource_files;
static std::list<file_resource_t*>::iterator current_resource_files_iterator; // TODO: hanging on to open file handles is one thing; keeping iterators is... I don't even


/*
 *  Find file in list of opened files
 */

static std::list<file_resource_t *>::iterator find_file_resource_t(SDL_RWops *f)
{
    std::list<file_resource_t *>::iterator i, end = opened_resource_files.end();
	for (i=opened_resource_files.begin(); i!=end; i++) {
		file_resource_t *r = *i;
		if (r->fh == f)
			return i;
	}
	return opened_resource_files.end();
}

// external resources: for Marathon 1, this is the App's exported resource fork ('term' terminal texts, 'text' strings, etc); in Marathon 2, the Images file contains picts and other shared assets which simplifies modding
ResourceFile external_resource_file;

void open_m1_external_resources_file(const ao_path& path)
{
	ao_err err = external_resource_file.open(path); // TODO: error handling?
    if (err)
    {
        log_warning_f("couldn't open: '%s'", path.c_str());
    }
}

static void close_external_resources()
{
	external_resource_file.Close();
}

/*
 *  Initialize resource management
 */

void initialize_resources()
{
	atexit(close_external_resources);
}


/*
 *  Read and parse resource map from file
 */

bool file_resource_t::read_map()
{
	SDL_RWseek(fh, 0, SEEK_END);
	uint32 file_size = SDL_RWtell(fh);
	SDL_RWseek(fh, 0, SEEK_SET);
	uint32 fork_start = 0;

        if(file_size < 16)
        {
            log_error_f("file too small (%d bytes) to be valid", file_size);
            return false;
        }

	// Determine file type (AppleSingle and MacBinary II files are handled transparently)
	int32 offset, data_length, rsrc_length;
	if (is_applesingle(fh, true, offset, rsrc_length)) {
       // log_trace("file is_applesingle");
		fork_start = offset;
		file_size = offset + rsrc_length;
	} else if (is_macbinary(fh, data_length, rsrc_length)) {
        //log_trace("file is_macbinary");
		fork_start = 128 + ((data_length + 0x7f) & ~0x7f);
		file_size = fork_start + rsrc_length;
	}
        else
            log_trace("file is raw resource fork format");

	// Read resource header
	SDL_RWseek(fh, fork_start, SEEK_SET);
	uint32 data_offset = SDL_ReadBE32(fh) + fork_start;
	uint32 map_offset = SDL_ReadBE32(fh) + fork_start;
	uint32 data_size = SDL_ReadBE32(fh);
	uint32 map_size = SDL_ReadBE32(fh);
//    log_dump_f("resource header: data offset %d, map_offset %d, data_size %d, map_size %d", data_offset, map_offset, data_size, map_size);

	// Verify integrity of resource header
	if (data_offset >= file_size || map_offset >= file_size ||
	    data_offset + data_size > file_size || map_offset + map_size > file_size) {
        log_trace("file's resource header corrupt");
		return false;
	}

	// Read map header
	SDL_RWseek(fh, map_offset + 24, SEEK_SET);
	uint32 type_list_offset = map_offset + SDL_ReadBE16(fh);
	//uint32 name_list_offset = map_offset + SDL_ReadBE16(f);
	//printf(" type_list_offset %d, name_list_offset %d\n", type_list_offset, name_list_offset);

	// Verify integrity of map header
	if (type_list_offset >= file_size) {
        log_trace("file's resource map header corrupt");
		return false;
	}

	// Read resource type list
	SDL_RWseek(fh, type_list_offset, SEEK_SET);
	int num_types = SDL_ReadBE16(fh) + 1;
	for (int i=0; i<num_types; i++) {

		// Read type list item
		uint32 type = SDL_ReadBE32(fh);
		int num_refs = SDL_ReadBE16(fh) + 1;
		uint32 ref_list_offset = type_list_offset + SDL_ReadBE16(fh);
		//printf("  type %c%c%c%c, %d refs\n", type >> 24, type >> 16, type >> 8, type, num_refs);

		// Verify integrity of item
		if (ref_list_offset >= file_size) {
            log_trace("file's resource type list corrupt");
			return false;
		}

		// Create ID map for this type
		id_map_t &id_map = types[type];

		// Read reference list
		uint32 cur = SDL_RWtell(fh);
		SDL_RWseek(fh, ref_list_offset, SEEK_SET);
		for (int j=0; j<num_refs; j++) {

			// Read list item
			int id = SDL_ReadBE16(fh);
			SDL_RWseek(fh, 2, SEEK_CUR);
			uint32 rsrc_data_offset = data_offset + (SDL_ReadBE32(fh) & 0x00ffffff);
			//printf("   id %d, rsrc_data_offset %d\n", id, rsrc_data_offset);

			// Verify integrify of item
			if (rsrc_data_offset >= file_size) {
                log_trace("file's resource reference list corrupt");
				return false;
			}

			// Add ID to map
			id_map[id] = rsrc_data_offset;

			SDL_RWseek(fh, 4, SEEK_CUR);
		}
		SDL_RWseek(fh, cur, SEEK_SET);
	}
	return true;
}



// TODO: what is this doing that couldn't be done with DataFile?
//  Open resource file, set current file to the newly opened one
// TODO: AO's file management needs to be clarified (it shouldn't need to keep any files open except film recording and logging)
static SDL_RWops* try_to_open_resource_file_at_path(const ao_path& path)
{
    SDL_RWops* f = SDL_RWFromFile(path.c_str(), "rb");
    if (f)
    {
        // Successful, create file_resource_t object and read resource map
        file_resource_t *r = new file_resource_t(f);
        if (r->read_map())
        {
            // Successful, add file to list of open files
            opened_resource_files.push_back(r);
            current_resource_files_iterator = --opened_resource_files.end();
            
            // ZZZ: this exists mostly to help the user understand (via log_contexts) which of
            // potentially several copies of a resource fork is actually being used.
            //log_note_f("Opened resource file (%p) at: %s", f, path.c_str());
        }
        else
        {
            // Error reading resource map
            delete r;
            SDL_RWclose(f);
            return NULL;
        }
    }
    else
    {
    //    log_note_f("file could not be opened: %s", path.c_str()); // this is unhelpful as AO optimistically calls open on every possible file location, few of which actually have a file
    }
    return f;
}


// Open file, try <name>.rsrc first, then <name>.resources, then <name>/rsrc then <name> // TODO: the order below is different to comment (<name>.rsrc, <name>.resources, <name>, <name>/rsrc); which is appropriate?
SDL_RWops* open_resource_file(const ao_path& path) // used in ResourceFile.open and directly in Font.cpp
{
    SDL_RWops* fh = nullptr;

    ao_path rsrc_path = path;
    rsrc_path.replace_extension(".rsrc");
    fh = try_to_open_resource_file_at_path(rsrc_path);
    
    if (!fh)
    {
        ao_path resources_path = path;
        resources_path.replace_extension(".resources");
        fh = try_to_open_resource_file_at_path(resources_path);
    }
    if (!fh)
    {
        fh = try_to_open_resource_file_at_path(path);
    }
    if (!fh)
    {
        ao_path darwin_rsrc_path = path;
        darwin_rsrc_path /= "..namedfork";
        darwin_rsrc_path /= "rsrc";
        fh = try_to_open_resource_file_at_path(darwin_rsrc_path);
    }
    return fh;
}


void close_file_resource(SDL_RWops *file)
{
	if (!file) return; // TODO: in what situation could `file` be null? null checks like this one do not inspure great confidence

	// Find file in list
    std::list<file_resource_t *>::iterator i = find_file_resource_t(file);
	if (i != opened_resource_files.end()) {

		// Remove it from the list, close the file and delete the file_resource_t
		file_resource_t *r = *i;
		SDL_RWclose(r->fh);
		opened_resource_files.erase(i);
		delete r;

		current_resource_files_iterator = opened_resource_files.empty() ? decltype(current_resource_files_iterator){}
                                                                        : --opened_resource_files.end();
	}
}


/*
 *  Return current resource file
 */

SDL_RWops *get_current_resource_file()
{
	file_resource_t *r = *current_resource_files_iterator;
	assert_fail(r, "");
	return r->fh;
}


/*
 *  Set current resource file
 */

void use_file_resource(SDL_RWops *file)
{
    std::list<file_resource_t *>::iterator i = find_file_resource_t(file);
	assert_fail(i != opened_resource_files.end(), "");
	current_resource_files_iterator = i;
}


/*
 *  Count number of resources of given type
 */

size_t file_resource_t::count_resources(uint32 type) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i == types.end())
		return 0;
	else
		return i->second.size();
}

size_t count_1_resources(uint32 type)
{
	return (*current_resource_files_iterator)->count_resources(type);
}

size_t count_resources(uint32 type)
{
	if (!opened_resource_files.size())
		return 0;
	size_t count = 0;
    std::list<file_resource_t *>::const_iterator i = current_resource_files_iterator, begin = opened_resource_files.begin();
	while (true) {
		count += (*i)->count_resources(type);
		if (i == begin)
			break;
		i--;
	}
	return count;
}


/*
 *  Get list of id of resources of given type
 */

void file_resource_t::get_resource_id_list(uint32 type, std::vector<int> &ids) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j, end = i->second.end();
		for (j=i->second.begin(); j!=end; j++)
			ids.push_back(j->first);
	}
}

void get_1_resource_id_list(uint32 type, std::vector<int> &ids)
{
	ids.clear();
	(*current_resource_files_iterator)->get_resource_id_list(type, ids);
}

void get_resource_id_list(uint32 type, std::vector<int> &ids)
{
	ids.clear();
	if (!opened_resource_files.size())
		return;
    std::list<file_resource_t *>::const_iterator i = current_resource_files_iterator, begin = opened_resource_files.begin();
	while (true) {
		(*i)->get_resource_id_list(type, ids);
		if (i == begin)
			break;
		i--;
	}
}


/*
 *  Get resource data (must be freed with free())
 */

bool file_resource_t::get_resource(uint32 type, int id, LoadedResource &rsrc) const
{
	rsrc.Unload();

	// Find resource in map
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j = i->second.find(id);
		if (j != i->second.end()) {

			// Found, read data size
			SDL_RWseek(fh, j->second, SEEK_SET);
			uint32 size = SDL_ReadBE32(fh);

			// Allocate memory and read data
			void* p = ao_malloc(size);
			SDL_RWread(fh, p, 1, size);
			rsrc.p = p;
			rsrc.size = size;

//			fprintf(stderr, "get_resource type %c%c%c%c, id %d -> data %p, size %d\n", type >> 24, type >> 16, type >> 8, type, id, p, size);
			return true;
		}
	}
	return false;
}

bool get_1_resource(uint32 type, int id, LoadedResource &rsrc)
{
	return (*current_resource_files_iterator)->get_resource(type, id, rsrc);
}

bool get_resource(uint32 type, int id, LoadedResource &rsrc)
{
	if (!opened_resource_files.size())
		return false;
    std::list<file_resource_t *>::const_iterator i = current_resource_files_iterator, begin = opened_resource_files.begin();
	while (true) {
		bool found = (*i)->get_resource(type, id, rsrc);
		if (found)
			return true;
		if (i == begin)
			break;
		i--;
	}
	return false;
}


/*
 *  Get resource data by index (must be freed with free())
 */

bool file_resource_t::get_ind_resource(uint32 type, int index, LoadedResource &rsrc) const
{
	rsrc.Unload();

	// Find resource in map
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		if (index < 1 || index > int(i->second.size()))
			return false;
		id_map_t::const_iterator j = i->second.begin();
		for (int k=1; k<index; k++)
			++j;

		// Read data size
		SDL_RWseek(fh, j->second, SEEK_SET);
		uint32 size = SDL_ReadBE32(fh);

		// Allocate memory and read data
		void* p = ao_malloc(size);
		SDL_RWread(fh, p, 1, size);
		rsrc.p = p;
		rsrc.size = size;

//		fprintf(stderr, "get_ind_resource type %c%c%c%c, index %d -> data %p, size %d\n", type >> 24, type >> 16, type >> 8, type, index, p, size);
		return true;
	}
	return false;
}

bool get_1_ind_resource(uint32 type, int index, LoadedResource &rsrc)
{
	return (*current_resource_files_iterator)->get_ind_resource(type, index, rsrc);
}

bool get_ind_resource(uint32 type, int index, LoadedResource &rsrc)
{
	if (!opened_resource_files.size())
		return false;
    std::list<file_resource_t *>::const_iterator i = current_resource_files_iterator, begin = opened_resource_files.begin();
	while (true) {
		bool found = (*i)->get_ind_resource(type, index, rsrc);
		if (found)
			return true;
		if (i == begin)
			break;
		i--;
	}
	return false;
}


/*
 *  Check if resource is present
 */

bool file_resource_t::has_resource(uint32 type, int id) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j = i->second.find(id);
		if (j != i->second.end())
			return true;
	}
	return false;
}

bool has_1_resource(uint32 type, int id)
{
	return (*current_resource_files_iterator)->has_resource(type, id);
}

bool has_resource(uint32 type, int id)
{
	if (!opened_resource_files.size())
		return false;
    std::list<file_resource_t *>::const_iterator i = current_resource_files_iterator, begin = opened_resource_files.begin();
	while (true) {
		if ((*i)->has_resource(type, id))
			return true;
		if (i == begin)
			break;
		i--;
	}
	return false;
}
