/*
 Blitter_OGL.hpp -- Convert SDL_Surface to GPU texture[s] for OpenGL rendering.
 
 written by Gregory Smith, 2006
 
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

#ifndef __blitter_ogl__
#define __blitter_ogl__


#include "Blitter.hpp"

#include "ImageLoader.h"
#include "OGL_Headers.h"


#ifdef HAVE_OPENGL

class Blitter_OGL : public Blitter
{
public:
    Blitter_OGL(GLuint nearFilter = GL_LINEAR) : Blitter(), near_filter(nearFilter) {}
    
    virtual ~Blitter_OGL() { unload(); }
    
    void unload() override;
    
    // note: if the Surface is subsequently modified, call take_surface/borrow_surface to reload it
    
    void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr) override;
    
    static void unload_all();
    
private:
    struct texture_tile_t
    {
        SDL_Rect rect;
        GLuint ref;
    };
    
    GLuint near_filter;
    std::vector<texture_tile_t> m_tiles;
    int32_t m_tile_width, m_tile_height;

    void create_texture_tiles();
};


#endif /* HAVE_OPENGL */

#endif /* __blitter_ogl__ */
