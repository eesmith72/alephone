/*
 environment_preferences.hpp
 
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

#ifndef environment_preferences_hpp
#define environment_preferences_hpp

#include "cseries.h"

#include "wad.h" // read_wad_file_checksum



#define MAXIMUM_PATCHES_PER_ENVIRONMENT (32)

struct environment_preferences_data
{
    void reset();
    
    ao_path map_file;
    uint32_t map_checksum; // checksums/modification dates for identity comparisons
    
    void set_map_file(const ao_path& path)
    {
        map_file = path;
        map_checksum = read_wad_file_checksum(map_file);
    }
    
    
    ao_path physics_file;
    uint32_t physics_checksum;
    
    void set_physics_file(const ao_path& path)
    {
        physics_file = path;
        physics_checksum = read_wad_file_checksum(physics_file);
    }
    
    
    ao_path shapes_file;
    std::filesystem::file_time_type shapes_mod_date;
    
    void set_shapes_file(const ao_path& path)
    {
        shapes_file = path;
        // Shapes and Sounds don't have checksums, so use modification date for identity checks; TODO: this is not ideal
        shapes_mod_date = std::filesystem::is_regular_file(shapes_file) ? std::filesystem::last_write_time(shapes_file)
                                                                        : std::filesystem::file_time_type::min();
    }
    
    
    ao_path sounds_file;
    std::filesystem::file_time_type sounds_mod_date;
    
    void set_sounds_file(const ao_path& path)
    {
        sounds_file = path;
        sounds_mod_date = std::filesystem::is_regular_file(sounds_file) ? std::filesystem::last_write_time(sounds_file)
                                                                        : std::filesystem::file_time_type::min();
    }
    
    
    ao_path resources_file; // the Marathon 1 App's extracted resource fork // TODO: what about M2 Images file?

    void set_resources_file(const ao_path& path)
    {
        resources_file = path;
    }
    
    // TODO: API for lua file[s]
    ao_path solo_lua_file;
    bool use_solo_lua;
    bool use_replay_net_lua;
    bool hide_extensions;
    
    uint32_t patches[MAXIMUM_PATCHES_PER_ENVIRONMENT];
    
    // ZZZ: these aren't really environment preferences, but preferences that affect the environment preferences dialog
    bool group_by_directory;    // if not, display popup as one giant flat list
    bool reduce_singletons;        // make groups of a single element part of a larger parent group

    // ghs: are themes part of the environment? they are now
    bool smooth_text;

    FilmProfileType film_profile; // for legacy films

    // how many auto-named save files to keep around (0 is unlimited)
    uint32 maximum_quick_saves;

#ifdef HAVE_NFD
    bool use_native_file_dialogs;
#endif

    bool auto_play_demos;
    
    
    void read(InfoTree root, std::string version);
    
    InfoTree write();
};


extern environment_preferences_data environment_preferences;




void load_scenario_from_environment_preferences(); // sets the file paths in environment_preferences struct as the current map, shapes, sounds, physics, images/external-resources files // TODO: where should this go? (the paths are currently stored on the prefs struct above); it's quite confused in what it should do (may be best to leave for now as all this stuff needs redesigned anyway)




void environment_dialog(void *arg);

void plugins_dialog(void *arg);





#endif /* environment_preferences_hpp */
