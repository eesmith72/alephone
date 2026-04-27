/*
 cscluts.h -- used in fades.cpp and shapes.cpp
 
 Copyright (C) 1991-2001 and beyond by Bo Lindbergh
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

#ifndef __cscluts_h__
#define __cscluts_h__

#include "cseries.h"


struct rgb_color
{
	uint16_t red;
	uint16_t green;
	uint16_t blue;
    
    rgb_color(uint16_t red, uint16_t green, uint16_t blue) : red(red), green(green), blue(blue) {}
    rgb_color(const SDL_Color& c) : red(c.r << 8), green(c.g << 8), blue(c.b << 8) {}
    rgb_color() : red(0), green(0), blue(0) {}
    
    explicit operator SDL_Color() const { return {(uint8_t)(red >> 8), (uint8_t)(green >> 8), (uint8_t)(blue >> 8), 0xff}; }
};


struct color_table
{
	short color_count;
	rgb_color colors[256];
};


extern struct color_table *uncorrected_color_table; // the pristine color environment of the game (can be 16bit)
extern struct color_table *world_color_table;       // the gamma-corrected color environment of the game (can be 16bit)
extern struct color_table *interface_color_table;   // always 8bit, for mixed-mode (i.e., valkyrie) fades
extern struct color_table *visible_color_table;     // the color environment the player sees (can be 16bit)


extern const rgb_color rgb_black;
extern const rgb_color rgb_white;

extern SDL_PixelFormat pixel_format_16; // used in shapes.cpp
extern SDL_PixelFormat pixel_format_32; // used in shapes.cpp, screen.cpp, terminal_renderer.cpp also uses it in randomize_pixel



void initialize_cluts();

void build_sdl_color_table(const color_table *color_table, SDL_Color *colors);


void initialize_gamma();

void change_gamma_level(short gamma_level);

void assert_world_color_table(struct color_table *world_color_table, struct color_table *interface_color_table);


#endif /* __cscluts_h__ */
