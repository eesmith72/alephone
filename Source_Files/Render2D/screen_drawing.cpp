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

#include "fonts.hpp"

#include "preferences.h"

#include "InfoTree.h"


// TODO: relocate HUD rects and computer terminal rects to their respective modules and get rid of this array

static const std::array<SDL_Rect, 31> interface_rectangles_std = {
    // M2 HUD rects
    300, 326, 173,  12, // _player_name_rect       = 0,
    398, 464, 180,  11, // _oxygen_rect            = 1,
    181, 464, 180,  11, // _shield_rect            = 2,
    17, 338, -17, -338, // _motion_sensor_rect     = 3,
      0,   0,   0,   0, // _microphone_rect        = 4,
    204, 352, 180, 102, // _inventory_rect         = 5,
    384, 352, 212, 102, // _weapon_display_rect    = 6,
    
    // main menu rects // TODO: no longer used; main_menu.cpp has its own config table now
    101, 179, 167,  31, // _new_game_button_rect       =  7,
     25, 221, 213,  32, // _load_game_button_rect      =  8,
     11, 263, 212,  31, // _gather_button_rect         =  9,
     38, 301, 198,  32, // _join_button_rect           = 10,
    421, 304, 142,  27, // _prefs_button_rect          = 11,
    231, 386, 175,  27, // _replay_last_button_rect    = 12,
    363, 345, 153,  27, // _save_last_button_rect      = 13,
     83, 344, 188,  30, // _replay_saved_button_rect   = 14,
    246, 206, 136, 141, // _credits_button_rect        = 15,
    500, 263,  85,  31, // _quit_button_rect           = 16, // adjusted to work with both m2 and inf
      0,   0,   0,   0, // _center_button_rect         = 17,
      0,   0,   0,   0, // _singleton_game_button_rect = 18,
    560, 440,  80,  40, // _about_alephone_rect        = 19,
    
    // computer terminal rects
      0,   0, 640, 320, // _terminal_screen_rect           = 20, // in M2, terminal view fully filled top two-thirds of 640x480 screen; in AO, is must adjust for widescreen and Lua HUD positions (but should presumably maintain the original 2:1 aspect ratio)
      0,   0, 640,  18, // _terminal_header_rect           = 21,
      0, 302, 640,  18, // _terminal_footer_rect           = 22,
     72,  27, 496, 266, // _terminal_full_text_rect        = 23,
      9,  27, 307, 266, // _terminal_left_rect             = 24,
    324,  27, 307, 266, // _terminal_right_rect            = 25,
      9,  27, 622, 266, // _terminal_logon_graphic_rect    = 26,
      0,   0,   0,   0, // _terminal_logon_title_rect      = 27,
      0,   0,   0,   0, // _terminal_logon_location_rect   = 28,
      0,   0,   0,   0, // _respawn_indicator_rect         = 29,
      0,   0,   0,   0, // _blinker_rect                   = 30,
};


static std::array<SDL_Rect, 31> interface_rectangles;


SDL_Rect get_interface_rect(int32_t index)
{
    return interface_rectangles.at(index);
}


// TODO: for now, these use the original limits; once the above array is split into 3 (with MML-compatibility preserved), the menu rect array can be converted to vector to allow arbitrary number of buttons, and merged into the other button-related arrays (e.g. item order)
SDL_Rect get_hud_rect(int32_t index)
{
    return interface_rectangles.at(index);
}



SDL_Rect get_computer_terminal_rect(int32_t index)
{
    return interface_rectangles.at(index);
}




// from original M2 'clut' resource
static const std::array<SDL_Color, 26> interface_colors_std = {
    // HUD inventory
    0x00, 0xff, 0x00, 0xff,
    0x00, 0x14, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff,
    0x00, 0x32, 0x00, 0xff,
    0x00, 0x13, 0x00, 0xff,
    
    // Player colors
    0x24, 0x5f, 0xa3, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0xb0, 0x00, 0x5e, 0xff,
    0xff, 0xff, 0x00, 0xff,
    0xea, 0xea, 0xea, 0xff,
    0xf6, 0x58, 0x00, 0xff,
    0x0c, 0x00, 0xff, 0xff,
    0x00, 0xff, 0x00, 0xff,
    
    // _white_color, _invalid_weapon_color // TODO: where are these used? HUD?
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x14, 0x00, 0xff,
    
    // computer terminal colors
    0x27, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x00, 0x9c, 0x00, 0xff,
    0x00, 0xb0, 0xc9, 0xff,
    0xff, 0xe7, 0x00, 0xff,
    0xaf, 0x00, 0x00, 0xff,
    0x0c, 0x00, 0xff, 0xff,
};


static std::array<SDL_Color, 26> interface_colors;

SDL_Color get_interface_color(int32_t index)
{
    return interface_colors.at(index);
}


SDL_Color get_player_color(int32_t color_index)
{
    return interface_colors.at(color_index + PLAYER_COLOR_BASE_INDEX);
}


SDL_Color get_computer_terminal_color(int32_t index)
{
    return interface_colors.at(index);
}



void initialize_screen_drawing()
{
}








/*
 *  Redirect drawing to screen or offscreen buffer
 */
// From screen_sdl.cpp
extern SDL_Surface *Map_Buffer;

SDL_Surface *draw_surface = NULL;    // Target surface for drawing commands
static SDL_Surface *old_draw_surface = NULL;

void _restore_port(void)
{
	draw_surface = old_draw_surface;
	old_draw_surface = NULL;
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




// TODO: sort out what moves into Canvas and what can get chucked; see also automap classes

/*
 *  Draw line
 */

bool draw_clip_rect_active = false;            // Flag: clipping rect active
screen_rectangle draw_clip_rect;            // Current clipping rectangle


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


void draw_line_xxxx(SDL_Surface *s, const world_point2d *v1, const world_point2d *v2, uint32 pixel, int pen_size)
{
	// Make line going downwards
	if (v1->y > v2->y) {
		const world_point2d *tmp = v1;
		v1 = v2;
		v2 = tmp;
	}

	if (pen_size == 1) {

		// Thin line, clip with Cohen/Sutherland and draw with DDA

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
				draw_line_xxxx(s, hexagon + 0, hexagon + 5, pixel, 1);
		} else {				// Line going to the right
			hexagon[2].x = hexagon[3].x;
			hexagon[2].y = hexagon[3].y - pen_size + 1;
			hexagon[5].x = hexagon[0].x;
			hexagon[5].y = hexagon[0].y + pen_size - 1;
			if (v2->x - v1->y > v2->y - v1->y)	// Pixels missing from polygon filler
				draw_line_xxxx(s, hexagon + 1, hexagon + 2, pixel, 1);
		}

		// Draw hexagon
		draw_polygon_xxxx(s, hexagon, 6, pixel);
	}
}


/*
 *  Draw clipped, filled, convex polygon
 */

void draw_polygon_xxxx(SDL_Surface *s, const world_point2d *vertex_array, int vertex_count, uint32 pixel)
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
}





// MML

void reset_mml_interface_rectangles()
{
    interface_rectangles = interface_rectangles_std;
}


void parse_mml_interface_rectangles(const InfoTree& root)
{
    reset_mml_interface_rectangles();

    for (const InfoTree &rect : root.children_named("rect"))
    {
        int16 index, top = 0, left = 0, bottom = 0, right = 0;
        if (rect.read_indexed("index", index, NUMBER_OF_INTERFACE_RECTANGLES)
            && rect.read_attr("top", top) && rect.read_attr("left", left)
            && rect.read_attr("bottom", bottom) && rect.read_attr("right", right))
        {
            interface_rectangles[index] = {left, top, right - left, bottom - top};
        }
    }
}


void reset_mml_interface_colors()
{
    interface_colors = interface_colors_std;
}


void parse_mml_interface_colors(const InfoTree& root)
{
    reset_mml_interface_colors();
    
    for (const InfoTree &color : root.children_named("color"))
    {
        int16 index;
        if (color.read_indexed("index", index, NUMBER_OF_INTERFACE_COLORS))
        {
            color.read_color(interface_colors[index]);
        }
    }
}
