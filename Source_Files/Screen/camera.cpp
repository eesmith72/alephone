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

#include "camera.h"

#include "world.h"
#include "SoundManager.h"
#include "shell.h"
#include "screen.hpp"
#include "InfoTree.h"
#include "preferences.h"
#include "interpolated_world.h" // TickWorldView

#include "overhead_map.h" // DEFAULT_OVERHEAD_MAP_SCALE

#include "lua_script.h" // UseLuaCameras (which should be split and the bulk moved here)

#include "network.h" // NetAllowTunnelVision

#include "render.h" // _render_effect_fold_in, _render_effect_fold_out

#include "computer_interface.h" // dirty_terminal_view

#include "lua_hud_script.h" // LuaHUDRunning

#include "OGL_Render.h" // modern_renderer_is_active


// TODO: these are MML customizations and user gameplay state; move them to static vars in camera.cpp


// dumped here from `view_settings_definition` struct; these are really MML scenario customizations; some or all of these will likely move again
static bool automap_is_enabled; // automap_is_available?
static bool DoFoldEffect; // do the view folding effect (stretch horizontally, squeeze vertically) when teleporting
static bool DoStaticEffect; // also do the static effect / folding effect on viewed teleported objects
static bool DoInterlevelTeleportInEffects; // do all effects (and sounds) teleporting into the level
static bool DoInterlevelTeleportOutEffects; // do all effects (and sounds) teleporting out of the level


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



static const font_key_t default_on_screen_font_key = {kFontIDMono, styleNormal, 12};

static font_key_t on_screen_font_key = default_on_screen_font_key;

static const font_t* LoadedOnScreenFont = nullptr;



//-----------------------------------------------------------------------------
// moved here from screen_shared.cpp


// TODO: pretty sure this can/should be static allocated (there can be additional instances for rendering custom views, but this one is the global standard settings)

camera_settings_t main_camera_settings;

// TODO: extracted from Screen::Initialize in screen.cpp
void initialize_camera_settings()
{
    // TODO: this doesn't initialize the precalculated fields; another good argument for splitting the struct
    
    
    reset_screen();
    
    // TODO: what about these?
    automap_is_enabled = true;
    DoFoldEffect = true;
    DoStaticEffect = true;
    DoInterlevelTeleportInEffects = true;
    DoInterlevelTeleportOutEffects = true;
}


//  see also reset_mml_view at bottom
void reset_screen() // called in activate_gameworld_renderer; it is badly named
{
    graphics_preferences->automap_size     = DEFAULT_OVERHEAD_MAP_SCALE; // hrmm; this doesn't belong here
    main_camera_settings.horizontal_scale     = 1;
    main_camera_settings.vertical_scale       = 1;
    main_camera_settings.effect = NONE; // Adding this view-effect resetting here since initialize_world_view() no longer resets it

    reset_fov();
}



// moved here from render.cpp

/* just in case anyone was wondering, standard_screen_width will usually be the same as
    screen_width.  the renderer assumes that the given field_of_view matches the standard
    width provided (so if the actual width provided is larger, you'll be able to see more;
    if it's smaller you'll be able to see less).  this allows the destination bitmap to not
    only grow and shrink while maintaining a constant aspect ratio, but to also change in
    geometry without effecting the image being projected onto it.  if you don't understand
    this, pass standard_width==width */

void camera_settings_t::initialize_view_data(bool ignore_preferences)
{
    static double two_pi = 8.0 * atan(1.0);
     // half_cone needs to be extended for non oblique perspective projection (gluPerspective).
    // (this is required because the viewing angle is different for about the same field of view)
    double half_cone = (!ignore_preferences && modern_renderer_is_active())
                       ? (field_of_view * 1.3) * (two_pi / 360.0) / 2 : field_of_view * (two_pi / 360.0) / 2;
    
        
    double adjusted_half_cone = (ignore_preferences || graphics_preferences->horizontal_fov_is_constant)
                                ? half_cone : atan(screen_width*tan(half_cone)/standard_screen_width);
    
    half_screen_width  = screen_width  / 2;
    half_screen_height = screen_height / 2;
    
    // if there’s a round-off error in half_cone, we want to make the cone too big (so when we clip
    // lines ‘to the edge of the screen’ they’re actually off the screen, thus +1.0)
    half_cone = (angle)(adjusted_half_cone * ((double)NUMBER_OF_ANGLES) / two_pi + 1.0);
    
    // LP change: find the adjusted yaw for the landscapes; this is the effective yaw value for the left edge.
    // A landscape rotation can also be added if desired.
    landscape_yaw = yaw - half_cone;

    // calculate world_to_screen
    // (we could calculate this with standard_screen_width/2 and the old half_cone and get the same result)
    double world_to_screen = half_screen_width / tan(adjusted_half_cone);
    world_to_screen_x = real_world_to_screen_x = (short)((world_to_screen / horizontal_scale) + 0.5);
    world_to_screen_y = real_world_to_screen_y = (short)((world_to_screen / vertical_scale) + 0.5);
    
    // calculate the vertical cone angle; again, overflow instead of underflow when rounding
    half_vertical_cone = (angle)(NUMBER_OF_ANGLES * atan(((double)half_screen_height * vertical_scale) / world_to_screen) / two_pi + 1.0);
}



void camera_settings_t::initialize(int32_t screen_w, int32_t screen_h)
{
    // TODO: which members need to be assigned when? we may be missing some old code
    screen_width  = screen_w;
    screen_height = screen_h;
    
    initialize_view_data(false);
}

void camera_settings_t::initialize_for_m1_exploration()
{
    // M1 exploration missions require the player *sees* the exploration polys (this uses separate camera_settings_t instance so the behavior is stable and it isn't affected by e.g. custom FOV). // TODO: Modern automap really needs a 'Mission: Exploration objectives 0/5' banner so user knows when they've found everything, possibly also highlighting the found polys on automap.
    tunnel_vision_active = false;
    effect               = NONE;
    horizontal_scale     = 1;
    vertical_scale       = 1;
    
    // For cross-player stability, we don't leave any view settings up to the preferences or MML.
    field_of_view = target_field_of_view  = 80;
    screen_width  = standard_screen_width = 640;
    screen_height = 320;
    
    initialize_view_data(true); // TODO: what is correct order in which to call this? we may be missing some old code
}


void camera_settings_t::update()
{
    main_camera_settings.yaw = current_player->facing;
    main_camera_settings.pitch = current_player->elevation;
    main_camera_settings.maximum_depth_intensity = current_player->weapon_intensity;

    main_camera_settings.origin = current_player->camera_location;
    if (graphics_preferences->bobbing_type != BobbingType::camera_and_weapon)
    {
        main_camera_settings.origin.z -= current_player->step_height;
    }
    main_camera_settings.origin_polygon_index = current_player->camera_polygon_index;

    // Script-based camera control
    auto lua_controlled = UseLuaCameras();

    main_camera_settings.virtual_yaw = main_camera_settings.yaw * FIXED_ONE;
    main_camera_settings.virtual_pitch = main_camera_settings.pitch * FIXED_ONE;

    if (!lua_controlled)
    {
        main_camera_settings.weapons_in_hand_is_visible = !ChaseCam_GetPosition(main_camera_settings.origin,
                                                                              main_camera_settings.origin_polygon_index,
                                                                              main_camera_settings.yaw, main_camera_settings.pitch);

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
    auto view = &main_camera_settings;
    
    view->yaw   = lerp_angle(prev->yaw,   next->yaw,   heartbeat_fraction);
    view->pitch = lerp_angle(prev->pitch, next->pitch, heartbeat_fraction);
    
    view->virtual_yaw   = lerp_fixed_angle(prev->virtual_yaw,   next->virtual_yaw,   heartbeat_fraction);
    view->virtual_pitch = lerp_fixed_angle(prev->virtual_pitch, next->virtual_pitch, heartbeat_fraction);
    
    view->maximum_depth_intensity = lerp(prev->maximum_depth_intensity, next->maximum_depth_intensity, heartbeat_fraction);
    
    view->origin.x = lerp(prev->origin.x, next->origin.x, heartbeat_fraction);
    view->origin.y = lerp(prev->origin.y, next->origin.y, heartbeat_fraction);
    view->origin.z = lerp(prev->origin.z, next->origin.z, heartbeat_fraction);
    
    if (prev->origin_polygon_index != next->origin_polygon_index)
    {
        auto polygon_index = find_new_object_polygon(reinterpret_cast<world_point2d*>(&prev->origin),
                                                     reinterpret_cast<world_point2d*>(&view->origin), prev->origin_polygon_index);
        if (polygon_index == NONE)
        {
            view->origin = next->origin;
        }
        else
        {
            view->origin_polygon_index = polygon_index;
        }
    }
}


// Move field-of-view value closer to some target value:
bool camera_settings_t::update_fov()
{
    if (FOV_settings.ChangeRate < 0) { FOV_settings.ChangeRate *= -1; }
    
    if (field_of_view > target_field_of_view)
    {
        field_of_view -= FOV_settings.ChangeRate;
        field_of_view = MAX(field_of_view, target_field_of_view);
        return true;
    }
    else if (field_of_view < target_field_of_view)
    {
        field_of_view += FOV_settings.ChangeRate;
        field_of_view = MIN(field_of_view, target_field_of_view);
        return true;
    }
    return false;
}







bool crosshairs_is_visible()
{
    return graphics_preferences->crosshairs_is_visible && NetAllowCrosshair();
}

// this doesn't guarantee crosshairs *are* visible, as they may be disabled in netgame config
bool set_crosshairs_is_visible(bool is_visible)
{
    return graphics_preferences->crosshairs_is_visible = is_visible;
}


bool hud_is_visible()
{
    return graphics_preferences->hud_size > 0 && LuaHUDRunning();
}


bool computer_terminal_is_visible()
{
    return player_in_terminal_mode(current_player_index);
    
   // if (is_visible) dirty_terminal_view(current_player_index);
}


// TODO: these need to notify the automap of changes

bool automap_is_visible()
{
    return automap_is_enabled &&  PLAYER_HAS_MAP_OPEN(current_player);
}


// Determine if the translucent map is in use (may be disallowed for network games)
bool automap_is_translucent()
{
    return (modern_renderer_is_active() && graphics_preferences->translucent_map && NetAllowOverlayMap());
}



bool decrease_automap_size()
{
    bool Success = false;
    if (graphics_preferences->automap_size > OVERHEAD_MAP_MINIMUM_SCALE)
    {
        graphics_preferences->automap_size--;
        Success = true;
    }
    return Success;
}


bool increase_automap_size()
{
    bool Success = false;
    if (graphics_preferences->automap_size < OVERHEAD_MAP_MAXIMUM_SCALE)
    {
        graphics_preferences->automap_size++;
        Success = true;
    }
    return Success;
}










void start_teleport_in_effect()
{
    if (teleporting_uses_fold_effect()) { start_render_effect(&main_camera_settings, _render_effect_fold_in); }
}


void start_teleport_out_effect()
{
    if (teleporting_uses_fold_effect()) { start_render_effect(&main_camera_settings, _render_effect_fold_out); }
}


void start_extravision_activate_effect()
{
    main_camera_settings.target_field_of_view = get_extravision_FOV();
}

void start_extravision_deactivate_effect()
{
    main_camera_settings.target_field_of_view = get_normal_FOV();
}





bool get_zoom_is_enabled()
{
    return main_camera_settings.tunnel_vision_active;
}


bool set_zoom_is_enabled(bool is_on)
{
    main_camera_settings.tunnel_vision_active = is_on;
    if (is_on)
    {
        if (NetAllowTunnelVision()) { main_camera_settings.target_field_of_view = get_zoom_FOV(); }
    }
    else
    {
        main_camera_settings.target_field_of_view = ((current_player->extravision_duration) ? get_extravision_FOV() : get_normal_FOV());
    }
    return main_camera_settings.tunnel_vision_active;
}


bool teleporting_uses_fold_effect()
{
    return DoFoldEffect;
}

bool teleporting_uses_static_effect()
{
    return DoStaticEffect;
}

bool entering_level_uses_teleport_effect()
{
    return DoInterlevelTeleportInEffects;
}

bool exiting_level_uses_teleport_effect()
{
    return DoInterlevelTeleportOutEffects;
}






float get_normal_FOV()
{
	if (graphics_preferences->fov != 0)
	{
		return graphics_preferences->fov;
	}
	else
	{
		return FOV_settings.Normal;
	}
}

float get_extravision_FOV()
{
	// controversial; next someone will complain this isn't a separate slider
	if (graphics_preferences->fov != 0)
	{
		return std::min(130, graphics_preferences->fov + 50);
	}
	else
	{
		return FOV_settings.ExtraVision;
	}
}

float get_zoom_FOV()
{
	if (graphics_preferences->fov != 0)
	{
		return std::max(30, graphics_preferences->fov - 50);
	}
	else
	{
		return FOV_settings.TunnelVision;
	}
}


// LP change: resets field of view to whatever the player had had when reviving
void reset_fov()
{
    main_camera_settings.tunnel_vision_active = false;

    if (current_player->extravision_duration)
    {
        main_camera_settings.field_of_view        = get_extravision_FOV();
        main_camera_settings.target_field_of_view = get_extravision_FOV();
    }
    else
    {
        main_camera_settings.field_of_view        = get_normal_FOV();
        main_camera_settings.target_field_of_view = get_normal_FOV();
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
static std::vector<LandscapeOptionsEntry> LOList[NUMBER_OF_COLLECTIONS];

// Deletes a collection's landscape-texture sequences
static void LODelete(int c)
{
	LOList[c].clear();
}

// Deletes all of them
static void LODeleteAll()
{
	for (int c=0; c<NUMBER_OF_COLLECTIONS; c++) LODelete(c);
}


LandscapeOptions *View_GetLandscapeOptions(shape_descriptor Desc)
{
	// Pull out frame and collection ID's:
	short Frame = GET_DESCRIPTOR_SHAPE(Desc);
	short CollCT = GET_DESCRIPTOR_COLLECTION(Desc);
	short Collection = GET_COLLECTION(CollCT);
	
    std::vector<LandscapeOptionsEntry>& LOL = LOList[Collection];
	for (std::vector<LandscapeOptionsEntry>::iterator LOIter = LOL.begin(); LOIter < LOL.end(); LOIter++)
	{
		if (LOIter->Frame == Frame || LOIter->Frame == AnyFrame)
		{
			// Get a pointer from the iterator in order to return it
			return &(LOIter->OptionsData);
		}
	}
	
	// Return the default if no matching entry was found
	return &DefaultLandscape;
}


//-----------------------------------------------------------------------------
// MML


void reset_mml_view()
{
    automap_is_enabled = true;
    DoFoldEffect = true;
    DoStaticEffect = true;
    DoInterlevelTeleportInEffects = true;
    DoInterlevelTeleportOutEffects = true;
    FOV_settings = default_FOV_settings;
    
    // TODO: smells; why are these here? (aside from being defined in the MML)
    on_screen_font_key = default_on_screen_font_key;
    LoadedOnScreenFont = nullptr;
}


void parse_mml_view(const InfoTree& root)
{
	root.read_attr("map", automap_is_enabled);
	root.read_attr("fold_effect", DoFoldEffect);
	root.read_attr("static_effect", DoStaticEffect);
	root.read_attr("interlevel_in_effects", DoInterlevelTeleportInEffects);
	root.read_attr("interlevel_out_effects", DoInterlevelTeleportOutEffects);
	
	for (const InfoTree &fov : root.children_named("fov"))
	{
		fov.read_attr_bounded<float>("normal", FOV_settings.Normal, 0, 180);
		fov.read_attr_bounded<float>("extra", FOV_settings.ExtraVision, 0, 180);
		fov.read_attr_bounded<float>("tunnel", FOV_settings.TunnelVision, 0, 180);
		fov.read_attr_bounded<float>("rate", FOV_settings.ChangeRate, 0, 180);
	}
    
    for (const InfoTree &font : root.children_named("font"))
    {
        font.read_font(on_screen_font_key);
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
            std::vector<LandscapeOptionsEntry>& LOL = LOList[coll];
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
