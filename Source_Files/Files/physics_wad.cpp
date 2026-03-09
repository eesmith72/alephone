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


static ao_err get_physics_wad_data(wad_data*& wad);

static void import_physics_wad_data(wad_data* wad);

static void import_m1_physics_data();

static void import_m1_physics_data_from_network(uint8 *data, uint32 length);

static bool physics_file_is_m1();

static bool open_current_physics_file(DataFile& file)
{
    return file.open(current_external_physics_path) == no_err;
}


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


void load_external_physics_file() // this is [presumably] allowed to fail as an external physics file is optional (it doesn't distinguish missing file from unsupported version or other read errors though)
{
	load_default_physics();

	if (physics_file_is_m1())
	{
		import_m1_physics_data();
	}
	else
	{
        wad_data* wad;
        ao_err err = get_physics_wad_data(wad);
        if (err) return;
		
        import_physics_wad_data(wad);
        free_wad(wad);
	}
}


#define M1_PHYSICS_MAGIC_COOKIE (0xDEAFDEAF)


ao_err get_network_physics_buffer(uint8_t*& data, int64_t& physics_length)
{
    ao_err err = no_err;
    physics_length = 0;
    
	if (physics_file_is_m1())
	{
		DataFile PhysicsFile;
        err = PhysicsFile.open(current_external_physics_path);
        if (err) return err;
        
        physics_length = (int32_t)PhysicsFile.get_length() + 8; // add space for magic cookie
        data = ao_malloc(physics_length);
        SDL_RWops *ops = SDL_RWFromMem(data, (int32_t)physics_length);
        bool success = SDL_WriteBE32(ops, uint32(M1_PHYSICS_MAGIC_COOKIE)) && SDL_WriteBE32(ops, uint32(physics_length - 8));
        SDL_RWclose(ops);
        if (success)
        {
            PhysicsFile.read(physics_length - 8, &data[8]);
        }
		else
        {
            free(data);
            data = nullptr;
            err = STRID(strERRORS, cantWriteFile);
        }
	}
    else // M2 physics
    {
        err = get_flat_data(current_external_physics_path, 0, data); // this can fail, returning null (should return ao_err since there's DataFile.open())
        
        if (data) { physics_length = get_flat_data_length(data); }
        
    }
    return err;
}


void load_physics_from_network_physics_buffer(void* flat_data)
{
	load_default_physics();

    // check for M1 physics
    SDL_RWops *ops = SDL_RWFromConstMem(flat_data, 8);
    uint32 cookie = SDL_ReadBE32(ops);
    if(cookie == M1_PHYSICS_MAGIC_COOKIE)
    {
        uint32 length= SDL_ReadBE32(ops);
        SDL_RWclose(ops);
        uint8 *s= (uint8 *)flat_data;
        import_m1_physics_data_from_network(&s[8], length);
        return;
    }
    SDL_RWclose(ops);

    wad_header_t header;
    wad_data* wad = inflate_flat_data((uint8_t*)flat_data, &header); // on return, the wad_data struct owns the flat data and will dispose it when free_wad is called
    assert_fail(wad, "inflate_flat_data should never return nullptr");
    
    import_physics_wad_data(wad);
    free_wad(wad); // this disposes the flat_data automatically
}


uint32_t get_external_physics_file_checksum()
{
    DataFile file;
    if (file.open(current_external_physics_path)) return 0; // not ideal
	uint32_t checksum = calculate_crc_for_file(current_external_physics_path);
    file.close();
    return checksum;
}



// -----------------------------------------------------------------------------------------
// private


static ao_err get_physics_wad_data(wad_data*& wad)
{
    DataFile PhysicsFile;
    ao_err err = PhysicsFile.open(current_external_physics_path);
    if (err) return err;

    wad_header_t header;
    err = read_wad_header(PhysicsFile, &header);
    if (err) return err;
    if (!is_valid_physics_data(header.data_version)) return errUnknownWadVersion;
    
    return read_indexed_wad_from_file(PhysicsFile, &header, 0, true, wad);
}


static void import_physics_wad_data(wad_data* wad)
{
	// LP: this code is copied out of map_wad.c
	size_t data_length;
	byte *data;
	size_t count;
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, MONSTER_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_monster_definition;
	assert_fail(count*SIZEOF_monster_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_MONSTER_TYPES, "");
	if (data_length > 0)
	{
		unpack_monster_definition(data,count);
	}
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, EFFECTS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_effect_definition;
	assert_fail(count*SIZEOF_effect_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_EFFECT_TYPES, "");
	if (data_length > 0)
	{
		unpack_effect_definition(data,count);
	}
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, PROJECTILE_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_projectile_definition;
	assert_fail(count*SIZEOF_projectile_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_PROJECTILE_TYPES, "");
	if (data_length > 0)
	{
		unpack_projectile_definition(data,count);
	}
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, PHYSICS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_physics_constants;
	assert_fail(count*SIZEOF_physics_constants == data_length, "");
	assert_fail(count <= get_number_of_physics_models(), "");
	if (data_length > 0)
	{
		unpack_physics_constants(data,count);
	}
	
	data= (unsigned char*) get_wad_resource_for_tag(wad, WEAPONS_PHYSICS_TAG, &data_length);
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

