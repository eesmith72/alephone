/*
 ImageBlitter.hpp -- Draws an SDL_Surface to screen using OpenGL.
 
 - Used to draw splash and chapter screens, and main menu.
 
 - Also used to draw Lua HUD in Classic rendering mode.
 
 - Probably used for other things too, but need to work on that.
 
 Copyright (C) 2009 by Jeremiah Morris and the Aleph One developers
               OGL implementation written by Gregory Smith, 2006
 
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


// TODO: confirm this works as-is inside gameworld (to draw Lua HUD and terminal picts); we had some issues a few revisions back re. which OGL API calls to make when setting up and tearing down


#ifndef _IMAGE_BLITTER_
#define _IMAGE_BLITTER_

#include "cseries.h"

#include "ImageLoader.h"
#include "OGL_Headers.h"





// TODO: use SDL_Rect (ShapeBlitter still uses it)
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






class ImageBlitter
{
public:
    
    ImageBlitter(GLuint nearFilter = GL_LINEAR)
        : m_surface(nullptr), owns_surface(false), rotation(0.0), near_filter(nearFilter),
          tint_color_r(1.0), tint_color_g(1.0), tint_color_b(1.0), tint_color_a(1.0) {}
    
    virtual ~ImageBlitter() { unload(); }
    
    // important: when creating an ImageBlitter for a long-lived Surface that will be repeatedly drawn on, use `borrow_surface` to load it (do NOT use `take_surface` as that will free it on reload); then call `borrow_surface` to refresh the GPU textures after each update // TODO: implement a `refresh` method that clears the GPU textures only
    
    void take_surface(SDL_Surface* s); // this takes ownership of the surface
	void borrow_surface(SDL_Surface* s); // mash this in as we're using _Blitter to throw Canvas_SDL onto screen in the interim
    
	virtual void unload();
    
    bool has_surface() { return m_surface != nullptr; }
    
    
	int32_t width();
	int32_t height();
    
    // this renders to backbuffer and [should] set flag requesting screen swap on next screen update; caller shouldn't call current_screen.swap
    virtual void render_to_screen(const SDL_Rect* dst = nullptr, const SDL_Rect* src = nullptr);
    
    SDL_Surface* get_surface() { return m_surface; } // Classic render might use this, but it's probably easier for it to call render_to_screen to draw the HUD into its rect first, then blit the world_pixels surface into its rect on top (will need to check if pixel smearing is needed at boundaries)
    
	// TODO: get rid of this crap
	// tint the output image -- (1, 1, 1, 1) is untinted
	float tint_color_r, tint_color_g, tint_color_b, tint_color_a;
	
	// rotate the output image about the center of destination rect (in degrees clockwise)
	float rotation;
	
	// set default cropping rectangle
	//Image_Rect crop_rect;
    
    static void unload_all();
	
protected:
	SDL_Surface *m_surface;
    bool owns_surface;
    
    virtual void load(SDL_Surface* surface, bool own_it);
    
    // OGL
    
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

#endif
