/*
 ClassicRasterizer.h -- original M2 SW renderer
 adapted from SCOTTISH_TEXTURES.c by Loren Petrich, August 7, 2000
 
 
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

#ifndef ClassicRasterizer_h
#define ClassicRasterizer_h

#include "cseries.hpp"

#include "Rasterizer.h"
#include "ImageBlitter.hpp"
#include "low_level_textures.h"

#include "images.h" // DEBUG



typedef void (*convert_bitmap_to_rgb32_proc)(bitmap_definition_t& bitmap_definition);


void initialize_classic_color_map();


class ClassicRasterizer: public Rasterizer
{
public:
    
    ClassicRasterizer()
    {
        initialize_classic_color_map();
        m_bitmap_definition.bitmap.resize(800 * 600 * 4); // allocate enough space to transform Classic16 to RGBA32 in-place
    }
    
    void configure(const SDL_Point& size, int32_t bit_depth) override;
    
    void End() override;

    // draw ceiling/floor poly
	void texture_horizontal_polygon(polygon_definition& textured_polygon) override;
	
    // draw wall (side) poly
    void texture_vertical_polygon(polygon_definition& textured_polygon) override;

    // draw sprite (monsters, items)
	void texture_rectangle(billboard_t& textured_rectangle) override;
    
    
private:
    
    SDL_Point size = {0, 0};
    int32_t bit_depth = 0;
    
    // the pixel buffer to which the SW rasterizer draws
    bitmap_definition_t m_bitmap_definition;
    
    convert_bitmap_to_rgb32_proc convert_bitmap_to_rgb32;
    
    
    void extracted(int32_t offset, uint8_t *&p);
    
    void darken(); // draw 1px black dither effect over gameworld when game is paused
    
    
    void calculate_shading_table(void*& result, void* shading_tables, short depth, ao_fixed ambient_shade);
    
    void _prelandscape_horizontal_polygon_lines(polygon_definition* polygon, short y0, short* x0_table, short* x1_table, short line_count);

    template<int TEXBITS>
    void _pretexture_vertical_polygon_lines(polygon_definition *polygon, short x0, short *y0_table, short *y1_table, short line_count);
    
    template<int TEXBITS>
    void _pretexture_horizontal_polygon_lines(polygon_definition *polygon, short y0, short *x0_table, short *x1_table, short line_count);
};




void set_classic_gamma(float gamma); // called by Screen::set_gameworld_gamma


// called by fades.cpp to get the 8/16-bit color table to apply any tint and/or damage effects
const color_table_t* get_classic_color_table();


// called by fades.cpp once it's applied any tint and/or hit effects

void set_classic_color_map(const color_table_t& color_table);

void reset_classic_color_map();


#endif /* ClassicRasterizer_h */
