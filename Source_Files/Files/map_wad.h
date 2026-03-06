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


bool load_game_from_file(const ao_path& File, bool run_scripts);


ao_err save_game_file(const ao_path& File, const std::string& metadata, const std::string& imagedata);


ao_err export_level(const ao_path& File);



wad_data* build_meta_game_wad(const std::string& metadata, const std::string& imagedata, struct wad_header *header, int32 *length);



// ZZZ: split this out from new_game; it sets a default filespec in the revert-game info
void set_saved_game_name_to_default(); // called in map_wad.cpp and in interface.cpp



// ZZZ: exposed this for netgame-resuming code
bool process_map_wad(struct wad_data *wad, bool restoring_game, short version);


bool match_checksum_with_map(short vRefNum, long dirID, uint32 checksum, const ao_path& File);


dynamic_data get_dynamic_data_from_save(const ao_path& File);

bool get_dynamic_data_from_wad(wad_data* wad, dynamic_data* dest);

bool get_player_data_from_wad(wad_data* wad);


void set_current_map_path(const ao_path& path, bool runScript = true);

const ao_path& get_current_map_path();


void level_has_embedded_physics_lua(int Level, bool& HasPhysics, bool& HasLua);



#endif /* __map_wad_h__ */
