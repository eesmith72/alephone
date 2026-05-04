/*
 camera.hpp -- renamed `view_data` struct from render.h
 
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

#ifndef _VIEW_CONTROL_
#define _VIEW_CONTROL_

#include "world.h"
#include "fonts.hpp"
#include "shapes.h"


// TODO: camera_settings_t belongs in Render3D/; not sure where the stuff for showing/hiding automap, etc should go yet (it's currently parked in Screen/ while disentangling Screen.cpp from `render_game_to_screen`)

//-----------------------------------------------------------------------------
// a mixture of gameplay state, precalculated SW renderer params, user prefs, and whatever else got thrown in it back in 1995

// TODO: Camera class?

struct TickWorldView; // from interpolated_world.h


struct camera_settings_t // originally `view_data`
{
    // To change the FOV, set a new target so the current FOV adjusts toward it.
    float current_field_of_view;
    float target_field_of_view;
    
    short standard_screen_width; // this is *not* the width of the projected image // EES: Least. Helpful. Comment. Ever. // TODO: FIX: was 800, now 1280
    short screen_width, screen_height; // dimensions of the projected image // TODO: what does this mean? if it's the worldview area, rename it and sort out the corresponding rect in screen.cpp // was 640x400, now 640x480
    
    // precalcuated values
    short half_screen_width, half_screen_height; // TODO: half height was 200, now 240
    short world_to_screen_x, world_to_screen_y; // 320x320
    short dtanpitch; // world_to_screen*tan(pitch) // 0
    angle half_cone; // often ==field_of_view/2 (when screen_width==standard_screen_width)
    angle half_vertical_cone; // was 46, now 53
    
    short real_world_to_screen_x, real_world_to_screen_y; // 320x320
    
    // will be set by update_camera in render.cpp
    long_vector2d left_edge, right_edge; // view cone edges as world directions
    
    short effect_ticks_elapsed;
    uint32 effect_tick_count; // for effects and transfer modes
    float heartbeat_fraction;
    
    // camera origin, presumably
    short origin_polygon_index; // TODO: FIX: should be 58
    angle yaw, pitch, roll; // TODO: FIX: was {466,0,0}, now {0,0,0}
    fixed_angle virtual_yaw, virtual_pitch; // TODO: was {30539776,0}, now {0,0}
    world_point3d origin; // not yet set
    _fixed maximum_depth_intensity; // in fixed units // TODO: FIX: was 32768, now 0
   
    angle landscape_yaw; // LP addition: value of yaw used by landscapes; this is so that the center can stay stationary // TODO: was 401, now -65
    
    // these are whole-screen effects; not sure if they should be here or elsewhere (they were on this struct originally)
    short shading_mode; // e.g. _shading_infravision
    short effect, effect_phase;
    bool under_media_boundary;
    short under_media_index;
    
   
    bool weapons_in_hand_is_visible; // true in first-person view; false for third-person (external camera) view
    
    
    
    void initialize_for_game_view(const SDL_Point& virtual_screen_size);
    
    void initialize_for_m1_exploration();
    
    void update();
    
    bool update_fov();
    
    void reset_fov();
    
    void interpolate_view(TickWorldView* previous_tick_world_view, TickWorldView* current_tick_world_view, float heartbeat_fraction);
    
    void clear_effects() // what's left of [confusingly named] `reset_screen`; called in revive_player and enter_gameworld
    {
        effect = NONE;
        reset_fov();
    }
    
private:
    
    // called by the 2 initialize methods above
    void initialize(const SDL_Point& virtual_screen_size, float fov, bool is_m1_exploration_view);
};


// the current player's world view
extern camera_settings_t main_camera_settings;


//-----------------------------------------------------------------------------




// Player settings; TODO: move these to player.h/.cpp; they should probably be on the Player struct (so coop+PvP film recording picks them up but we'll need to overhaul actions for that)

bool crosshairs_is_visible(); // only checks preferences plus netgame options so isn't player-specific
// technically this is set_crosshairs_wants_to_be_visible, since crosshairs can be suppressed in netgame config
bool set_crosshairs_is_visible(bool is_visible);

bool automap_is_visible(); // this tracks player so in film replay we should see the map when original user saw it
bool automap_is_translucent(); // only checks prefs and netgame options so isn't player-specific
bool decrease_automap_size(); // these 2 only check prefs so aren't player-specific
bool increase_automap_size();

#define opaque_automap_is_visible()  (automap_is_visible() && !automap_is_translucent())

bool computer_terminal_is_visible(); // this tracks player, so again in film replay we should see it when user does

bool hud_is_visible(); // this only tracks user's size preference (is it off?) and whether LuaHUD plugin's running, so we don't see exactly what original user does


// TODO: merge these functions and make them get_/set_fov_mode methods on Player, with an enum for normal/zoom/wide, and current and target FOVs should be stored and managed on the Player struct; when updating camera, get the current fov to use from that

// TODO: while not an original M2 feature, zoom could add gameplay value as a second trigger on Magnum/SMG/SPNKR/rifle or other weapon that has a targeting reticule for long-distance sniping (with corresponding drastic reduction in player's movement speed); if we get multiple cameras working, we might even put the enlarged view in the center of the standard view
void activate_zoom_vision();
void deactivate_zoom_vision();
bool zoom_is_active(); // exposed to Lua and F-key toggle

// extravision is original M2 powerup; renamed functions for consistency with zoom
void activate_wide_vision();
void deactivate_wide_vision();


// camera effects (these apply full-screen; not sure how they'll interact with multiplayer film replays)
void start_teleport_in_effect();
void start_teleport_out_effect();



// get the current (standard/scenario-defined/prefs-defined) FOV target values

float get_normal_FOV(); // used in graphics_preferences' FOV slider to set its default position, otherwise only used in camera.cpp
float get_extravision_FOV(); // these 2 are only used in camera.cpp; could be made static
float get_zoom_FOV();



// scenario MML can disable these standard effects // TODO: move these to Scenario as they're MML customizations

// Indicates whether to do fold-in/fold-out effect when one is teleporting
bool teleporting_uses_fold_effect();

// Indicates whether to do the "static" effect when one is teleporting
bool teleporting_uses_static_effect();

// Indicates whether to skip all teleport effects teleporting into a level
bool entering_level_uses_teleport_effect();

// Indicates whether to skip all teleport effects teleporting out of a level
bool exiting_level_uses_teleport_effect();



// Landscape stuff

struct LandscapeOptions
{
	// 2^(HorizExp) is the number of texture repeats when going in a circle;
	// it is a horizontal scaling factor
	short HorizExp;
	// 2^(VertExp) is a vertical scaling factor, which creates an amount of scaling
	// equal to the corresponding horizontal scaling factor.
	short VertExp;
	// Aspect-ratio exponent to use in OpenGL rendering;
	// (height) = 2^(-OGL_AspRatExp)*(width).
	// Necessary because OpenGL prefers powers of 2, and Bungie's landscapes have heights
	// that are not powers of 2.
	short OGL_AspRatExp;
	// Whether the texture repeats in the vertical direction (true: like Marathon 1)
	// or gets clamped in the vertical direction (false: like Marathon 2/oo)
	bool VertRepeat;
	// This is the azimuth or yaw (full circle = 512);
	// the texture is shifted leftward, relative to view direction, by this amount.
	angle Azimuth;

	bool SphereMap; // currently ignores all scaling and repeat settings
	
	// Constructor: sets everything to defaults appropriate for standard textures
	// Same scale for horizontal and vertical, 2^1 = 2 repeats,
	// OpenGL hight is half width, and the azimuth is zero
	LandscapeOptions(): HorizExp(1), VertExp(1), OGL_AspRatExp(0), VertRepeat(false), Azimuth(0), SphereMap{false} {}
};

LandscapeOptions *View_GetLandscapeOptions(shape_descriptor Desc);


class InfoTree;
void parse_mml_view(const InfoTree& root);
void reset_mml_view();
void parse_mml_landscapes(const InfoTree& root);
void reset_mml_landscapes();

#endif
