/*
 SCREEN_DRAWING.H -- NOT related to screen.h, despite the (confusing!) naming.
                     This is mostly M2's hardcoded UI and HUD rects, functions
                     for inflicting math on those rects, and lots of pretentious
                     pompous "ports" bullshit which I'm guessing was LP being a
                     RealProgrammer(TM), for how's an ubernerd to have fun using
                     just ONE SDL_Surface for drawing everything 2D to screen?
 
                     Wanna guess whose code is about to get nuked from orbit?
                     'Cos its the only way to be sure.
 
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



#include "shapes.h" // for main screen/HUD graphics that live in Shapes files

#include "fonts.hpp"




enum {
    // M2 HUD rects are now in lua_hud_script.h
    
	// main menu rects
    START_OF_MENU_INTERFACE_RECTS,
	_new_game_button_rect = 7,
	_load_game_button_rect,
	_gather_button_rect,
	_join_button_rect,
	_prefs_button_rect,
	_replay_last_button_rect,
	_save_last_button_rect,
	_replay_saved_button_rect,
	_credits_button_rect,
	_quit_button_rect,
	_center_button_rect,
	_singleton_game_button_rect,
	_about_alephone_rect,
	END_OF_MENU_INTERFACE_RECTS,
};



enum {
    // computer terminal rects // TODO: move these to Terminal/
	_terminal_screen_rect = 20,
	_terminal_header_rect,
	_terminal_footer_rect,
	_terminal_full_text_rect,
	_terminal_left_rect,
	_terminal_right_rect,
	_terminal_logon_graphic_rect,
	_terminal_logon_title_rect,
	_terminal_logon_location_rect,
	_respawn_indicator_rect,
	_blinker_rect,
	
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
	PLAYER_COLOR_BASE_INDEX = 6, // 8 colors
	
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


// TODO: while we want to standardize on SDL_Rect's {x,y,w,h} going forwards, wondering if we should define our own aorect_t which casts to/from the other types; this allows us to provide the various rect math functions as struct methods, for cleaner namespacing and fewer args
/* Structure for portable rectangles.  notice it is exactly same as Rect */
struct screen_rectangle {
	short top, left;
	short bottom, right;
};
typedef struct screen_rectangle screen_rectangle;

/* ------- Prototypes */
void initialize_screen_drawing(void);




void _scroll_window(short dy, short rectangle_id, short background_color_index);

SDL_Rect get_interface_rect(int32_t index); // used by lua_hud_objects.cpp

SDL_Rect get_hud_rect(int32_t index);
SDL_Rect get_main_menu_rect(int32_t index);
SDL_Rect get_computer_terminal_rect(int32_t index);

SDL_Color get_interface_color(int32_t index);

SDL_Color get_player_color(int32_t color_index);

SDL_Color get_computer_terminal_color(int32_t index);




struct world_point2d;

extern void draw_polygon(SDL_Surface *s, const world_point2d *vertex_array, int vertex_count, uint32 pixel);
extern void draw_line(SDL_Surface *s, const world_point2d *v1, const world_point2d *v2, uint32 pixel, int pen_size);
extern void draw_outlined_rect(SDL_Surface *s, const SDL_Rect *r, uint32 pixel);

inline void draw_outlined_rect(SDL_Surface *s, const SDL_Rect *r, const SDL_Color& color)
{
    draw_outlined_rect(s, r, SDL_MapRGB(s->format, color.r, color.g, color.b));
}


inline void draw_filled_rect(SDL_Surface *s, const SDL_Rect *r, const SDL_Color& color)
{
    SDL_FillRect(s, r, SDL_MapRGB(s->format, color.r, color.g, color.b));
}


// seriously, dude
void _set_port_to_screen_window(void);
void _set_port_to_gworld(void);
void _restore_port(void);
void _set_port_to_term(void);
void _set_port_to_intro(void);
void _set_port_to_map(void);
void _set_port_to_custom(SDL_Surface *surface);
// LP addition: stuff to use a buffer for the Heads-Up Display
void _set_port_to_HUD();


// EES: and here's why I'm having to touch this file in indecent places:

/*
// If source==NULL, source= the shapes bounding rectangle
void screen_drawing___draw_screen_text(const std::string& text, screen_rectangle *destination, short flags, short font_id, short text_color);

short _text_width(const std::string& buffer, int start, short font_id);



short _get_font_line_height(short font_index);




// TODO: these inline funcs are a lot of shite: if font is missing it should never get this far

static inline int draw_text(SDL_Surface *s, const std::string& text, int x, int y, uint32 pixel, const Font *font, uint16 style)
{
	return font ? font->draw_text(s, text, x, y, pixel, style) : 0;
}


static inline int trunc_text(const std::string& text, int max_width, const Font *font, uint16 style)
{
	return font ? font->trunc_text(text, max_width, style) : 0;
}
*/


// MML

void parse_mml_interface_rectangles(const InfoTree& root);
void reset_mml_interface_rectangles();

void parse_mml_interface_colors(const InfoTree& root);
void reset_mml_interface_colors();

#endif
