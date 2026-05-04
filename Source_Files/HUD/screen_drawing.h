/*
 SCREEN_DRAWING.H
 
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

#ifndef __SCREEN_DRAWING_H
#define __SCREEN_DRAWING_H


// TODO: finish getting rid of this


#include "shapes.h" // for main screen/HUD graphics that live in Shapes files

#include "fonts.hpp"





enum {
    // computer terminal rects // TODO: move these to Terminal/
    START_OF_TERMINAL_RECTS         = 20,
	_terminal_screen_rect           = 20,
	_terminal_header_rect           = 21,
	_terminal_footer_rect           = 22,
	_terminal_full_text_rect        = 23,
	_terminal_left_rect             = 24,
	_terminal_right_rect            = 25,
	_terminal_logon_graphic_rect    = 26,
	_terminal_logon_title_rect      = 27,
	_terminal_logon_location_rect   = 28,
	_respawn_indicator_rect         = 29,
	_blinker_rect                   = 30,
    END_OF_TERMINAL_RECTS           = 31,
	
	NUMBER_OF_INTERFACE_RECTANGLES
};


/* Colors for drawing.. */
enum {
    // moved M2 HUD colors to hud_definitions.hpp
    /*
	_energy_weapon_full_color,
	_energy_weapon_empty_color,
	_black_color,
	_inventory_text_color,
	_inventory_header_background_color,
	_inventory_background_color,
     */
//	PLAYER_COLOR_BASE_INDEX = 6, // 8 colors
	
	_white_color = 14,
	_invalid_weapon_color,
    
	_computer_border_background_text_color = 16,
	_computer_border_text_color,
	_computer_interface_text_color,
	_computer_interface_color_purple,
	_computer_interface_color_red,
	_computer_interface_color_pink,
	_computer_interface_color_aqua,
	_computer_interface_color_yellow,
	_computer_interface_color_brown,
	_computer_interface_color_blue,
    NUMBER_OF_INTERFACE_COLORS
};


SDL_Rect get_interface_rect(int32_t index); // used by lua_hud_objects.cpp


// dividing get_interface_rect() according to where rects are used gives us these:

SDL_Rect get_hud_rect(int32_t index);

SDL_Rect get_computer_virtual_terminal_rect(int32_t index);


// likewise, get_interface_color is HUD, marine colors, terminals, and a couple odd ones

SDL_Color get_interface_color(int32_t index);

SDL_Color get_player_color(int32_t color_index);

SDL_Color get_computer_terminal_color(int32_t index);




// MML

void parse_mml_interface_rectangles(const InfoTree& root);
void reset_mml_interface_rectangles();

void parse_mml_interface_colors(const InfoTree& root);
void reset_mml_interface_colors();

#endif
