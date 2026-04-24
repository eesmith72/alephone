/*
 screen_shared.h -- a LOT of header-defined functions that do... stuff; cleanup TBD

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

#ifndef __screen_shared_h__
#define __screen_shared_h__

#include "cseries.h"

#include "computer_interface.h"
#include "fades.h"
#include "network.h"
#include "OGL_Render.h"
#include "overhead_map.h"
#include "screen.h" // the irony of a file called 'screen_shared.h' importing 'screen.h' instead of vice-versa

#include "Console.h"
#include "screen_drawing.h"

#include "network_games.h"


enum class BobbingType
{
    none,
    camera_and_weapon,
    weapon_only
};


struct screen_mode_data
{
    bool acceleration;
    
    bool high_resolution;
    bool fullscreen;
    bool draw_every_other_line;
    
    short bit_depth;  // currently 8 or 16
    short gamma_level;

    short width;
    short height;
    bool auto_resolution;
    bool high_dpi;
    bool hud;
    short hud_scale_level;
    short term_scale_level;
    bool fix_h_not_v;
    bool translucent_map;
    BobbingType bobbing_type;

    int fov; // 0 = use default (or MML/plugin)
};


extern struct screen_mode_data screen_mode;

screen_mode_data *get_screen_mode(); // yes, the irony

void change_gamma_level(short gamma_level);




// TODO: rename gameworld_view_data_t
struct view_data
{
    // LP change: specifying current and target field-of-view as floats;
    // one changes the field of view by setting a new target then adjusting
    // the current FOV toward it
    float field_of_view;
    float target_field_of_view;
    short standard_screen_width; /* this is *not* the width of the projected image (see initialize_view_data() in RENDER.C */
    short screen_width, screen_height; /* dimensions of the projected image */
    short horizontal_scale, vertical_scale;
    
    short half_screen_width, half_screen_height;
    short world_to_screen_x, world_to_screen_y;
    short dtanpitch; /* world_to_screen*tan(pitch) */
    angle half_cone; /* often ==field_of_view/2 (when screen_width==standard_screen_width) */
    angle half_vertical_cone;

    long_vector2d left_edge, right_edge; // view cone edges as world directions

    short ticks_elapsed;
    uint32 tick_count; /* for effects and transfer modes */
    float heartbeat_fraction;
    short origin_polygon_index;
    angle yaw, pitch, roll;
    fixed_angle virtual_yaw, virtual_pitch;
    world_point3d origin;
    _fixed maximum_depth_intensity; /* in fixed units */

    short shading_mode;

    short effect, effect_phase;
    short real_world_to_screen_x, real_world_to_screen_y;
    
    bool overhead_map_active;
    short overhead_map_scale;

    bool under_media_boundary;
    short under_media_index;
    
    bool terminal_mode_active;
    
    // LP addition: this indicates whether to show weapons-in-hand display;
    // this is on in first-person, off in third-person
    bool show_weapons_in_hand;
    
    // LP: Indicates whether or not tunnel vision is active
    bool tunnel_vision_active;
    
    // LP addition: value of yaw used by landscapes; this is so that the center
    // can stay stationary
    angle landscape_yaw;
    
    // whether to mimic software renderer when looking up/down
    bool mimic_sw_perspective;

    // whether to correct sprite parallax when not mimicking software
    bool billboard_xy;
};

extern struct view_data *world_view; // a mixture of gameplay state, precalculated SW renderer params, user prefs, and whatever else got thrown in it back in 1995, and since



// reset field of view to whatever the player had had when reviving
void ResetFieldOfView();

bool zoom_overhead_map_out();
bool zoom_overhead_map_in();

void set_automap_is_visible(bool status);
void set_computer_terminal_is_visible(bool status);


void start_teleport_in_effect();
void start_teleport_out_effect();

void start_extravision_activate_effect();
void start_extravision_deactivate_effect();

bool set_zoom_is_enabled(bool is_on);
bool get_zoom_is_enabled();



// TODO: everything above should move into ViewControl.h/.cpp (preferably given a meaningful filename, e.g. gameworld_view_settings)










// rest of this is HUD overlay



void screen_print(const std::string& s);



class FpsCounter
{
public:
	using clock = std::chrono::high_resolution_clock;
	static constexpr auto update_time = std::chrono::milliseconds(250);
	
	FpsCounter() :
		next_update_{clock::now() + update_time},
		sum_{0},
		count_{0},
		fps_{0}
		{
			
		}
	
	void update() {
		auto now = clock::now();
		sum_ += 1.0 / std::chrono::duration_cast<std::chrono::duration<float>>(now - prev_).count();
		++count_;
		prev_ = now;
		
		if (now >= next_update_)
		{
			next_update_ = now + update_time;
			if (count_) {
				fps_ = sum_ / count_;
			} else {
				fps_ = 0.f;
			}
			
			sum_ = 0;
			count_ = 0;
		}
	}
	
	float get() const { return fps_; }
	bool ready() const { return fps_ != 0; }

	void reset() {
		sum_ = 0;
		count_ = 0;
		prev_ = clock::now();

		fps_ = 0.f;
	}

private:
	clock::time_point prev_;
	clock::time_point next_update_;
	
	float sum_;
	int count_;

	bool ready_;
	float fps_;
};

constexpr std::chrono::milliseconds FpsCounter::update_time;

extern FpsCounter fps_counter;

extern bool displaying_fps;

extern bool ShowPosition;
extern bool ShowScores;


void reset_messages();

// Displays a message on the screen for a second or so; may be good for debugging
void ShowMessage(char *Text);

/* SB: Custom Blizzard-style overlays */
#define MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS 6
bool IsScriptHUDNonlocal();
void SetScriptHUDNonlocal(bool nonlocal = true);
/* color is a terminal color */
void SetScriptHUDColor(int player, int idx, int color);
/* text == NULL or "" removes that HUD element
   to turn HUD elements off, set all elements NULL or "" */
void SetScriptHUDText(int player, int idx, const char* text);
/* icon == NULL turns the icon off
   someday I'll document the format */
bool SetScriptHUDIcon(int player, int idx, const char* icon, size_t length);
/* sets the icon for that HUD to a colored square (same colors as SetScriptHUDColor) */
void SetScriptHUDSquare(int player, int idx, int color);



void update_fps_display(SDL_Surface* s);
void DisplayPosition(SDL_Surface* s);
void DisplayMessages(SDL_Surface* s);
void DisplayNetLoadingScreen(SDL_Surface* s);
void DisplayScores(SDL_Surface* s);
void DisplayInputLine(SDL_Surface* s);


#endif /*  __screen_shared_h__ */
