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
 */


#ifndef Canvas_hpp
#define Canvas_hpp

#include "cseries.h"

#include "fonts.hpp"

#include "Image_Blitter.h"
#include "Shape_Blitter.h"


#define OUTLINE_THICKNESS (1)


// ideally a Canvas is instantiated with 1:1 relationship between Surface and screen pixels, avoiding scaling

// TODO: may want a CanvasAdapter for translating screen coords and scaling



class Canvas
{
public:
    Canvas() : m_drawing(false) {}
    ~Canvas() {}
    
    enum class mask_mode : int32_t // int-compatible for Lua bridging
    {
        disabled,
        enabled,
        drawing,
        erasing,
    };
    
    int32_t w, h;
    
    virtual void start_draw();
    virtual void end_draw();
    
    virtual void set_clip(const SDL_Rect& rect);
    virtual SDL_Rect get_clip();
    virtual void clear_clip();


    void set_masking_mode(mask_mode masking_mode); // TODO: rename set_mask_mode
    mask_mode masking_mode() { return m_masking_mode; } // TODO: rename get_mask_mode
    virtual void clear_mask();

    virtual void clear(const SDL_Color& color = {0x00, 0x00, 0x00, 0xff}) = 0;
    
    virtual void draw_filled_rect(const SDL_Rect& rect, const SDL_Color& color) = 0;
    
    virtual void draw_outlined_rect(const SDL_Rect& rect, const SDL_Color& color, int32_t line_weight = OUTLINE_THICKNESS) = 0;
    
    virtual void draw_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect) = 0; // TODO: optional justify?
    
    virtual void draw_image(Image_Blitter* image, const SDL_Point& point) = 0;
    
    virtual void draw_shape(Shape_Blitter* shape, const SDL_Point& point) = 0;
    
    virtual void draw_surface(SDL_Surface* surface, const SDL_Rect& rect) = 0; // TODO: optional src_rect?
    
    virtual void draw_styled_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect)
    {
        draw_text(text, font, color, rect); // TODO: implement style support
    }

    
    void draw_polygon(int16_t vertex_count, const int16* vertices, const SDL_Color& color); // TODO: vertices was int16*, presumably [x0,y0,x1,y1,...] with max length 16; std::array<SDL_Point,8> might be nicer, caveat shorter lists must be terminated by -1 (or whatever is currently used to indicate end of C array)

    void draw_line(const int16_t* vertices, const SDL_Color& color, short line_weight);

    void draw_circle(const SDL_Point& center, const SDL_Color& color, int16_t radius); // center was world_point2d; previously could draw circle OR square, but fill_rect already does squares

    void draw_triangle(const SDL_Point& center, angle facing, const SDL_Color& color, short shrink, short front, short rear, short rear_theta); // isoceles triangle (player)

    void set_path_drawing(const SDL_Color& color);

    void draw_path(short step/* 0 = first point */, world_point2d &location); // presumably used by draw_line/polygon?
    
protected:
    bool m_drawing; // TODO: is there any point to this?
    SDL_Rect m_clip_rect;
    mask_mode m_masking_mode; // TODO: what is point to this?
    
    virtual void apply_clip();
    
    // OGL overrides
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
