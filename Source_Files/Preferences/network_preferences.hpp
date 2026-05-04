/*
 network_preferences.hpp
 
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

#ifndef network_preferences_hpp
#define network_preferences_hpp

#include "cseries.hpp"

#include "InfoTree.h"


extern SDL_Color get_interface_color(int32_t index); // in screen_drawing.cpp; need to sort out


#define DEFAULT_GAME_PORT (4226)

extern void DefaultStarPreferences(); // in StarGameProtocol.cpp


struct network_preferences_data
{
    bool game_is_untimed;
    int16 game_type;
    int16 difficulty_level;
    uint16 game_options; // Penalize suicide, etc... see map.h for constants
    int32 time_limit;
    int16 kill_limit;
    int16 level_identity;
    bool autogather;
    bool join_by_address;
    std::string join_address;
    uint16 game_port;    // TCP and UDP port number used for game traffic (not player-location traffic)
    bool use_netscript;
    ao_path netscript_file;
    uint16 cheat_flags;
    bool advertise_on_metaserver;
    bool attempt_upnp;
    bool check_for_updates;
    bool verify_https;

    enum {
        kMetaserverLoginLength = 16
    };

    std::string metaserver_login;
    std::string metaserver_password;
    bool use_custom_metaserver_colors;
    SDL_Color metaserver_colors[2];
    bool mute_metaserver_guests;
    bool join_metaserver_by_default;
    bool allow_stats;
    
    
    
    void reset()
    {
        game_is_untimed = false;
        difficulty_level = 2;
        game_options = _multiplayer_game
                     | _ammo_replenishes
                     | _weapons_replenish
                     | _specials_replenish
                     | _burn_items_on_death
                     | _force_unique_teams
                     | _live_network_stats;
        time_limit = 10 * TICKS_PER_SECOND * 60;
        kill_limit = 10;
        level_identity= 0;
        game_type= _game_of_kill_monsters;
        
        autogather= false;
        join_by_address= false;
        join_address.clear();
        game_port= DEFAULT_GAME_PORT;
        DefaultStarPreferences();
        use_netscript = false;
        netscript_file.clear();
        cheat_flags = _allow_tunnel_vision | _allow_crosshair | _allow_behindview | _allow_overlay_map;
        
        advertise_on_metaserver = false;
        attempt_upnp = false;
        check_for_updates = true;
        verify_https = false;
        metaserver_login = "guest";
        metaserver_password.clear();
        mute_metaserver_guests = false;
        use_custom_metaserver_colors = false;
        metaserver_colors[0] = get_interface_color(PLAYER_COLOR_BASE_INDEX);
        metaserver_colors[1] = get_interface_color(PLAYER_COLOR_BASE_INDEX);
        join_metaserver_by_default = false;
        allow_stats = false;
    }
    
    void read(InfoTree root, std::string version);
    
    InfoTree write();
};


extern network_preferences_data network_preferences;



void online_dialog(void *arg);





#endif /* network_preferences_hpp */
