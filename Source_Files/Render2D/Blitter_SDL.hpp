/*
 Blitter_SDL.hpp -- Convert SDL_Surface to SDL_Texture and render it on screen.
                    Used by the software renderer.
 
 Copyright (C) 2006 and beyond by Bungie Studios, Inc.
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

#ifndef Blitter_SDL_hpp
#define Blitter_SDL_hpp

#include "cseries.h"

#include "Blitter.hpp"


// TODO: this is unnecessary complexity; for SW rendering, Canvas_SDL should get a borrowed Surface from SDL_GetWindowSurface and do all drawing (including world view) into that, then call SDL_UpdateWindowSurface to put it on screen; this will also work better with 8-bit palette-based fades as SDL_Palette requires a Surface, as does AO's gamma adjustment function (though that needs modified to manipulate pixel values in-place instead of copying pixels to a new surface [I do wonder how JJ's original 8-bit renderer did gamma, though I'm guessing it adjusted hardware tables which are long gone])

// TODO: check how the original M2 scaled picts from 640x480 to 800x600; ideally Surface-based SW renderer should use the same algorithm


class Blitter_SDL : public Blitter
{
public:
    Blitter_SDL() : Blitter(), m_texture(nullptr) {}
    virtual ~Blitter_SDL() {}
    
    virtual void unload() override;
    
    void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr) override;
    
    
    // tint the output image -- (1, 1, 1, 1) is untinted
    //float tint_color_r, tint_color_g, tint_color_b, tint_color_a;
    
    // rotate the output image about the center of destination rect
    // (in degrees clockwise)
    //float rotation;
    
    // set default cropping rectangle
    //Image_Rect crop_rect;
    
protected:
    SDL_Texture *m_texture;
};



#endif /* Blitter_SDL_hpp */
