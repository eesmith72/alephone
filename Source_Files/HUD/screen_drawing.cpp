/*
	SCREEN_DRAWING.C

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

#include "cseries.hpp"

#include "map.h"
#include "interface.hpp"
#include "screen_drawing.h"
#include "visual_effects.hpp"
#include "Screen.hpp"

#include "fonts.hpp"

#include "preferences.hpp"

#include "InfoTree.h"


// TODO: relocate HUD rects and computer terminal rects to their respective modules and get rid of this array

static const std::array<SDL_Rect, 31> interface_rectangles_std = {
    // M2 HUD rects
    300, 326, 173,  12, // _player_name_rect       = 0,
    398, 464, 180,  11, // _oxygen_rect            = 1,
    181, 464, 180,  11, // _shield_rect            = 2,
    17, 338, -17, -338, // _motion_sensor_rect     = 3,
      0,   0,   0,   0, // _microphone_rect        = 4,
    204, 352, 180, 102, // _inventory_rect         = 5,
    384, 352, 212, 102, // _weapon_display_rect    = 6,
    
    // main menu rects // TODO: no longer used; main_menu.cpp has its own config table now
    101, 179, 167,  31, // _new_game_button_rect       =  7,
     25, 221, 213,  32, // _load_game_button_rect      =  8,
     11, 263, 212,  31, // _gather_button_rect         =  9,
     38, 301, 198,  32, // _join_button_rect           = 10,
    421, 304, 142,  27, // _prefs_button_rect          = 11,
    231, 386, 175,  27, // _replay_last_button_rect    = 12,
    363, 345, 153,  27, // _save_last_button_rect      = 13,
     83, 344, 188,  30, // _replay_saved_button_rect   = 14,
    246, 206, 136, 141, // _credits_button_rect        = 15,
    500, 263,  85,  31, // _quit_button_rect           = 16, // adjusted to work with both m2 and inf
      0,   0,   0,   0, // _center_button_rect         = 17,
      0,   0,   0,   0, // _singleton_game_button_rect = 18,
    560, 440,  80,  40, // _about_alephone_rect        = 19,
    
    // computer terminal rects
      0,   0, 640, 320, // _terminal_screen_rect           = 20, // in M2, terminal view fully filled top two-thirds of 640x480 screen; in AO, is must adjust for widescreen and Lua HUD positions (but should presumably maintain the original 2:1 aspect ratio)
      0,   0, 640,  18, // _terminal_header_rect           = 21,
      0, 302, 640,  18, // _terminal_footer_rect           = 22,
     72,  27, 496, 266, // _terminal_full_text_rect        = 23,
      9,  27, 307, 266, // _terminal_left_rect             = 24,
    324,  27, 307, 266, // _terminal_right_rect            = 25,
      9,  27, 622, 266, // _terminal_logon_graphic_rect    = 26,
      0,   0,   0,   0, // _terminal_logon_title_rect      = 27,
      0,   0,   0,   0, // _terminal_logon_location_rect   = 28,
      0,   0,   0,   0, // _respawn_indicator_rect         = 29,
      0,   0,   0,   0, // _blinker_rect                   = 30,
};


static std::array<SDL_Rect, 31> interface_rectangles;


SDL_Rect get_interface_rect(int32_t index)
{
    return interface_rectangles.at(index);
}


// TODO: for now, these use the original limits; once the above array is split into 3 (with MML-compatibility preserved), the menu rect array can be converted to vector to allow arbitrary number of buttons, and merged into the other button-related arrays (e.g. item order)
SDL_Rect get_hud_rect(int32_t index)
{
    return interface_rectangles.at(index);
}



SDL_Rect get_computer_virtual_terminal_rect(int32_t index)
{
    return interface_rectangles.at(index);
}




// from original M2 'clut' resource
static const std::array<SDL_Color, 26> interface_colors_std = {
    // HUD inventory
    0x00, 0xff, 0x00, 0xff,
    0x00, 0x14, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff,
    0x00, 0x32, 0x00, 0xff,
    0x00, 0x13, 0x00, 0xff,
    
    // Player colors
    0x24, 0x5f, 0xa3, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0xb0, 0x00, 0x5e, 0xff,
    0xff, 0xff, 0x00, 0xff,
    0xea, 0xea, 0xea, 0xff,
    0xf6, 0x58, 0x00, 0xff,
    0x0c, 0x00, 0xff, 0xff,
    0x00, 0xff, 0x00, 0xff,
    
    // _white_color, _invalid_weapon_color // TODO: where are these used? HUD?
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x14, 0x00, 0xff,
    
    // computer terminal colors
    0x27, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x00, 0x9c, 0x00, 0xff,
    0x00, 0xb0, 0xc9, 0xff,
    0xff, 0xe7, 0x00, 0xff,
    0xaf, 0x00, 0x00, 0xff,
    0x0c, 0x00, 0xff, 0xff,
};


static std::array<SDL_Color, 26> interface_colors;

SDL_Color get_interface_color(int32_t index)
{
    return interface_colors.at(index);
}


SDL_Color get_player_color(int32_t color_index)
{
    return interface_colors.at(color_index + PLAYER_COLOR_BASE_INDEX);
}


SDL_Color get_computer_terminal_color(int32_t index)
{
    return interface_colors.at(index);
}


// MML

void reset_mml_interface_rectangles()
{
    interface_rectangles = interface_rectangles_std;
}


void parse_mml_interface_rectangles(const InfoTree& root)
{
    for (const InfoTree &rect : root.children_named("rect"))
    {
        int16 index, top = 0, left = 0, bottom = 0, right = 0;
        if (rect.read_indexed("index", index, NUMBER_OF_INTERFACE_RECTANGLES)
            && rect.read_attr("top", top) && rect.read_attr("left", left)
            && rect.read_attr("bottom", bottom) && rect.read_attr("right", right))
        {
            interface_rectangles[index] = {left, top, right - left, bottom - top};
        }
    }
}


void reset_mml_interface_colors()
{
    interface_colors = interface_colors_std;
}


void parse_mml_interface_colors(const InfoTree& root)
{
    for (const InfoTree &color : root.children_named("color"))
    {
        int16 index;
        if (color.read_indexed("index", index, NUMBER_OF_INTERFACE_COLORS))
        {
            color.read_color(interface_colors[index]);
        }
    }
}
