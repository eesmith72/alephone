#ifndef __WAD_H
#define __WAD_H

/*
	WAD.H

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

	Thursday, June 30, 1994 10:55:20 PM

	Sunday, July 3, 1994 5:51:47 PM
	I wonder if I should include an element size in the entry_header structure...

	Tuesday, December 13, 1994 4:10:48 PM
	Included element size in the entry_header for the directory size.  The directory
	is application_specific_directory_data_size+sizeof(struct directory_entry).

Feb 3, 2000 (Loren Petrich):
	Defined "WADFILE_HAS_INFINITY_STUFF" as 4
	Changed CURRENT_WADFILE_VERSION to WADFILE_HAS_INFINITY_STUFF
		to achieve Marathon Infinity compatibility

Aug 12, 2000 (Loren Petrich):
	Using object-oriented file handler
*/

#include "cseries.h"

#include "DataFile.hpp"
#include "tags.h"


// from wad.h

#define PRE_ENTRY_POINT_WADFILE_VERSION (0)
#define WADFILE_HAS_DIRECTORY_ENTRY     (1)
#define WADFILE_SUPPORTS_OVERLAYS       (2)
#define WADFILE_HAS_INFINITY_STUFF      (4)
#define CURRENT_WADFILE_VERSION         (WADFILE_HAS_INFINITY_STUFF)

// from editor.h
#define MARATHON_ONE_DATA_VERSION       (0)
#define MARATHON_TWO_DATA_VERSION       (1)
#define MARATHON_INFINITY_DATA_VERSION  (2)
#define EDITOR_MAP_VERSION              (MARATHON_INFINITY_DATA_VERSION)

#define CURRENT_WADFILE_DATA_VERSION    (MARATHON_INFINITY_DATA_VERSION)


bool is_supported_wad_file_version(int16_t file_version, int16_t data_version)
{
    return (file_version >= PRE_ENTRY_POINT_WADFILE_VERSION && file_version <= CURRENT_WADFILE_VERSION)
        && (data_version >= MARATHON_ONE_DATA_VERSION && data_version <= CURRENT_WADFILE_DATA_VERSION);
}


#define MAXIMUM_DIRECTORY_ENTRIES_PER_FILE 64
#define MAXIMUM_WADFILE_NAME_LENGTH 64
#define MAXIMUM_UNION_WADFILES 16
#define MAXIMUM_OPEN_WADFILES 3



typedef uint32 WadDataType;



struct wad_header_t { /* 128 bytes */
	int16 version;									/* Used internally */
	int16 data_version;								/* Used by the data.. */
	char file_name[MAXIMUM_WADFILE_NAME_LENGTH];
	uint32 checksum;
	int32 directory_offset;
	int16 wad_count;
	int16 application_specific_directory_data_size;
	int16 entry_header_size;
	int16 directory_entry_base_size;
	uint32 parent_checksum;	/* If non-zero, this is the checksum of our parent, and we are simply modifications! */
	int16 unused[20];
    
    bool is_supported() { return is_supported_wad_file_version(version, data_version); }
};
const int SIZEOF_wad_header = 128;	// don't trust sizeof()

struct old_directory_entry { /* 8 bytes */
	int32 offset_to_start; /* From start of file */
	int32 length; /* Of total level */
};
const int SIZEOF_old_directory_entry = 8;

struct directory_entry { /* >=10 bytes */
	int32 offset_to_start; /* From start of file */
	int32 length; /* Of total level */
	int16 index; /* For inplace modification of the wadfile! */
};
const int SIZEOF_directory_entry = 10;

struct old_entry_header { /* 12 bytes */
	WadDataType tag;
	int32 next_offset; /* From current file location-> ie directory_entry.offset_to_start+next_offset */
	int32 length; /* Of entry */

	/* Element size? */
	
	/* Data follows */
};
const int SIZEOF_old_entry_header = 12;

struct entry_header { /* 16 bytes */
	WadDataType tag;
	int32 next_offset; /* From current file location-> ie directory_entry.offset_to_start+next_offset */
	int32 length; /* Of entry */
	int32 offset; /* Offset for inplace expansion of data */

	/* Element size? */
	
	/* Data follows */
};
const int SIZEOF_entry_header = 16;

/* ---------- Memory Data structures ------------ */
struct tag_data {
	WadDataType tag;		/* What type of data is this? */
	byte *data; 			/* Offset into the wad.. */
	int32 length;			/* Length of the data */
	int32 offset;			/* Offset for patches */
};

/* This is what a wad * actually is.. */
struct wad_data {
	short tag_count;			/* Tag count */
	short padding;
	byte *read_only_data;		/* If this is non NULL, we are read only.... */
	struct tag_data *tag_data;	/* Tag data array */
};

/* ----- miscellaneous functions */


/* Find out how many wads there are in the map */
short number_of_wads_in_file(const ao_path& File); /* returns -1 on error */



/* ----- Read File functions */

/* Read the header from the wad file */
ao_err read_wad_header(DataFile& file, wad_header_t* header);

/* Read the indexed wad from the file */
ao_err read_indexed_wad_from_file(DataFile& file, wad_header_t* header, int16_t index, bool read_only, wad_data*& read_wad);

/* Properly deal with the memory.. */
void free_wad(struct wad_data *wad);

int32 get_size_of_directory_data(struct wad_header_t *header);

/* -----  Read Wad functions */

/* Given a wad, extract the given tag from it */
uint8_t* get_wad_resource_for_tag(wad_data* wad, WadDataType type, size_t *length);

/* Calculate the length of the wad */
int32 calculate_wad_length(struct wad_header_t *file_header, struct wad_data *wad);

/* Note wad_count and directory offset in the header! better be correct! */
void *get_indexed_directory_data(struct wad_header_t *header, short index, void *directories);

uint8_t *read_directory_data(DataFile& file, wad_header_t* header);

uint32 read_wad_file_checksum(const ao_path& File);
uint32 read_wad_file_parent_checksum(const ao_path& File);


/* ------------ Flat wad functions */

// These functions are used for transferring data, and it completely encapsulates a given wad from a given file

ao_err get_flat_data(const ao_path& File, short wad_index, uint8_t*& data);

int32_t get_flat_data_length(uint8_t* data);

/* This is how you dispose of it-> you inflate it, then use free_wad() */
wad_data* inflate_flat_data(uint8_t* data, wad_header_t* header); // this returns header as well


/* ------------  Write File functions */

void fill_default_wad_header(const ao_path& File, short wadfile_version, short data_version,
                             short wad_count, short application_directory_data_size, wad_header_t *header);

void write_wad_header(DataFile& file, wad_header_t* header);


bool write_directorys(DataFile& OFile, struct wad_header_t *header, void *entries);

void calculate_and_store_wadfile_checksum(DataFile& OFile);

void write_wad(DataFile& OFile, struct wad_header_t *file_header, wad_data *wad, int32 offset);

void set_indexed_directory_offset_and_length(wad_header_t *header, void *entries, short index,
                                             int32 offset, int32 length, short wad_index);


struct wad_data *append_data_to_wad(wad_data *wad, WadDataType type, const void *data, size_t size, size_t offset);

void remove_tag_from_wad(struct wad_data *wad, WadDataType type);
	
// debug function
void dump_wad(struct wad_data *wad);



#endif

