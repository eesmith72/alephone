/*
 graphics_preferences.hpp
 
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

#ifndef graphics_preferences_hpp
#define graphics_preferences_hpp

#include "cseries.h"

#include "InfoTree.h"


#define FPS_UNLIMITED  (0)
#define FPS_DEFAULT   (30)
//#define FPS_2X        (60)
//#define FPS_4X       (120)


enum
{
    NUMBER_OF_GAMMA_LEVELS= 8,
    DEFAULT_GAMMA_LEVEL= 2
};

// Lua_EphemeraQuality
enum {
    _ephemera_off,
    _ephemera_low,
    _ephemera_medium,
    _ephemera_high,
    _ephemera_ultra
};


enum class BobbingType
{
    none,
    camera_and_weapon,
    weapon_only
};



struct graphics_preferences_data
{
    screen_mode_t screen_mode;
    bool fullscreen;
    int32_t gamma_level;
    
    short hud_size; // 0-3 (0=none)
    short terminal_size;
    short automap_size;
    bool translucent_map;
    bool crosshairs_is_visible; // Lua plugin only now
    
    BobbingType bobbing_type;
    // Indicates whether to fix the horizontal or the vertical field-of-view angle (default: fix vertical FOV angle)
    // i.e. in netgames, the player with the widest monitor has a significant advantage if that monitor shows a wider FOV
    bool horizontal_fov_is_constant;
    int fov; // TODO: relation to camera's field_of_vision?
    int16 ephemera_quality; // this value is passed to lua_ephemera scripts; the SW and HW graphics dialogs shared the same graphics_preferences.ephemera_quality; ideally we'd want ephemera turned off in Classic mode but changing it in-game when resolution switches would be tricky as existing Lua scripts presumably don't expect this value to change on the fly; best leave it for now and consider removing ephemera rendering calls from the Classic renderer so that even if running fog/rain/etc simply aren't drawn

    
   // OGL_ConfigureData OGL_Configure; // moved to OGL_Setup.h and renamed ogl_preferences

    bool show_fps;
    
    int16 in_game_fps_target; // should be a multiple of 30 (0 = unlimited)
    
    int16 current_fps_target(); // for the current display mode (UI or in-game)
    
    
    // TODO: movie_export_video_resolution
    int16 movie_export_video_quality;
    int32 movie_export_video_bitrate; // 0 is automatic
    int16 movie_export_audio_quality;
        
    void reset();
    
    void read(InfoTree root, std::string version);
    
    InfoTree write();
};


extern graphics_preferences_data graphics_preferences;




void graphics_dialog(void *arg);





#endif /* graphics_preferences_hpp */
