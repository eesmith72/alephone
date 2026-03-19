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

#include "cstypes.h"

#include "map.h"
#include "wad.h" // wad_data


const int SAVE_GAME_METADATA_INDEX = 1000; // used in save_game_file in map_wad.cpp and in QuickSave.cpp



/* Return true if it finds the file, and it sets the mapfile to that file. */
/* Otherwise it returns false, meaning that we need have the file sent to us. */
ao_err set_current_map_path_to_file_with_checksum(uint32 checksum);

ao_err load_level_from_map(short level_index);

uint32 get_current_map_checksum();




ao_err load_game_from_file(const ao_path& File, bool run_scripts);


// a saved game file is a WAD file containing the saved level's static and dynamic state (this includes the level's terminals and any level-specific physics), captured when user saved game at a pattern buffer; loading this file restores the gameworld to that state, though I don't think there's any checks for correct Shapes and Sounds
ao_err save_game_to_file(const ao_path& File, const std::string& metadata, const std::string& imagedata);


ao_err export_level(const ao_path& File);



wad_data* build_meta_game_wad(const std::string& metadata, const std::string& imagedata, wad_header_t *header, int32 *length);






// ZZZ: exposed this for netgame-resuming code
void process_map_wad(struct wad_data *wad, bool restoring_game, short version);


bool match_checksum_with_map(short vRefNum, long dirID, uint32 checksum, const ao_path& File);


void process_net_map_data(uint8_t* flat_data); // Note that this frees it as well

ao_err get_map_for_net_transfer(entry_point* entry, uint8_t*& flat_data);


ao_err get_dynamic_data_from_wad(wad_data* wad, dynamic_data* result);

bool get_player_data_from_wad(wad_data* wad);


void set_current_map_path(const ao_path& path, bool runScript = true);

const ao_path& get_current_map_path();


// TODO: this is only called when loading a saved game file for which the original Map file can't be found; since this breaks level jump and also screws up script loading, it'd make more sense not to load that saved game and go straight to error dialog; also, it is unclear why this resets to the scenario's default map (defined by standard or MML-defined strFILENAMES) instead of the map currently selected in Environment prefs
#define reset_current_map_path_to_default()  (set_current_map_path(get_default_map_path()))

// TODO: also not sure what this one is doing, given QuickSave has been the standard saving behavior for years; stuff's an absolute shambles
// ZZZ: split this out from new_game; it sets a default filespec in the revert-game info
void reset_revert_game_file_to_default(); // called in map_wad.cpp and in interface.cpp


ao_err revert_game();


ao_err level_has_embeds(int Level, bool& HasPhysics, bool& HasLua);



#endif /* __map_wad_h__ */
