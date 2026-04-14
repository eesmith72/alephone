/*
 Canvas.hpp - decouples the drawing of high-level 2D elements (HUD, terminal, etc) from SDL/OGL APIs
 
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
 
 --
 
 EES: Here lies what remains of HUDRenderer_Lua.cpp as it's stripped and repurposed
 to make a general-purpose drawing API, properly decoupled from SW/OGL/whatever rendering.
 
 Currently, Canvas_SDL implements [some of] the drawing API needed for dialogs, automap,
 and terminal. Calling render_to_screen lazily instantiates an SDL/OGL Blitter which
 copies the Surface contents to GPU Texture[s], which can then be drawn to off-screen
 video buffer by SDL2/OpenGL APIs.
 
 (Note: what AO calls 'software rendering' runs on SDL_Renderer, which is also GPU-based and
 may use OGL as its own backend. However, SDL_Renderer cannot be used while OGL APIs are
 in use, and vice-versa. Replacing OGL someday with SDL3's SDL_gpu will be Nice.)
 
 Canvas_OGL should absorb OGL_RenderLine/Fill/etc functions from RenderMain/OGL_Render.h,
 which draw directly to screen buffer, eliminating AO's original baroque rendering pathway
 with its myriad Surface-to-Surface blits, so it generally won't use SDL_Surface or Blitter.
 The one exception is text, which will use TTF_RenderUTF8_Blended to create SDL_Surfaces
 and Blitter_OGL to transfer them to GPU textures for compositing; because that works and
 we have much better things to do in life than teach LP's OGL_RenderText Unicode.
 */


#ifndef Canvas_hpp
#define Canvas_hpp

#include "cseries.h"

#include "fonts.hpp"



struct Blitter;
struct Shape_Blitter;


#define OUTLINE_THICKNESS (1)


// ideally a Canvas is instantiated with 1:1 relationship between Surface and screen pixels, avoiding scaling (presumably line_thickness is treated as 1px on 640x480)

// TODO: how best to integrate resize_surface? (it's best to draw at 1:1 to screen, multiplying coords, line thicknesses, and font sizes automatically; when blitting lower-resolution surfaces, e.g. original M2 main menu + chapter screens, ideally the scaling would integrate into the blitting, avoiding need for intermediate surface; alternatively, use the existing scaling code for now and once we move to SDL3 it has a SDL_BlitSurfaceScaled that hopefully has decent linear quality; )



class Canvas
{
public:
    Canvas(int32_t w, int32_t h) : w(w), h(h), m_drawing(false) {}
    virtual ~Canvas() { unload(); }
    
    virtual void unload() {}
    
    enum class mask_mode : int32_t // int-compatible for Lua bridging
    {
        disabled,
        enabled,
        drawing,
        erasing,
    };
    
    int32_t w, h;
    
    virtual void start_draw() = 0;
    virtual void end_draw() = 0;
    
    virtual void set_clip(const SDL_Rect& rect) = 0;
    virtual SDL_Rect get_clip() = 0;
    virtual void clear_clip() = 0;


    void set_masking_mode(mask_mode masking_mode); // TODO: seriously wondering what the point of this is: the ONLY thing that calls it is the lua_hud_objects' Lua_Screen_Set_Masking_Mode
    mask_mode masking_mode() { return m_masking_mode; } // TODO: ditto

    virtual void clear(const SDL_Color& color = {0x00, 0x00, 0x00, 0xff}) = 0;
    
    virtual void draw_filled_rect(const SDL_Rect& rect, const SDL_Color& color) = 0;
    
    virtual void draw_outlined_rect(const SDL_Rect& rect, const SDL_Color& color, int32_t line_weight = OUTLINE_THICKNESS) = 0;
    
    virtual void draw_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect) = 0; // TODO: optional justify?
    
    virtual void draw_image(Blitter* image, const SDL_Point& point) = 0;
    
    virtual void draw_shape(Shape_Blitter* shape, const SDL_Point& point) = 0;
    
    virtual void draw_surface(SDL_Surface* surface, const SDL_Rect& rect) = 0;
    
    virtual void draw_surface(SDL_Surface* shape, const SDL_Rect& dst_rect, const SDL_Rect& src_rect) = 0;
    
    
    // TODO: these methods still need implemented, and lua_hud_class, OverheadMapRenderer, dialogs updated to use them. While the SW/HW gameworld renderers won't use Canvas or Blitter themselves, it should be practical to use them to produce enhancements such as live terminal screens, signage and decals, and anything else modders want to throw into the HW-rendered world as a Lua-drawn wall texture or sprite.
    
    virtual void draw_styled_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect)
    {
        draw_text(text, font, color, rect); // TODO: implement style support
    }
    
    /*
    void draw_polygon(int16_t vertex_count, const int16* vertices, const SDL_Color& color); // TODO: vertices was int16*, presumably [x0,y0,x1,y1,...] with max length 16; std::array<SDL_Point,8> might be nicer, caveat shorter lists must be terminated by -1 (or whatever is currently used to indicate end of C array)

    void draw_line(const int16_t* vertices, const SDL_Color& color, short line_weight);

    void draw_circle(const SDL_Point& center, const SDL_Color& color, int16_t radius); // center was world_point2d; previously could draw circle OR square, but fill_rect already does squares

    void draw_triangle(const SDL_Point& center, angle facing, const SDL_Color& color, short shrink, short front, short rear, short rear_theta); // isoceles triangle (player)

    void set_path_drawing(const SDL_Color& color);

    void draw_path(short step, world_point2d &location); // step 0 = first point; presumably used by draw_line/polygon? smells nasty and stateful
    */
    
    virtual void render_to_screen(const SDL_Rect* dst_rect = nullptr, const SDL_Rect* src_rect = nullptr) = 0;
    
protected:
    bool m_drawing; // TODO: is there any point to this? (the direct-draw OGL code uses it but I’m really tempted to sack that off and always use SDL_Canvas; I think the main demand is automap, which is probably a bit heavy for SDL)
    SDL_Rect m_clip_rect;
    mask_mode m_masking_mode; // TODO: what is point to this?
    
    // called by set_masking_mode; Canvas_OGL implements these, though it'd be simpler if it just override set_masking_mode
    virtual void start_using_mask() {}
    virtual void end_using_mask() {}
    virtual void start_drawing_mask(bool erase) {}
    virtual void end_drawing_mask() {}
    
    /*
    // these are methods on SW renderer, because idiot
    void DrawShape(shape_descriptor shape, screen_rectangle *dest, screen_rectangle *src) override {}
    void DrawShapeAtXY(shape_descriptor shape, short x, short y, bool transparency = false) override {}
    void DrawText(const std::string& text, screen_rectangle *dest, short flags, short font_id, short text_color) override {}
    void FillRect(screen_rectangle *r, short color_index) override {}
    void FrameRect(screen_rectangle *r, short color_index) override {}

    void DrawTexture(shape_descriptor texture, short texture_type, short x, short y, int size) override {}

    void SetClipPlane(int x, int y, int c_x, int c_y, int radius) override {}
    void DisableClipPlane(void) override {}

    int TextWidth(const std::string&, short) override;
     */
};



#endif /* Canvas_hpp */
