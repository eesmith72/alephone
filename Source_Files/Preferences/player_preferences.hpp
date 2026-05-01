/*
 player_preferences.hpp
 
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

#ifndef player_preferences_hpp
#define player_preferences_hpp

#include "cseries.h"

#include "InfoTree.h"




enum SoloProfileType
{
    _solo_profile_aleph_one,
    _solo_profile_unused,        // hope springs eternal
    _solo_profile_marathon_2,
    _solo_profile_marathon_infinity,
    NUMBER_OF_SOLO_PROFILE_TYPES
};


enum // Chase-cam flags

{
    _ChaseCam_OnWhenEntering = 0x0004,
    _ChaseCam_NeverActive = 0x0002,
    _ChaseCam_ThroughWalls = 0x0001
};


struct ChaseCamData
{
    short Behind;
    short Upward;
    short Rightward;
    short Flags;
    float Damping;
    float Spring;
    float Opacity;
};


struct player_preferences_data
{
    std::string  name;
    int16_t      color;
    int16_t      team;
    uint32_t     last_time_ran;
    int16_t      difficulty_level;
    bool         background_music_on;
    bool         crosshairs_active;
    int32_t      solo_profile;
    
    ChaseCamData ChaseCam;
    
    
    void reset()
    {
        name                = get_username_os();
        color               = 0;
        team                = 0;
        last_time_ran       = 0;
        difficulty_level    = 2;
        background_music_on = false;
        crosshairs_active   = false;
        solo_profile        = _solo_profile_aleph_one;
        
        ChaseCam.Behind     = 1536;
        ChaseCam.Upward     = 0;
        ChaseCam.Rightward  = 0;
        ChaseCam.Flags      = 0;
        ChaseCam.Damping    = 0.5;
        ChaseCam.Spring     = 0;
        ChaseCam.Opacity    = 1;
        
    }
    
    void read(InfoTree root, std::string version);

    InfoTree write();
};


extern player_preferences_data player_preferences;



bool dont_switch_to_new_weapon();
bool dont_auto_recenter();

// ZZZ: let code disable (standardize)/enable behavior modifiers like dont_switch
void set_custom_behaviors_enabled(bool can_customize);

// ZZZ: return whether the user's behavior matches standard behavior (either by being forced so or by chosen that way)
bool is_player_behavior_standard();



void player_dialog(void *arg);





#endif /* player_preferences_hpp */
