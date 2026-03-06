#ifndef __SHELL_H
#define __SHELL_H

/*
SHELL.H

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

Saturday, August 22, 1992 2:18:48 PM

Saturday, January 2, 1993 10:22:46 PM
	thank god c doesn’t choke on incomplete structure references.

Jul 5, 2000 (Loren Petrich):
	Added XML support for controlling the cheats

Jul 7, 2000 (Loren Petrich):
	Added Ben Thompson's change: an Input-Sprocket-only input mode

Aug 12, 2000 (Loren Petrich):
	Using object-oriented file handler

Dec 29, 2000 (Loren Petrich):
	Added function for showing text messages on the screen
*/

#include "cstypes.h"
#include <string>

struct RGBColor;
struct SDL_Color;
struct SDL_Surface;


/* ---------- resources */


enum class BobbingType
{
	none,
	camera_and_weapon,
	weapon_only
};

/* ---------- structures */

struct screen_mode_data
{
	short acceleration;
	
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

#define NUMBER_OF_KEYS 21
#define NUMBER_UNUSED_KEYS 10

enum // input devices
{
	_keyboard_or_game_pad,
	_mouse_yaw_pitch
};

#define PREFERENCES_NAME_LENGTH 32

/* ---------- prototypes/SHELL.C [now shell_misc.cpp, shell_macintosh.cpp, shell_sdl.cpp] */

void global_idle_proc(void);

// Load the base MML scripts:
void LoadBaseMMLScripts(bool load_menu_mml_only);


/* ---------- prototypes/SHAPES.C */

/* ---------- prototypes/SCREEN_DRAWING.C */

void _get_player_color(size_t color_index, RGBColor *color);
void _get_interface_color(size_t color_index, RGBColor *color);
void _get_player_color(size_t color_index, SDL_Color *color);
void _get_interface_color(size_t color_index, SDL_Color *color);


/* ---------- protoypes/INTERFACE_MACINTOSH.C */
void update_game_window(void);

/* ---------- prototypes/PREFERENCES.C */
void load_environment_from_preferences(void);


/*
// Command-line options
bool option_nogl = false;             // Disable OpenGL
bool option_nosound = false;          // Disable sound output
bool option_nogamma = false;          // Disable gamma table effects (menu fades)
bool option_debug = false;
bool option_nojoystick = false;
bool insecure_lua = false;
static bool force_fullscreen = false; // Force fullscreen mode
static bool force_windowed = false;   // Force windowed mode
*/

void main_event_loop(void);

void initialize_application(void);
void shutdown_application(void);

bool handle_open_document(const ao_path& filename);


#endif
