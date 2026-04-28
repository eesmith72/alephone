/*
 camera.h -- renamed `view_data` struct from render.h
 
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


//-----------------------------------------------------------------------------
// a mixture of gameplay state, precalculated SW renderer params, user prefs, and whatever else got thrown in it back in 1995

// TODO: Camera class

struct TickWorldView; // from interpolated_world.h

struct camera_settings_t
{
    // LP change: specifying current and target field-of-view as floats
    // (To change the FOV, set a new target so the current FOV adjusts toward it.)
    float field_of_view; // current_fov
    float target_field_of_view;
    
    short standard_screen_width; // this is *not* the width of the projected image (see initialize_view_data() in RENDER.C
    short screen_width, screen_height; // dimensions of the projected image // TODO: what does this mean? if it's the worldview area, rename it and sort out the corresponding rect in screen.cpp
    short horizontal_scale, vertical_scale; // TODO: these are *always* 1 (do they have any potential uses, e.g. in external cams? if not, take them out)
    
    // precalcuated values
    short half_screen_width, half_screen_height;
    short world_to_screen_x, world_to_screen_y;
    short dtanpitch; // world_to_screen*tan(pitch)
    angle half_cone; // often ==field_of_view/2 (when screen_width==standard_screen_width)
    angle half_vertical_cone;
    
    short real_world_to_screen_x, real_world_to_screen_y;
    
    long_vector2d left_edge, right_edge; // view cone edges as world directions
    
    short ticks_elapsed;
    uint32 tick_count; // for effects and transfer modes // TODO: rename it then, so it isn't confused with state in dynamic/static world
    float heartbeat_fraction;
    
    // camera origin, presumably
    short origin_polygon_index;
    angle yaw, pitch, roll;
    fixed_angle virtual_yaw, virtual_pitch;
    world_point3d origin;
    _fixed maximum_depth_intensity; // in fixed units
   
    angle landscape_yaw; // LP addition: value of yaw used by landscapes; this is so that the center can stay stationary
    
    // EES: pretty sure these are camera attributes
    short shading_mode; // e.g. _shading_infravision
    short effect, effect_phase;
    bool under_media_boundary;
    short under_media_index;
    
        
    // LP: Indicates whether or not tunnel vision is active // TODO: this should be on Player (alongside extravision, nightvision, etc flags) for reasons that really should be obvious (for exterior cameras that have zoom lenses, I think field_of_view and horizontal_/vertical_scale? ought to cover it)
    bool tunnel_vision_active;
    
    
    // LP addition: this indicates whether to show the current weapon-in-hand; on in first-person, off in third-person/external camera; EES: think this is a camera attribute
    bool weapons_in_hand_is_visible;
    
    
    
    void initialize(int32_t screen_w, int32_t screen_h);
    
    void initialize_for_m1_exploration();
    
    void update();
    
    bool update_fov();
    
    void interpolate_view(TickWorldView* previous_tick_world_view, TickWorldView* current_tick_world_view, float heartbeat_fraction);
    
    
    // private; called by the 2 initialize methods above
    void initialize_view_data(bool ignore_preferences);
};


// the current player's world view
extern camera_settings_t main_camera_settings;


//-----------------------------------------------------------------------------


void reset_screen(); // LP's nonsense to consolidate


// Player settings; TODO: move these to graphics_preferences.cpp, once the current Preferences.h/.cpp is split up

bool crosshairs_is_visible();

// technically this is set_crosshairs_wants_to_be_visible, since crosshairs can be suppressed in netgame config
bool set_crosshairs_is_visible(bool is_visible);


// Returns whether or not the overhead map can possibly be active
bool automap_is_visible();
bool automap_is_translucent();
bool decrease_automap_size();
bool increase_automap_size();

bool computer_terminal_is_visible();

bool hud_is_visible();



// camera effects

void start_teleport_in_effect();
void start_teleport_out_effect();

void start_extravision_activate_effect();
void start_extravision_deactivate_effect();

float get_normal_FOV();
float get_extravision_FOV();
float get_zoom_FOV();

// reset field of view to whatever the player had had when reviving
void reset_fov();

// TODO: while not an original M2 feature, this could add gameplay value as a second trigger on Magnum/SMG/SPNKR/rifle or other weapon that has a targeting reticule for long-distance sniping (with corresponding drastic reduction in player's movement speed); if we get multiple cameras working, we might even put the enlarged view in the center of the standard view
bool set_zoom_is_enabled(bool is_on);
bool get_zoom_is_enabled();


// scenario MML can disable these standard effects

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
