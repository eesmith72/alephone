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
#include "interface.hpp"
#include "map_wad.h"
#include "wad.h"
#include "shell.h"
#include "preferences.hpp"
#include "DataFile.hpp"
#include "shell.h"
#include "preferences.hpp"

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


static void import_m1_physics_data();

static void try_to_load_physics_from_m1_wad_data(uint8 *data, uint32 length);

static bool physics_file_is_m1();


// -----------------------------------------------------------------------------------------
// public

void load_default_physics() // embedded M2 Physics data
{
    init_monster_definitions();
    init_effect_definitions();
    init_projectile_definitions();
    init_physics_constants();
    init_weapon_definitions();
}
    
    
void set_external_physics_file(const ao_path& path)
{
    current_external_physics_path = path;
}


void try_to_load_external_physics() // this is [presumably] allowed to fail as an external physics file is optional (it doesn't distinguish missing file from unsupported version or other read errors though)
{
    // this assumes the default (built-in) physics has already been loaded
    
	if (physics_file_is_m1())
	{
        DataFile PhysicsFile;
        ao_err err = PhysicsFile.open(current_external_physics_path);
        if (err) return;
        
        int64_t length = PhysicsFile.get_length();
        std::vector<uint8_t> data(length);
        PhysicsFile.read(data.size(), &data[0]);
        try_to_load_physics_from_m1_wad_data(data.data(), (uint32_t)length);
	}
	else
	{
        
        DataFile PhysicsFile;
        ao_err err = PhysicsFile.open(current_external_physics_path);
        if (err) return; // typically file not found, which we ignore

        wad_header_t header;
        err = read_wad_header(PhysicsFile, &header);
        if (err) return;
        if (!is_supported_physics_wad_version(header.data_version)) return; // errUnknownWadVersion
        
        wad_data* wad;
        err = read_indexed_wad_from_file(PhysicsFile, &header, 0, true, wad);
        if (err) return;
		
        try_to_load_physics_from_m2_wad_data(wad);
        free_wad(wad);
	}
}


#define M1_PHYSICS_MAGIC_COOKIE (0xDEAFDEAF)


ao_err export_physics_to_network_physics_buffer(uint8_t*& data, int64_t& physics_length)
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
        err = get_flat_data_from_wad_file(current_external_physics_path, 0, data); // this can fail, returning null (should return ao_err since there's DataFile.open())
        
        if (data) { physics_length = get_flat_data_length(data); }
        
    }
    return err;
}


void import_physics_from_network_physics_buffer(void* flat_data)
{
	load_default_physics();

    // check for M1 physics // TODO: wonder why physics data isn't in M2 format (other than export_physics_to_network_physics_buffer is reading from local file rather than re-packing from data structures)
    SDL_RWops *ops = SDL_RWFromConstMem(flat_data, 8); // 4 bytes = magic code, 4 bytes = length
    uint32 cookie = SDL_ReadBE32(ops);
    if (cookie == M1_PHYSICS_MAGIC_COOKIE)
    {
        uint32 length = SDL_ReadBE32(ops);
        uint8 *s= (uint8 *)flat_data;
        try_to_load_physics_from_m1_wad_data(&s[8], length);
    }
    else // M2 physics
    {
        wad_header_t header;
        wad_data* wad = inflate_flat_data((uint8_t*)flat_data, &header); // on return, the wad_data struct owns the flat data and will dispose it when free_wad is called
        assert_fail(wad, "inflate_flat_data should never return nullptr");
        
        try_to_load_physics_from_m2_wad_data(wad);
        free_wad(wad); // this disposes the flat_data automatically
    }
    SDL_RWclose(ops);
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


void try_to_load_physics_from_m2_wad_data(wad_data* wad)
{
	// LP: this code is copied out of map_wad.c
    uint8_t* data;
	size_t data_length;
	size_t count;
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, MONSTER_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_monster_definition;
	assert_fail(count*SIZEOF_monster_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_MONSTER_TYPES, "");
	if (data_length > 0) { unpack_m2_monster_definition(data,count); }
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, EFFECTS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_effect_definition;
	assert_fail(count*SIZEOF_effect_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_EFFECT_TYPES, "");
	if (data_length > 0) { unpack_m2_effect_definition(data,count); }
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, PROJECTILE_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_projectile_definition;
	assert_fail(count*SIZEOF_projectile_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_PROJECTILE_TYPES, "");
	if (data_length > 0) { unpack_m2_projectile_definition(data,count); }
	
	data= (unsigned char *)get_wad_resource_for_tag(wad, PHYSICS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_physics_constants;
	assert_fail(count*SIZEOF_physics_constants == data_length, "");
	assert_fail(count <= get_number_of_physics_models(), "");
	if (data_length > 0) { unpack_m2_physics_constants(data,count); }
	
	data= (unsigned char*) get_wad_resource_for_tag(wad, WEAPONS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_weapon_definition;
	assert_fail(count*SIZEOF_weapon_definition == data_length, "");
	assert_fail(count <= get_number_of_weapon_types(), "");
	if (data_length > 0) { unpack_m2_weapon_definition(data,count); }
}


// TODO: these 2 functions should be consolidated: read the entire file into one buffer instead of this piecemeal nonsense
static void import_m1_physics_data()
{
	DataFile PhysicsFile;
    ao_err err = PhysicsFile.open(current_external_physics_path);
    if (err) return;
    
	int64_t position  = 0;
	int64_t length = PhysicsFile.get_length();

	while (position < length)
	{
		std::vector<uint8> header(12);
		PhysicsFile.read(header.size(), &header[0]);
		AIStreamBE header_stream(&header[0], (uint32_t)header.size());

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


static void try_to_load_physics_from_m1_wad_data(uint8 *data, uint32 length)
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
    ao_err err = PhysicsFile.open(current_external_physics_path);
    if (err) return false;
    
    SDL_RWops* fh = PhysicsFile.borrow_rwops();
    uint32_t tag = SDL_ReadBE32(fh);
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

