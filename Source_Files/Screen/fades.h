/*
 FADES.H

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

#ifndef __FADES_H
#define __FADES_H


#include "cseries.h"


enum
{
	NUMBER_OF_GAMMA_LEVELS= 8,
	DEFAULT_GAMMA_LEVEL= 2
};

enum /* fade types */
{
	_start_cinematic_fade_in, /* force all colors to black immediately */
	_cinematic_fade_in, /* fade in from black */
	_long_cinematic_fade_in,
	_cinematic_fade_out, /* fade out from black */
	_end_cinematic_fade_out, /* force all colors from black immediately */

	_fade_red, /* bullets and fist */
	_fade_big_red, /* bigger bullets and fists */
	_fade_bonus, /* picking up items */
	_fade_bright, /* teleporting */
	_fade_long_bright, /* nuclear monster detonations */
	_fade_yellow, /* explosions */
	_fade_big_yellow, /* big explosions */
	_fade_purple, /* ? */
	_fade_cyan, /* fighter staves and projectiles */
	_fade_white, /* absorbed */
	_fade_big_white, /* rocket (probably) absorbed */
	_fade_orange, /* flamethrower */
	_fade_long_orange, /* marathon lava */
	_fade_green, /* hunter projectile */
	_fade_long_green, /* alien green goo */
	_fade_static, /* compiler projectile */
	_fade_negative, /* minor fusion projectile */
	_fade_big_negative, /* major fusion projectile */
	_fade_flicker_negative, /* hummer projectile */
	_fade_dodge_purple, /* alien weapon */
	_fade_burn_cyan, /* armageddon beast electricity */
	_fade_dodge_yellow, /* armageddon beast projectile */
	_fade_burn_green, /* hunter projectile */

	_fade_tint_green, /* under goo */
	_fade_tint_blue, /* under water */
	_fade_tint_orange, /* under lava */
	_fade_tint_gross, /* under sewage */
	_fade_tint_jjaro, /* under JjaroGoo */ // LP addition
	
	NUMBER_OF_FADE_TYPES
};

// LP change: rearranged to get order: water, lava, sewage, jjaro, pfhor
enum /* effect types */
{
	_effect_under_water,
	_effect_under_lava,
	_effect_under_sewage,
	_effect_under_jjaro,
	_effect_under_goo,
	NUMBER_OF_FADE_EFFECT_TYPES
};

// LP addition, since XML does not support direct specification of callbacks
// very well.
// Moved out here for the convenience of OpenGL fader implementations.
enum
{
	_tint_fader_type,
	_randomize_fader_type,
	_negate_fader_type,
	_dodge_fader_type,
	_burn_fader_type,
	_soft_tint_fader_type,
	NUMBER_OF_FADER_FUNCTIONS
};



// dumping these here for now until SW+OGL fades are straightened out as they are entangled in Interface/chapter_screens.cpp
extern short current_picture_clut_depth;
extern struct color_table *animated_color_table;
extern struct color_table *current_picture_clut;



void initialize_fades(void);

void gamma_correct_color_table(struct color_table *uncorrected_color_table, struct color_table *corrected_color_table, short gamma_level);
float get_actual_gamma_adjust(short gamma_level);


bool update_fades(bool game_in_progress = false);

void start_gameworld_fade(short type); // doesn't actually start it, only sets it up ready to be animated

void stop_fade(void);

bool fade_finished(void);

void set_fade_effect(short type);

// interface fades (mostly moved here from interface.cpp)

void start_ui_fade(int16_t type, color_table* original_color_table, color_table* animated_color_table);

void animate_ui_fade_blocking(short type, color_table* original_color_table); // blocking (runs its own minimal event loop to detect keypresses and skip to end)


void animate_ui_fade_in_blocking(bool is_slow = false);

void animate_ui_fade_out_blocking(bool fade_music = false);

// call this when displaying an OS dialog; in 8-bit color depth, this ensures it uses the correct colors // TODO: still appropriate? modern OSes don't even do 8-bit color; the only thing that's 8-bit is the SDL_Surface used to draw 2D interface so 99% sure these calls can be replaced with animate_ui_fade_out_blocking
void force_system_colors(bool fade_music = false);

void start_interface_fade(short type);
void update_interface_fades();
bool can_interface_fade_out();
bool interface_fade_finished();
void stop_ui_fade();




// see if the screen was set to black by the last fade
bool screen_is_faded_black(void);

// LP: sets the number of calls of set_fade_effect() that get ignored;
// this is a workaround for a MacOS-version bug where something gets painted on the screen
// after certain dialog boxes are cleared, thus canceling out the fader effect.
void SetFadeEffectDelay(int _FadeEffectDelay);

class InfoTree;
void parse_mml_faders(const InfoTree& root);
void reset_mml_faders();

#endif

