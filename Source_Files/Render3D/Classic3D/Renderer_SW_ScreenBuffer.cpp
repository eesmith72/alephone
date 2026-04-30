/*
 Renderer_SW_ScreenBuffer.cpp 
 
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

#include "Renderer_SW_ScreenBuffer.hpp"


Renderer_SW_ScreenBuffer classic_renderer_buffer;


void Renderer_SW_ScreenBuffer::configure(int32_t w, int32_t h, int32_t bit_depth)
{
    if (m_surface && m_surface->format->BytesPerPixel * 8 == bit_depth) return;
    
    SDL_FreeSurface(m_surface);
    
    switch (bit_depth)
    {
        case 8:
        {
            if (!(m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 8, 0, 0, 0, 0))) { exit(outOfMemory); }
            SDL_Color colors[256];
            build_sdl_color_table(world_color_table, colors); // world_color_table (defined in cluts.cpp) = Shapes file's 8-bit color palette
            SDL_SetPaletteColors(m_surface->format->palette, colors, 0, 256);
            break;
        }
            
        case 16:
            m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, pixel_format_16.Rmask, pixel_format_16.Gmask, pixel_format_16.Bmask, 0);
            break;
            
        case 32:
            m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, pixel_format_32.Rmask, pixel_format_32.Gmask, pixel_format_32.Bmask, 0);
            break;
            
        default:
            throw_bug_report_f("Renderer_SW_ScreenBuffer.configure received bad bit depth: %d", bit_depth);
    }
    
    if (m_surface)
    {
        buffer.resize(sizeof(bitmap_definition) + (m_surface->h - 1) * sizeof(pixel8*));
        
        bitmap_definition* def = get_buffer();
        def->width            = m_surface->w;
        def->height           = m_surface->h;
        def->bytes_per_row    = m_surface->pitch;
        def->flags            = 0;
        def->bit_depth        = m_surface->format->BitsPerPixel;
        def->row_addresses[0] = static_cast<pixel8*>(m_surface->pixels);
        precalculate_bitmap_row_addresses(def);
    }
    else
    {
        buffer.clear();
    }
}




void Renderer_SW_ScreenBuffer::darken()
{
    assert_fail(m_surface, "");
    
    uint8_t* p = (uint8_t*)m_surface->pixels;
    int32_t width = m_surface->w, height = m_surface->h;
    int32_t pixel_size = m_surface->format->BytesPerPixel;
    uint32_t black = SDL_MapRGB(m_surface->format, 0, 0, 0);
    int32_t pitch = m_surface->pitch;
    
    for (int32_t y = 0; y < height; y++)
    {
        for (int32_t x = y & 1; x < width; x += 2)
        {
            p[x] = black;
        }
        p += pitch / pixel_size;
    }
}
