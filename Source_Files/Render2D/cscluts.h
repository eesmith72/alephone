/*

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
#ifndef _CSERIES_CLUTS_
#define _CSERIES_CLUTS_

#include "cstypes.h"

class LoadedResource;
struct RGBColor;


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


extern void build_color_table(color_table *table, LoadedResource &clut);

enum {
	gray15Percent,
	windowHighlight,
	NUM_SYSTEM_COLORS
};


extern RGBColor rgb_black;
extern RGBColor rgb_white;
extern RGBColor system_colors[NUM_SYSTEM_COLORS];


#endif
