/*
GAME_WAD.C

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

// TODO: A good modern UX/UX design is to decouple solo/coop scenario setup from pvp setup, relocating UI for selecting a scenario/netmap into Begin New Game and Gather Network Game. Every saved game and film file should contain a full list of package dependencies, so the user can open any one of those files and it immediately Just Works.

// This needs to do the right thing on save game, which is storing the precalculated crap.

#include "cseries.hpp"

#include "find_files.hpp"

#include "map.h"
#include "monsters.h"
#include "network.h"
#include "projectiles.h"
#include "effects.h"
#include "player.h"
#include "platforms.h"
#include "flood_map.h"
#include "scenery.h"
#include "lightsource.h"
#include "media.h"
#include "weapons.h"
#include "shell.h"
#include "preferences.hpp"
#include "DataFile.hpp"
#include "vbl.h" // reset_recording

#include "tags.h"
#include "wad.h"
#include "map_wad.h"
#include "physics_wad.h" // load_default_physics
#include "interface.hpp"
//#include "hud_manager.h"
#include "computer_interface.h" // for loading/saving terminal state.
#include "images.h"
#include "shell.h"
#include "preferences.hpp"
#include "SoundManager.h"
#include "Plugins.h"
#include "ephemera.h"
#include "SoundsPatch.h"

// LP change: added chase-cam init and render allocation
#include "ChaseCam.h"
#include "render.h"

#include "lua_script.h"
#include "XML_LevelScript.h"

// For packing and unpacking some of the stuff
#include "Packing.h"

#include "Music.h"


#include "automap_data.hpp"


// unify the save game code into one structure.

/* -------- local globals */

static ao_path MapFileSpec; // this is distinct from environment_preferences.map_file; with any luck the environment prefs will go away once Shapes, Sounds, Map, addons, etc adopt a single standard package format



static std::vector<polygon_data> PolygonListCopy;
static std::vector<platform_data> PlatformListCopy;


static void scan_and_add_scenery(void);
static wad_data* build_export_wad(wad_header_t* header, int32* length);
static wad_data* build_save_game_wad(wad_header_t* header, int32* length);

static void load_objects(uint8 *map_objects, size_t count, short version);


static void scan_and_add_platforms(uint8 *platform_static_data, size_t count, short version);

static void unpack_directory_data(uint8 *Stream, directory_data& object);

//static uint8 *pack_directory_data(uint8 *Stream, directory_data *Objects, int Count);

/* ------------------------ Net functions */



ao_err get_flat_wad_for_level_of_current_map(int16_t level_number, uint8_t*& flat_data)
{
    assert_fail(!MapFileSpec.empty(), "map file path is not set");
	
	return get_flat_data_from_wad_file(MapFileSpec, level_number, flat_data);
}

/* ---------------------- End Net Functions ----------- */


void set_current_map_path(const ao_path& path)
{
	// Do whatever parameter restoration is specified before changing the file // TODO: this is nasty; when the map changes, all previous script states should be cleared so everything loads afresh
    if (!MapFileSpec.empty()) load_restore_level_scripts();

	MapFileSpec = path;
	open_map_file_resources(path);

	Plugins::instance()->set_map_checksum(get_current_map_checksum());
    
    read_scripts_from_current_map(); // TODO: hurm...
}


const ao_path& get_current_map_path()
{
    return MapFileSpec;
}



// TODO: improving package management should ensure that a missing package is detected as soon as a saved game/film file is selected by user, at which point they can be prompted with download option/error/cancel
ao_err set_current_map_path_to_file_with_checksum(uint32_t checksum)
{
    if (!MapFileSpec.empty() && get_current_map_checksum() == checksum) return no_err;
	
    ao_path map_path = find_scenario_file({match_file_type(_typecode_map), match_checksum(checksum)});
    if (map_path.empty())
    {
        log_error_f("Can't find an installed Map file with checksum: %x", checksum);
        return STRID(strERRORS, cantFindMap);
    }
    else
    {
        set_current_map_path(map_path);
        return no_err;
    }
}


ao_err get_dynamic_data_from_saved_game_file(const ao_path& path, dynamic_world_t& result)
{
	DataFile MapFile;
    ao_err err = MapFile.open(path);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(MapFile, &header);
    if (err) return err;
    
    wad_data* wad;
    err = read_indexed_wad_from_file(MapFile, &header, 0, true, wad);
    if (!wad) return errMapCantBeRead;
    
    err = get_dynamic_data_from_wad(wad, result);
    assert_fail(err == no_err, "dynamic WAD data not found");
    free_wad(wad);
    
    return err;
}



// TODO: WADFile class; this should extend/wrap DataFile and allow unpack functions to read directly from it; none of LP's stupid '"streams" that are actually bigass malloced buffers'
ao_err load_level_from_map_wad_file(const ao_path& map_path, short level_index, bool is_saved_game) // note: saved game files always use level_index = 0
{
    if (map_path.empty()) return STRID(gameError, errMapFileNotSet);
    
    DataFile map_file;
    ao_err err = map_file.open(map_path);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(map_file, &header);
    if (err) return err;
    
    if (level_index < 0 || level_index >= header.wad_count) return STRID(gameError, errWadIndexOutOfRange);
    
    wad_data* wad;
    err = read_indexed_wad_from_file(map_file, &header, level_index, true, wad);
    if (err) return err;
    
    initialize_level_from_wad_data(wad, is_saved_game, header.data_version); // unpack the level into memory
    free_wad(wad);
    
    // M1 terminals are stored in App's resource fork (or Shapes file's resource fork in case of Trojan)
    // (these are loaded after the map as initialize_level_from_wad_data will clear existing terms)
    // This doesn't interact with the Map file at all, so it's safe to pass any map file.
    if (header.data_version == M1_MAP_WAD_VERSION) // saved game files use newest version
    {
        load_m1_computer_terminals_for_level(level_index);
    }
    
	return no_err;
}

// keep these around for level export
static std::vector<static_platform_data> static_platforms;

extern bool ok_to_reset_scenery_solidity;


short get_player_starting_location_and_facing(short team, short index, object_location& location)
{
	short count = 0;
    for (auto& saved_object : SavedObjectList)
	{
		if (saved_object.type == _saved_player && (saved_object.index == team || team == NONE)) // index=NONE means use any starting location
        {
            if (count == index)
            {
                location.p             = saved_object.location;
                location.polygon_index = saved_object.polygon_index;
                location.yaw           = saved_object.facing;
                location.pitch         = 0;
                location.flags         = saved_object.flags;
                return count;
            }
            count++;
		}
	}
    throw_out_of_bounds_f("Tried to place: %d only %d starting pts.", index, count);
}


short get_number_of_players_starting_location_and_facing(short team, short index) // index=NONE means use any starting location
{
    short count = 0;
    for (auto& saved_object : SavedObjectList)
    {
        if (saved_object.type == _saved_player && (saved_object.index == team || team == NONE)) { count++; }
    }
    return count;
}



uint32_t get_current_map_checksum()
{
	DataFile MapFile;
    ao_err err = MapFile.open(MapFileSpec);
    assert_fail(err == no_err, "failed to open current map");

    wad_header_t header;
	read_wad_header(MapFile, &header);
	return header.checksum;
}






ao_err get_next_level_for_game_types(int32_t game_type_flags, int16_t& start_at_index, level_identity& level_info)
{
    assert_fail(!MapFileSpec.empty(), "not set");
    DataFile MapFile;
    ao_err err = MapFile.open(MapFileSpec);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(MapFile, &header);
    if (err) return err;
    
    bool success = false;
    if (header.application_specific_directory_data_size == SIZEOF_directory_data) // New-style wad
    {
        
        uint8_t* total_directory_data = read_directory_data(MapFile, &header);
        
        assert_fail(total_directory_data, "no data");
        for (int16_t index = start_at_index; index < header.wad_count; index++)
        {
            uint8 *p = (uint8 *)get_indexed_directory_data(&header, index, total_directory_data);
            directory_data directory;
            unpack_directory_data(p, directory);
            
            // Find the flags that match.
            if (directory.entry_point_flags & game_type_flags)
            {
                // This one is valid!
                level_info.level_number = index;
                level_info.utf8_level_name = directory.level_name;
                
                start_at_index = index + 1;
                success = true;
                break; // Out of the for loop
            }
        }
        free(total_directory_data);
    }
    else // Old-style wad, find the index
    {
        err = 7; // TODO: errWADIndexNotFound or whatever
        for (int16_t index = start_at_index; !success && index < header.wad_count; index++)
        {
            wad_data* wad;
            err = read_indexed_wad_from_file(MapFile, &header, index, true, wad);
            if (err) continue; // IF this has the proper type.
            
            size_t length;
            uint8_t* p = (uint8*)get_wad_resource_for_tag(wad, MAP_INFO_TAG, &length);
            assert_fail(length == SIZEOF_static_data, "wrong size");
            static_world_t map_info;
            map_info.unpack_stream(p);
            
            // single-player Marathon 1 levels aren't always marked
            if (header.data_version == M1_MAP_WAD_VERSION && map_info.entry_point_flags == 0)
            {
                map_info.entry_point_flags = _single_player_entry_point;
            }
            // Marathon 1 handled (then-unused) coop flag differently
            if (header.data_version == M1_MAP_WAD_VERSION)
            {
                if (map_info.entry_point_flags & _single_player_entry_point)
                {
                    map_info.entry_point_flags |= _multiplayer_cooperative_entry_point;
                }
                if (map_info.entry_point_flags & _multiplayer_carnage_entry_point)
                {
                    map_info.entry_point_flags &= ~_multiplayer_cooperative_entry_point;
                }
            }
            
            if (map_info.entry_point_flags & game_type_flags) // This one is valid!
            {
                level_info.level_number = index;
                level_info.utf8_level_name = map_info.level_name;
                start_at_index = index + 1;
                err = no_err;
            }
            
            free_wad(wad);
        }
    }
    return success;
}


// Get vector of map entry points matching given type
bool get_all_levels_for_game_types(std::vector<level_identity> &vec, int32 game_type_flags)
{
	vec.clear();
    
	// Open map file
	assert_fail(!MapFileSpec.empty(), "not set");
	DataFile MapFile;
	if (MapFile.open(MapFileSpec)) return false;

	// Read header
	wad_header_t header;
	if (!read_wad_header(MapFile, &header)) return false;

	bool success = false;
	if (header.application_specific_directory_data_size == SIZEOF_directory_data)
    {
		// New style wad, read directory data
        uint8_t *total_directory_data = read_directory_data(MapFile, &header);
		assert_fail(total_directory_data, "no data");

		// Push matching directory entries into vector
		for (int i=0; i<header.wad_count; i++)
        {
			uint8 *p = (uint8 *)get_indexed_directory_data(&header, i, total_directory_data);
			directory_data directory;
			unpack_directory_data(p, directory);

			if (directory.entry_point_flags & game_type_flags)
            {
				// This one is valid
				level_identity point;
				point.level_number = i;
				point.utf8_level_name = directory.level_name;
				vec.push_back(point);
				success = true;
			}
		}
		free(total_directory_data);
	}
    else
    {
		// Old style wad
		for (int i=0; i<header.wad_count; i++)
        {
            wad_data* wad;
            ao_err err = read_indexed_wad_from_file(MapFile, &header, i, true, wad);
			if (err) continue;

			// Read map_info data
			size_t length;
			uint8 *p = get_wad_resource_for_tag(wad, MAP_INFO_TAG, &length);
			assert_fail(length == SIZEOF_static_data, "wrong size");
			static_world_t map_info;
            map_info.unpack_stream(p);

			// single-player Marathon 1 levels aren't always marked
			if (header.data_version == M1_MAP_WAD_VERSION && map_info.entry_point_flags == 0)
            {
                map_info.entry_point_flags = _single_player_entry_point;
            }
			// Marathon 1 handled (then-unused) coop flag differently
			if (header.data_version == M1_MAP_WAD_VERSION)
			{
				if (map_info.entry_point_flags & _single_player_entry_point)
                {
                    map_info.entry_point_flags |= _multiplayer_cooperative_entry_point;
                }
				if (map_info.entry_point_flags & _multiplayer_carnage_entry_point)
                {
                    map_info.entry_point_flags &= ~_multiplayer_cooperative_entry_point;
                }
			}

			if (map_info.entry_point_flags & game_type_flags)
            {
				// This one is valid
				level_identity point;
				point.level_number = i;
				point.utf8_level_name = map_info.level_name;
				vec.push_back(point);
				success = true;
			}
				
			free_wad(wad);
		}
	}

	return success;
}




/* -------------------- Private or map editor functions */



ao_err export_level(const ao_path& path) // TODO: what is difference between this and save_game_to_file?
{
    ao_path tmp_path = path;
    ao_err err = make_temp_file(tmp_path);
    if (err) return err;
    
	// Fill in the default wad header (we are using File instead of TempFile to get the name right in the header)
    wad_header_t header;
	fill_default_wad_header(path, CURRENT_WADFILE_VERSION, M2_MAP_WAD_VERSION, 1, 0, &header); // TODO: why is this using an old data version instead of CURRENT_MAP_WAD_VERSION?
    
    DataFile SaveFile;
    err = SaveFile.open(tmp_path, DataFile::mode_binary_write);
    if (err) return err;
    
    /* Write out the new header */
    write_wad_header(SaveFile, &header);
        
    int32_t wad_length, offset = SIZEOF_wad_header;
    
    wad_data* wad = build_export_wad(&header, &wad_length);
    if (wad)
    {
        directory_entry entry;
        set_indexed_directory_offset_and_length(&header, &entry, 0, offset, wad_length, 0);
        
        write_wad(SaveFile, &header, wad, offset);

        // Update the new header
        offset += wad_length;
        header.directory_offset = offset;
        write_wad_header(SaveFile, &header);
        write_directorys(SaveFile, &header, &entry);
        
        free_wad(wad);
    }

    calculate_and_store_wadfile_checksum(SaveFile);
    SaveFile.close();
    
    if (!err)
    {
        // We can't delete open files on Windows, so close the current level before we overwrite it.
        bool restore_images = false;
        if (path == MapFileSpec)
        {
            close_map_file_resources();
            restore_images = true;
        }
        err = rename_file(tmp_path, path);
        if (!err && restore_images)
        {
            open_map_file_resources(path);
        }
    }
    
	return err;
}



// The current mapfile should be set to the save game file // EES: wut? the path is to the final save file
ao_err save_game_to_file(const ao_path& path, const std::string& metadata, const std::string& imagedata)
{
	ao_err err = no_err;
    
    directory_entry entries[2]; // map wad and metadata entries

	// Save off the random seed.
	dynamic_world.random_seed = get_random_seed();
    
    ao_path temp_path;
    err = make_temp_file(temp_path);
    if (err) return err;
	
	/* Fill in the default wad header (we are using File instead of TempFile to get the name right in the header) */
    wad_header_t header;
	fill_default_wad_header(path, CURRENT_WADFILE_VERSION, CURRENT_MAP_WAD_VERSION, 2, 0, &header);
		
    DataFile temp_file;
    err = temp_file.open(temp_path, DataFile::mode_binary_write);
    if (err) return err; // TODO: this shouldn't fail, but what about deleting temp file if it does?
    
    write_wad_header(temp_file, &header);
        
    int32_t wad_length, offset = SIZEOF_wad_header;
    wad_data* wad = build_save_game_wad(&header, &wad_length);
    // Set the entry data
    set_indexed_directory_offset_and_length(&header, entries, 0, offset, wad_length, 0);
    
    // Save it
    write_wad(temp_file, &header, wad, offset);
    
    // Update the new header
    offset += wad_length;
    header.directory_offset = offset;
    header.original_map_file_checksum = read_wad_file_checksum(MapFileSpec);
    
    // Create metadata wad
    wad_data* meta_wad = build_meta_game_wad(metadata, imagedata, &header, &wad_length);

    set_indexed_directory_offset_and_length(&header, entries, 1, offset, wad_length, SAVE_GAME_METADATA_INDEX);
    
    write_wad(temp_file, &header, meta_wad, offset);
    
    offset += wad_length;
    header.directory_offset = offset;

    write_wad_header(temp_file, &header);
    write_directorys(temp_file, &header, entries);
    
    free_wad(meta_wad);
    free_wad(wad);

    temp_file.close();
    std::error_code code;
    std::filesystem::rename(temp_file.get_path(), path, code);
	if (code)
    {
        err = code.value(); // TODO: what error code?
    }
	return err;
}


/* -------- static functions */
static void scan_and_add_platforms(uint8 *platform_static_data, size_t count, short version)
{
	PlatformList.resize(count);
	objlist_clear(PlatformList.data(), count);

	static_platforms.resize(count);
	unpack_static_platform_data(platform_static_data, static_platforms.data(), count, version);

	for (short loop = 0; loop < PolygonList.size(); loop++)
	{
        polygon_data *polygon = &PolygonList[loop];
        
		if (polygon->type==_polygon_is_platform)
		{
			/* Search and find the extra data.  If it is not there, use the permutation for */
			/* backwards compatibility! */

			size_t platform_static_data_index;
			for(platform_static_data_index = 0; platform_static_data_index<count; ++platform_static_data_index)
			{
				if (static_platforms[platform_static_data_index].polygon_index == loop)
				{
					new_platform(&static_platforms[platform_static_data_index], loop);
					break;
				}
			}
			
			/* DIdn't find it- use a standard platform */
			if(platform_static_data_index==count)
			{
				polygon->permutation= 1;
				new_platform(get_defaults_for_platform_type(polygon->permutation), loop);
			}	
		}
		++polygon;
	}
}


extern void unpack_lua_states(uint8*, size_t);


// ENTRY POINT FOR PARSING MAP LEVEL DATA

void initialize_level_from_wad_data(wad_data* wad, bool is_saved_game, short version)
{
    // Order of unpacking is important!
    
	size_t data_length;
	uint8 *data;
	size_t count;
	bool is_preprocessed_map = false;

	assert_fail_f(version == M3_MAP_WAD_VERSION || version == M2_MAP_WAD_VERSION || version == M1_MAP_WAD_VERSION, "Map version: %d", version); // TODO: there should be one ingress point for ALL WADs and, again, ALWAYS check version there
    
    // EES: ripped out all the stupid makework asserts
    
    // EES: moved unpacking embedded physics to top of initialize_level_from_wad_data, although ideally it should be outside it
    load_default_physics(); // reset gameworld Physics to M2 default
    
    // TODO: why does this care about networked vs non-networked?
    // try to load external Physics file, if one exists (note: M1 scenarios MUST provide Physics.phys file to work correctly)
    if (!game_is_networked()) { try_to_load_external_physics(); }
    try_to_load_physics_from_m2_wad_data(wad);
    // EES: somewhere below it's going to try loading the embedded physics (if the level has one)
    // Q. are embedded physics always single level-specific, or is it possible to embed a single physics that applies to all levels?
    
    // This one method call is that remains of initialize_map_for_new_level; it clears level-specific fields but preserves some (e.g. tick_count, total_civilian_causalties) which persist when teleporting into a new level. (When loading a new level, this function unpacks only static level data from the Map file; only saved games contain dynamic data.)
    dynamic_world.initialize_for_new_level();
    
    
	// GEOMETRY
    
	data = get_wad_resource_for_tag(wad, POINT_TAG, &data_length);
    if (data_length > 0)
	{
        unpack_point_data(data, data_length / SIZEOF_world_point2d);
	}
    else
    {
		data = get_wad_resource_for_tag(wad, ENDPOINT_DATA_TAG, &data_length);
		unpack_endpoint_data(data, data_length / SIZEOF_endpoint_data);
        is_preprocessed_map = version > M1_MAP_WAD_VERSION;
	}

	data = get_wad_resource_for_tag(wad, LINE_TAG, &data_length);
    size_t line_count = data_length / SIZEOF_line_data;
    unpack_line_data(data, line_count);

	data = get_wad_resource_for_tag(wad, SIDE_TAG, &data_length);
    unpack_side_data(data, data_length / SIZEOF_side_data, version);

    data = get_wad_resource_for_tag(wad, POLYGON_TAG, &data_length);
    size_t polygon_count = data_length / SIZEOF_polygon_data;
    unpack_polygon_data(data, polygon_count, version);
    
    player_automap_visibility.configure(line_count, polygon_count); // this needs to be here as it gets populated below with saved game state
    
    allocate_render_memory(EndpointList.size(), LineList.size(), PolygonList.size());
    allocate_flood_map_memory(PolygonList.size()); // this needs to be here as precalculate_map_indexes is dependent
    
    // TODO: LightList still behaves like a fixed-size array with 'used' slots
	
	// Extract the lightsources
	if (is_saved_game) // When you are restoring a game, the actual light structure is set
	{
        data = get_wad_resource_for_tag(wad, LIGHTSOURCE_TAG, &data_length);
        unpack_dynamic_light_data(data, data_length / SIZEOF_dynamic_light_data, version);
	}
	else
	{
		data = get_wad_resource_for_tag(wad, LIGHTSOURCE_TAG, &data_length);
        unpack_static_light_data(data, data_length / (version ? SIZEOF_m1_static_light_data : SIZEOF_m2_static_light_data), version);
		//	HACK!!!!!!!!!!!!!!! vulcan doesn’t NONE .first_object field after adding scenery // TODO: still needed? who knows; vulcan predated Forge iirc
        for (auto& polygon : PolygonList) { polygon.first_object= NONE; }
	}

    data = get_wad_resource_for_tag(wad, ANNOTATION_TAG, &data_length);
    unpack_map_annotations(data, data_length / SIZEOF_map_annotation);

    data = get_wad_resource_for_tag(wad, OBJECT_TAG, &data_length);
    unpack_map_objects(data, data_length / SIZEOF_map_object, version);

    data = get_wad_resource_for_tag(wad, MAP_INFO_TAG, &data_length);
    static_world.unpack_stream(data);
    static_world.ball_in_play = false;

    if (version == M1_MAP_WAD_VERSION)
    {
        if (static_world.mission_flags & _mission_exploration)
        {
            static_world.mission_flags &= ~_mission_exploration;
            static_world.mission_flags |= _mission_exploration_m1;
        }
        if (static_world.mission_flags & _mission_rescue)
        {
            static_world.mission_flags &= ~_mission_rescue;
            static_world.mission_flags |= _mission_rescue_m1;
        }
        if (static_world.mission_flags & _mission_repair)
        {
            static_world.mission_flags &= ~_mission_repair;
            static_world.mission_flags |= _mission_repair_m1;
        }
        if (static_world.environment_flags & _environment_rebellion)
        {
            static_world.environment_flags &= ~_environment_rebellion;
            static_world.environment_flags |= _environment_rebellion_m1;
        }
        static_world.environment_flags |= _environment_glue_m1|_environment_ouch_m1|_environment_song_index_m1|_environment_terminals_stop_time|_environment_activation_ranges|_environment_m1_weapons;
        
    }

    if (static_world.environment_flags & _environment_song_index_m1) {
	    Music::instance()->SetClassicLevelMusic(static_world.song_index);
    }

	// game difficulty info
    data = get_wad_resource_for_tag(wad, OBJECT_PLACEMENT_STRUCTURE_TAG, &data_length);
    count = data_length / SIZEOF_object_frequency_definition; // should be 64 (32 items + 32 monsters) but may be 0 if placement chunk is missing, in which case fill with empty
    assert_fail(data_length == 0 || data_length == 2 * MAXIMUM_OBJECT_TYPES * SIZEOF_object_frequency_definition, "");
    unpack_placement_data(data, count);
    
    data = get_wad_resource_for_tag(wad, TERMINAL_DATA_TAG, &data_length);
    unpack_m2_computer_terminals(data, data_length);

    data = get_wad_resource_for_tag(wad, MEDIA_TAG, &data_length);
    unpack_media_data(data, data_length / SIZEOF_media_data, is_saved_game);

    data = get_wad_resource_for_tag(wad, AMBIENT_SOUND_TAG, &data_length);
    unpack_ambient_sound_image_data(data, data_length / SIZEOF_ambient_sound_image_data);
    
	data = get_wad_resource_for_tag(wad, RANDOM_SOUND_TAG, &data_length);
    unpack_random_sound_image_data(data, data_length / SIZEOF_random_sound_image_data);

    data = get_wad_resource_for_tag(wad, SHAPE_PATCH_TAG, &data_length);
	set_shapes_patch_data(data, data_length);

    data = get_wad_resource_for_tag(wad, SOUND_PATCH_TAG, &data_length);
	set_sounds_patch_data(data, data_length);
    
    // unpack level scripts that are embedded in the level wad; TODO: there is a LOT of confusion over who does what and when; it is also unclear if the loaded MML commands are order-sensitive wrt rest of level loading below (i.e. can MML commands influence any of the following?)
    
    data = get_wad_resource_for_tag(wad, MMLS_TAG, &data_length);
	unpack_mml_level_scripts_data(data, data_length);

    data = get_wad_resource_for_tag(wad, LUAS_TAG, &data_length);
	unpack_lua_level_scripts_data(data, data_length);

	data = get_wad_resource_for_tag(wad, LUA_STATE_TAG, &data_length);
	unpack_lua_states(data, data_length);
    
    
	init_ephemera(PolygonList.size());
    
	PolygonListCopy = PolygonList; // must be done before polygons heights are modified below (presumably this means current platform positions in a saved game)
    
    
    // TODO: is this checkpoints or something else?
    // Map indexes: start off with none of them (of course), but reserve a size equal to the map index length // TODO: why? if it's sparse, use a map
    MapIndexList.reserve(PolygonList.size() * 32 + 1024); // Give the map indexes a whole bunch of memory (cause we can't calculate it) // TODO: really?

	// If we are restoring the game, then we need to add the dynamic data
	if (is_saved_game)
    {
        data = get_wad_resource_for_tag(wad, MAP_INDEXES_TAG, &data_length);
        unpack_map_index_data(data, data_length / sizeof(short));
        
        // dynamic world
        ao_err err = unpack_player_data_from_wad(wad);
        assert_fail(err, "Missing player data in WAD.");
        err = get_dynamic_data_from_wad(wad, dynamic_world);
        assert_fail(err, "Missing dynamic world data in WAD.");
        
        data = get_wad_resource_for_tag(wad, OBJECT_STRUCTURE_TAG, &data_length);
        unpack_object_data(data, data_length / SIZEOF_object_data);
        
        // automap line+polygon vectors are already resized to correct size [we hope]
        data = (uint8_t*)get_wad_resource_for_tag(wad, AUTOMAP_LINES, &data_length);
        unpack_automap_line_data(data, data_length / SIZEOF_line_data);
        
		data = (uint8_t*)get_wad_resource_for_tag(wad, AUTOMAP_POLYGONS, &data_length);
        unpack_automap_polygon_data(data, data_length / SIZEOF_polygon_data);
        
        data = get_wad_resource_for_tag(wad, MONSTERS_STRUCTURE_TAG, &data_length);
        unpack_monster_data(data, data_length / SIZEOF_monster_data);

        data = get_wad_resource_for_tag(wad, EFFECTS_STRUCTURE_TAG, &data_length);
		unpack_effect_data(data, data_length / SIZEOF_effect_data);

        data = get_wad_resource_for_tag(wad, PROJECTILES_STRUCTURE_TAG, &data_length);
        unpack_projectile_data(data, data_length / SIZEOF_projectile_data);
		
        data = get_wad_resource_for_tag(wad, PLATFORM_STRUCTURE_TAG, &data_length);
		unpack_platform_data(data, data_length / SIZEOF_platform_data); // TODO: there is another unpack_platform_data call later, in complete_loading_level
		
        data = get_wad_resource_for_tag(wad, WEAPON_STATE_TAG, &data_length);
		unpack_player_weapon_data(data, data_length / SIZEOF_player_weapon_data);
		
        data = get_wad_resource_for_tag(wad, TERMINAL_STATE_TAG, &data_length);
        unpack_player_terminal_state(data, data_length / SIZEOF_player_terminal_state);
		
        ok_to_reset_scenery_solidity = false;
	}
    else
    {
        player_automap_visibility.clear_all();
        
        uint8_t* static_platform_data = get_wad_resource_for_tag(wad, PLATFORM_STATIC_DATA_TAG, &data_length);
		size_t static_platform_count = data_length/SIZEOF_static_platform_data;
		
		uint8_t* actual_platform_data = get_wad_resource_for_tag(wad, PLATFORM_STRUCTURE_TAG, &data_length);
		size_t actual_platform_data_count = data_length/SIZEOF_platform_data;
        
        // Scan, add the doors, recalculate, and generally tie up all loose ends.
        // Recalculate all the redundant crap; must be done before platforms/doors/etc.
        MapIndexList.clear();
        if (version == M1_MAP_WAD_VERSION)
        {
            assert_fail(!is_preprocessed_map, "");
            for (short i = 0; i < PolygonList.size(); i++)  { recalculate_redundant_polygon_data(i);  }
            for (short i = 0; i < LineList.size(); i++)     { recalculate_redundant_line_data(i);     }
            for (short i = 0; i < EndpointList.size(); i++) { recalculate_redundant_endpoint_data(i); }

            precalculate_map_indexes(); // in map_constructors.cpp
        }
        else
        {
            assert_fail(is_preprocessed_map, "");
            data = get_wad_resource_for_tag(wad, MAP_INDEXES_TAG, &data_length);
            unpack_map_index_data(data, data_length / sizeof(short));
        }
        
        static_platforms.clear();

        // Add the platforms.
        if (static_platform_data || !actual_platform_data)
        {
            scan_and_add_platforms(static_platform_data, static_platform_count, version);
        }
        else
        {
            assert_fail(actual_platform_data, "was null");
            unpack_platform_data(actual_platform_data, actual_platform_data_count);
            assert_fail(actual_platform_data_count == static_cast<size_t>(static_cast<int16>(actual_platform_data_count)), "bad count");
            assert_fail(0 <= static_cast<int16>(actual_platform_data_count), "bad count");
        }

        scan_and_add_scenery();
        ok_to_reset_scenery_solidity = true;
        
        // Gotta do this after recalculate redundant.
        if (version == M1_MAP_WAD_VERSION)
        {
            for (int32_t i = 0; i < SideList.size(); i++)
            {
                guess_side_lightsource_indexes(i);
                if (static_world.environment_flags & _environment_vacuum)
                {
                    side_data& side= SideList[i];
                    if (side.flags & _side_is_control_panel) { side.flags |= _side_is_m1_lighted_switch; }
                }
            }
        }
	}

	PlatformListCopy = PlatformList;
}


// TODO: eventually these could throw as they're not supposed to fail, but leaving as error codes for now
ao_err get_dynamic_data_from_wad(wad_data* wad, dynamic_world_t& result)
{
	size_t data_length;
	uint8_t* data = get_wad_resource_for_tag(wad, DYNAMIC_STRUCTURE_TAG, &data_length); // this can fail, errWadTagNotFound; however, there's a LOT of calls to it and not gonna change them all right now
    if (!data || data_length != SIZEOF_dynamic_data) { return STRID(gameError, errWadTagNotFound); }
	result.unpack_stream(data);
    return no_err;
}


int16_t get_number_of_players_from_wad(wad_data* wad) // saved game
{
    size_t data_length;
    get_wad_resource_for_tag(wad, PLAYER_STRUCTURE_TAG, &data_length);
    return data_length / SIZEOF_player_data;
}


ao_err unpack_player_data_from_wad(wad_data* wad) // saved game, presumably
{    
	size_t data_length;
	auto data = get_wad_resource_for_tag(wad, PLAYER_STRUCTURE_TAG, &data_length);
	auto count = data_length / SIZEOF_player_data;
    if (count == 0) { return STRID(gameError, errWadTagNotFound); }
	unpack_player_data(data, count);
    return no_err;
}


static void scan_and_add_scenery()
{
	for (auto& saved_object : SavedObjectList)
	{
		if (saved_object.type==_saved_object)
		{
			object_location location;
			location.p              = saved_object.location;
			location.flags          = saved_object.flags;
			location.yaw            = saved_object.facing;
			location.polygon_index  = saved_object.polygon_index;
			new_scenery(&location, saved_object.index);
		}
	}
}


struct save_game_data 
{
	uint32 tag;
	short unit_size;
	bool loaded_by_level;
};

#define NUMBER_OF_EXPORT_ARRAYS (sizeof(export_data)/sizeof(struct save_game_data))
save_game_data export_data[]=
{
	{ POINT_TAG, SIZEOF_world_point2d, true },
	{ LINE_TAG, SIZEOF_line_data, true },
	{ POLYGON_TAG, SIZEOF_polygon_data, true },
	{ SIDE_TAG, SIZEOF_side_data, true },
	{ LIGHTSOURCE_TAG, SIZEOF_m2_static_light_data, true, },
	{ ANNOTATION_TAG, SIZEOF_map_annotation, true },
	{ OBJECT_TAG, SIZEOF_map_object, true },
	{ MAP_INFO_TAG, SIZEOF_static_data, true },
	{ OBJECT_PLACEMENT_STRUCTURE_TAG, SIZEOF_object_frequency_definition, true },
	{ PLATFORM_STATIC_DATA_TAG, SIZEOF_static_platform_data, true },
	{ TERMINAL_DATA_TAG, sizeof(byte), true },
	{ MEDIA_TAG, SIZEOF_media_data, true }, // false },
	{ AMBIENT_SOUND_TAG, SIZEOF_ambient_sound_image_data, true },
	{ RANDOM_SOUND_TAG, SIZEOF_random_sound_image_data, true },
	{ SHAPE_PATCH_TAG, sizeof(byte), true },
	{ SOUND_PATCH_TAG, sizeof(byte), true },
//	{ PLATFORM_STRUCTURE_TAG, SIZEOF_platform_data, true },
};

#define NUMBER_OF_SAVE_ARRAYS (sizeof(save_data)/sizeof(struct save_game_data))
struct save_game_data save_data[]=
{
	{ ENDPOINT_DATA_TAG, SIZEOF_endpoint_data, true },
	{ LINE_TAG, SIZEOF_line_data, true },
	{ SIDE_TAG, SIZEOF_side_data, true },
	{ POLYGON_TAG, SIZEOF_polygon_data, true },
	{ LIGHTSOURCE_TAG, SIZEOF_dynamic_light_data, true }, // false },
	{ ANNOTATION_TAG, SIZEOF_map_annotation, true },
	{ OBJECT_TAG, SIZEOF_map_object, true },
	{ MAP_INFO_TAG, SIZEOF_static_data, true },
	{ OBJECT_PLACEMENT_STRUCTURE_TAG, SIZEOF_object_frequency_definition, true },
	{ MEDIA_TAG, SIZEOF_media_data, true }, // false },
	{ AMBIENT_SOUND_TAG, SIZEOF_ambient_sound_image_data, true },
	{ RANDOM_SOUND_TAG, SIZEOF_random_sound_image_data, true },
	{ TERMINAL_DATA_TAG, sizeof(byte), true },
	
	// LP addition: handling of physics models
	{ MONSTER_PHYSICS_TAG, SIZEOF_monster_definition, true},
	{ EFFECTS_PHYSICS_TAG, SIZEOF_effect_definition, true},
	{ PROJECTILE_PHYSICS_TAG, SIZEOF_projectile_definition, true},
	{ PHYSICS_PHYSICS_TAG, SIZEOF_physics_constants, true},
	{ WEAPONS_PHYSICS_TAG, SIZEOF_weapon_definition, true},

	// GHS: save the new embedded shapes
	{ SHAPE_PATCH_TAG, sizeof(byte), true },
	{ SOUND_PATCH_TAG, sizeof(byte), true },

	{ MMLS_TAG, sizeof(byte), true },
	{ LUAS_TAG, sizeof(byte), true },

	{ MAP_INDEXES_TAG, sizeof(short), true }, // false },
	{ PLAYER_STRUCTURE_TAG, SIZEOF_player_data, true }, // false },
	{ DYNAMIC_STRUCTURE_TAG, SIZEOF_dynamic_data, true }, // false },
	{ OBJECT_STRUCTURE_TAG, SIZEOF_object_data, true }, // false },
	{ AUTOMAP_LINES, sizeof(byte), true }, // false },
	{ AUTOMAP_POLYGONS, sizeof(byte), true }, // false },
	{ MONSTERS_STRUCTURE_TAG, SIZEOF_monster_data, true }, // false },
	{ EFFECTS_STRUCTURE_TAG, SIZEOF_effect_data, true }, // false },
	{ PROJECTILES_STRUCTURE_TAG, SIZEOF_projectile_data, true }, // false },
	{ PLATFORM_STRUCTURE_TAG, SIZEOF_platform_data, true }, // false },
	{ WEAPON_STATE_TAG, SIZEOF_player_weapon_data, true }, // false },
	{ TERMINAL_STATE_TAG, SIZEOF_player_terminal_state, true }, // false }

	{ LUA_STATE_TAG, sizeof(byte), true },
};



static uint8 *export_tag_to_global_array_and_size(uint32 tag, size_t *size)
{
	uint8 *array = NULL;
	size_t unit_size = 0;
	size_t count = 0;
	unsigned index;

	for (index=0; index<NUMBER_OF_EXPORT_ARRAYS; ++index)
	{
		if(export_data[index].tag==tag)
		{
			unit_size= export_data[index].unit_size;
			break;
		}
	}
	assert_fail(index != NUMBER_OF_EXPORT_ARRAYS, "");

	switch (tag)
	{
	case POINT_TAG:
		count = EndpointList.size();
		break;

	case LIGHTSOURCE_TAG:
            count = LightList.size();
		break;

	case PLATFORM_STATIC_DATA_TAG:
            count = PlatformList.size();
		break;

	case POLYGON_TAG:
		count = PolygonList.size();
		break;

	default:
            throw_ao_exception_f("bad WAD tag: %x", 1, tag);
		break;
	}

	// Allocate a temporary packed-data chunk;
	// indicate if there is nothing to be written
	*size= count*unit_size;
	if (*size > 0)
		array = new byte[*size];
	else
		return NULL;

	objlist_clear(array, *size);
	
	// An OK-to-alter version of that array pointer
	uint8 *temp_array = array;

	switch (tag)
	{
	case POINT_TAG:
		for (size_t loop = 0; loop < count; ++loop)
		{
			world_point2d& vertex = EndpointList[loop].vertex;
			ValueToStream(temp_array, vertex.x);
			ValueToStream(temp_array, vertex.y);
		}
		break;

	case LIGHTSOURCE_TAG:
		for (size_t loop = 0; loop < count; ++loop)
		{
			temp_array = pack_static_light_data(temp_array, &LightList[loop].static_data, 1);
		}
		break;

	case PLATFORM_STATIC_DATA_TAG:
		if (static_platforms.size() == count)
		{
			// export them directly as they came in
			pack_static_platform_data(array, &static_platforms[0], count);
		}
		else
		{
			for (const auto& platform : PlatformList)
			{
				// ghs: this belongs somewhere else
				static_platform_data static_data;
				obj_clear(static_data);
                static_data.type = platform.type;
                static_data.speed = platform.speed;
                static_data.delay = platform.delay;
				if (PLATFORM_GOES_BOTH_WAYS(&platform))
				{
                    static_data.maximum_height = platform.maximum_ceiling_height;
                    static_data.minimum_height = platform.minimum_floor_height;
				}
				else if (PLATFORM_COMES_FROM_FLOOR(&platform))
				{
                    static_data.maximum_height = platform.maximum_floor_height;
                    static_data.minimum_height = platform.minimum_floor_height;
				}
				else
				{
                    static_data.maximum_height = platform.maximum_ceiling_height;
                    static_data.minimum_height = platform.minimum_floor_height;
				}
                static_data.static_flags = platform.static_flags;
                static_data.polygon_index = platform.polygon_index;
                static_data.tag = platform.tag;

				temp_array = pack_static_platform_data(temp_array, &static_data, 1);
			}
		}
		break;

	case POLYGON_TAG:
		for (size_t loop = 0; loop < count; ++loop)
		{
			// Forge visual mode crashes if we don't do this
			polygon_data polygon = PolygonList[loop];
			polygon.first_object = NONE;
			temp_array = pack_polygon_data(temp_array, &polygon, 1);
		}
		break;

	default:
            throw_ao_exception_f("bad WAD tag: %x", 1, tag);
		break;
	}

	return array;
}

extern size_t save_lua_states();
extern void pack_lua_states(uint8*, size_t);


/* the sizes are the sizes to save in the file, be aware! */
// TODO: lotta slop: if it can write direct to stream, there is no need to allocate temp memory to hold it and it simplifies greatly; even simpler if chunks can be written contiguously (no need for separate export_data/save_data tables; just call pack_ functions directly)
static uint8 *tag_to_global_array_and_size(uint32 tag, size_t *size)
{
	uint8 *array= NULL;
	size_t unit_size = 0;
	size_t count = 0;
	unsigned index;
	
	for (index=0; index<NUMBER_OF_SAVE_ARRAYS; ++index)
	{
		if(save_data[index].tag==tag)
		{
			unit_size= save_data[index].unit_size;
			break;
		}
	}
	assert_fail(index != NUMBER_OF_SAVE_ARRAYS, "unit_size mismatch");
		
	switch (tag)
	{
		case ENDPOINT_DATA_TAG:
			count= EndpointList.size();
			break;
		case LINE_TAG:
			count= LineList.size();
			break;
		case SIDE_TAG:
            count= SideList.size();
			break;
		case POLYGON_TAG:
			count= PolygonList.size();
			break;
		case LIGHTSOURCE_TAG:
			count= LightList.size();
			break;
		case ANNOTATION_TAG:
            count= MapAnnotationList.size();
			break;
		case OBJECT_TAG:
			count= SavedObjectList.size();
			break;
		case MAP_INFO_TAG:
			count= 1;
			break;
		case PLAYER_STRUCTURE_TAG:
			count= get_number_of_players();
			break;
		case DYNAMIC_STRUCTURE_TAG:
			count= 1;
			break;
		case OBJECT_STRUCTURE_TAG:
            count= ObjectList.size();
			break;
		case MAP_INDEXES_TAG:
			count= MapIndexList.size();
			break;
		case AUTOMAP_LINES:
            count= player_automap_visibility.lines.size();
			break;
		case AUTOMAP_POLYGONS:
            count= player_automap_visibility.polygons.size();
			break;
		case MONSTERS_STRUCTURE_TAG:
            count= MonsterList.size();
			break;
		case EFFECTS_STRUCTURE_TAG:
            count= EffectList.size();
			break;
		case PROJECTILES_STRUCTURE_TAG:
            count= ProjectileList.size();
			break;
		case MEDIA_TAG:
			count= count_number_of_medias_used();
			break;
		case OBJECT_PLACEMENT_STRUCTURE_TAG:
            count= item_placement_info.size() + monster_placement_info.size();
			break;
		case PLATFORM_STRUCTURE_TAG:
            count= PlatformList.size();
			break;
		case AMBIENT_SOUND_TAG:
            count= AmbientSoundImageList.size();
			break;
		case RANDOM_SOUND_TAG:
            count= RandomSoundImageList.size();
			break;
		case TERMINAL_DATA_TAG:
			count= get_bytesize_of_packed_computer_terminals();
			break;
		case WEAPON_STATE_TAG:
			count= get_number_of_players();
			break;
		case TERMINAL_STATE_TAG:
			count= get_number_of_players();
			break;
		case MONSTER_PHYSICS_TAG:
			count= NUMBER_OF_MONSTER_TYPES;
			break;
		case EFFECTS_PHYSICS_TAG:
			count= NUMBER_OF_EFFECT_TYPES;
			break;
		case PROJECTILE_PHYSICS_TAG:
			count= NUMBER_OF_PROJECTILE_TYPES;
			break;
		case PHYSICS_PHYSICS_TAG:
			count= get_number_of_physics_models();
			break;
		case WEAPONS_PHYSICS_TAG:
			count= get_number_of_weapon_types();
			break;
		case SHAPE_PATCH_TAG:
			get_shapes_patch_data(count);
			break;
		case SOUND_PATCH_TAG:
			get_sounds_patch_data(count);
			break;
	    case MMLS_TAG:
            count = get_length_of_mml_level_scripts_data();
			break;
	    case LUAS_TAG:
			count = get_length_of_lua_level_scripts_data();
			break;
		case LUA_STATE_TAG:
			count= save_lua_states();
			break;
		default:
            throw_ao_exception_f("unknown WAD tag: %x", 1, tag);
			break;
	}
	
	// Allocate a temporary packed-data chunk;
	// indicate if there is nothing to be written
	*size= count*unit_size;
	if (*size > 0)
		array = new byte[*size];
	else
		return NULL;

	objlist_clear(array, *size);
	
	// An OK-to-alter version of that array pointer
	uint8 *temp_array = array;
	
	switch (tag)
	{
		case ENDPOINT_DATA_TAG:
			pack_endpoint_data(array,EndpointList.data(),count);
			break;
		case LINE_TAG:
			pack_line_data(array, LineList.data(), count);
			break;
		case SIDE_TAG:
			pack_side_data(array, SideList.data(), count);
			break;
		case POLYGON_TAG:
            pack_polygon_data(array, PolygonList.data(), count);
			break;
		case LIGHTSOURCE_TAG:
			pack_dynamic_light_data(array, LightList.data(), count);
			break;
		case ANNOTATION_TAG:
			pack_map_annotation(array, MapAnnotationList.data(), count);
			break;
		case OBJECT_TAG:
			pack_map_object(array, SavedObjectList.data(), count);
			break;
		case MAP_INFO_TAG:
            static_world.pack_stream(array);
			break;
		case PLAYER_STRUCTURE_TAG:
            pack_player_data(array, players.data(), count);
			break;
		case DYNAMIC_STRUCTURE_TAG:
            assert_fail(count == 1, "");
            dynamic_world.pack_stream(array);
			break;
		case OBJECT_STRUCTURE_TAG:
            pack_object_data(array, ObjectList.data(), count);
			break;
		case MAP_INDEXES_TAG:
			ListToStream(temp_array, MapIndexList.data(), count);
			break;
		case AUTOMAP_LINES:
			memcpy(array, player_automap_visibility.lines.data(), *size);
			break;
		case AUTOMAP_POLYGONS:
            memcpy(array, player_automap_visibility.polygons.data(), *size);
			break;
		case MONSTERS_STRUCTURE_TAG:
            pack_monster_data(array, MonsterList.data(), count);
			break;
		case EFFECTS_STRUCTURE_TAG:
			pack_effect_data(array, EffectList.data(), count);
			break;
		case PROJECTILES_STRUCTURE_TAG:
            pack_projectile_data(array, ProjectileList.data(), count);
			break;
		case MEDIA_TAG:
			pack_media_data(array, MediaList.data(), count);
			break;
		case OBJECT_PLACEMENT_STRUCTURE_TAG:
			array = pack_object_frequency_definition(array, item_placement_info.data(), count);
            array = pack_object_frequency_definition(array, monster_placement_info.data(), count);
			break;
		case PLATFORM_STRUCTURE_TAG:
            pack_platform_data(array, PlatformList.data(),count);
			break;
		case AMBIENT_SOUND_TAG:
			pack_ambient_sound_image_data(array, AmbientSoundImageList.data(), count);
			break;
		case RANDOM_SOUND_TAG:
            pack_random_sound_image_data(array, RandomSoundImageList.data(), count);
			break;
		case TERMINAL_DATA_TAG:
            pack_computer_terminals(array,count); // note: M1 terms will re-pack in M2 format
			break;
		case WEAPON_STATE_TAG:
			pack_player_weapon_data(array,count);
			break;
		case TERMINAL_STATE_TAG:
            pack_player_terminal_state(array,count);
			break;
		case MONSTER_PHYSICS_TAG:
			pack_monster_definition(array,count);
			break;
		case EFFECTS_PHYSICS_TAG:
			pack_effect_definition(array,count);
			break;
		case PROJECTILE_PHYSICS_TAG:
			pack_projectile_definition(array,count);
			break;
		case PHYSICS_PHYSICS_TAG:
			pack_physics_constants(array,count);
			break;
		case WEAPONS_PHYSICS_TAG:
			pack_weapon_definition(array,count);
			break;
		case SHAPE_PATCH_TAG:
			memcpy(array, get_shapes_patch_data(count), count);
			break;
		case SOUND_PATCH_TAG:
			memcpy(array, get_sounds_patch_data(count), count);
			break;
		case MMLS_TAG:
			pack_mml_level_scripts_data(array);
			break;
		case LUAS_TAG:
            pack_lua_level_scripts_data(array);
			break;
		case LUA_STATE_TAG:
			pack_lua_states(array, count);
			break;
		default:
            throw_ao_exception_f("unknown WAD tag: %x", 1, tag);
			break;
	}
	
	return array;
}

static wad_data *build_export_wad(wad_header_t *header, int32 *length)
{
	uint8 *array_to_slam;
	size_t size;

    wad_data* wad = (wad_data*)ao_calloc(1, sizeof(wad_data));
    
    recalculate_map_counts();

    // try to divine initial platform/polygon states
    std::vector<platform_data> SavedPlatforms = PlatformList;
    std::vector<polygon_data> SavedPolygons = PolygonList;
    std::vector<line_data> SavedLines = LineList;
    std::vector<side_data> SavedSides = SideList;

    for (size_t loop = 0; loop < PlatformList.size(); ++loop)
    {
        platform_data* platform = &PlatformList[loop];
        platform_data* original_platform = &PlatformListCopy[loop];

        if (PLATFORM_COMES_FROM_CEILING(platform))
        {
            auto new_ceiling_height = PLATFORM_IS_INITIALLY_EXTENDED(platform) ? platform->minimum_ceiling_height : platform->maximum_ceiling_height;
            adjust_platform_sides(platform, platform->ceiling_height, new_ceiling_height);
        }

        platform->floor_height = original_platform->floor_height;
        platform->ceiling_height = original_platform->ceiling_height;
        platform->maximum_ceiling_height = original_platform->maximum_ceiling_height;
        platform->minimum_ceiling_height = original_platform->minimum_ceiling_height;
        platform->maximum_floor_height = original_platform->maximum_floor_height;
        platform->minimum_floor_height = original_platform->minimum_floor_height;

        PolygonList[platform->polygon_index].floor_height = PolygonListCopy[platform->polygon_index].floor_height;
        PolygonList[platform->polygon_index].ceiling_height = PolygonListCopy[platform->polygon_index].ceiling_height;
    }

    for (size_t loop = 0; loop < LineList.size(); ++loop)
    {
        line_data *line = &LineList[loop];
        if (LINE_IS_VARIABLE_ELEVATION(line))
        {
            SET_LINE_VARIABLE_ELEVATION(line, false);
            SET_LINE_SOLIDITY(line, false);
            SET_LINE_TRANSPARENCY(line, true);
        }
    }

    for(unsigned loop= 0; loop<NUMBER_OF_EXPORT_ARRAYS; ++loop)
    {
        /* If there is a conversion function, let it handle it */
        switch (export_data[loop].tag)
        {
        case POINT_TAG:
        case LIGHTSOURCE_TAG:
        case PLATFORM_STATIC_DATA_TAG:
        case POLYGON_TAG:
            array_to_slam= export_tag_to_global_array_and_size(export_data[loop].tag, &size);
            break;
        default:
            array_to_slam= tag_to_global_array_and_size(export_data[loop].tag, &size);
        }

        /* Add it to the wad.. */
        if(size)
        {
            wad= append_data_to_wad(wad, export_data[loop].tag, array_to_slam, size, 0);
            delete []array_to_slam;
        }
    }

    PlatformList = SavedPlatforms;
    PolygonList = SavedPolygons;
    LineList = SavedLines;
    SideList = SavedSides;

    *length = calculate_wad_length(header, wad);
	
	return wad;
}


/* Build the wad, with all the crap */
static struct wad_data *build_save_game_wad(
	struct wad_header_t *header, 
	int32 *length)
{
	uint8 *array_to_slam;
	size_t size;

    wad_data* wad = (wad_data*)ao_calloc(1, sizeof(wad_data));	if(wad)

    recalculate_map_counts();
    for(unsigned loop= 0; loop<NUMBER_OF_SAVE_ARRAYS; ++loop)
    {
        /* If there is a conversion function, let it handle it */
        array_to_slam= tag_to_global_array_and_size(save_data[loop].tag, &size);

        /* Add it to the wad.. */
        if(size)
        {
            wad= append_data_to_wad(wad, save_data[loop].tag, array_to_slam, size, 0);
            delete []array_to_slam;
        }
    }
    *length= calculate_wad_length(header, wad);
	
	return wad;
}


// Build save game wad holding metadata and preview image 
wad_data* build_meta_game_wad(const std::string& metadata, const std::string& imagedata, wad_header_t *header, int32 *length)
{
    wad_data* wad = (wad_data*)ao_calloc(1, sizeof(wad_data));

    size_t size = metadata.length();
    if (size > 0) { wad = append_data_to_wad(wad, SAVE_META_TAG, metadata.c_str(), size, 0); }

    size_t imgsize = imagedata.length();
    if (imgsize > 0) { wad = append_data_to_wad(wad, SAVE_IMG_TAG, imagedata.c_str(), imgsize, 0); }
    
    *length = calculate_wad_length(header, wad);
	return wad;
}



ao_err level_has_embeds(int Level, bool& HasPhysics, bool& HasLua)
{
	// load the wad file and look for chunks !!??
	DataFile MapFile;
    ao_err err = MapFile.open(MapFileSpec);
    if (err) return err;

    wad_header_t header;
    err = read_wad_header(MapFile, &header);
    if (err) return err;

    wad_data* wad;
    err = read_indexed_wad_from_file(MapFile, &header, Level, true, wad);
    if (err) return err;
    
    size_t data_length;
    get_wad_resource_for_tag(wad, PHYSICS_PHYSICS_TAG, &data_length);
    HasPhysics = data_length > 0;

    get_wad_resource_for_tag(wad, LUAS_TAG, &data_length);
    HasLua = data_length > 0;
    free_wad(wad);
    
    return err;
}


/*
 *  Unpacking/packing functions
 */

static void unpack_directory_data(uint8 *Stream, directory_data& Object)
{
	uint8* S = Stream;
    StreamToValue(S, Object.mission_flags);                             //  2-byte
    StreamToValue(S, Object.environment_flags);                         //  2-byte
    StreamToValue(S, Object.entry_point_flags);                         //  4-byte
    
    read_macroman_string(S,  Object.level_name, MAX_LEVEL_NAME_LENGTH); // 64-byte
    S += 2;                                                             //  2-byte
    
	assert_fail((S - Stream) == SIZEOF_directory_data, "corrupt WAD directory"); // 74-byte
}


// ZZZ: gnu cc swears this is currently unused, and I don't see any sneaky #includes that might need it...; EES: it'll be needed (in theory) if implementing 2D map editor
/*
static uint8 *pack_directory_data(uint8 *Stream, directory_data *Objects, int Count)
{
	uint8* S = Stream;
	directory_data* ObjPtr = Objects;

	for (int k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->mission_flags);
		ValueToStream(S,ObjPtr->environment_flags);
		ValueToStream(S,ObjPtr->entry_point_flags);
		BytesToStream(S,ObjPtr->level_name,LEVEL_NAME_LENGTH);
	}

	assert_fail((S - Stream) == SIZEOF_directory_data, "");
	return S;
}
*/
