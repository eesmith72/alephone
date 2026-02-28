/*
	SCREEN_DRAWING.C

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

#include "cseries.h"

#include "map.h"
#include "interface.h"
#include "shell.h"
#include "screen_drawing.h"
#include "fades.h"
#include "screen.h"

// LP addition: color and font parsers
#include "FontRenderer_OGL.h"

#include "FontRenderer_SDL.hpp"

#include <SDL2/SDL_ttf.h>
#include "preferences.h"

#define clutSCREEN_COLORS 130
#define finfFONTS 128

extern TextSpec *_get_font_spec(short font_index);

/*
struct interface_FontRenderer_SDL 
{
	TextSpec fonts[NUMBER_OF_INTERFACE_FONTS];
	short heights[NUMBER_OF_INTERFACE_FONTS];
	short line_spacing[NUMBER_OF_INTERFACE_FONTS];
};
*/

/* --------- Globals. */
// LP change: hardcoding this quantity since we know how many we need
// Putting in the Moo definitions
static screen_rectangle interface_rectangles[NUMBER_OF_INTERFACE_RECTANGLES] = 
{
	{326, 300, 338, 473},
	{464, 398, 475, 578},
	{464, 181, 475, 361},
	{338, 17, 0, 0},
	{0, 0, 0, 0},
	{352, 204, 454, 384},
	{352, 384, 454, 596},
	{179, 101, 210, 268},
	{221, 25, 253, 238},
	{263, 11, 294, 223},
	{301, 38, 333, 236},
	{304, 421, 331, 563},
	{386, 231, 413, 406},
	{345, 363, 372, 516},
	{344, 83, 374, 271},
	{206, 246, 347, 382},
	// {264, 522, 291, 588}, // inf's bounds
	// {263, 497, 294, 565}, // m2's bounds
	{263, 500, 294, 585}, // adjusted to work with both m2 and inf
      	{0,0,0,0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 320, 640},
	{0, 0, 18, 640},
	{302, 0, 320, 640},
	{27, 72, 293, 568},
	{27, 9, 293, 316},
	{27, 324, 293, 631},
	{27, 9, 293, 631},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0}
};

void set_about_alephone_rect(int width, int height)
{
	if (!width || !height) return;

	interface_rectangles[_about_alephone_rect].top = 480 - height;
	interface_rectangles[_about_alephone_rect].left = 640 - width;
	interface_rectangles[_about_alephone_rect].bottom = 480;
	interface_rectangles[_about_alephone_rect].right = 640;
}

// static screen_rectangle *interface_rectangles;
// static CTabHandle screen_colors;
// LP change: now hardcoded and XML-changeable

// Copied off of original 'finf' resource
// static struct interface_FontRenderer_SDL interface_fonts = 
FontRenderer_OGL InterfaceFonts[NUMBER_OF_INTERFACE_FONTS] =
{
	{"Monaco",   9, styleBold,  0, "#4"},
	{"Monaco",   9, styleBold,  0, "#4"},
	{"Monaco",   9, styleBold,  0, "#4"},
	{"Monaco",   9, styleNormal,0, "#4"},
	{"Courier", 12, styleNormal,0, "#22"},
	{"Courier", 14, styleBold,  0, "#22"},
	{"Monaco",   9, styleNormal,0, "#4"}
};

// LP change: hardcoding the interface and player colors,
// so as to banish the 'clut' resources
const int NumInterfaceColors = 26;
static rgb_color InterfaceColors[NumInterfaceColors] = 
{
	{0, 65535, 0},
	{0, 5140, 0},
	{0, 0, 0},
	
	{0, 65535, 0},
	{0, 12956, 0},
	{0, 5100, 0},
	
	{9216, 24320, 41728},
	{65535, 0, 0},
	{45056, 0, 24064},
	{65535, 65535, 0},
	{60000, 60000, 60000},
	{62976, 22528, 0},
	{3072, 0, 65535},
	{0, 65535, 0},
	
	{65535, 65535, 65535},
	{0, 5140, 0},
	
	{10000, 0, 0},
	{65535, 0, 0},
	
	{0, 65535, 0},
	{65535, 65535, 65535},
	{65535, 0, 0},
	{0, 40000, 0},
	{0, 45232, 51657},
	{65535, 59367, 0},
	{45000, 0, 0},
	{3084, 0, 65535}
};

/* ------- Private prototypes */
static void load_interface_rectangles(void);
static void	load_screen_interface_colors(void);

/* -------- Code */
void initialize_screen_drawing(
	void)
{
	short loop;

	/* Load the rectangles */
	load_interface_rectangles();
	
	/* Load the colors */
	load_screen_interface_colors();
	
	/* load the font stuff. */
	for(loop=0; loop<NUMBER_OF_INTERFACE_FONTS; ++loop)
	{
		InterfaceFonts[loop].Init();
	}
}

screen_rectangle *get_interface_rectangle(short index)
{
	assert_fail(index>=0 && index<NUMBER_OF_INTERFACE_RECTANGLES, "");
	return interface_rectangles + index;
}

rgb_color &get_interface_color(short index)
{
	assert_fail(index>=0 && index<NumInterfaceColors, "");
	return InterfaceColors[index];
}

FontRenderer_OGL &get_interface_font(short index)
{
	assert_fail(index >= 0 && index < NUMBER_OF_INTERFACE_FONTS, "");
	return InterfaceFonts[index];
}



// Global variables
SDL_Surface *draw_surface = NULL;	// Target surface for drawing commands
static SDL_Surface *old_draw_surface = NULL;


// Gets interface font and style;
// used in computer_interface.cpp
extern FontRenderer_SDL *GetInterfaceFont(short font_index);
extern uint16 GetInterfaceStyle(short font_index);

bool draw_clip_rect_active = false;			// Flag: clipping rect active
screen_rectangle draw_clip_rect;			// Current clipping rectangle; externed by images.cpp and FontRenderer_SDL.cpp

// From screen_sdl.cpp
extern SDL_Surface *world_pixels, *HUD_Buffer, *Term_Buffer, *Intro_Buffer, *Map_Buffer;
extern bool intro_buffer_changed;

// Prototypes
extern TextSpec *_get_font_spec(short font_index);


/*
 *  Redirect drawing to screen or offscreen buffer
 */

void _set_port_to_screen_window(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = MainScreenSurface();
}

void _set_port_to_gworld(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = world_pixels;
}

void _set_port_to_HUD(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = HUD_Buffer;
}

void _restore_port(void)
{
	draw_surface = old_draw_surface;
	old_draw_surface = NULL;
}

void _set_port_to_term(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = Term_Buffer;
}

void _set_port_to_intro(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = Intro_Buffer;
	intro_buffer_changed = true;
}

void _set_port_to_map(void)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = Map_Buffer;
}

void _set_port_to_custom(SDL_Surface *surface)
{
	assert_fail(old_draw_surface == NULL, "");
	old_draw_surface = draw_surface;
	draw_surface = surface;
}

/*
 *  Set clipping rectangle
 */

void set_drawing_clip_rectangle(short top, short left, short bottom, short right)
{
	if (top < 0)
		draw_clip_rect_active = false;
	else {
		draw_clip_rect_active = true;
		draw_clip_rect.top = top;
		draw_clip_rect.left = left;
		draw_clip_rect.bottom = bottom;
		draw_clip_rect.right = right;
	}
}





/*
 *  Draw text
 */

// Draw single glyph at given position in frame buffer, return glyph width
template <class T>
inline static int draw_glyph(uint8 c, int x, int y, T *p, int pitch, int clip_left, int clip_top, int clip_right, int clip_bottom, uint32 pixel, const FontRenderer_SDL_Pixmap *font, bool oblique)
{

	int cpos = c - font->first_character;

	// Calculate source and destination pointers (kerning, ascent etc.)
	uint8 *src = font->pixmap + font->location_table[cpos];
	int width = font->location_table[cpos + 1] - font->location_table[cpos];
	int height = font->rect_height;
	int advance = font->width_table[cpos * 2 + 1];
	y -= font->ascent;
	x += font->maximum_kerning + font->width_table[cpos * 2];
	p += y * pitch / sizeof(T) + x;
	if (oblique)
		p += font->ascent / 2 - 1;

	// Clip on top
	if (y < clip_top) {
		height -= clip_top - y;
		if (height <= 0)
			return advance;
		p += (clip_top - y) * pitch / sizeof(T);
		src += (clip_top - y) * font->bytes_per_row;
	}

	// Clip on bottom
	if (y + height - 1 > clip_bottom) {
		height -= y + height - 1 - clip_bottom;
		if (height <= 0)
			return advance;
	}

	// Clip on left
	if (x < clip_left) {
		width -= (clip_left - x);
		if (width <= 0)
			return advance;
		p += (clip_left - x);
		src += (clip_left - x);
	}

	// Clip on right
	if (x + width - 1 > clip_right) {
		width -= x + width - 1 - clip_right;
		if (width <= 0)
			return advance;
	}

	// Blit glyph to screen
	for (int iy=0; iy<height; iy++) {
		for (int ix=0; ix<width; ix++) {
			if (src[ix])
				p[ix] = pixel;			
		}
		if (oblique && (iy % 2) == 1)
			p--;
		src += font->bytes_per_row;
		p += pitch / sizeof(T);
	}

	return advance;
}

// Draw text at given position in frame buffer, return width
template <class T>
inline static int draw_text(const uint8 *text, size_t length,
                            int x, int y, T *p, int pitch, int clip_left, int clip_top, int clip_right, int clip_bottom,
                            uint32 pixel, const FontRenderer_SDL_Pixmap *font, uint16 style)
{
	bool oblique = ((style & styleItalic) != 0);
	int total_width = 0;

	uint8 c;
	while (length--) {
		c = *text++;
		if (c < font->first_character || c > font->last_character)
			continue;

                int width;

		width = draw_glyph(c, x, y, p, pitch, clip_left, clip_top, clip_right, clip_bottom, pixel, font, oblique);
		if (style & styleBold) {
			draw_glyph(c, x + 1, y, p, pitch, clip_left, clip_top, clip_right, clip_bottom, pixel, font, oblique);
			width++;
		}
		if (style & styleUnderline) {
			for (int i=0; i<width; i++)
				p[y * pitch / sizeof(T) + x + i] = pixel;
		}

		total_width += width;
		x += width;
	}
	return total_width;
}







static void draw_text(const char *text, int x, int y, uint32 pixel, const FontRenderer_SDL *font, uint16 style)
{
	draw_text(draw_surface, text, x, y, pixel, font, style);
}	




void screen_drawing___draw_screen_text(const std::string& text, screen_rectangle *destination, short flags, short font_id, short text_color)
{
    TODO("fucking char width");

    /*
    int x, y;
    // Find font information
    assert_fail(font_id >= 0 && font_id < NUMBER_OF_INTERFACE_FONTS, "");
    uint16 style = InterfaceFonts[font_id].Style;
    const FontRenderer_SDL *font = InterfaceFonts[font_id].Info;
    if (font == NULL)
        return;
    
     // Get color
     
     // Check for wrapping, and if it occurs, be recursive
     if (flags & _wrap_text) {
     int last_non_printing_character = 0, text_width = 0;
     unsigned count = 0;
     auto len = text.size(); // wrong, obvs
     
     
     while (count < len && text_width < RECTANGLE_WIDTH(destination)) {
     text_width += char_width_muckroman(text_to_draw[count], font, style);
     if (text_to_draw[count] == ' ')
     last_non_printing_character = count;
     count++;
     }
     * /
     
     if( count != len) {
     char remaining_text_to_draw[256];
     screen_rectangle new_destination;
     
     // If we ever have to wrap text, we can't also center vertically. Sorry.
     flags &= ~_center_vertical;
     flags |= _top_justified;
     
     // Pass the rest of it back in, recursively, on the next line
     memcpy(remaining_text_to_draw, text_to_draw + last_non_printing_character + 1, strlen(text_to_draw + last_non_printing_character + 1) + 1);
     
     new_destination = *destination;
     new_destination.top += InterfaceFonts[font_id].LineSpacing;
     screen_drawing___draw_screen_text(remaining_text_to_draw, &new_destination, flags, font_id, text_color);
     
     // Now truncate our text to draw
     text_to_draw[last_non_printing_character] = 0;
     }
     }
     
     // Truncate text if necessary
     int t_width = text_width(text_to_draw, font, style);
     if (t_width > RECTANGLE_WIDTH(destination)) {
     text_to_draw[trunc_text(text_to_draw, RECTANGLE_WIDTH(destination), font, style)] = 0;
     t_width = text_width(text_to_draw, font, style);
     }
     
     // Horizontal positioning
     if (flags & _center_horizontal)
     x = destination->left + (((destination->right - destination->left) - t_width) / 2);
     else if (flags & _right_justified)
     x = destination->right - t_width;
     else
     x = destination->left;
     
     // Vertical positioning
     int t_height = InterfaceFonts[font_id].Height;
     if (flags & _center_vertical) {
     if (t_height > RECTANGLE_HEIGHT(destination))
     y = destination->top;
     else {
     y = destination->bottom;
     int offset = RECTANGLE_HEIGHT(destination) - t_height;
     y -= (offset / 2) + (offset & 1) + 1;
     }
     } else if (flags & _top_justified) {
     if (t_height > RECTANGLE_HEIGHT(destination))
     y = destination->bottom;
     else
     y = destination->top + t_height;
     } else
     y = destination->bottom;
    
        SDL_Color color;
    _get_interface_color(text_color, &color);
    draw_text(draw_surface, text, x, y, SDL_MapRGB(draw_surface->format, color.r, color.g, color.b), font, style);
     */

}
 


static TextSpec NullSpec = {0, 0, 0};

TextSpec *_get_font_spec(short font_index)
{
	return &NullSpec;
}


// Sets current font to this index of interface font;
// used in computer_interface.cpp
FontRenderer_SDL *GetInterfaceFont(short font_index)
{
	assert_fail(font_index>=0 && font_index<NUMBER_OF_INTERFACE_FONTS, "");
	
	return static_cast<FontRenderer_SDL*>(InterfaceFonts[font_index].Info);
}


// Gets the current font style;
// used in computer_interface.cpp
uint16 GetInterfaceStyle(short font_index)
{
	assert_fail(font_index>=0 && font_index<NUMBER_OF_INTERFACE_FONTS, "");
	
	return InterfaceFonts[font_index].Style;
}


short _get_font_line_height(short font_id)
{
	assert_fail(font_id >= 0 && font_id < NUMBER_OF_INTERFACE_FONTS, "");
	return InterfaceFonts[font_id].LineSpacing;
}




/*
 *  Draw rectangle
 */

void _fill_rect(screen_rectangle *rectangle, short color_index)
{
	// Convert source rectangle
	SDL_Rect r;
	if (rectangle) {
		r.x = rectangle->left;
		r.y = rectangle->top;
		r.w = rectangle->right - rectangle->left;
		r.h = rectangle->bottom - rectangle->top;
	}

	// Get color
	SDL_Color color;
	_get_interface_color(color_index, &color);

	// Fill rectangle
	SDL_FillRect(draw_surface, rectangle ? &r : NULL, SDL_MapRGB(draw_surface->format, color.r, color.g, color.b));
	if (draw_surface == MainScreenSurface()) {
		if (rectangle)
			MainScreenUpdateRects(1, &r);
		else
			MainScreenUpdateRect(0, 0, 0, 0);
	}
}

void _fill_screen_rectangle(screen_rectangle *rectangle, short color_index)
{
	_fill_rect(rectangle, color_index);
}

void draw_rectangle(SDL_Surface *s, const SDL_Rect *rectangle, uint32 pixel)
{
	bool do_update = (s == MainScreenSurface());
	SDL_Rect r = {rectangle->x, rectangle->y, rectangle->w, 1};
	SDL_FillRect(s, &r, pixel);
	if (do_update)
		MainScreenUpdateRects(1, &r);
	r.y += rectangle->h - 1;
	SDL_FillRect(s, &r, pixel);
	if (do_update)
		MainScreenUpdateRects(1, &r);
	r.y = rectangle->y;
	r.w = 1;
	r.h = rectangle->h;
	SDL_FillRect(s, &r, pixel);
	if (do_update)
		MainScreenUpdateRects(1, &r);
	r.x += rectangle->w - 1;
	SDL_FillRect(s, &r, pixel);
	if (do_update)
		MainScreenUpdateRects(1, &r);
}

void _frame_rect(screen_rectangle *rectangle, short color_index)
{
	// Get color
	SDL_Color color;
	_get_interface_color(color_index, &color);
	uint32 pixel = SDL_MapRGB(draw_surface->format, color.r, color.g, color.b);

	// Draw rectangle
	SDL_Rect r = {rectangle->left, rectangle->top, rectangle->right - rectangle->left, rectangle->bottom - rectangle->top};
	draw_rectangle(draw_surface, &r, pixel);
}

void _erase_screen(short color_index)
{
	_fill_rect(NULL, color_index);
}


/*
 *  Draw line
 */

static inline uint8 cs_code(const world_point2d *p, int clip_top, int clip_bottom, int clip_left, int clip_right)
{
	uint8 code = 0;
	if (p->x < clip_left)
		code |= 1;
	if (p->x > clip_right)
		code |= 2;
	if (p->y < clip_top)
		code |= 4;
	if (p->y > clip_bottom)
		code |= 8;
	return code;
}

template <class T>
static inline void draw_thin_line_noclip(T *p, int pitch, const world_point2d *v1, const world_point2d *v2, uint32 pixel)
{
	int xdelta = v2->x - v1->x;
	int ydelta = v2->y - v1->y;

	if (abs(xdelta) > ydelta) {	// X axis is major axis
		int32 y = v1->y << 16;
		int32 delta = (xdelta == 0 ? 0 : (ydelta << 16) / xdelta);
		int x = v1->x;
		p += v1->x;
		if (xdelta < 0) {		// Line going left
			while (true) {
				p[(y >> 16) * pitch / sizeof(T)] = pixel;
				if (x == v2->x)
					break;
				x--;
				p--;
				y -= delta;
			}
		} else {
			while (true) {
				p[(y >> 16) * pitch / sizeof(T)] = pixel;
				if (x == v2->x)
					break;
				x++;
				p++;
				y += delta;
			}
		}
	} else {					// Y axis is major axis
		int32 x = v1->x << 16;
		int32 delta = (ydelta == 0 ? 0 : (xdelta << 16) / ydelta);
		p += v1->y * pitch / sizeof(T);
		int y = v1->y;
		while (true) {
			p[x >> 16] = pixel;
			if (y == v2->y)
				break;
			y++;
			x += delta;
			p += pitch / sizeof(T);
		}
	}
}

void draw_line(SDL_Surface *s, const world_point2d *v1, const world_point2d *v2, uint32 pixel, int pen_size)
{
	// Make line going downwards
	if (v1->y > v2->y) {
		const world_point2d *tmp = v1;
		v1 = v2;
		v2 = tmp;
	}

	if (pen_size == 1) {

		// Thin line, clip with Cohen/Sutherland and draw with DDA

    // Get clipping rectangle // TODO: this block is all over the place; refactor into shared function
    int clip_top, clip_bottom, clip_left, clip_right;
    if (draw_clip_rect_active) {
        clip_top = draw_clip_rect.top;
        clip_right = draw_clip_rect.right - 1;
        clip_bottom = draw_clip_rect.bottom - 1;
        clip_left = draw_clip_rect.left;
    } else {
        clip_top = clip_left = 0;
        clip_right = s->w - 1;
        clip_bottom = s->h - 1;
    }

		// Get codes for start/end points
		uint8 code1 = cs_code(v1, clip_top, clip_bottom, clip_left, clip_right);
		uint8 code2 = cs_code(v2, clip_top, clip_bottom, clip_left, clip_right);

		world_point2d clip_start, clip_end;

clip_line:
		if ((code1 | code2) == 0) {
		  if (SDL_MUSTLOCK(s)) {
		    if (SDL_LockSurface(s) < 0) return;
		  }

			// Line completely visible, draw it
			switch (s->format->BytesPerPixel) {
				case 1:
					draw_thin_line_noclip((pixel8 *)s->pixels, s->pitch, v1, v2, pixel);
					break;
				case 2:
					draw_thin_line_noclip((pixel16 *)s->pixels, s->pitch, v1, v2, pixel);
					break;
				case 4:
					draw_thin_line_noclip((pixel32 *)s->pixels, s->pitch, v1, v2, pixel);
					break;
			}
			if (SDL_MUSTLOCK(s)) {
			  SDL_UnlockSurface(s);
			}

		} else if ((code1 & code2) == 0) {

			// Line partially visible, clip it
#define clipx(p, clip, v, code) \
	p.y = v1->y + (v2->y - v1->y) * (clip - v1->x) / (v2->x - v1->x); \
	p.x = clip; \
	v = &p; \
	if (p.y < clip_top) \
		code = 4; \
	else if (p.y > clip_bottom) \
		code = 8; \
	else \
		code = 0;

#define clipy(p, clip, v, code) \
	p.x = v1->x + (v2->x - v1->x) * (clip - v1->y) / (v2->y - v1->y); \
	p.y = clip; \
	v = &p; \
	if (p.x < clip_left) \
		code = 1; \
	else if (p.x > clip_right) \
		code = 2; \
	else \
		code = 0;

			if (code1) {					// Clip start point
				if (code1 & 1) {			// Left
					clipx(clip_start, clip_left, v1, code1);
				} else if (code1 & 2) {		// Right
					clipx(clip_start, clip_right, v1, code1);
				} else {					// Top (bottom can't happen because the line goes downwards)
					clipy(clip_start, clip_top, v1, code1);
				}
			} else {			 			// Clip end point
				if (code2 & 1) {			// Left
					clipx(clip_end, clip_left, v2, code2);
				} else if (code2 & 2) {		// Right
					clipx(clip_end, clip_right, v2, code2);
				} else {					// Bottom (top can't happen because the line goes downwards)
					clipy(clip_end, clip_bottom, v2, code2);
				}
			}

			goto clip_line;
		}

	} else {

		// Thick line: to emulate the QuickDraw behaviour of moving a
		// rectangular pen along a line, we convert the line into a hexagon

		// Calculate hexagon points
		world_point2d hexagon[6];
		hexagon[0].x = v1->x - pen_size / 2;
		hexagon[1].x = hexagon[0].x + pen_size - 1;
		hexagon[0].y = hexagon[1].y = v1->y - pen_size / 2;
		hexagon[4].x = v2->x - pen_size / 2;
		hexagon[3].x = hexagon[4].x + pen_size - 1;
		hexagon[3].y = hexagon[4].y = v2->y - pen_size / 2 + pen_size - 1;
		if (v1->x > v2->x) {	// Line going to the left
			hexagon[2].x = hexagon[1].x;
			hexagon[2].y = hexagon[1].y + pen_size - 1;
			hexagon[5].x = hexagon[4].x;
			hexagon[5].y = hexagon[4].y - pen_size + 1;
			if (v1->x - v2->y > v2->y - v1->y)	// Pixels missing from polygon filler
				draw_line(s, hexagon + 0, hexagon + 5, pixel, 1);
		} else {				// Line going to the right
			hexagon[2].x = hexagon[3].x;
			hexagon[2].y = hexagon[3].y - pen_size + 1;
			hexagon[5].x = hexagon[0].x;
			hexagon[5].y = hexagon[0].y + pen_size - 1;
			if (v2->x - v1->y > v2->y - v1->y)	// Pixels missing from polygon filler
				draw_line(s, hexagon + 1, hexagon + 2, pixel, 1);
		}

		// Draw hexagon
		draw_polygon(s, hexagon, 6, pixel);
	}
}


/*
 *  Draw clipped, filled, convex polygon
 */

void draw_polygon(SDL_Surface *s, const world_point2d *vertex_array, int vertex_count, uint32 pixel)
{
	if (vertex_count == 0)
		return;

	// Reallocate temporary vertex lists if necessary
	static world_point2d *va1 = NULL, *va2 = NULL;
	static int max_vertices = 0;
	if (vertex_count > max_vertices) {
		delete[] va1;
		delete[] va2;
		va1 = new world_point2d[vertex_count * 2];	// During clipping, each vertex can become two vertices
		va2 = new world_point2d[vertex_count * 2];
		max_vertices = vertex_count;
	}

	// Get clipping rectangle
	int clip_top, clip_bottom, clip_left, clip_right;
	if (draw_clip_rect_active) {
		clip_top = draw_clip_rect.top;
        clip_right = draw_clip_rect.right - 1;
		clip_bottom = draw_clip_rect.bottom - 1;
		clip_left = draw_clip_rect.left;
	} else {
		clip_top = clip_left = 0;
		clip_right = s->w - 1;
		clip_bottom = s->h - 1;
	}

	// Clip polygon
	const world_point2d *v1, *v2;
	world_point2d *vp;
	world_point2d clip_point;
	int new_vertex_count;

#define clip_min(X, Y, clip, dst_array) \
	clip_point.Y = clip; \
	v1 = vertex_array + vertex_count - 1; \
	v2 = vertex_array; \
	vp = dst_array; \
	new_vertex_count = 0; \
	for (int i=0; i<vertex_count; i++, v1 = v2, v2++) { \
		if (v1->Y < clip) { \
			if (v2->Y < clip) { 		/* Edge completely clipped */ \
				continue; \
			} else {		 			/* Clipped edge going down, find clip point */ \
				clip_point.X = v1->X + (v2->X - v1->X) * (clip - v1->Y) / (v2->Y - v1->Y); \
				*vp++ = clip_point;		/* Add clip point to array */ \
				*vp++ = *v2;			/* Add visible endpoint to array */ \
				new_vertex_count += 2; \
			} \
		} else { \
			if (v2->Y < clip) {			/* Clipped edge going up, find clip point */ \
				clip_point.X = v2->X + (v1->X - v2->X) * (clip - v2->Y) / (v1->Y - v2->Y); \
				*vp++ = clip_point;		/* Add clip point to array */ \
				new_vertex_count++; \
			} else {					/* Edge completely visible, add endpoint to array */ \
				*vp++ = *v2; \
				new_vertex_count++; \
			} \
		} \
	} \
	vertex_count = new_vertex_count; \
	if (vertex_count == 0) \
		return;		/* Polygon completely clipped */ \
	vertex_array = dst_array;

#define clip_max(X, Y, clip, dst_array) \
	clip_point.Y = clip; \
	v1 = vertex_array + vertex_count - 1; \
	v2 = vertex_array; \
	vp = dst_array; \
	new_vertex_count = 0; \
	for (int i=0; i<vertex_count; i++, v1 = v2, v2++) { \
		if (v1->Y < clip) { \
			if (v2->Y < clip) {			/* Edge completely visible, add endpoint to array */ \
				*vp++ = *v2; \
				new_vertex_count++; \
			} else {		 			/* Clipped edge going down, find clip point */ \
				clip_point.X = v1->X + (v2->X - v1->X) * (clip - v1->Y) / (v2->Y - v1->Y); \
				*vp++ = clip_point;		/* Add clip point to array */ \
				new_vertex_count++; \
			} \
		} else { \
			if (v2->Y < clip) {			/* Clipped edge going up, find clip point */ \
				clip_point.X = v2->X + (v1->X - v2->X) * (clip - v2->Y) / (v1->Y - v2->Y); \
				*vp++ = clip_point;		/* Add clip point to array */ \
				*vp++ = *v2;			/* Add visible endpoint to array */ \
				new_vertex_count += 2; \
			} else {					/* Edge completely clipped */ \
				continue; \
			} \
		} \
	} \
	vertex_count = new_vertex_count; \
	if (vertex_count == 0) \
		return;		/* Polygon completely clipped */ \
	vertex_array = dst_array;

	clip_min(x, y, clip_top, va1);
	clip_max(x, y, clip_bottom, va2);
	clip_min(y, x, clip_left, va1);
	clip_max(y, x, clip_right, va2);

	// Reallocate span list if necessary
	struct span_t {
		int left, right;
	};
	static span_t *span = NULL;
	static int max_spans = 0;
	if (!span || s->h > max_spans) {
		delete[] span;
		span = new span_t[s->h];
		max_spans = s->h;
	}

	// Scan polygon edges and build span list
	v1 = vertex_array + vertex_count - 1;
	v2 = vertex_array;
	int xmin = INT16_MAX, xmax = INT16_MIN;
	int ymin = INT16_MAX, ymax = INT16_MIN;
	for (int i=0; i<vertex_count; i++, v1 = v2, v2++) {

		if (v1->x < xmin)	// Find minimum and maximum coordinates
			xmin = v1->x;
		if (v1->x > xmax)
			xmax = v1->x;
		if (v1->y < ymin)
			ymin = v1->y;
		if (v1->y > ymax)
			ymax = v1->y;

		int x1x2 = v1->x - v2->x;
		int y1y2 = v1->y - v2->y;

		if (y1y2 == 0)				// Horizontal edge
			continue;
		else if (y1y2 < 0) {		// Edge going down -> left span boundary
			int32 x = v1->x << 16;	// 16.16 fixed point
			int32 delta = (x1x2 << 16) / y1y2;
			for (int y=v1->y; y<=v2->y; y++) {
				span[y].left = x >> 16;
				x += delta;			// DDA line drawing
			}
		} else {					// Edge going up -> right span boundary
			int32 x = v2->x << 16;
			int32 delta = (x1x2 << 16) / y1y2;
			for (int y=v2->y; y<=v1->y; y++) {
				span[y].right = x >> 16;
				x += delta;			// Draw downwards to ensure that adjacent polygon fits perfectly
			}
		}
	}

	// Fill spans
	SDL_Rect r = {0, 0, 0, 1};
	for (int y=ymin; y<=ymax; y++) {
		int left = span[y].left, right = span[y].right;
		if (left == right)
			continue;
		else if (left < right) {
			r.x = left;
			r.y = y;
			r.w = right - r.x + 1;
		} else {
			r.x = right;
			r.y = y;
			r.w = left - r.x + 1;
		}
		SDL_FillRect(s, &r, pixel);
	}

	if (draw_surface == MainScreenSurface())
		MainScreenUpdateRect(xmin, ymin, xmax - xmin + 1, ymax - ymin + 1);
}


/*
 *  Interface color management
 */

void _get_interface_color(size_t color_index, SDL_Color *color)
{	
	assert_fail(color_index<NumInterfaceColors, "");
	
	rgb_color &c = InterfaceColors[color_index];
	color->r = c.red >> 8;
	color->g = c.green >> 8;
	color->b = c.blue >> 8;
	color->a = 0xff;
}

#define NUMBER_OF_PLAYER_COLORS 8

void _get_player_color(size_t color_index, RGBColor *color)
{
	assert_fail(color_index<NUMBER_OF_PLAYER_COLORS, "");

	rgb_color &c = InterfaceColors[color_index + PLAYER_COLOR_BASE_INDEX];
	color->red = c.red;
	color->green = c.green;
	color->blue = c.blue;
}

void _get_player_color(size_t color_index, SDL_Color *color)
{
    assert_fail(color_index<NUMBER_OF_PLAYER_COLORS, "");

    rgb_color &c = InterfaceColors[color_index + PLAYER_COLOR_BASE_INDEX];
    color->r = static_cast<Uint8>(c.red);
    color->g = static_cast<Uint8>(c.green);
    color->b = static_cast<Uint8>(c.blue);
    color->a = 0xff;
}

/*
 *  Rectangle XML parser
 */

static void load_interface_rectangles(void)
{
}

static void load_screen_interface_colors(void)
{
}

