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

// This needs to do the right thing on save game, which is storing the precalculated crap.

#include "cseries.h"

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
#include "preferences.h"
#include "DataFile.hpp"

#include "tags.h"
#include "wad.h"
#include "map_wad.h"
#include "physics_wad.h" // load_default_physics
#include "interface.h"
#include "game_window.h"
#include "computer_interface.h" // for loading/saving terminal state.
#include "images.h"
#include "shell.h"
#include "preferences.h"
#include "SoundManager.h"
#include "Plugins.h"
#include "ephemera.h"
#include "SoundsPatch.h"

// LP change: added chase-cam init and render allocation
#include "ChaseCam.h"
#include "render.h"

#include "XML_LevelScript.h"

// For packing and unpacking some of the stuff
#include "Packing.h"

#include "motion_sensor.h"	// ZZZ for reset_motion_sensor()

#include "Music.h"

// unify the save game code into one structure.

/* -------- local globals */

static ao_path MapFileSpec; // this is distinct from environment_preferences.map_file


static std::vector<polygon_data> PolygonListCopy;
static std::vector<platform_data> PlatformListCopy;

// The following local globals are for handling games that need to be restored.
struct revert_game_info
{
	bool game_is_from_disk;
	struct game_data game_information;
	struct player_start_data player_start;
	struct entry_point entry_point;
    ao_path SavedGame;
};
static struct revert_game_info revert_game_data;

/* -------- static functions */
static void scan_and_add_scenery(void);
static void complete_restoring_level(struct wad_data *wad);
static void load_redundant_map_data(short *redundant_data, size_t count);
static void allocate_map_structure_for_map(struct wad_data *wad);
static wad_data *build_export_wad(wad_header_t *header, int32 *length);
static struct wad_data *build_save_game_wad(struct wad_header_t *header, int32 *length);

static void allocate_map_for_counts(size_t polygon_count, size_t side_count,
	size_t endpoint_count, size_t line_count);
static void load_points(uint8 *points, size_t count);
static void load_lines(uint8 *lines, size_t count);
static void load_sides(uint8 *sides, size_t count, short version);
static void load_polygons(uint8 *polys, size_t count, short version);
static void load_lights(uint8 *_lights, size_t count, short version);
static void load_annotations(uint8 *annotations, size_t count);
static void load_objects(uint8 *map_objects, size_t count, short version);
static void load_media(uint8 *_medias, size_t count);
static void load_map_info(uint8 *map_info);
static void load_ambient_sound_images(uint8 *data, size_t count);
static void load_random_sound_images(uint8 *data, size_t count);
static void load_terminal_data(uint8 *data, size_t length);

/* Used _ONLY_ by map_wad.c internally and precalculate.c. */
// ZZZ: hmm, no longer true, now using when resuming a network saved-game... hope that's ok?...
//static bool process_map_wad(struct wad_data *wad, bool restoring_game, short version);

/* Final three calls, must be in this order! */
static void recalculate_redundant_map(void);
static void scan_and_add_platforms(uint8 *platform_static_data, size_t count, short version);
static void complete_loading_level(short *_map_indexes, size_t map_index_count, 
	uint8 *_platform_data, size_t platform_data_count,
	uint8 *actual_platform_data, size_t actual_platform_data_count, short version);

static uint8 *unpack_directory_data(uint8 *Stream, directory_data *Objects, size_t Count);
//static uint8 *pack_directory_data(uint8 *Stream, directory_data *Objects, int Count);

/* ------------------------ Net functions */


/* Note that this frees it as well */
void process_net_map_data(uint8_t* flat_data)
{
    wad_header_t header;
    wad_data* wad = inflate_flat_data(flat_data, &header);
    assert_fail(wad, "inflate_flat_data should never return nullptr");
    
    process_map_wad(wad, false, header.data_version);
    free_wad(wad); /* Note that the flat data points into the wad. */
}


ao_err get_map_for_net_transfer(entry_point* entry, uint8_t*& flat_data)
{
    assert_fail(!MapFileSpec.empty(), "map file path is not set");
	
	return get_flat_data(MapFileSpec, entry->level_number, flat_data);
}

/* ---------------------- End Net Functions ----------- */


void set_current_map_path(const ao_path& path, bool loadScripts)
{
	// Do whatever parameter restoration is specified before changing the file
    if (!MapFileSpec.empty()) RunRestorationScript();

	MapFileSpec = path;
	open_map_file_resources(path);

	Plugins::instance()->set_map_checksum(get_current_map_checksum());
	
	// Only need to do this here
	if (loadScripts) LoadLevelScripts(path);
}




const ao_path& get_current_map_path()
{
    return MapFileSpec;
}




ao_err set_current_map_path_to_file_with_checksum(uint32_t checksum)
{
    if (!MapFileSpec.empty() && get_current_map_checksum() == checksum) return no_err;
	
    ao_path map_path = find_scenario_file({match_file_type(_typecode_map), match_checksum(checksum)});
    if (map_path.empty()) return errMapFileNotFound;
    
    set_current_map_path(map_path);
    return no_err;
}


static ao_err get_dynamic_data_from_save(const ao_path& path, dynamic_data* result)
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


ao_err load_level_from_map(short level_index)
{
	DataFile OFile;
	bool restoring_game = level_index == NONE;

    if (MapFileSpec.empty()) return STRID(gameError, errMapFileNotSet);
    
    short index_to_load = restoring_game ? 0 : level_index; // Saved games are always index 0
    
    DataFile MapFile;
    ao_err err = MapFile.open(MapFileSpec);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(MapFile, &header);
    if (err) return err;
    
    if (index_to_load < 0 || index_to_load >= header.wad_count) return STRID(gameError, errWadIndexOutOfRange);
    
    wad_data* wad;
    err = read_indexed_wad_from_file(MapFile, &header, index_to_load, true, wad);
    if (err) return err;
    
    process_map_wad(wad, restoring_game, header.data_version);
    free_wad(wad);
    
    // M1 terminals are stored in App's resource fork (or Shapes file's resource fork in case of Trojan)
    // (these are loaded after the map as process_map_wad will clear existing terms)
    if (header.data_version == MARATHON_ONE_DATA_VERSION)
    {
        load_m1_computer_terminals_for_level(level_index);
    }
    
	return no_err;
}

// keep these around for level export
static std::vector<static_platform_data> static_platforms;

extern bool ok_to_reset_scenery_solidity;

/* Hopefully this is in the correct order of initialization... */
/* This sucks, beavis. */
void complete_loading_level(
	short *_map_indexes,
	size_t map_index_count,
	uint8 *_platform_data,
	size_t platform_data_count,
	uint8 *actual_platform_data,
	size_t actual_platform_data_count,
	short version)
{
	/* Scan, add the doors, recalculate, and generally tie up all loose ends */
	/* Recalculate the redundant data.. */
	load_redundant_map_data(_map_indexes, map_index_count);

	static_platforms.clear();

	/* Add the platforms. */
	if(_platform_data || (_platform_data==NULL && actual_platform_data==NULL))
	{
		scan_and_add_platforms(_platform_data, platform_data_count, version);
	} else {
		assert_fail(actual_platform_data, "was null");
		PlatformList.resize(actual_platform_data_count);
		unpack_platform_data(actual_platform_data,platforms,actual_platform_data_count);
		assert_fail(actual_platform_data_count == static_cast<size_t>(static_cast<int16>(actual_platform_data_count)), "bad count");
		assert_fail(0 <= static_cast<int16>(actual_platform_data_count), "bad count");
		dynamic_world->platform_count= static_cast<int16>(actual_platform_data_count);
	}

	scan_and_add_scenery();
	ok_to_reset_scenery_solidity = true;
	
	/* Gotta do this after recalculate redundant.. */
	if(version==MARATHON_ONE_DATA_VERSION)
	{
		short loop;
		
		for(loop= 0; loop<dynamic_world->side_count; ++loop)
		{
			guess_side_lightsource_indexes(loop);
			if (static_world->environment_flags&_environment_vacuum)
			{
				side_data *side= get_side_data(loop);
				if (side->flags&_side_is_control_panel)
					side->flags |= _side_is_m1_lighted_switch;
			}
		}
	}
}

/* Call with location of NULL to get the number of start locations for a */
/* given team or player */
short get_player_starting_location_and_facing(
	short team, 
	short index, 
	struct object_location *location)
{
	short ii;
	struct map_object *saved_object;
	short count= 0;
	bool done= false;
	
	saved_object= saved_objects;
	for(ii=0; !done && ii<dynamic_world->initial_objects_count; ++ii)
	{
		if(saved_object->type==_saved_player)
		{
			/* index=NONE means use any starting location */
			if(saved_object->index==team || team==NONE)
			{
				if(location && count==index)
				{
					location->p= saved_object->location;
					location->polygon_index= saved_object->polygon_index;
					location->yaw= saved_object->facing;
					location->pitch= 0;
					location->flags= saved_object->flags;
					done= true;
				}
				count++;
			}
		}
		++saved_object;
	}
	
	/* If they asked for a valid location, make sure that we gave them one */
	if (location) assert_fail_f(done, "Tried to place: %d only %d starting pts.", index, count);
	
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

static void create_players_for_new_game(short number_of_players, player_start_data* player_start_information)
{
	const short intended_local_player_index = game_is_networked ? NetGetLocalPlayerIndex() : 0;

	/* Initialize the players-> note there may be more than one player in a */
	/* non-network game, for playback.. */
	for (int i = 0; i < number_of_players; ++i)
	{
		new_player_flags flags = (i == intended_local_player_index ? new_player_make_local_and_current : 0);
		auto player_index = new_player(player_start_information[i].team,
			player_start_information[i].color, player_start_information[i].identifier, flags);
		assert_fail(player_index == i, "mispositioned");

		/* Now copy in the name of the player.. */
		players[i].name = player_start_information[i].name;
	}
}

// ZZZ: split this out from new_game for sharing
void reset_revert_game_file_to_default()
{
    revert_game_data.SavedGame = get_saved_games_dir() / get_string(STRID(strFILENAMES, filenameDEFAULT_SAVE_GAME));
}

extern void ResetPassedLua();


ao_err new_game(short number_of_players, bool is_netgame, game_data *game_information,
                player_start_data *player_start_information, entry_point *entry_point)
{
	assert_fail(!is_netgame || number_of_players == NetGetNumberOfPlayers(), "nobody's home");
		
	ResetPassedLua();

	// Make sure our code is synchronized
	assert_fail(MAXIMUM_PLAYER_START_NAME_LENGTH==MAXIMUM_PLAYER_NAME_LENGTH, "ffs, idiocy");

	game_is_networked = is_netgame;
	
	// If we want to save it, this is an untitled map
    reset_revert_game_file_to_default();

	set_random_seed(game_information->initial_random_seed);

	// Initialize the players to a known state. This must be done before goto_level because it sets
	// dynamic_world->player_count to 0, which is crucial for when I try to recreate the players...
	initialize_map_for_new_game(); // memsets dynamic_world to 0

	/* Copy the game data into the dynamic_world */
	/* ajr-this used to be done only when we successfully loaded the map. however, goto_level
	 * will place the initial monsters on a level, which calls new_monster, which relies
	 * on this information being setup properly, so we do it here instead. */
	obj_copy(dynamic_world->game_information, *game_information);

	// Load the level
	assert_fail(!MapFileSpec.empty(), "not set");
	ao_err err = goto_level(entry_point, number_of_players, player_start_information);
    if (err) return err;
    
    if (!film_profile.network_items)
    {
        create_players_for_new_game(number_of_players, player_start_information);
    }

    // we need to alert the function that reverts the game of the game setup so that
    // new_game can be called if the user wants to revert later.
    setup_revert_game_info(game_information, player_start_information, entry_point);
    
    // Reset the player queues (done here and in load_game)
    reset_action_queues();
    
    entering_map(false);
    
    // ZZZ: set motion sensor to sane state - needs to come after entering_map() (which calls load_collections())
    reset_motion_sensor(current_player_index);
	ChaseCam_Initialize();

	return no_err;
}


ao_err get_next_level_for_game_types(int32_t game_type_flags, int16_t& start_at_index, entry_point& level_info)
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
            unpack_directory_data(p, &directory, 1);
            
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
            static_data map_info;
            unpack_static_data(p, &map_info, 1);
            
            // single-player Marathon 1 levels aren't always marked
            if (header.data_version == MARATHON_ONE_DATA_VERSION && map_info.entry_point_flags == 0)
            {
                map_info.entry_point_flags = _single_player_entry_point;
            }
            // Marathon 1 handled (then-unused) coop flag differently
            if (header.data_version == MARATHON_ONE_DATA_VERSION)
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
bool get_entry_points(std::vector<entry_point> &vec, int32 type)
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
	if (header.application_specific_directory_data_size == SIZEOF_directory_data) {

		// New style wad, read directory data
        uint8_t *total_directory_data = read_directory_data(MapFile, &header);
		assert_fail(total_directory_data, "no data");

		// Push matching directory entries into vector
		for (int i=0; i<header.wad_count; i++) {
			uint8 *p = (uint8 *)get_indexed_directory_data(&header, i, total_directory_data);
			directory_data directory;
			unpack_directory_data(p, &directory, 1);

			if (directory.entry_point_flags & type) {

				// This one is valid
				entry_point point;
				point.level_number = i;
				point.utf8_level_name = directory.level_name;
				vec.push_back(point);
				success = true;
			}
		}
		free(total_directory_data);

	} else {

		// Old style wad
		for (int i=0; i<header.wad_count; i++) {

            wad_data* wad;
            ao_err err = read_indexed_wad_from_file(MapFile, &header, i, true, wad);
			if (err) continue;

			// Read map_info data
			size_t length;
			uint8 *p = (uint8 *)get_wad_resource_for_tag(wad, MAP_INFO_TAG, &length);
			assert_fail(length == SIZEOF_static_data, "wrong size");
			static_data map_info;
			unpack_static_data(p, &map_info, 1);

			// single-player Marathon 1 levels aren't always marked
			if (header.data_version == MARATHON_ONE_DATA_VERSION &&
			    map_info.entry_point_flags == 0)
				map_info.entry_point_flags = _single_player_entry_point;

			// Marathon 1 handled (then-unused) coop flag differently
			if (header.data_version == MARATHON_ONE_DATA_VERSION)
			{
				if (map_info.entry_point_flags & _single_player_entry_point)
					map_info.entry_point_flags |= _multiplayer_cooperative_entry_point;
				if (map_info.entry_point_flags & _multiplayer_carnage_entry_point)
					map_info.entry_point_flags &= ~_multiplayer_cooperative_entry_point;
			}

			if (map_info.entry_point_flags & type) {

				// This one is valid
				entry_point point;
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

extern void LoadSoloLua();
extern void LoadReplayNetLua();
extern void LoadStatsLua();
extern void LoadAchievementsLua();
extern bool RunLuaScript();


// This is called when the game level is changed somehow
// The only thing that has to be valid in the entry point is the level_index
ao_err goto_level(entry_point *entry, short number_of_players, player_start_data* player_start_information)
{
    ao_err err = no_err;
    
    bool is_new_game = player_start_information != nullptr;

	if (!is_new_game)
	{
		// Clear the current map
		leaving_map();

		// ghs: hack to get new MML-specified sounds loaded
		SoundManager::instance()->UnloadAllSounds();
	}

	// LP: doing this here because level-specific MML may specify which level-specific
	// textures to load.
	ResetLevelScript();
	if (!game_is_networked || set_current_map_path_to_file_with_checksum(((game_info*)NetGetGameData())->parent_checksum) == no_err)
	{
		RunLevelScript(entry->level_number);
	}

#if !defined(DISABLE_NETWORKING)
	// If the game is networked, then I must call the network code to do the right thing with the map.
	if(game_is_networked)
	{
		// This function, if it is a server, calls get_map_for_net_transfer, and then calls
        // process_map_wad on it. Non-server receives the map and then calls process_map_wad on it.
		err = NetChangeMap(entry);
	}
	else 
#endif // !defined(DISABLE_NETWORKING)
	{
		err = load_level_from_map(entry->level_number);
	}
	
	if (!err)
	{
		if (!game_is_networked && number_of_players == 1)
		{
			LoadSoloLua();
		}
		else if (!game_is_networked)
		{
			LoadReplayNetLua();
		}
		LoadAchievementsLua();
		LoadStatsLua();
		
		if (!is_new_game)
		{
			recreate_players_for_new_level();
		}
		else if (film_profile.network_items)
		{
			create_players_for_new_game(number_of_players, player_start_information);
		}
		
		// Load the collections // EES: where?
		dynamic_world->current_level_number= entry->level_number;

		// ghs: this runs very early now: we want to be before place_initial_objects, and before MarkLuaCollections
		RunLuaScript();

		if (film_profile.early_object_initialization)
		{
			place_initial_objects();
			initialize_control_panels_for_level();
		}

		if (!is_new_game) 
		{
			// entering_map might fail if netsync fails, but we will have already displayed the error. // EES: sure, but NetSync right now never returns an error condition, so until entering_map has some useful errors to return, it doesn't // TODO: if network code is displaying error dialogs, it shouldn't: it should pass specific error codes back here so all the user-facing error reporting happens in one place
			entering_map(false);
		}

		if (!film_profile.early_object_initialization)
		{
			place_initial_objects();
			initialize_control_panels_for_level();
		}
		
	}
	
    return err;
}

/* -------------------- Private or map editor functions */
void allocate_map_for_counts(
	size_t polygon_count, 
	size_t side_count,
	size_t endpoint_count,
	size_t line_count)
{
	//long cumulative_length= 0;
	size_t automap_line_count, automap_polygon_count, map_index_count;
	// long automap_line_length, automap_polygon_length, map_index_length;

	/* Give the map indexes a whole bunch of memory (cause we can't calculate it) */
	// map_index_length= (polygon_count*32+1024)*sizeof(int16);
	map_index_count= (polygon_count*32+1024);
	
	/* Automap lines. */
	// automap_line_length= (line_count/8+((line_count%8)?1:0))*sizeof(byte);
	automap_line_count= (line_count/8+((line_count%8)?1:0));
	
	/* Automap Polygons */
	// automap_polygon_length= (polygon_count/8+((polygon_count%8)?1:0))*sizeof(byte);
	automap_polygon_count= (polygon_count/8+((polygon_count%8)?1:0));

	// cumulative_length+= polygon_count*sizeof(struct polygon_data);
	// cumulative_length+= side_count*sizeof(struct side_data);
	// cumulative_length+= endpoint_count*sizeof(struct endpoint_data);
	// cumulative_length+= line_count*sizeof(struct line_data);
	// cumulative_length+= map_index_length;
	// cumulative_length+= automap_line_length;
	// cumulative_length+= automap_polygon_length;

	/* Okay, we now have the length.  Allocate our block.. */
	// reallocate_map_structure_memory(cumulative_length);

	/* Tell the recalculation data how big it is.. */
	// set_map_index_buffer_size(map_index_length);

	/* Setup our pointers. */
	// map_polygons= (struct polygon_data *) get_map_structure_chunk(polygon_count*sizeof(struct polygon_data));
	// map_sides= (struct side_data *) get_map_structure_chunk(side_count*sizeof(struct side_data));
	// map_endpoints= (struct endpoint_data *) get_map_structure_chunk(endpoint_count*sizeof(struct endpoint_data));
	// map_lines= (struct line_data *) get_map_structure_chunk(line_count*sizeof(struct line_data));
	// map_indexes= (short *) get_map_structure_chunk(map_index_length);
	// automap_lines= (uint8 *) get_map_structure_chunk(automap_line_length);
	// automap_polygons= (uint8 *) get_map_structure_chunk(automap_polygon_length);
	
	// Most of the other stuff: reallocate here
	EndpointList.resize(endpoint_count);
	LineList.resize(line_count);
	SideList.resize(side_count);
	PolygonList.resize(polygon_count);
	AutomapLineList.resize(automap_line_count);
	AutomapPolygonList.resize(automap_polygon_count);
	
	// Map indexes: start off with none of them (of course),
	// but reserve a size equal to the map index length
	MapIndexList.clear();
	MapIndexList.reserve(map_index_count);
	dynamic_world->map_index_count= 0;
	
	// Stuff that needs the max number of polygons
	allocate_render_memory();
	allocate_flood_map_memory();
}

void load_points(
	uint8 *points,
	size_t count)
{
	size_t loop;
	
	// OK to modify input-data pointer since it's called by value
	for(loop=0; loop<count; ++loop)
	{
		world_point2d& vertex = map_endpoints[loop].vertex;
		StreamToValue(points,vertex.x);
		StreamToValue(points,vertex.y);
	}
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "wrong size");
	assert_fail(0 <= static_cast<int16>(count), "wrong size");
	dynamic_world->endpoint_count= static_cast<int16>(count);
}

void load_lines(
	uint8 *lines, 
	size_t count)
{
	// assert_fail(count>=0 && count<=MAXIMUM_LINES_PER_MAP, "");
	unpack_line_data(lines,map_lines,count);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "wrong size");
	assert_fail(0 <= static_cast<int16>(count), "wrong size");
	dynamic_world->line_count= static_cast<int16>(count);
}

void load_sides(
	uint8 *sides, 
	size_t count,
	short version)
{
	size_t loop;

	// assert_fail(count>=0 && count<=MAXIMUM_SIDES_PER_MAP, "");

	unpack_side_data(sides,map_sides,count);

	if (version == MARATHON_ONE_DATA_VERSION)
	{
		for (loop = 0; loop < count; ++loop)
		{
			// some editors set unused flags; clear them out
			static constexpr int m1_side_flags_mask = 0x0007;
			
			map_sides[loop].transparent_texture.texture= UNONE;
			map_sides[loop].ambient_delta= 0;
			map_sides[loop].flags &= m1_side_flags_mask;
			map_sides[loop].flags |= _side_item_is_optional;
		}
	}
	else
	{
		bool editor_set_unused_flags = false;
		for (loop = 0; loop < count; ++loop)
		{
			// some editors set unused flags; clear them out
			if (map_sides[loop].flags & _reserved_side_flag)
			{
				editor_set_unused_flags = true;
				break;
			}
		}
			
		if (editor_set_unused_flags)
		{
			for (loop = 0; loop < count; ++loop)
			{
				static constexpr int m2_side_flags_mask = 0x007f;
				map_sides[loop].flags &= m2_side_flags_mask;
			}
		}
	}

	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "wrong size");
	assert_fail(0 <= static_cast<int16>(count), "wrong size");
	dynamic_world->side_count= static_cast<int16>(count);
}

void load_polygons(
	uint8 *polys, 
	size_t count,
	short version)
{
	size_t loop;

	// assert_fail(count>=0 && count<=MAXIMUM_POLYGONS_PER_MAP, "");
	
	unpack_polygon_data(polys,map_polygons,count);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "wrong size");
	assert_fail(0 <= static_cast<int16>(count), "wrong size");
	dynamic_world->polygon_count= static_cast<int16>(count);

	/* Allow for backward compatibility! */
	switch(version)
	{
		case MARATHON_ONE_DATA_VERSION:
			for(loop= 0; loop<count; ++loop)
			{
				map_polygons[loop].media_index= NONE;
				map_polygons[loop].floor_origin.x= map_polygons[loop].floor_origin.y= 0;
				map_polygons[loop].ceiling_origin.x= map_polygons[loop].ceiling_origin.y= 0;
                
                switch (map_polygons[loop].type)
                {
                    case _polygon_is_hill:
                        map_polygons[loop].type = _polygon_is_minor_ouch;
                        break;
                    case _polygon_is_base:
                        map_polygons[loop].type = _polygon_is_major_ouch;
                        break;
                    case _polygon_is_zone_border:
                        map_polygons[loop].type = _polygon_is_glue;
                        break;
                    case _polygon_is_goal:
                        map_polygons[loop].type = _polygon_is_glue_trigger;
                        break;
                    case _polygon_is_visible_monster_trigger:
                        map_polygons[loop].type = _polygon_is_superglue;
                        break;
                    case _polygon_is_invisible_monster_trigger:
                        map_polygons[loop].type = _polygon_must_be_explored;
                        break;
                    case _polygon_is_dual_monster_trigger:
                        map_polygons[loop].type = _polygon_is_automatic_exit;
                        break;
                }

				// this is set on some m1 maps, but it's unknown what the flag
				// does. Operating on the assumption that old m1 editors didn't
				// clear out flags, just unset the flag. Otherwise the map will
				// assert out later
				map_polygons[loop].flags &= ~POLYGON_IS_DETACHED_BIT;
			}
			break;
			
		case MARATHON_TWO_DATA_VERSION:
		// LP addition:
		case MARATHON_INFINITY_DATA_VERSION:
			break;
			
		default:
            throw_ao_exception("Map version: %i", errDataFileTooNew, version);
			break;
	}
}

void load_lights(
	uint8 *_lights, 
	size_t count,
	short version)
{
	unsigned short loop, new_index;
	
	LightList.resize(count);
	objlist_clear(lights,count);
	// assert_fail_f(count>=0 && count<=MAXIMUM_LIGHTS_PER_MAP, "Light count: %d vers: %d",
	//	count, version);
	
	old_light_data *OldLights;
	
	switch(version)
	{
	case MARATHON_ONE_DATA_VERSION: {
		
		// Unpack the old lights into a temporary array
		OldLights = new old_light_data[count];
		unpack_old_light_data(_lights,OldLights,count);
		
		old_light_data *OldLtPtr = OldLights;
		for(loop= 0; loop<count; ++loop, OldLtPtr++)
		{
			static_light_data TempLight;
			convert_old_light_data_to_new(&TempLight, OldLtPtr, 1);
			
			new_index = new_light(&TempLight);
			assert_fail(new_index==loop, "M1 lights failed to convert to M2");
		}
		delete []OldLights;
		break;			
	}
		
	case MARATHON_TWO_DATA_VERSION:
	case MARATHON_INFINITY_DATA_VERSION:
		// OK to modify the data pointer since it was passed by value
		for(loop= 0; loop<count; ++loop)
		{
			static_light_data TempLight;
			_lights = unpack_static_light_data(_lights, &TempLight, 1);
			
			new_index = new_light(&TempLight);
			assert_fail(new_index==loop, "bad static light data");
		}
		break;			
		
	default:
            throw_ao_exception("Map version: %i", errDataFileTooNew, version);
		break;
	}
}

void load_annotations(
	uint8 *annotations, 
	size_t count)
{
	// assert_fail(count>=0 && count<=MAXIMUM_ANNOTATIONS_PER_MAP, "");
	MapAnnotationList.resize(count);
	unpack_map_annotation(annotations,map_annotations,count);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "corrupt annotation data");
	assert_fail(0 <= static_cast<int16>(count), "corrupt annotation data");
	dynamic_world->default_annotation_count= static_cast<int16>(count);
}

void load_objects(uint8 *map_objects, size_t count, short version)
{
	// assert_fail(count>=0 && count<=MAXIMUM_SAVED_OBJECTS, "");
	SavedObjectList.resize(count);
        unpack_map_object(map_objects,saved_objects,count, version);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "corrupt annotation data");
	assert_fail(0 <= static_cast<int16>(count), "corrupt annotation data");
	dynamic_world->initial_objects_count= static_cast<int16>(count);
}

void load_map_info(
	uint8 *map_info)
{
	unpack_static_data(map_info,static_world,1);
	static_world->ball_in_play = false;
}

void load_media(
	uint8 *_medias,
	size_t count)
{
	// struct media_data *media= _medias;
	size_t ii;
	
	MediaList.resize(count);
	objlist_clear(medias,count);
	// assert_fail(count>=0 && count<=MAXIMUM_MEDIAS_PER_MAP, "");
	
	for(ii= 0; ii<count; ++ii)
	{
		media_data TempMedia;
		_medias = unpack_media_data(_medias,&TempMedia,1);
		
		size_t new_index = new_media(&TempMedia);
		assert_fail(new_index==ii, "corrput media");
	}
}

void load_ambient_sound_images(
	uint8 *data,
	size_t count)
{
	// assert_fail(count>=0 &&count<=MAXIMUM_AMBIENT_SOUND_IMAGES_PER_MAP, "");
	AmbientSoundImageList.resize(count);
	unpack_ambient_sound_image_data(data,ambient_sound_images,count);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "corrupt ambient sound data"); // TODO: it really should go without saying that checking for corrupt Map data should be a permanent safety-check, not a debug test that gets turned off in release; convert all of these to AOException (preferably after creating a range-check macro)
	assert_fail(0 <= static_cast<int16>(count), "corrupt ambient sound data");
	dynamic_world->ambient_sound_image_count= static_cast<int16>(count);
}

void load_random_sound_images(
	uint8 *data,
	size_t count)
{
	// assert_fail(count>=0 &&count<=MAXIMUM_RANDOM_SOUND_IMAGES_PER_MAP, "");
	RandomSoundImageList.resize(count);
	unpack_random_sound_image_data(data,random_sound_images,count);
	assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "corrupt random sound data");
	assert_fail(0 <= static_cast<int16>(count), "corrupt random sound data");
	dynamic_world->random_sound_image_count= static_cast<int16>(count);
}

/* Recalculate all the redundant crap- must be done before platforms/doors/etc.. */
void recalculate_redundant_map(
	void)
{
	short loop;

	for(loop=0;loop<dynamic_world->polygon_count;++loop) recalculate_redundant_polygon_data(loop);
	for(loop=0;loop<dynamic_world->line_count;++loop) recalculate_redundant_line_data(loop);
	for(loop=0;loop<dynamic_world->endpoint_count;++loop) recalculate_redundant_endpoint_data(loop);
}


ao_err load_game_from_file(const ao_path& File, bool run_scripts) // TODO: should consolidate load_saved_game_from_flat_data which is in interface.cpp of all places
{
	ao_err err = no_err;

	ResetPassedLua();
	ResetLevelScript();

	/* Setup for a revert.. */
	revert_game_data.game_is_from_disk = true;
	revert_game_data.SavedGame = File;

	uint32 parent_checksum = read_wad_file_parent_checksum(File);
	err = set_current_map_path_to_file_with_checksum(parent_checksum); /* Find the original scenario this saved game was a part of.. */
    if (err)
    {
        reset_current_map_path_to_default();

        return err; // the original Map file wasn't found. The original M2 behavior was to continue playing the saved game file, then fail when exiting the level, but since we have no idea if this level requires AO scripts to work correctly, this is the right time to bail.
    }
    
    ao_path parent_map_path = MapFileSpec;
    
    dynamic_data dynamic_data;
    err = get_dynamic_data_from_save(File, &dynamic_data);
    if (err) return err;
    
    RunLevelScript(dynamic_data.current_level_number);
    
	// temporarily set the global map path to the saved game file before calling load_level_from_map...
	set_current_map_path(File, false);
    
	// Load the level from the map
	err = load_level_from_map(NONE); // Save games are ALWAYS index NONE; TODO: passing the File path here might not be a bad idea, avoids messing with global
    
    // ...now restore the original
    set_current_map_path(parent_map_path, false);

    if (err) return err;
    
    if (run_scripts)
    {
        // LP: getting the level scripting off of the map file
        if (!game_is_networked)
        {
            LoadSoloLua();
        }
        LoadAchievementsLua();
        LoadStatsLua();
    }

	return no_err;
}


void setup_revert_game_info(game_data* game_info, player_start_data* start, entry_point* entry)
{
	revert_game_data.game_is_from_disk = false;
	obj_copy(revert_game_data.game_information, *game_info);
	obj_copy(revert_game_data.player_start, *start);
	obj_copy(revert_game_data.entry_point, *entry);
}


ao_err revert_game()
{
	assert_fail(dynamic_world->player_count==1, "wrong count");
    ao_err err = no_err;

	leaving_map();
    reset_recording();
    
	if (revert_game_data.game_is_from_disk) // Reload their last saved game
	{
        err = load_game_from_file(revert_game_data.SavedGame, true);
        if (err) return err;
        
        RunLuaScript();
        entering_map(true);
	}
	else
	{
		err = new_game(1, false, &revert_game_data.game_information, &revert_game_data.player_start, &revert_game_data.entry_point);
        if (err) return err;
	}
    
    update_interface(NONE);
    ChaseCam_Reset();
    ResetFieldOfView();
    ReloadViewContext();
	
	return err;
}



ao_err export_level(const ao_path& path)
{
    ao_path tmp_path = path;
    ao_err err = make_temp_file(tmp_path);
    if (err) return err;
    
	// Fill in the default wad header (we are using File instead of TempFile to get the name right in the header)
    wad_header_t header;
	fill_default_wad_header(path, CURRENT_WADFILE_VERSION, MARATHON_TWO_DATA_VERSION, 1, 0, &header);
    
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
    
    directory_entry entries[2];

	// Save off the random seed.
	dynamic_world->random_seed = get_random_seed();

	// Setup to revert the game properly
	revert_game_data.game_is_from_disk = true;
	revert_game_data.SavedGame = path;

    ao_path temp_path;
    err = make_temp_file(temp_path);
    if (err) return err;
	
	/* Fill in the default wad header (we are using File instead of TempFile to get the name right in the header) */
    wad_header_t header;
	fill_default_wad_header(path, CURRENT_WADFILE_VERSION, EDITOR_MAP_VERSION, 2, 0, &header);
		
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
    header.parent_checksum = read_wad_file_checksum(MapFileSpec);
    
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
static void scan_and_add_platforms(
	uint8 *platform_static_data,
	size_t count,
	short version)
{
	struct polygon_data *polygon;
	short loop;

	PlatformList.resize(count);
	objlist_clear(platforms,count);

	static_platforms.resize(count);
	unpack_static_platform_data(platform_static_data, static_platforms.data(), count, version);

	polygon= map_polygons;
	for(loop=0; loop<dynamic_world->polygon_count; ++loop)
	{
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

/* Load a level from a wad-> mainly used by the net stuff. */
void process_map_wad(wad_data* wad, bool restoring_game, short version)
{
	size_t data_length;
	uint8 *data;
	size_t count;
	bool is_preprocessed_map= false;

	assert_fail_f(version==MARATHON_INFINITY_DATA_VERSION || version==MARATHON_TWO_DATA_VERSION || version==MARATHON_ONE_DATA_VERSION, "Map version:", version); // TODO: there should be one ingress point for ALL WADs and, again, ALWAYS check version there

	/* zero everything so no slots are used */	
	initialize_map_for_new_level();

	/* Calculate the length (for reallocate map) */
	allocate_map_structure_for_map(wad);

	/* Extract points */
	data= (uint8 *)get_wad_resource_for_tag(wad, POINT_TAG, &data_length);
	count= data_length/SIZEOF_world_point2d;
	assert_fail(data_length == count*SIZEOF_world_point2d, "corrupt points"); // TODO: fuck this, am gonna stub messages for now
	
	if(count)
	{
		load_points(data, count);
	} else {
         
		data= (uint8 *)get_wad_resource_for_tag(wad, ENDPOINT_DATA_TAG, &data_length);
		count= data_length/SIZEOF_endpoint_data;
		assert_fail(data_length == count*SIZEOF_endpoint_data, "");
		// assert_fail(count>=0 && count<MAXIMUM_ENDPOINTS_PER_MAP, "");

		/* Slam! */
		unpack_endpoint_data(data,map_endpoints,count);
		assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "");
		assert_fail(0 <= static_cast<int16>(count), "");
		dynamic_world->endpoint_count= static_cast<int16>(count);

		if (version > MARATHON_ONE_DATA_VERSION)
			is_preprocessed_map= true;
	}

	/* Extract lines */
	data= (uint8 *)get_wad_resource_for_tag(wad, LINE_TAG, &data_length);
	count = data_length/SIZEOF_line_data;
	assert_fail(data_length == count*SIZEOF_line_data, "");
	load_lines(data, count);

	/* Order is important! */
	data= (uint8 *)get_wad_resource_for_tag(wad, SIDE_TAG, &data_length);
	count = data_length/SIZEOF_side_data;
	assert_fail(data_length == count*SIZEOF_side_data, "");
	load_sides(data, count, version);

	/* Extract polygons */
	data= (uint8 *)get_wad_resource_for_tag(wad, POLYGON_TAG, &data_length);
	count = data_length/SIZEOF_polygon_data;
	assert_fail(data_length == count*SIZEOF_polygon_data, "");
	load_polygons(data, count, version);
	
	/* Extract the lightsources */
	if(restoring_game)
	{
		// Slurp them in
		data= (uint8 *)get_wad_resource_for_tag(wad, LIGHTSOURCE_TAG, &data_length);
		count = data_length/SIZEOF_light_data;
		assert_fail(data_length == count*SIZEOF_light_data, "");
		LightList.resize(count);
		unpack_light_data(data,lights,count);
	}
	else
	{
		/* When you are restoring a game, the actual light structure is set. */
		data= (uint8 *)get_wad_resource_for_tag(wad, LIGHTSOURCE_TAG, &data_length);
		if(version==MARATHON_ONE_DATA_VERSION) 
		{
			/* We have an old style light */
			count= data_length/SIZEOF_old_light_data;
			assert_fail(count*SIZEOF_old_light_data==data_length, "");
			load_lights(data, count, version);
		} else {
			count= data_length/SIZEOF_static_light_data;
			assert_fail(count*SIZEOF_static_light_data==data_length, "");
			load_lights(data, count, version);
		}

		//	HACK!!!!!!!!!!!!!!! vulcan doesn’t NONE .first_object field after adding scenery
		{
			for (count= 0; count<static_cast<size_t>(dynamic_world->polygon_count); ++count)
			{
				map_polygons[count].first_object= NONE;
			}
		}
	}

	/* Extract the annotations */
	data= (uint8 *)get_wad_resource_for_tag(wad, ANNOTATION_TAG, &data_length);
	count = data_length/SIZEOF_map_annotation;
	assert_fail(data_length == count*SIZEOF_map_annotation, "");
	load_annotations(data, count);

	/* Extract the objects */
	data= (uint8 *)get_wad_resource_for_tag(wad, OBJECT_TAG, &data_length);
	count = data_length/SIZEOF_map_object;
	assert_fail(data_length == count*static_cast<size_t>(SIZEOF_map_object), "");
	load_objects(data, count, version);

	/* Extract the map info data */
	data= (uint8 *)get_wad_resource_for_tag(wad, MAP_INFO_TAG, &data_length);
	// LP change: made this more Pfhorte-friendly
	assert_fail(static_cast<size_t>(SIZEOF_static_data)==data_length || static_cast<size_t>(SIZEOF_static_data-2)==data_length, "");
	load_map_info(data);
    if (version == MARATHON_ONE_DATA_VERSION)
    {
        if (static_world->mission_flags & _mission_exploration)
        {
            static_world->mission_flags &= ~_mission_exploration;
            static_world->mission_flags |= _mission_exploration_m1;
        }
        if (static_world->mission_flags & _mission_rescue)
        {
            static_world->mission_flags &= ~_mission_rescue;
            static_world->mission_flags |= _mission_rescue_m1;
        }
        if (static_world->mission_flags & _mission_repair)
        {
            static_world->mission_flags &= ~_mission_repair;
            static_world->mission_flags |= _mission_repair_m1;
        }
        if (static_world->environment_flags & _environment_rebellion)
        {
            static_world->environment_flags &= ~_environment_rebellion;
            static_world->environment_flags |= _environment_rebellion_m1;
        }
        static_world->environment_flags |= _environment_glue_m1|_environment_ouch_m1|_environment_song_index_m1|_environment_terminals_stop_time|_environment_activation_ranges|_environment_m1_weapons;
        
    }

    if (static_world->environment_flags & _environment_song_index_m1) {
	    Music::instance()->SetClassicLevelMusic(static_world->song_index);
    }

	/* Extract the game difficulty info.. */
	data= (uint8 *)get_wad_resource_for_tag(wad, ITEM_PLACEMENT_STRUCTURE_TAG, &data_length);
	// In case of an absent placement chunk...
	if (data_length == 0)
	{
		data = new uint8[2*MAXIMUM_OBJECT_TYPES*SIZEOF_object_frequency_definition];
		memset(data,0,2*MAXIMUM_OBJECT_TYPES*SIZEOF_object_frequency_definition);
	}
	else
		assert_fail(data_length == 2*MAXIMUM_OBJECT_TYPES*SIZEOF_object_frequency_definition, "");
	load_placement_data(data + MAXIMUM_OBJECT_TYPES*SIZEOF_object_frequency_definition, data);
	if (data_length == 0)
		delete []data;
	
	/* Extract the terminal data. */
	data= (uint8 *)get_wad_resource_for_tag(wad, TERMINAL_DATA_TAG, &data_length);
	load_terminal_data(data, data_length);

	/* Extract the media definitions */
	if(restoring_game)
	{
		// Slurp it in
		data= (uint8 *)get_wad_resource_for_tag(wad, MEDIA_TAG, &data_length);
		count= data_length/SIZEOF_media_data;
		assert_fail(count*SIZEOF_media_data==data_length, "");
		MediaList.resize(count);
		unpack_media_data(data,medias,count);
	}
	else
	{
		data= (uint8 *)get_wad_resource_for_tag(wad, MEDIA_TAG, &data_length);
		count= data_length/SIZEOF_media_data;
		assert_fail(count*SIZEOF_media_data==data_length, "");
		load_media(data, count);
	}

	/* Extract the ambient sound images */
	data= (uint8 *)get_wad_resource_for_tag(wad, AMBIENT_SOUND_TAG, &data_length);
	count = data_length/SIZEOF_ambient_sound_image_data;
	assert_fail(data_length == count*SIZEOF_ambient_sound_image_data, "");
	load_ambient_sound_images(data, count);
	load_ambient_sound_images(data, data_length/SIZEOF_ambient_sound_image_data);

	/* Extract the random sound images */
	data= (uint8 *)get_wad_resource_for_tag(wad, RANDOM_SOUND_TAG, &data_length);
	count = data_length/SIZEOF_random_sound_image_data;
	assert_fail(data_length == count*SIZEOF_random_sound_image_data, "");
	load_random_sound_images(data, count);

	/* Extract embedded shapes */
	data= (uint8 *)get_wad_resource_for_tag(wad, SHAPE_PATCH_TAG, &data_length);
	set_shapes_patch_data(data, data_length);

	/* Extract embedded sounds */
	data= (uint8 *)get_wad_resource_for_tag(wad, SOUND_PATCH_TAG, &data_length);
	set_sounds_patch_data(data, data_length);

	/* Extract MMLS */
	data= (uint8 *)get_wad_resource_for_tag(wad, MMLS_TAG, &data_length);
	SetMMLS(data, data_length);

	/* Extract LUAS */
	data= (uint8 *)get_wad_resource_for_tag(wad, LUAS_TAG, &data_length);
	SetLUAS(data, data_length);

	/* Extract saved Lua state */
	data =(uint8 *)get_wad_resource_for_tag(wad, LUA_STATE_TAG, &data_length);
	unpack_lua_states(data, data_length);

	// LP addition: load the physics-model chunks (all fixed-size)
	bool PhysicsModelLoaded = false;
	
	data= (uint8 *)get_wad_resource_for_tag(wad, MONSTER_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_monster_definition;
	assert_fail(count*SIZEOF_monster_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_MONSTER_TYPES, "");
	if (data_length > 0)
	{
		if (!PhysicsModelLoaded) load_default_physics();
		PhysicsModelLoaded = true;
		unpack_monster_definition(data,count);
	}
	
	data= (uint8 *)get_wad_resource_for_tag(wad, EFFECTS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_effect_definition;
	assert_fail(count*SIZEOF_effect_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_EFFECT_TYPES, "");
	if (data_length > 0)
	{
		if (!PhysicsModelLoaded) load_default_physics();
		PhysicsModelLoaded = true;
		unpack_effect_definition(data,count);
	}
	
	data= (uint8 *)get_wad_resource_for_tag(wad, PROJECTILE_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_projectile_definition;
	assert_fail(count*SIZEOF_projectile_definition == data_length, "");
	assert_fail(count <= NUMBER_OF_PROJECTILE_TYPES, "");
	if (data_length > 0)
	{
		if (!PhysicsModelLoaded) load_default_physics();
		PhysicsModelLoaded = true;
		unpack_projectile_definition(data,count);
	}
	
	data= (uint8 *)get_wad_resource_for_tag(wad, PHYSICS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_physics_constants;
	assert_fail(count*SIZEOF_physics_constants == data_length, "");
	assert_fail(count <= get_number_of_physics_models(), "");
	if (data_length > 0)
	{
		if (!PhysicsModelLoaded) load_default_physics();
		PhysicsModelLoaded = true;
		unpack_physics_constants(data,count);
	}
	
	data= (uint8 *)get_wad_resource_for_tag(wad, WEAPONS_PHYSICS_TAG, &data_length);
	count = data_length/SIZEOF_weapon_definition;
	assert_fail(count*SIZEOF_weapon_definition == data_length, "");
	assert_fail(count <= get_number_of_weapon_types(), "");
	if (data_length > 0)
	{
		if (!PhysicsModelLoaded) load_default_physics();
		PhysicsModelLoaded = true;
		unpack_weapon_definition(data,count);
	}
	
	// ghs: always reload the physics model if there isn't one merged
	if (!PhysicsModelLoaded && !game_is_networked) load_external_physics_file(); // TODO: load_external_physics_file could fail; however, it currently silently suppresses any errors (which may include there not being an external/embedded physics file present, in which case it's a no-op, so we aren't going to futz with it right now)
	
	RunScriptChunks();

	init_ephemera(dynamic_world->polygon_count);

	PolygonListCopy = PolygonList; // must be done before polygons heights are modified below

	/* If we are restoring the game, then we need to add the dynamic data */
	if(restoring_game)
	{
		// Slurp it all in...
		data= (uint8 *)get_wad_resource_for_tag(wad, MAP_INDEXES_TAG, &data_length);
		count= data_length/sizeof(short);
		assert_fail(count*int32(sizeof(short))==data_length, "");
		MapIndexList.resize(count);
		StreamToList(data,map_indexes,count);
		
		bool result = get_player_data_from_wad(wad);
		assert_fail(result, "");
		
		result = get_dynamic_data_from_wad(wad, dynamic_world);
		assert_fail(result, "");
		
		data= (uint8 *)get_wad_resource_for_tag(wad, OBJECT_STRUCTURE_TAG, &data_length);
		count= data_length/SIZEOF_object_data;
		assert_fail(count*SIZEOF_object_data==data_length, "");
		assert_fail_f(count <= MAXIMUM_OBJECTS_PER_MAP, "Number of map objects %zu > limit %u",count,MAXIMUM_OBJECTS_PER_MAP, "");
		unpack_object_data(data,objects,count);
		
		// Unpacking is E-Z here...
		data= (uint8 *)get_wad_resource_for_tag(wad, AUTOMAP_LINES, &data_length);
		memcpy(automap_lines,data,data_length);
		data= (uint8 *)get_wad_resource_for_tag(wad, AUTOMAP_POLYGONS, &data_length);
		memcpy(automap_polygons,data,data_length);

		data= (uint8 *)get_wad_resource_for_tag(wad, MONSTERS_STRUCTURE_TAG, &data_length);
		count= data_length/SIZEOF_monster_data;
		assert_fail(count*SIZEOF_monster_data==data_length, "");
		assert_fail_f(count <= MAXIMUM_MONSTERS_PER_MAP, "Number of monsters %zu > limit %u",count,MAXIMUM_MONSTERS_PER_MAP, "");
		unpack_monster_data(data,monsters,count);

		data= (uint8 *)get_wad_resource_for_tag(wad, EFFECTS_STRUCTURE_TAG, &data_length);
		count= data_length/SIZEOF_effect_data;
		assert_fail(count*SIZEOF_effect_data==data_length, "");
		assert_fail_f(count <= MAXIMUM_EFFECTS_PER_MAP, "Number of effects %zu > limit %u",count,MAXIMUM_EFFECTS_PER_MAP, "");
		unpack_effect_data(data,EffectList.data(),count);

		data= (uint8 *)get_wad_resource_for_tag(wad, PROJECTILES_STRUCTURE_TAG, &data_length);
		count= data_length/SIZEOF_projectile_data;
		assert_fail(count*SIZEOF_projectile_data==data_length, "");
		assert_fail_f(count <= MAXIMUM_PROJECTILES_PER_MAP, "Number of projectiles %zu > limit %u",count,MAXIMUM_PROJECTILES_PER_MAP, "");
		unpack_projectile_data(data,projectiles,count);
		
		data= (uint8 *)get_wad_resource_for_tag(wad, PLATFORM_STRUCTURE_TAG, &data_length);
		count= data_length/SIZEOF_platform_data;
		assert_fail(count*SIZEOF_platform_data==data_length, "");
		PlatformList.resize(count);
		unpack_platform_data(data,platforms,count);
		
		data= (uint8 *)get_wad_resource_for_tag(wad, WEAPON_STATE_TAG, &data_length);
		count= data_length/SIZEOF_player_weapon_data;
		assert_fail(count*SIZEOF_player_weapon_data==data_length, "");
		unpack_player_weapon_data(data,count);
		
		data= (uint8 *)get_wad_resource_for_tag(wad, TERMINAL_STATE_TAG, &data_length);
		count= data_length/SIZEOF_player_terminal_data;
		assert_fail(count*SIZEOF_player_terminal_data==data_length, "");
		unpack_player_terminal_data(data,count);
		
		complete_restoring_level(wad);
	} else {
		uint8 *map_index_data;
		size_t map_index_count;
		uint8 *platform_structures;
		size_t platform_structure_count;

		if(version==MARATHON_ONE_DATA_VERSION)
		{
			/* Force precalculation */
			map_index_data= NULL;
			map_index_count= 0; 
		} else {
			map_index_data= (uint8 *)get_wad_resource_for_tag(wad, MAP_INDEXES_TAG, &data_length);
			map_index_count= data_length/sizeof(short);
			assert_fail(map_index_count*sizeof(short)==data_length, "");
		}

		assert_fail((is_preprocessed_map && map_index_count) || (!is_preprocessed_map && !map_index_count), "");

		data= (uint8 *)get_wad_resource_for_tag(wad, PLATFORM_STATIC_DATA_TAG, &data_length);
		count= data_length/SIZEOF_static_platform_data;
		assert_fail(count*SIZEOF_static_platform_data==data_length, "");
		
		platform_structures= (uint8 *)get_wad_resource_for_tag(wad, PLATFORM_STRUCTURE_TAG, &data_length);
		platform_structure_count= data_length/SIZEOF_platform_data;
		assert_fail(platform_structure_count*SIZEOF_platform_data==data_length, "");
		
		complete_loading_level((short *) map_index_data, map_index_count,
			data, count, platform_structures,
			platform_structure_count, version);

	}

	PlatformListCopy = PlatformList;
}

ao_err get_dynamic_data_from_wad(wad_data* wad, dynamic_data* result)
{
	size_t data_length;
	uint8_t* data = get_wad_resource_for_tag(wad, DYNAMIC_STRUCTURE_TAG, &data_length); // this can fail, errWadTagNotFound; however, there's a LOT of calls to it and not gonna change them all right now
    if (!data || data_length != SIZEOF_dynamic_data) return STRID(strERRORS, errWadTagNotFound);
	unpack_dynamic_data(data, result, 1);
    return no_err;
}

bool get_player_data_from_wad(wad_data* wad)
{
	size_t data_length;
	auto data = (uint8*)get_wad_resource_for_tag(wad, PLAYER_STRUCTURE_TAG, &data_length);
	auto count = data_length / SIZEOF_player_data;
	bool success = count * SIZEOF_player_data == data_length ? (bool)unpack_player_data(data, players, count) : false;
	if (success) team_damage_from_player_data();
	return success;
}

static void allocate_map_structure_for_map(
	struct wad_data *wad)
{
	size_t data_length;
	size_t line_count, polygon_count, side_count, endpoint_count;

	/* Extract points */
	get_wad_resource_for_tag(wad, POINT_TAG, &data_length);
	endpoint_count= data_length/SIZEOF_world_point2d;
    if(endpoint_count*SIZEOF_world_point2d!=data_length) { exit(corruptedMap); } // 'pt'
	
	if(!endpoint_count)
	{
		get_wad_resource_for_tag(wad, ENDPOINT_DATA_TAG, &data_length);
		endpoint_count= data_length/SIZEOF_endpoint_data;
        if(endpoint_count*SIZEOF_endpoint_data!=data_length) { exit(corruptedMap); } // 'ep'
	}

	/* Extract lines */
	get_wad_resource_for_tag(wad, LINE_TAG, &data_length);
	line_count= data_length/SIZEOF_line_data;
    if(line_count*SIZEOF_line_data!=data_length) { exit(corruptedMap); } // 'li'

	/* Sides.. */
	get_wad_resource_for_tag(wad, SIDE_TAG, &data_length);
	side_count= data_length/SIZEOF_side_data;
    if(side_count*SIZEOF_side_data!=data_length) { exit(corruptedMap); } // 'si'

	/* Extract polygons */
	get_wad_resource_for_tag(wad, POLYGON_TAG, &data_length);
	polygon_count= data_length/SIZEOF_polygon_data;
    if(polygon_count*SIZEOF_polygon_data!=data_length) { exit(corruptedMap); } // 'si'

	allocate_map_for_counts(polygon_count, side_count, endpoint_count, line_count);
}

/* Note that we assume the redundant data has already been recalculated... */
static void load_redundant_map_data(
	short *redundant_data,
	size_t count)
{
	if (redundant_data)
	{
		// assert_fail(redundant_data && map_indexes, "");
		uint8 *Stream = (uint8 *)redundant_data;
		MapIndexList.resize(count);
		StreamToList(Stream,map_indexes,count);
		assert_fail(count == static_cast<size_t>(static_cast<int16>(count)), "");
		assert_fail(0 <= static_cast<int16>(count), "");
		dynamic_world->map_index_count= static_cast<int16>(count);
	}
	else
	{
		recalculate_redundant_map();
		precalculate_map_indexes();
	}
}

void load_terminal_data(
	uint8 *data, 
	size_t length)
{
	/* I would really like it if I could get these into computer_interface.c statically */
	unpack_map_terminal_data(data,length);
}

static void scan_and_add_scenery(
	void)
{
	short ii;
	struct map_object *saved_object;
	
	saved_object= saved_objects;
	for(ii=0; ii<dynamic_world->initial_objects_count; ++ii)
	{
		if (saved_object->type==_saved_object)
		{
			struct object_location location;
			
			location.p= saved_object->location;
			location.flags= saved_object->flags;
			location.yaw= saved_object->facing;
			location.polygon_index= saved_object->polygon_index;
			new_scenery(&location, saved_object->index);
		}
		
		++saved_object;
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
	{ LIGHTSOURCE_TAG, SIZEOF_static_light_data, true, },
	{ ANNOTATION_TAG, SIZEOF_map_annotation, true },
	{ OBJECT_TAG, SIZEOF_map_object, true },
	{ MAP_INFO_TAG, SIZEOF_static_data, true },
	{ ITEM_PLACEMENT_STRUCTURE_TAG, SIZEOF_object_frequency_definition, true },
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
	{ LIGHTSOURCE_TAG, SIZEOF_light_data, true }, // false },
	{ ANNOTATION_TAG, SIZEOF_map_annotation, true },
	{ OBJECT_TAG, SIZEOF_map_object, true },
	{ MAP_INFO_TAG, SIZEOF_static_data, true },
	{ ITEM_PLACEMENT_STRUCTURE_TAG, SIZEOF_object_frequency_definition, true },
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
	{ TERMINAL_STATE_TAG, SIZEOF_player_terminal_data, true }, // false }

	{ LUA_STATE_TAG, sizeof(byte), true },
};

static uint8 *export_tag_to_global_array_and_size(
	uint32 tag,
	size_t *size
	)
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
		count = dynamic_world->endpoint_count;
		break;

	case LIGHTSOURCE_TAG:
		count = dynamic_world->light_count;
		break;

	case PLATFORM_STATIC_DATA_TAG:
		count = dynamic_world->platform_count;
		break;

	case POLYGON_TAG:
		count = dynamic_world->polygon_count;
		break;

	default:
            throw_ao_exception("bad WAD tag: %x", 1, tag);
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
			world_point2d& vertex = map_endpoints[loop].vertex;
			ValueToStream(temp_array, vertex.x);
			ValueToStream(temp_array, vertex.y);
		}
		break;

	case LIGHTSOURCE_TAG:
		for (size_t loop = 0; loop < count; ++loop)
		{
			temp_array = pack_static_light_data(temp_array, &lights[loop].static_data, 1);
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
			for (size_t loop = 0; loop < count; ++loop)
			{
				// ghs: this belongs somewhere else
				static_platform_data platform;
				obj_clear(platform);
				platform.type = platforms[loop].type;
				platform.speed = platforms[loop].speed;
				platform.delay = platforms[loop].delay;
				if (PLATFORM_GOES_BOTH_WAYS(&platforms[loop]))
				{
					platform.maximum_height = platforms[loop].maximum_ceiling_height;
					platform.minimum_height = platforms[loop].minimum_floor_height;
				}
				else if (PLATFORM_COMES_FROM_FLOOR(&platforms[loop]))
				{
					platform.maximum_height = platforms[loop].maximum_floor_height;
					platform.minimum_height = platforms[loop].minimum_floor_height;
				}
				else
				{
					platform.maximum_height = platforms[loop].maximum_ceiling_height;
					platform.minimum_height = platforms[loop].minimum_floor_height;
				}
				platform.static_flags = platforms[loop].static_flags;
				platform.polygon_index = platforms[loop].polygon_index;
				platform.tag = platforms[loop].tag;

				temp_array = pack_static_platform_data(temp_array, &platform, 1);
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
            throw_ao_exception("bad WAD tag: %x", 1, tag);
		break;
	}

	return array;
}

extern size_t save_lua_states();
extern void pack_lua_states(uint8*, size_t);
		

/* the sizes are the sizes to save in the file, be aware! */
static uint8 *tag_to_global_array_and_size(
	uint32 tag, 
	size_t *size
	)
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
	
	// LP: had fixed off-by-one error in medias saving,
	// and had added physics-model saving
	
	switch (tag)
	{
		case ENDPOINT_DATA_TAG:
			count= dynamic_world->endpoint_count;
			break;
		case LINE_TAG:
			count= dynamic_world->line_count;
			break;
		case SIDE_TAG:
			count= dynamic_world->side_count;
			break;
		case POLYGON_TAG:
			count= dynamic_world->polygon_count;
			break;
		case LIGHTSOURCE_TAG:
			count= dynamic_world->light_count;
			break;
		case ANNOTATION_TAG:
			count= dynamic_world->default_annotation_count;
			break;
		case OBJECT_TAG:
			count= dynamic_world->initial_objects_count;
			break;
		case MAP_INFO_TAG:
			count= 1;
			break;
		case PLAYER_STRUCTURE_TAG:
			count= dynamic_world->player_count;
			break;
		case DYNAMIC_STRUCTURE_TAG:
			count= 1;
			break;
		case OBJECT_STRUCTURE_TAG:
			count= dynamic_world->object_count;
			break;
		case MAP_INDEXES_TAG:
			count= static_cast<unsigned short>(dynamic_world->map_index_count);
			break;
		case AUTOMAP_LINES:
			count= (dynamic_world->line_count/8+((dynamic_world->line_count%8)?1:0)); 
			break;
		case AUTOMAP_POLYGONS:
			count= (dynamic_world->polygon_count/8+((dynamic_world->polygon_count%8)?1:0));
			break;
		case MONSTERS_STRUCTURE_TAG:
			count= dynamic_world->monster_count;
			break;
		case EFFECTS_STRUCTURE_TAG:
			count= dynamic_world->effect_count;
			break;
		case PROJECTILES_STRUCTURE_TAG:
			count= dynamic_world->projectile_count;
			break;
		case MEDIA_TAG:
			count= count_number_of_medias_used();
			break;
		case ITEM_PLACEMENT_STRUCTURE_TAG:
			count= 2*MAXIMUM_OBJECT_TYPES;
			break;
		case PLATFORM_STRUCTURE_TAG:
			count= dynamic_world->platform_count;
			break;
		case AMBIENT_SOUND_TAG:
			count= dynamic_world->ambient_sound_image_count;
			break;
		case RANDOM_SOUND_TAG:
			count= dynamic_world->random_sound_image_count;
			break;
		case TERMINAL_DATA_TAG:
			count= calculate_packed_terminal_data_length();
			break;
		case WEAPON_STATE_TAG:
			count= dynamic_world->player_count;
			break;
		case TERMINAL_STATE_TAG:
			count= dynamic_world->player_count;
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
			GetMMLS(count);
			break;
	    case LUAS_TAG:
			GetLUAS(count);
			break;
		case LUA_STATE_TAG:
			count= save_lua_states();
			break;
		default:
            throw_ao_exception("unknown WAD tag: %x", 1, tag);
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
			pack_endpoint_data(array,map_endpoints,count);
			break;
		case LINE_TAG:
			pack_line_data(array,map_lines,count);
			break;
		case SIDE_TAG:
			pack_side_data(array,map_sides,count);
			break;
		case POLYGON_TAG:
			pack_polygon_data(array,map_polygons,count);
			break;
		case LIGHTSOURCE_TAG:
			pack_light_data(array,lights,count);
			break;
		case ANNOTATION_TAG:
			pack_map_annotation(array,map_annotations,count);
			break;
		case OBJECT_TAG:
			pack_map_object(array,saved_objects,count);
			break;
		case MAP_INFO_TAG:
			pack_static_data(array,static_world,count);
			break;
		case PLAYER_STRUCTURE_TAG:
			pack_player_data(array,players,count);
			break;
		case DYNAMIC_STRUCTURE_TAG:
			pack_dynamic_data(array,dynamic_world,count);
			break;
		case OBJECT_STRUCTURE_TAG:
			pack_object_data(array,objects,count);
			break;
		case MAP_INDEXES_TAG:
			ListToStream(temp_array,map_indexes,count); // E-Z packing here...
			break;
		case AUTOMAP_LINES:
			memcpy(array,automap_lines,*size);
			break;
		case AUTOMAP_POLYGONS:
			memcpy(array,automap_polygons,*size);
			break;
		case MONSTERS_STRUCTURE_TAG:
			pack_monster_data(array,monsters,count);
			break;
		case EFFECTS_STRUCTURE_TAG:
			pack_effect_data(array,EffectList.data(),count);
			break;
		case PROJECTILES_STRUCTURE_TAG:
			pack_projectile_data(array,projectiles,count);
			break;
		case MEDIA_TAG:
			pack_media_data(array,medias,count);
			break;
		case ITEM_PLACEMENT_STRUCTURE_TAG:
			pack_object_frequency_definition(array,get_placement_info(),count);
			break;
		case PLATFORM_STRUCTURE_TAG:
			pack_platform_data(array,platforms,count);
			break;
		case AMBIENT_SOUND_TAG:
			pack_ambient_sound_image_data(array,ambient_sound_images,count);
			break;
		case RANDOM_SOUND_TAG:
			pack_random_sound_image_data(array,random_sound_images,count);
			break;
		case TERMINAL_DATA_TAG:
			pack_map_terminal_data(array,count);
			break;
		case WEAPON_STATE_TAG:
			pack_player_weapon_data(array,count);
			break;
		case TERMINAL_STATE_TAG:
			pack_player_terminal_data(array,count);
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
			memcpy(array, GetMMLS(count), count);
			break;
		case LUAS_TAG:
			memcpy(array, GetLUAS(count), count);
			break;
		case LUA_STATE_TAG:
			pack_lua_states(array, count);
			break;
		default:
            throw_ao_exception("unknown WAD tag: %x", 1, tag);
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


/* Load and slam all of the arrays */
static void complete_restoring_level(
	struct wad_data *wad)
{
	ok_to_reset_scenery_solidity = false;
	/* Loading games needs this done. */
	reset_action_queues();
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

static uint8 *unpack_directory_data(uint8 *Stream, directory_data *Objects, size_t Count)
{
	uint8* S = Stream;
	directory_data* ObjPtr = Objects;

	for (size_t k = 0; k < Count; k++, ObjPtr++)
    {
        StreamToValue(S,ObjPtr->mission_flags);                             //  2-byte
        StreamToValue(S,ObjPtr->environment_flags);                         //  2-byte
        StreamToValue(S,ObjPtr->entry_point_flags);                         //  4-byte
        
        read_macroman_string(S, ObjPtr->level_name, MAX_LEVEL_NAME_LENGTH); // 64-byte
        S += 2;                                                             //  2-byte
    }
    
	assert_fail((S - Stream) == SIZEOF_directory_data, "corrupt WAD directory");                          // 74-byte
	return S;
}

// ZZZ: gnu cc swears this is currently unused, and I don't see any sneaky #includes that might need it...
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
