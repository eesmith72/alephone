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



typedef void (*normalize_virtual_screen_buffer_proc)(SDL_Surface *src, SDL_Surface *dst);


class ClassicRasterizer: public Rasterizer
{
public:
    
    void configure(const SDL_Point& size, int32_t bit_depth) override;
    
    void Begin(camera_settings_t* view) override // TODO: can view be const'd?
    {
        //SDL_LockSurface(m_surface);
        Rasterizer::Begin(view);
    }
    
    void End() override;

    // draw ceiling/floor poly
	void texture_horizontal_polygon(polygon_definition& textured_polygon) override;
	
    // draw wall (side) poly
    void texture_vertical_polygon(polygon_definition& textured_polygon) override;

    // draw sprite (monsters, items)
	void texture_rectangle(rectangle_definition& textured_rectangle) override;
    
    
private:
    
    // EES: bitmap_definition_t is a variable-length struct (predating C99, which introduced formal syntax for this), ending in array of pointers into the pixel data (in this case, the Surface's pixels buffer); while it'd be nice to modernize the struct's implementation (replacing the variable-length array with std::vector) so it's easy to understand, it's heavily used in shapes.cpp and cleaning that up is a job in itself
    std::vector<uint8_t> m_bitmap_definition;
    
    SDL_Surface* m_surface; // TODO: we can eventually get rid of this and allocate a std::vector<uint8_t> buffer that is initially empty and resized to 800*600*4 on first use.
    
    
    bitmap_definition_t* bitmap_definition() { return reinterpret_cast<bitmap_definition_t*>(m_bitmap_definition.data()); }
    
    normalize_virtual_screen_buffer_proc normalize_virtual_screen_buffer;
    
    
    void darken(); // draw 1px black dither effect over gameworld when game is paused; must be within begin+end calls
    
    
    void clear()
    {
        m_bitmap_definition.clear(); 
        SDL_FreeSurface(m_surface);
    }
    
    void calculate_shading_table(void*& result, void* shading_tables, short depth, ao_fixed ambient_shade);
    
    void _prelandscape_horizontal_polygon_lines(polygon_definition* polygon, short y0, short* x0_table, short* x1_table, short line_count);

    template<int TEXBITS>
    void _pretexture_vertical_polygon_lines(polygon_definition *polygon, short x0, short *y0_table, short *y1_table, short line_count);
    
    template<int TEXBITS>
    void _pretexture_horizontal_polygon_lines(polygon_definition *polygon, short y0, short *x0_table, short *x1_table, short line_count);
};




void allocate_sw_texture_tables(); // called by initialize_marathon() in marathon2.cpp


void set_classic_gamma(float gamma); // called by Screen::set_gameworld_gamma


// called by fades.cpp to get the 8/16-bit color table to apply any tint and/or damage effects
const color_table_t* get_classic_color_table();


// called by fades.cpp once it's applied any tint and/or hit effects

void set_classic_color_map(const color_table_t& color_table);

void reset_classic_color_map();


#endif /* ClassicRasterizer_h */
