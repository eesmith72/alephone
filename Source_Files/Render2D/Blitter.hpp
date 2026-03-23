/*
IMAGE_BLITTER.H

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

#ifndef _IMAGE_BLITTER_
#define _IMAGE_BLITTER_

#include "cseries.h"
//#include "ImageLoader.h"


#ifdef HAVE_OPENGL
#define new_Blitter() (ogl_is_active() ? (Blitter*)new Blitter_OGL() : new Blitter_SDL())
#else
#define new_Blitter() (Blitter*)new Blitter_OGL())
#endif


// TODO: use SDL_Rect
struct Image_Rect
{
	float x = 0, y = 0, w = 0, h = 0;
	
	Image_Rect() = default;
	
	explicit constexpr Image_Rect(float x, float y, float w, float h)
		: x(x), y(y), w(w), h(h) {}
	
	/*implicit*/ constexpr Image_Rect(SDL_Rect r)
		: x(r.x), y(r.y), w(r.w), h(r.h) {}
    
    explicit operator SDL_Rect() const { return {(int32_t)x, (int32_t)y, (int32_t)w,  (int32_t)h}; }
};


// TODO: make Blitter an abstract base class and subclass as Blitter_SDL and Blitter_OGL (or, better, Texture_SW and Texture_HW); Canvas_SDL can use either one to convert Surface into SDL/OGL Texture; this also gives us control of dst AND src rects in SW Renderer, which is where we should be doing our final compositing (e.g. overlaying HUD on world view)

class Blitter
{
public:
    Blitter();
    virtual ~Blitter();
	
    void take_surface(SDL_Surface* s); // this takes ownership of the surface
	void borrow_surface(SDL_Surface* s); // mash this in as we're using _Blitter to throw Canvas_SDL onto screen in the interim
    
	virtual void unload();
    
    bool has_surface() { return m_surface != nullptr; }
    
	int32_t width();
	int32_t height();
    
    // this renders to backbuffer and [should] set flag requesting screen swap (caller is also free to swap immediately if it doesn't want to wait on event loop to update screen); it does not swap itself
    virtual void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr) = 0;
    
    
	// TODO: get rid of this crap
	// tint the output image -- (1, 1, 1, 1) is untinted
	float tint_color_r, tint_color_g, tint_color_b, tint_color_a;
	
	// rotate the output image about the center of destination rect (in degrees clockwise)
	float rotation;
	
	// set default cropping rectangle
	//Image_Rect crop_rect;
	
protected:
	SDL_Surface *m_surface; // TODO: once Surface contents are copied to (GPU-hosted) Texture, there's no real need to retain the Surface so this may be unnecessary
    bool owns_surface;
    
    virtual void load(SDL_Surface* surface, bool own_it);
};

#endif
