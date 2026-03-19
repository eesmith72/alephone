/*
 terminal_support.hpp
 
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

#ifndef terminal_support_hpp
#define terminal_support_hpp

#include "cseries.h"
//#include "csmacros.h"   // RECTANGLE_WIDTH, RECTANGLE_HEIGHT (these won't be needed once screen_rectangle is replaced with SDL_Rect)

#include "screen_drawing.h" // screen_rectangle
#include "player.h"         // action flag enums for keyboard input (page up/down/etc)
#include "lua_script.h"     // L_Call_Terminal_Exit
#include "Packing.h"        // crap, but fixing is for later


// -----------------------------------------------------------------------------------------
// enums

// old screen_rectangle crap
#define RECT_WIDTH(r)  ((r).right - (r).left)
#define RECT_HEIGHT(r) ((r).bottom - (r).top)

#define LABEL_INSET 3
#define LOG_DURATION_BEFORE_TIMEOUT (2*TICKS_PER_SECOND)
#define BORDER_HEIGHT 18
#define FUDGE_FACTOR 1


// font_style_t (bitflags), font_color_t (0-7) are int16_t typedefs in Font.hpp


// -----------------------------------------------------------------------------------------
// built-in/MML-defined rects, relative to _terminal_screen_rect on original 640x480 screen

SDL_Rect get_term_rect(int32_t index);


// -----------------------------------------------------------------------------------------
// calculate end-of-line breaks

bool can_break_after(char* ch, bool is_utf8 = false);

bool calculate_line_end_index(char* base_text, uint16_t font_style, int16_t line_width, int16_t start_index, int16_t text_end_index, int16_t* end_index);

int16_t calculate_lines_per_page();

int16_t count_total_lines(char* base_text, int16_t width, int16_t start_index, int16_t end_index);



// TODO: these functions are only used in Terminal/ so will be replaced/removed when screen_rectangle is replaced with SDL_Rect

inline void InsetRect(SDL_Rect& r, int32_t dx, int32_t dy)
{
    r.x += dx;
    r.y += dy;
    r.w -= dx * 2;
    r.h -= dy * 2;
}


inline void OffsetRect(SDL_Rect& r, int32_t dx, int32_t dy)
{
    r.x += dx;
    r.y += dy;
}



typedef uint32_t action_flag_t;


// -----------------------------------------------------------------------------------------


inline bool is_line_break(char c)
{
    // TODO: M1 and M2 terminal parsers originally checked for CR only; will accepting LF too be an issue?
    return c == '\r' || c == '\n';
}


const std::string get_date_string(bool is_m1);


#endif /* terminal_support_hpp */
