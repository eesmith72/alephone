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

class ClassicRasterizer: public Rasterizer
{
public:
    
    void configure(const SDL_Point& size, int32_t bit_depth);
    
    void Begin(camera_settings_t* view) override // TODO: can view be const'd?
    {
        SDL_LockSurface(m_surface);
        Rasterizer::Begin(view);
    }
    
    void End() override
    {
        SDL_UnlockSurface(m_surface);
        
        
        // TODO: FIX: nothing's appearing ATM, not even a static pict that we know works in UI, which says something in the OGL setup ain't right (Classic is 100% 2D drawing as far as OGL is concerned, and the UI's drawing fine so it's probably something in enter_/exit_gameworld changing the OGL setup)
        SDL_FillRect(m_surface, nullptr, SDL_MapRGBA(m_surface->format, 255, 0, 0, 127)); // DEBUG
        SDL_Surface* s = get_pict_resource_from_images(1114); // DEBUG
        m_ogl_blitter.borrow_surface(s); // DEBUG
        
        
        //m_ogl_blitter.borrow_surface(m_surface); // TODO: uncomment
        m_ogl_blitter.render_to_screen();
    }

    // draw ceiling/floor poly
	void texture_horizontal_polygon(polygon_definition& textured_polygon) override;
	
    // draw wall (side) poly
    void texture_vertical_polygon(polygon_definition& textured_polygon) override;

    // draw sprite (monsters, items; not sure about WIH)
	void texture_rectangle(rectangle_definition& textured_rectangle) override;
    
    
private:
    
    bitmap_definition* screen_buffer() { return reinterpret_cast<bitmap_definition*>(m_pixel_buffer.data()); }
    
    void darken(); // draw 1px black dither effect over gameworld when game is paused; must be within begin+end calls
    
    
    void clear()
    {
        m_pixel_buffer.clear(); // the data is borrowed from m_surface
        SDL_FreeSurface(m_surface);
    }
    
    // A [bitmap_definition][row pointer array] buffer (can be empty); this borrows the Surface's pixel buffer
    std::vector<uint8_t> m_pixel_buffer; // TODO: what is the point of this? why not get the Surface's pixels buffer directly?
    
    SDL_Surface* m_surface;
    
    ImageBlitter m_ogl_blitter; // transfers the drawn Surface to GPU texture (quick-n-lazy; now we use 2048x2048 as our max tile size it's more complicated than it needs to be for this job, but it'll do for now)
    
    void calculate_shading_table(void*& result, void* shading_tables, short depth, _fixed ambient_shade);
    
    void _prelandscape_horizontal_polygon_lines(polygon_definition* polygon, short y0, short* x0_table, short* x1_table, short line_count);

    template<int TEXBITS>
    void _pretexture_vertical_polygon_lines(polygon_definition *polygon, short x0, short *y0_table, short *y1_table, short line_count);
    
    template<int TEXBITS>
    void _pretexture_horizontal_polygon_lines(polygon_definition *polygon, short y0, short *x0_table, short *x1_table, short line_count);
};




void allocate_sw_texture_tables(); // called by initialize_marathon() in marathon2.cpp


#endif /* ClassicRasterizer_h */
