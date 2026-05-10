/*
 camera.cpp
 
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

#include "camera.hpp"

#include "ChaseCam.h"

#include "world.h"
#include "SoundManager.h"
#include "shell.h"
#include "Screen.hpp"
#include "InfoTree.h"
#include "preferences.hpp"
#include "interpolated_world.h" // TickWorldView

#include "automap_data.hpp" // DEFAULT_OVERHEAD_MAP_SCALE

#include "lua_script.h" // UseLuaCameras (which should be split and the bulk moved here)

#include "network.h" // NetAllowTunnelVision

#include "render.h" // _render_effect_fold_in, _render_effect_fold_out

#include "computer_interface.h" // dirty_terminal_view

#include "lua_hud_script.h" // LuaHUDRunning

#include "OGL_Render.h" // modern_renderer_is_active


// used in update_effect below
#define EXPLOSION_EFFECT_RANGE (WORLD_ONE / 12)


// TODO: these are MML customizations and user gameplay state; move them to static vars in camera.cpp


// dumped here from `view_settings_definition` struct; these are really MML scenario customizations; some or all of these will likely move again
static bool player_can_use_automap; // automap_is_available?
static bool use_teleport_fold_effect; // do the view folding effect (stretch horizontally, squeeze vertically) when teleporting
static bool use_teleport_static_effect; // also do the static effect / folding effect on viewed teleported objects
static bool use_teleport_effect_entering_level; // do all effects (and sounds) teleporting into the level
static bool use_teleport_effect_exiting_level; // do all effects (and sounds) teleporting out of the level


// This frame value means that a landscape option will be applied to any frame in a collection:
const int AnyFrame = -1;


// Field-of-view stuff with defaults:
struct FOV_settings_definition
{
    float Normal;
    float ExtraVision;
    float TunnelVision;
    float ChangeRate;    // this is 50 degrees/s
};


static const FOV_settings_definition default_FOV_settings = {
    80,
    130,
    30,
    1.66666667F,    // this is 50 degrees/s
};

static FOV_settings_definition FOV_settings;



//-----------------------------------------------------------------------------
// moved here from screen_shared.cpp


// the camera (in practice, the current player's view) to render to 3D gameworld view

camera_settings_t main_camera_settings;


// moved here from render.cpp

/* just in case anyone was wondering, standard_screen_width will usually be the same as // EES: huh? it's 2*screen.width in the original code!
    screen_width.  the renderer assumes that the given field_of_view matches the standard
    width provided (so if the actual width provided is larger, you'll be able to see more;
    if it's smaller you'll be able to see less).  this allows the destination bitmap to not
    only grow and shrink while maintaining a constant aspect ratio, but to also change in
    geometry without effecting the image being projected onto it.  if you don't understand
    this, pass standard_width==width */

// FOV is in degrees (originally 74 for 4:3 screen)
void camera_settings_t::initialize(const SDL_Point& virtual_screen_size, float fov, bool is_m1_exploration_view)
{
  //  assert_fail(current_field_of_view > 0, "");
    current_field_of_view = target_field_of_view = fov;
    
    screen_width  = virtual_screen_size.x;
    screen_height = virtual_screen_size.y;
    standard_screen_width = is_m1_exploration_view ? screen_width : screen_height * 2;
        
     // half_cone needs to be extended for non oblique perspective projection (gluPerspective).
    // (this is required because the viewing angle is different for about the same field of view)
    double half_cone_d = (!is_m1_exploration_view && modern_renderer_is_active())
                          ? degrees_to_radians(current_field_of_view * 1.3) / 2 : degrees_to_radians(current_field_of_view) / 2;
    
    double adjusted_half_cone = (is_m1_exploration_view || graphics_preferences.horizontal_fov_is_constant)
                                 ? half_cone_d : atan(screen_width * tan(half_cone) / standard_screen_width);
    
    half_screen_width  = screen_width  / 2;
    half_screen_height = screen_height / 2;
    
    // If there’s a round-off error in half_cone, we want to make the cone too big (so when we clip lines
    // ‘to the edge of the screen’ they’re actually off the screen, thus +1.0).
    half_cone = (angle)(adjusted_half_cone * double(NUMBER_OF_ANGLES) / TWO_PI + 1.0);
    
    // Find the adjusted yaw for the landscapes; this is the effective yaw value for the left edge.
    landscape_yaw = yaw - half_cone;

    // calculate world_to_screen
    // (we could calculate this with standard_screen_width/2 and the old half_cone and get the same result)
    double world_to_screen = half_screen_width / tan(adjusted_half_cone);
    world_to_screen_x = real_world_to_screen_x = (short)(world_to_screen + 0.5);
    world_to_screen_y = real_world_to_screen_y = (short)(world_to_screen + 0.5);
    
    // calculate the vertical cone angle; again, overflow instead of underflow when rounding
    half_vertical_cone = (angle)(NUMBER_OF_ANGLES * atan(double(half_screen_height) / world_to_screen) / TWO_PI + 1.0);
    
    // TODO: anything else needing set?
    clear_effect();
}


// TODO: FIX: FOV values need to be independent of current vscreen aspect so that switching from narrower to wider aspects and back does the right thing (I think the current rule is that 4:3 and 16:9 use the same FOV unless the v-not-h option is checked)

void camera_settings_t::initialize_for_game_view(const SDL_Point& virtual_screen_size)
{
    initialize(virtual_screen_size, get_normal_FOV(), false);
}


void camera_settings_t::initialize_for_m1_exploration()
{
    // Classic M1 exploration missions require the player *sees* the exploration polys (this uses separate camera_settings_t instance so the behavior is stable and it isn't affected by e.g. custom FOV). (for Modern M1, the maps will be overhauled to follow M2 conventions where, iirc, player must enter poly)
    // For cross-player stability, we don't leave any view settings up to the preferences or MML.
    initialize({640, 320}, 80, true); // TODO: what is correct order in which to call this? we may be missing some old code
}


void camera_settings_t::update()
{
    main_camera_settings.yaw = current_player->facing;
    main_camera_settings.pitch = current_player->elevation;
    main_camera_settings.maximum_depth_intensity = current_player->weapon_intensity;

    main_camera_settings.origin = current_player->camera_location;
    if (graphics_preferences.bobbing_type != BobbingType::camera_and_weapon)
    {
        main_camera_settings.origin.z -= current_player->step_height;
    }
    main_camera_settings.origin_polygon_index = current_player->camera_polygon_index;

    // Script-based camera control
    auto lua_controlled = UseLuaCameras(); // EES: smelly; it's doing a lot of work on every frame it really shouldn't need to, because the architecture is so bodged (it would help to know what Lua control is currently being used for; maybe a standard Camera class and a LuaCamera subclass are way forward, or maybe something else)

    main_camera_settings.virtual_yaw = main_camera_settings.yaw * FIXED_ONE;
    main_camera_settings.virtual_pitch = main_camera_settings.pitch * FIXED_ONE;

    if (!lua_controlled)
    {
        main_camera_settings.weapons_in_hand_is_visible = !ChaseCam_IsActive(); // EES: clunky; it should only need set when chasecam is turned on/off but leaving it for now

        if (current_player_index == local_player_index)
        {
            main_camera_settings.virtual_yaw += virtual_aim_delta().yaw;
            main_camera_settings.virtual_pitch += virtual_aim_delta().pitch;
        }
    }
}



// in interpolated_world.cpp
int16_t lerp(int16_t a, int16_t b, float t);
angle lerp_angle(angle a, angle b, float t);
fixed_angle lerp_fixed_angle(fixed_angle a, fixed_angle b, float t);


void camera_settings_t::interpolate_view(TickWorldView* prev, TickWorldView* next, float heartbeat_fraction)
{
    yaw   = lerp_angle(prev->yaw,   next->yaw,   heartbeat_fraction);
    pitch = lerp_angle(prev->pitch, next->pitch, heartbeat_fraction);
    
    virtual_yaw   = lerp_fixed_angle(prev->virtual_yaw,   next->virtual_yaw,   heartbeat_fraction);
    virtual_pitch = lerp_fixed_angle(prev->virtual_pitch, next->virtual_pitch, heartbeat_fraction);
    
    maximum_depth_intensity = lerp(prev->maximum_depth_intensity, next->maximum_depth_intensity, heartbeat_fraction);
    
    origin.x = lerp(prev->origin.x, next->origin.x, heartbeat_fraction);
    origin.y = lerp(prev->origin.y, next->origin.y, heartbeat_fraction);
    origin.z = lerp(prev->origin.z, next->origin.z, heartbeat_fraction);
    
    if (prev->origin_polygon_index != next->origin_polygon_index)
    {
        auto polygon_index = find_new_object_polygon(reinterpret_cast<world_point2d*>(&prev->origin),
                                                     reinterpret_cast<world_point2d*>(&origin), prev->origin_polygon_index);
        if (polygon_index == NONE)
        {
            origin = next->origin;
        }
        else
        {
            origin_polygon_index = polygon_index;
        }
    }
}


// Move field-of-view value closer to some target value:
bool camera_settings_t::update_fov()
{
    if (FOV_settings.ChangeRate < 0) { FOV_settings.ChangeRate *= -1; }
    
    if (current_field_of_view > target_field_of_view)
    {
        current_field_of_view -= FOV_settings.ChangeRate;
        current_field_of_view = MAX(current_field_of_view, target_field_of_view);
        return true;
    }
    else if (current_field_of_view < target_field_of_view)
    {
        current_field_of_view += FOV_settings.ChangeRate;
        current_field_of_view = MIN(current_field_of_view, target_field_of_view);
        return true;
    }
    return false;
}



static void shake_view_origin(camera_settings_t* view, world_distance delta)
{
    world_point3d new_origin= view->origin;
    short half_delta= delta>>1;
    
    new_origin.x+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE((view->effect_tick_count&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);
    new_origin.y+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE(((view->effect_tick_count+5*TICKS_PER_SECOND)&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);
    new_origin.z+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE(((view->effect_tick_count+7*TICKS_PER_SECOND)&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);

    /* only use the new origin if we didn’t cross a polygon boundary */
    if (find_line_crossed_leaving_polygon(view->origin_polygon_index, (world_point2d *) &view->origin,
        (world_point2d *) &new_origin)==NONE)
    {
        view->origin= new_origin;
    }
}


void camera_settings_t::update_effect()
{
    effect_phase = effect_phase == NONE ? 0 : (effect_phase + effect_ticks_elapsed);

    short period = (effect == _render_effect_explosion) ? TICKS_PER_SECOND : TICKS_PER_SECOND / 2;
    
    if (effect_phase > period)
    {
        effect = NONE;
    }
    else
    {
        float interpolated_phase = MAX(0, effect_phase - 1 + heartbeat_fraction);
        switch (effect)
        {
            case _render_effect_explosion:
                shake_view_origin(this, EXPLOSION_EFFECT_RANGE - ((EXPLOSION_EFFECT_RANGE / 2) * interpolated_phase) / period);
                break;
            
            case _render_effect_fold_in:
                interpolated_phase = period - interpolated_phase;
                // fall-thru
            
            case _render_effect_fold_out:
                // calculate world_to_screen based on phase
                world_to_screen_x = real_world_to_screen_x + (4 * real_world_to_screen_x * interpolated_phase) / period;
                world_to_screen_y = real_world_to_screen_y - (real_world_to_screen_y * interpolated_phase) / (period + period / 4);
                break;
            
            default:
                throw_bug_report_f("Invalid effect: %d", effect);

        }
    }
}







/*
 overhead_map_scale = DEFAULT_OVERHEAD_MAP_SCALE;
 overhead_map_active = false;
 terminal_mode_active = false;
 */


bool crosshairs_is_visible()
{
    return graphics_preferences.crosshairs_is_visible && NetAllowCrosshair();
}

// this doesn't guarantee crosshairs *are* visible, as they may be disabled in netgame config
bool set_crosshairs_is_visible(bool is_visible)
{
    return graphics_preferences.crosshairs_is_visible = is_visible;
}


bool hud_is_visible()
{
    return graphics_preferences.hud_size > 0 && LuaHUDRunning();
}


bool computer_terminal_is_visible()
{
    return player_in_terminal_mode(current_player_index);
    
   // if (is_visible) dirty_terminal_view(current_player_index);
}


// TODO: these need to notify the automap of changes

bool automap_is_visible()
{
    return player_can_use_automap &&  PLAYER_HAS_MAP_OPEN(current_player);
}


// Determine if the translucent map is in use (may be disallowed for network games)
bool automap_is_translucent()
{
    return (modern_renderer_is_active() && graphics_preferences.translucent_map && NetAllowOverlayMap());
}



bool decrease_automap_size()
{
    bool Success = false;
    if (graphics_preferences.automap_size > OVERHEAD_MAP_MINIMUM_SCALE)
    {
        graphics_preferences.automap_size--;
        Success = true;
    }
    return Success;
}


bool increase_automap_size()
{
    bool Success = false;
    if (graphics_preferences.automap_size < OVERHEAD_MAP_MAXIMUM_SCALE)
    {
        graphics_preferences.automap_size++;
        Success = true;
    }
    return Success;
}





void activate_wide_vision()
{
    main_camera_settings.target_field_of_view = get_extravision_FOV();
}

void deactivate_wide_vision()
{
    main_camera_settings.target_field_of_view = get_normal_FOV();
}


// TODO: this should be on Player (alongside extravision, nightvision, etc flags) for reasons that really should be obvious (for exterior cameras that have zoom lenses, I think field_of_view and horizontal_/vertical_scale? ought to cover it)
bool tunnel_vision_active;


bool zoom_is_active()
{
    return tunnel_vision_active;
}


void activate_zoom_vision()
{
    tunnel_vision_active = true;
    if (NetAllowTunnelVision()) { main_camera_settings.target_field_of_view = get_zoom_FOV(); }
}


void deactivate_zoom_vision()
{
    tunnel_vision_active = false;
    main_camera_settings.target_field_of_view = ((current_player->extravision_duration) ? get_extravision_FOV() : get_normal_FOV());
}




// TODO: integrate properly
// LP change: resets field of view to whatever the player had had when reviving
void camera_settings_t::reset_fov()
{
    tunnel_vision_active = false;

    if (current_player->extravision_duration)
    {
        main_camera_settings.current_field_of_view = get_extravision_FOV();
        main_camera_settings.target_field_of_view  = get_extravision_FOV();
    }
    else
    {
        main_camera_settings.current_field_of_view = get_normal_FOV();
        main_camera_settings.target_field_of_view  = get_normal_FOV();
    }
}




// Scenario customizations


void start_teleport_in_effect()
{
    if (teleporting_uses_fold_effect()) { main_camera_settings.start_effect(_render_effect_fold_in); }
}


void start_teleport_out_effect()
{
    if (teleporting_uses_fold_effect()) { main_camera_settings.start_effect(_render_effect_fold_out); }
}



bool teleporting_uses_fold_effect()
{
    return use_teleport_fold_effect;
}

bool teleporting_uses_static_effect()
{
    return use_teleport_static_effect;
}

bool entering_level_uses_teleport_effect()
{
    return use_teleport_effect_entering_level;
}

bool exiting_level_uses_teleport_effect()
{
    return use_teleport_effect_exiting_level;
}






float get_normal_FOV()
{
	return graphics_preferences.fov == 0 ? FOV_settings.Normal : graphics_preferences.fov;
}

float get_extravision_FOV()
{
	// controversial; next someone will complain this isn't a separate slider
	if (graphics_preferences.fov != 0)
	{
		return std::min(130, graphics_preferences.fov + 50);
	}
	else
	{
		return FOV_settings.ExtraVision;
	}
}

float get_zoom_FOV()
{
	if (graphics_preferences.fov != 0)
	{
		return std::max(30, graphics_preferences.fov - 50);
	}
	else
	{
		return FOV_settings.TunnelVision;
	}
}


// Landscape stuff: this is for being able to return a pointer to the default one
static LandscapeOptions DefaultLandscape;


// Store landscape stuff as a vector member
struct LandscapeOptionsEntry
{
	// Which frame to apply to (default: 0, since there is usually only one)
	short Frame;
	
	// Make a member for more convenient access
	LandscapeOptions OptionsData;
	
	LandscapeOptionsEntry(): Frame(0) {}
};

// Separate landscape-texture sequence lists for each collection ID, to speed up searching.
static std::vector<LandscapeOptionsEntry> landscape_options[NUMBER_OF_COLLECTIONS];

// Deletes a collection's landscape-texture sequences
static void LODelete(int c)
{
	landscape_options[c].clear();
}

// Deletes all of them
static void LODeleteAll()
{
	for (int c=0; c<NUMBER_OF_COLLECTIONS; c++) LODelete(c);
}


LandscapeOptions* View_GetLandscapeOptions(shape_descriptor Desc)
{
	// Pull out frame and collection ID's:
	short Frame = GET_DESCRIPTOR_SHAPE(Desc);
	short CollCT = GET_DESCRIPTOR_COLLECTION(Desc);
	short Collection = GET_COLLECTION_INDEX(CollCT);
	
	for (auto& option : landscape_options[Collection])
	{
		if (option.Frame == Frame || option.Frame == AnyFrame)
		{
			return &(option.OptionsData);
		}
	}
	
	// Return the default if no matching entry was found
	return &DefaultLandscape;
}


//-----------------------------------------------------------------------------
// MML


void reset_mml_view()
{
    player_can_use_automap             = true;
    use_teleport_fold_effect           = true;
    use_teleport_static_effect         = true;
    use_teleport_effect_entering_level = true;
    use_teleport_effect_exiting_level  = true;
    
    FOV_settings = default_FOV_settings;
}



void parse_mml_view(const InfoTree& root)
{
	root.read_attr("map", player_can_use_automap);
	root.read_attr("fold_effect", use_teleport_fold_effect);
	root.read_attr("static_effect", use_teleport_static_effect);
	root.read_attr("interlevel_in_effects", use_teleport_effect_entering_level);
	root.read_attr("interlevel_out_effects", use_teleport_effect_exiting_level);
	
	for (const InfoTree &fov : root.children_named("fov"))
	{
		fov.read_attr_bounded<float>("normal", FOV_settings.Normal, 0, 180);
		fov.read_attr_bounded<float>("extra", FOV_settings.ExtraVision, 0, 180);
		fov.read_attr_bounded<float>("tunnel", FOV_settings.TunnelVision, 0, 180);
		fov.read_attr_bounded<float>("rate", FOV_settings.ChangeRate, 0, 180);
	}
}


void reset_mml_landscapes()
{
	LODeleteAll();
}


void parse_mml_landscapes(const InfoTree& root)
{
	for (const InfoTree::value_type &v : root)
	{
		const std::string& name = v.first;
		const InfoTree& child = v.second;
		if (name == "clear")
		{
			int16 coll;
			if (child.read_indexed("coll", coll, NUMBER_OF_COLLECTIONS))
				LODelete(coll);
			else
				LODeleteAll();
		}
		else if (name == "landscape")
		{
			int16 coll;
			if (!child.read_indexed("coll", coll, NUMBER_OF_COLLECTIONS))
				continue;
			
			int16 frame = AnyFrame;
			child.read_indexed("frame", frame, MAXIMUM_SHAPES_PER_COLLECTION);
			
			LandscapeOptions data = DefaultLandscape;
			child.read_attr("horiz_exp", data.HorizExp);
			child.read_attr("vert_exp", data.VertExp);
			child.read_attr("vert_repeat", data.VertRepeat);
			child.read_attr("ogl_asprat_exp", data.OGL_AspRatExp);
			child.read_angle("azimuth", data.Azimuth);

			int16 projection;
			if (child.read_attr("projection", projection))
			{
				if (projection == 1)
				{
					data.SphereMap = true;
				}
				else
				{
					data.SphereMap = false;
				}
			}
			
			// Check to see if a frame is already accounted for
			bool found = false;
            std::vector<LandscapeOptionsEntry>& LOL = landscape_options[coll];
			for (std::vector<LandscapeOptionsEntry>::iterator LOIter = LOL.begin(); LOIter < LOL.end(); LOIter++)
			{
				if (LOIter->Frame == frame)
				{
					// Replace the data
					LOIter->OptionsData = data;
					found = true;
					break;
				}
			}
			
			// If not, then add a new frame entry
			if (!found)
			{
				LandscapeOptionsEntry DataEntry;
				DataEntry.Frame = frame;
				DataEntry.OptionsData = data;
				LOL.push_back(DataEntry);
			}
		}
	}
}
