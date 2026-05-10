/*
 cscolors.hpp -- used in [Classic] fades.cpp and shapes.cpp
 
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

#ifndef cscolors_hpp
#define cscolors_hpp

#include "cstypes.hpp"


// TODO: not sure where this file belongs; the color types relate to shapes.cpp, the color tables and gamma functions to fade/screen


//-----------------------------------------------------------------------------
// RGB16 color; used by shapes.cpp


typedef std::array<uint16_t, 256> gamma_curve_t;


// RGBA as array of 4 GLfloat, 0.0-1.0
typedef GLfloat ao_rgbaf[4];


#define c2f(channel) (float)((channel) / float(FIXED_ONE - 1))
#define o2f(opacity) (float)((opacity) / float(FIXED_ONE))

#define ao_rgb_to_rgbaf(color, opacity)  {c2f(color.r), c2f(color.g), c2f(color.b), o2f(transparency)}


struct ao_rgb
{
	uint16_t r;
	uint16_t g;
	uint16_t b;
    
    constexpr ao_rgb(uint16_t red, uint16_t green, uint16_t blue) : r(red), g(green), b(blue) {}
    
    constexpr ao_rgb(const SDL_Color& c) : r(c.r << 8), g(c.g << 8), b(c.b << 8) {}
    
    constexpr ao_rgb(const ao_rgb& c) : r(c.r), g(c.g), b(c.b) {}
    
    constexpr ao_rgb() : r(0), g(0), b(0) {}
    
    ~ao_rgb() {}
    
    explicit operator SDL_Color() const { return {(uint8_t)(r >> 8), (uint8_t)(g >> 8), (uint8_t)(b >> 8), 0xff}; }
};


// Used by ClassicRasterizer as 1. RGB lookup table for 8-bit indexed colors, and 2. gamma curve for 16/24-bit 'true color' modes.

#define COLOR_TABLE_MAX_COUNT (256)

struct color_table_t
{
    
    // (note: the SW renderer expects 256 entries, which is why this isn't a variable-length std::vector)
	int16_t color_count;
	ao_rgb colors[COLOR_TABLE_MAX_COUNT]; // while we could make this std::array<ao_rgb, 256>, it's fine as-is and keeps copying the struct simple
    
    color_table_t()
    {
        memset(this, 0, sizeof(color_table_t));
    }
    
    // in Classic 8, copy indexed color table based on
    
    void make_copy_with_gamma(const color_table_t& color_table, float gamma = 1.0)
    {
        color_count = COLOR_TABLE_MAX_COUNT;
        if (gamma > 0.999 && gamma < 1.001) // 1.0 = linear ramp
        {
            memcpy(this, &color_table, sizeof(color_table_t));
        }
        else
        {
            for (short i = 0; i < color_count; i++)
            {
                const ao_rgb& src = color_table.colors[i];
                ao_rgb& dst = colors[i];
                dst.r = uint16_t(pow(float(src.r / 65535.0), gamma) * 65535.0);
                dst.g = uint16_t(pow(float(src.g / 65535.0), gamma) * 65535.0);
                dst.b = uint16_t(pow(float(src.b / 65535.0), gamma) * 65535.0);
            }
        }
    }
    
    // direct 16/32-bit;
    
    void make_gamma(float gamma = 1.0)
    {
        color_count = COLOR_TABLE_MAX_COUNT;
        if (gamma > 0.999 && gamma < 1.001) // 1.0 = linear ramp
        {
            for (int16_t i = 0; i < color_count; i++)
            {
                ao_rgb& color = colors[i];
                color.r = color.g = color.b = i << 8;
            }
        }
        else
        {
            for (int16_t i = 0; i < color_count; i++)
            {
                ao_rgb& dst = colors[i];
                dst.r = dst.g = dst.b = uint16_t(pow(float(i / 255.0), gamma) * 65535.0);
            }
        }
    }

    
    // returns SDL_Color[256] for use in SDL_SetPaletteColors // TODO: should be able to get rid of this when done
    void get_sdl_color_table(SDL_Color* result)
    {
        for (int32_t i = 0; i < color_count; i++)
        {
            const ao_rgb& src = colors[i];
            SDL_Color& dst = result[i];
            dst.r = src.r >> 8;
            dst.g = src.g >> 8;
            dst.b = src.b >> 8;
            dst.a = 0xff;
        }
    }
    
    
    void print_debug()
    {
        for (int32_t i = 0; i < COLOR_TABLE_MAX_COUNT; i++)
        {
            printf("%3d {%3d, %3d, %3d}\n", i, colors[i].r, colors[i].g, colors[i].b);
        }
    }
};


#endif /* cscolors_hpp */
