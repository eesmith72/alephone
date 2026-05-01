/*
 map_wad.h
 
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

#ifndef __map_wad_h__
#define __map_wad_h__

#include "cstypes.hpp"

#include "map.h"
#include "wad.h" // wad_data


const int SAVE_GAME_METADATA_INDEX = 1000; // used in save_game_file in map_wad.cpp and in QuickSave.cpp



// Return true if it finds the file, and it sets the mapfile to that file.
// Otherwise it returns false, meaning that we need have the file sent to us. // TODO: this comment is probably out of date
ao_err set_current_map_path_to_file_with_checksum(uint32 checksum);

// important: load_base_and_default_scripts must be called before this function; TODO: why? the level scripts are being unpacked and parsed in middle of this function
ao_err load_level_from_map_wad_file(const ao_path& map_path, short level_index, bool is_saved_game);

uint32 get_current_map_checksum();


// bodge till we get WAD API sorted out, but this lets main loop find out if saved game file is solo or co-op
ao_err get_dynamic_data_from_saved_game_file(const ao_path& path, dynamic_world_t& result);




// a saved game file is a WAD file containing the saved level's static and dynamic state (this includes the level's terminals and any level-specific physics), captured when user saved game at a pattern buffer; loading this file restores the gameworld to that state, though I don't think there's any checks for correct Shapes and Sounds
ao_err save_game_to_file(const ao_path& File, const std::string& metadata, const std::string& imagedata);


ao_err export_level(const ao_path& File);



wad_data* build_meta_game_wad(const std::string& metadata, const std::string& imagedata, wad_header_t *header, int32 *length);




// parse the level data and set up map state (global vars in map.cpp)
void initialize_level_from_wad_data(struct wad_data *wad, bool is_saved_game, short version);


bool match_checksum_with_map(short vRefNum, long dirID, uint32 checksum, const ao_path& File);



// net transfer, solo films
ao_err get_flat_wad_for_level_of_current_map(int16_t level_number, uint8_t*& flat_data);


ao_err get_dynamic_data_from_wad(wad_data* wad, dynamic_world_t& result);

int16_t get_number_of_players_from_wad(wad_data* wad); // saved game (file/embedded in film/network-distributed)

ao_err unpack_player_data_from_wad(wad_data* wad); // ditto


void set_current_map_path(const ao_path& path);

const ao_path& get_current_map_path();


ao_err revert_game();


ao_err level_has_embeds(int Level, bool& HasPhysics, bool& HasLua);



#endif /* __map_wad_h__ */
