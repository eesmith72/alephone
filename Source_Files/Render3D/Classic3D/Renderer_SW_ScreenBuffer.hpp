/*
 Renderer_SW_ScreenBuffer.cpp -- "screen" buffer for the Classic M2 software renderer

 
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

#include "ImageBlitter.hpp"

#include "textures.h"


class Renderer_SW_ScreenBuffer // was `bitmap_definition_buffer` but it's only used as the "screen" buffer for the M2 SW renderer
{
private:
    // A [bitmap_definition][row pointer array] buffer (can be empty)
    std::vector<uint8_t> buffer;
    
    SDL_Surface* m_surface;
    
    ImageBlitter blitter; // quick-n-easy (an <=800x600 Surface easily fits into a single 2048x2048 GPU texture so we could have render_to_screen below call OGL directly, but this'll do for now)
    
public:
    Renderer_SW_ScreenBuffer() : m_surface(nullptr) {}
    
    void configure(int32_t w, int32_t h, int32_t bit_depth);
    
    //int row_count() const { return m_surface->h; }
    
    bool empty() const { return buffer.empty(); } // TODO: why would it be empty?
    
    
    void begin() { SDL_LockSurface(m_surface); }
    
    void end() { SDL_UnlockSurface(m_surface); }
    

    const SDL_PixelFormat* get_format() const
    {
        assert_fail(m_surface, "");
        return m_surface->format;
    }
    
    bitmap_definition* get_buffer()
    {
        assert_fail(!buffer.empty(), "");
        return reinterpret_cast<bitmap_definition*>(buffer.data());
    }
    
    
    void darken(); // draw 1px black dither effect over gameworld when game is paused
    
    void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr) // bodgy, but it's a step in the right direction
    {
        blitter.borrow_surface(m_surface);
        blitter.render_to_screen(dst, src);
    }
    
    
    void clear()
    {
        buffer.clear(); // the data is borrowed from m_surface
        SDL_FreeSurface(m_surface);
    }
};



extern Renderer_SW_ScreenBuffer classic_renderer_buffer;



#endif /* classic_renderer_hpp */
