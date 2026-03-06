/*
 physics_wad.cpp (was IMPORT_DEFINITIONS.C)
 
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

#include "physics_wad.h"

#include "find_files.hpp"
#include "crc.h"
#include "tags.h"
#include "map.h"
#include "interface.h"
#include "map_wad.h"
#include "wad.h"
#include "game_errors.h"
#include "shell.h"
#include "preferences.h"
#include "DataFile.hpp"
#include "shell.h"
#include "preferences.h"

// LP: get all the unpacker definitions
#include "monsters.h"
#include "effects.h"
#include "projectiles.h"
#include "player.h"
#include "weapons.h"
#include "physics_models.h"

#include "AStream.h"

#define IMPORT_STRUCTURE



static ao_path current_external_physics_path;


static wad_data* get_physics_wad_data(bool& bungie_physics);

static void import_physics_wad_data(wad_data* wad);

static void import_m1_physics_data();

static void import_m1_physics_data_from_network(uint8 *data, uint32 length);

static bool physics_file_is_m1();

static bool open_current_physics_file(DataFile& file) { return file.open(current_external_physics_path) == no_err; }


// -----------------------------------------------------------------------------------------
// public

void load_default_physics()
{
    init_monster_definitions();
    init_effect_definitions();
    init_projectile_definitions();
    init_physics_constants();
    init_weapon_definitions();
}
    
    
void set_external_physics_file(const ao_path& File)
{
    current_external_physics_path = File;
}


void load_external_physics_file()
{
	load_default_physics();

	if (physics_file_is_m1())
	{
		import_m1_physics_data();
	}
	else
	{
		bool bungie_physics;
        wad_data* wad = get_physics_wad_data(bungie_physics);
		if (wad)
		{
			import_physics_wad_data(wad);
			
			free_wad(wad);
		}
	}
}


#define M1_PHYSICS_MAGIC_COOKIE (0xDEAFDEAF)

void* get_network_physics_buffer(int64_t* physics_length)
{
	if (physics_file_is_m1())
	{
		bool success = false;
		uint8 *data = NULL;
		DataFile PhysicsFile;
        if (open_current_physics_file(PhysicsFile))
		{
            *physics_length = PhysicsFile.get_length();
			*physics_length += 8;
			data = ao_malloc(*physics_length);
            SDL_RWops *ops = SDL_RWFromMem(data, *physics_length);
            success = SDL_WriteBE32(ops, uint32(M1_PHYSICS_MAGIC_COOKIE));
            if (success) success = SDL_WriteBE32(ops, uint32(*physics_length - 8));
            SDL_RWclose(ops);
            if (success) PhysicsFile.read(*physics_length - 8, &data[8]);
			free(data);
		}
		if (!success)
		{
			*physics_length = 0;
			return NULL;
		}
		return data;
	}
	
	short SavedType, SavedError = get_game_error(&SavedType);
	void *data= get_flat_data(current_external_physics_path, false, 0);
	set_game_error(SavedType, SavedError);
	
	if(data)
	{
		*physics_length= get_flat_data_length(data);
	} else {
		*physics_length= 0;
	}
	
	return data;
}


void load_physics_from_network_physics_buffer(void* data)
{
	load_default_physics();

	if (data)
	{
		// check for M1 physics
		SDL_RWops *ops = SDL_RWFromConstMem(data, 8);
		uint32 cookie = SDL_ReadBE32(ops);
		if(cookie == M1_PHYSICS_MAGIC_COOKIE)
		{
			uint32 length= SDL_ReadBE32(ops);
			SDL_RWclose(ops);
			uint8 *s= (uint8 *)data;
			import_m1_physics_data_from_network(&s[8], length);
			return;
		}
		else
		{
			SDL_RWclose(ops);
		}

		struct wad_header header;
		struct wad_data *wad;
	
		wad= inflate_flat_data(data, &header);
		if(wad)
		{
			import_physics_wad_data(wad);
			free_wad(wad); /* Note that the flat data points into the wad. */
		}
	}
}


uint32_t get_external_physics_file_checksum()
{
    DataFile file;
    file.open(current_external_physics_path);
	uint32_t checksum = calculate_crc_for_file(current_external_physics_path);
    file.close();
    return checksum;
}



// -----------------------------------------------------------------------------------------
// private


static wad_data* get_physics_wad_data(bool& is_bungie_physics)
{
    DataFile PhysicsFile;
    if (PhysicsFile.open(current_external_physics_path)) return nullptr;

    wad_data* wad = nullptr;

    wad_header header;
    if (read_wad_header(PhysicsFile, &header) && is_valid_physics_data(header.data_version))
    {
        wad = read_indexed_wad_from_file(PhysicsFile, &header, 0, true);
        is_bungie_physics = header.data_version == BUNGIE_PHYSICS_DATA_VERSION;
    }
    
    return wad;
}


static void import_physics_wad_data(wad_data* wad)
{
	// LP: this code is copied out of map_wad.c
	size_t data_length;
	byte *data;
	size_t count;
	
	data= (unsigned char *)extract_type_from_wad(wad, MONSTER_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_monster_definition;
	assert_fail(count*SIZEOF_monster_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_MONSTER_TYPES, "");
	if (data_length > 0)
	{
		unpack_monster_definition(data,count);
	}
	
	data= (unsigned char *)extract_type_from_wad(wad, EFFECTS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_effect_definition;
	assert_fail(count*SIZEOF_effect_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_EFFECT_TYPES, "");
	if (data_length > 0)
	{
		unpack_effect_definition(data,count);
	}
	
	data= (unsigned char *)extract_type_from_wad(wad, PROJECTILE_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_projectile_definition;
	assert_fail(count*SIZEOF_projectile_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_PROJECTILE_TYPES, "");
	if (data_length > 0)
	{
		unpack_projectile_definition(data,count);
	}
	
	data= (unsigned char *)extract_type_from_wad(wad, PHYSICS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_physics_constants;
	assert_fail(count*SIZEOF_physics_constants == data_length, "");
	assert_fail(count <= get_number_of_physics_models(), "");
	if (data_length > 0)
	{
		unpack_physics_constants(data,count);
	}
	
	data= (unsigned char*) extract_type_from_wad(wad, WEAPONS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_weapon_definition;
	assert_fail(count*SIZEOF_weapon_definition == data_length, "");
	assert_fail(count <= get_number_of_weapon_types(), "");
	if (data_length > 0)
	{
		unpack_weapon_definition(data,count);
	}
}


static void import_m1_physics_data()
{
	DataFile PhysicsFile;
	if (!open_current_physics_file(PhysicsFile)) return;

	int64_t position  = 0;
	int64_t length = PhysicsFile.get_length();

	while (position < length)
	{
		std::vector<uint8> header(12);
		PhysicsFile.read(header.size(), &header[0]);
		AIStreamBE header_stream(&header[0], header.size());

		uint32 tag;
		uint16 count;
		uint16 size;

		header_stream >> tag;
		header_stream.ignore(4); // unused
		header_stream >> count;
		header_stream >> size;

		std::vector<uint8> data(count * size);
		PhysicsFile.read(data.size(), &data[0]);
		switch (tag) 
		{
		case M1_MONSTER_PHYSICS_TAG:
			unpack_m1_monster_definition(&data[0], count);
			break;
		case M1_EFFECTS_PHYSICS_TAG:
			unpack_m1_effect_definition(&data[0], count);
			break;
		case M1_PROJECTILE_PHYSICS_TAG:
			unpack_m1_projectile_definition(&data[0], count);
			break;
		case M1_PHYSICS_PHYSICS_TAG:
			unpack_m1_physics_constants(&data[0], count);
			break;
		case M1_WEAPONS_PHYSICS_TAG:
			unpack_m1_weapon_definition(&data[0], count);
			break;
		}
        position = PhysicsFile.get_position();
	}
}


static void import_m1_physics_data_from_network(uint8 *data, uint32 length)
{
	int32 position = 0;
	while (position < length)
	{
		AIStreamBE header_stream(&data[position], 12);
		position += 12;

		uint32 tag;
		uint16 count;
		uint16 size;

		header_stream >> tag;
		header_stream.ignore(4); // unused
		header_stream >> count;
		header_stream >> size;

		switch (tag)
		{
			case M1_MONSTER_PHYSICS_TAG:
				unpack_m1_monster_definition(&data[position], count);
				break;
			case M1_EFFECTS_PHYSICS_TAG:
				unpack_m1_effect_definition(&data[position], count);
				break;
			case M1_PROJECTILE_PHYSICS_TAG:
				unpack_m1_projectile_definition(&data[position], count);
				break;
			case M1_PHYSICS_PHYSICS_TAG:
				unpack_m1_physics_constants(&data[position], count);
				break;
			case M1_WEAPONS_PHYSICS_TAG:
				unpack_m1_weapon_definition(&data[position], count);
				break;
		}
		position += count * size;
	}
}


static bool physics_file_is_m1()
{
    DataFile PhysicsFile;
    if (!open_current_physics_file(PhysicsFile)) return false;
    
    uint32_t tag = SDL_ReadBE32(PhysicsFile.borrow_rwops());
    switch (tag)
    {
        case M1_MONSTER_PHYSICS_TAG:
        case M1_EFFECTS_PHYSICS_TAG:
        case M1_PROJECTILE_PHYSICS_TAG:
        case M1_PHYSICS_PHYSICS_TAG:
        case M1_WEAPONS_PHYSICS_TAG:
            return true;
        default:
            return false;
    }
}

