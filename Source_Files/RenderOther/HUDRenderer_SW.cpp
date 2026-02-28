/*

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

/*
 *  HUDRenderer_SW.cpp - HUD rendering using graphics functions from screen_drawing
 *
 *  Written in 2001 by Christian Bauer
 */

#include "HUDRenderer_SW.h"

#include "images.h"
#include "shell.h" // get_shape_surface!?
#include "Shape_Blitter.h"

#include "screen.h" // MainScreenSurface

extern bool MotionSensorActive;


/*
 *  Update motion sensor
 */

void HUD_SW_Class::update_motion_sensor(short time_elapsed)
{
	if (!(GET_GAME_OPTIONS() & _motion_sensor_does_not_work) && MotionSensorActive) {
		if (motion_sensor_has_changed() || time_elapsed == NONE) {
			render_motion_sensor(time_elapsed);
			ForceUpdate = true;
			screen_rectangle *r = get_interface_rectangle(_motion_sensor_rect);
			DrawShapeAtXY(BUILD_DESCRIPTOR(_collection_interface, _motion_sensor_mount), r->left, r->top);
		}
	}
}


/*
 *  Draw shapes
 */

extern SDL_Surface *draw_surface; // in screen_drawing.cpp


void HUD_SW_Class::DrawShape(shape_descriptor shape_id, screen_rectangle *destination, screen_rectangle *source)
{
    // Convert rectangles
    SDL_Rect src_rect;
    if (source) {
        src_rect.x = source->left;
        src_rect.y = source->top;
        src_rect.w = source->right - source->left;
        src_rect.h = source->bottom - source->top;
    }
    SDL_Rect dst_rect = {destination->left, destination->top, destination->right - destination->left, destination->bottom - destination->top};

    // Convert shape to surface
    SDL_Surface *s = get_shape_surface(shape_id);
    if (s == NULL)
        return;
    
//    if (draw_surface->format->BitsPerPixel == 8) {
//        // SDL doesn't seem to be able to handle direct blits between 8-bit surfaces with different cluts
//        SDL_Surface *s2 = SDL_DisplayFormat(s);
//        SDL_FreeSurface(s);
//        s = s2;
//    }
    
    // Blit the surface
    SDL_BlitSurface(s, source ? &src_rect : NULL, draw_surface, &dst_rect);
    if (draw_surface == MainScreenSurface()) MainScreenUpdateRects(1, &dst_rect);

    // Free the surface
    SDL_FreeSurface(s);
}





void HUD_SW_Class::DrawShapeAtXY(shape_descriptor shape_id, short x, short y, bool transparency)
{
	// "transparency" is only used for OpenGL motion sensor
    // Convert shape to surface
    SDL_Surface *s = get_shape_surface(shape_id);
    if (s == NULL)
        return;
    
//    if (draw_surface->format->BitsPerPixel == 8) {
//        // SDL doesn't seem to be able to handle direct blits between 8-bit surfaces with different cluts
//        SDL_Surface *s2 = SDL_DisplayFormat(s);
//        SDL_FreeSurface(s);
//        s = s2;
//    }
    
    // Setup destination rectangle
    SDL_Rect dst_rect = {x, y, s->w, s->h};

    // Blit the surface
    SDL_BlitSurface(s, NULL, draw_surface, &dst_rect);
    if (draw_surface == MainScreenSurface())
        MainScreenUpdateRects(1, &dst_rect);

    // Free the surface
    SDL_FreeSurface(s);
}

    

    
extern SDL_Surface *HUD_Buffer;

template <class T>
static void rotate(T *src_pixels, int src_pitch, T *dst_pixels, int dst_pitch, int width, int height)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			dst_pixels[x * dst_pitch + y] = src_pixels[y * src_pitch + x];
		}
	}
}

SDL_Surface *rotate_surface(SDL_Surface *s, int width, int height)
{
	if (!s) return 0;

	SDL_Surface *s2 = SDL_CreateRGBSurface(SDL_SWSURFACE, height, width, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);

	switch (s->format->BytesPerPixel) {
		case 1:
			rotate((pixel8 *)s->pixels, s->pitch, (pixel8 *)s2->pixels, s2->pitch, width, height);
			break;
		case 2:
			rotate((pixel16 *)s->pixels, s->pitch / 2, (pixel16 *)s2->pixels, s2->pitch / 2, width, height);
			break;
		case 4:
			rotate((pixel32 *)s->pixels, s->pitch / 4, (pixel32 *)s2->pixels, s2->pitch / 4, width, height);
			break;
	}

	if (s->format->palette)
		SDL_SetPaletteColors(s2->format->palette, s->format->palette->colors, 0, s->format->palette->ncolors);

	return s2;
}	

void HUD_SW_Class::DrawTexture(shape_descriptor shape, short texture_type, short x, short y, int size)
{
    Shape_Blitter b(
                    GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(shape)),
                    GET_DESCRIPTOR_SHAPE(shape),
                    texture_type,
                    GET_COLLECTION_CLUT(GET_DESCRIPTOR_COLLECTION(shape)));
    int w = b.Width();
    int h = b.Height();
    if (!w || !h) return;
    if (w >= h)
        b.Rescale(size, size * h / w);
    else
        b.Rescale(size * w / h, size);
    
    SDL_Rect r;
    r.w = b.Width();
    r.h = b.Height();
    r.x = x + (size - r.w)/2;
    r.y = y + (size - r.h)/2;
    b.SDL_Draw(HUD_Buffer, r);
}

/*
 *  Draw text
 */

void HUD_SW_Class::DrawText(const std::string& text, screen_rectangle *dest, short flags, short font_id, short text_color)
{
	screen_drawing___draw_screen_text(text, dest, flags, font_id, text_color);
}





#include "FontRenderer_OGL.h"
// in screen_drawing.cpp
extern FontRenderer_OGL InterfaceFonts[NUMBER_OF_INTERFACE_FONTS];

int HUD_SW_Class::TextWidth(const std::string& text, short font_id)
{
    // Find font information
    assert_fail(font_id >= 0 && font_id < NUMBER_OF_INTERFACE_FONTS, "");
    uint16 style = InterfaceFonts[font_id].Style;
    const FontRenderer_SDL *font = InterfaceFonts[font_id].Info;
    if (font == NULL)
        return 0;

    // Calculate width
    return text_width(text, font, style);
}




/*
 *  Fill rectangle
 */

void HUD_SW_Class::FillRect(screen_rectangle *r, short color_index)
{
	_fill_rect(r, color_index);
}


/*
 *  Frame rectangle
 */

void HUD_SW_Class::FrameRect(screen_rectangle *r, short color_index)
{
	_frame_rect(r, color_index);
}
