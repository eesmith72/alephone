/*
IMAGE_BLITTER.CPP
 
    Copyright (C) 2009 by Jeremiah Morris and the Aleph One developers

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

    Implements images for 2D UI
*/

#include "Blitter.hpp"

#include "resource_manager.h"
#include "images.h"
#include "screen.h" // sw_render_surface_to_screen

#include "sdl_resize.h"


// TODO: AFAIK there's no way to determine if an existing Surface's pixel data has been edited since the SDL_Texture was created so a Blitter cannot auto-update its Texture[s] once they've been created. Once Blitter becomes the rendering backend to Canvas we'll need to decide how Surfaces are shared (especially a temporary drawing Surface obtained from a locked Texture) and if/when the Canvas should reload it into the Blitter after changes. How to manage Canvas/Surface and Blitter/Texture lifetimes can be figured out once screen.cpp is overhauled to separate SW and OGL renderers cleanly, and to handle window size/bit depth/renderer changes sensibly (i.e. screen.cpp should notify its dependents when they need to update/reset, instead of constant polling)



// TODO: tint_color_ is only implemented on Blitter_OGL, which is unhelpful; ditto rotation; not going to worry about it right now as Blitter_SDL should go away in favor of drawing directly to Window's borrowed Surface in SW rendering mode
Blitter::Blitter() : m_surface(nullptr), owns_surface(false), tint_color_r(1.0), tint_color_g(1.0), tint_color_b(1.0), tint_color_a(1.0), rotation(0.0)
{
	//m_src.x = m_src.y = m_src.w = m_src.h = 0;
	//m_scaled_src.x = m_scaled_src.y = m_scaled_src.w = m_scaled_src.h = 0;
	//crop_rect.x = crop_rect.y = crop_rect.w = crop_rect.h = 0;
}


Blitter::~Blitter()
{
    unload();
}


void Blitter::load(SDL_Surface* surface, bool own_it)
{
    unload();
    
    m_surface = surface;
    owns_surface = own_it;
    
    // when blitting surface, make sure we copy rather than blend the alpha // TODO: probably want to blend now
    SDL_SetSurfaceBlendMode(m_surface, SDL_BLENDMODE_NONE);
}


void Blitter::take_surface(SDL_Surface* surface)
{
    load(surface, true);
}

void Blitter::borrow_surface(SDL_Surface* surface)
{
    load(surface, false);
}


void Blitter::unload()
{
    if (owns_surface) { SDL_FreeSurface(m_surface); }
    m_surface = nullptr;
}


int32_t Blitter::width()
{
    return m_surface ? m_surface->w : 0;
}


int32_t Blitter::height()
{
	return m_surface ? m_surface->h : 0;
}


