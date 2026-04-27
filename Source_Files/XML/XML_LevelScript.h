/*
 XML_LevelScript.h -- Support for XML scripts in map files
 by Loren Petrich, April 16, 2000
 
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

#ifndef __XML_LevelScript_h__
#define __XML_LevelScript_h__

#include "cseries.h"


// TODO: getting rid of LP's convoluted MML crap in favor of nice simple .lua files is for later (converting .mml to .lua will require some code generation)


void LoadBaseMMLScripts(bool load_menu_mml_only);


// Clears old scripts and reads an optional MML in resource 128 of current map file
// This MML has a `<marathon_levels>` root tag, containing 'LevelScriptCommands' (MML, Lua, Movie, Music).
void read_scripts_from_current_map();

//
void load_base_and_default_scripts(int level_number);
void parse_level_scripts();

// Intended to be run at the end of a game
void load_epilogue_scripts();

// Intended for restoring old parameter values, because MML sets values at a variety
// of different places, and it may be easier to simply set stuff back to defaults
// by including those defaults in the script.
void load_restore_level_scripts();


// Finds the level movie and the end movie, to be used in show_movie()
ao_path get_movie_path_for_level(int LevelIndex);


// For selecting the end-of-game screens -- what fake level index for them, and how many to display
// (resource numbers increasing in sequence)
void get_epilogue_screen_base_id_and_count(int32_t& end_offset, int32_t& end_count);



void unpack_mml_level_scripts_data(uint8* Stream, size_t length);
void pack_mml_level_scripts_data(uint8_t* Stream);
size_t get_length_of_mml_level_scripts_data();

void unpack_lua_level_scripts_data(uint8* Stream, size_t length);
void pack_lua_level_scripts_data(uint8_t* Stream);
size_t get_length_of_lua_level_scripts_data();


class InfoTree;
void parse_mml_default_levels(const InfoTree& root);
void reset_mml_default_levels();


#endif /* __XML_LevelScript_h__ */
