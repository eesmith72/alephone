/*
 classic_renderer.cpp -- "screen" buffer for the Classic M2 software renderer

 
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


#ifndef classic_renderer_hpp
#define classic_renderer_hpp

#include "cseries.h"

#include "textures.h"


class ClassicRenderer_ScreenBuffer // was `bitmap_definition_buffer` but it's only used as the "screen" buffer for the M2 SW renderer
{
private:
    // A [bitmap_definition][row pointer array] buffer (can be empty)
    std::vector<uint8_t> buffer;
    
    SDL_Surface* m_surface;
    
    ImageBlitter blitter; // TODO: interim (since an <=800x600 Surface fits into a single 2048x2048 OGL texture, we can streamline render_to_screen to make its own OGL calls later)
    
public:
    ClassicRenderer_ScreenBuffer() : m_surface(nullptr) {}
    
    void configure(int32_t w, int32_t h, int32_t bit_depth)
    {
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
                throw_bug_report_f("ClassicRenderer_ScreenBuffer.configure received bad bit depth: %d", bit_depth);
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
    
    bool empty() const { return buffer.empty(); } // TODO: why would it be empty?
    
    int row_count() const { return m_surface->h; }
    
    SDL_Surface* get_surface() { return m_surface; } // TODO: don't expose this if we don't have to
    
    
    
    void fill(const SDL_Color* color = nullptr)
    {
        if (color)
            SDL_FillRect(m_surface, nullptr, SDL_MapRGB(m_surface->format, color->r, color->g, color->b));
        else
            SDL_FillRect(m_surface, nullptr, SDL_MapRGB(m_surface->format, 0, 0, 0));
    }
    
    void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr) // bodgy, but it's a step in the right direction
    {
        blitter.borrow_surface(m_surface);
        blitter.render_to_screen(dst, src);
    }

    const SDL_PixelFormat* get_format() { return m_surface ? m_surface->format : nullptr; }
    
    bitmap_definition* get_buffer() { return empty() ? nullptr : reinterpret_cast<bitmap_definition*>(buffer.data()); }
    
    bitmap_definition const* get() const { return empty() ? nullptr : reinterpret_cast<const bitmap_definition*>(buffer.data()); }
    
    
    void clear() // TODO: actually not clear: who owns which memory?
    {
        buffer.clear();
        SDL_FreeSurface(m_surface);
    }
};



extern ClassicRenderer_ScreenBuffer classic_renderer_buffer;



#endif /* classic_renderer_hpp */
