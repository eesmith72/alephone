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

#include "cstypes.h"
#include "csmacros.h"   // RECTANGLE_WIDTH, RECTANGLE_HEIGHT (these won't be needed once Rect is replaced with SDL_Rect)

#include "screen_drawing.h" // screen_rectangle
#include "player.h"         // action flag enums for keyboard input (page up/down/etc)
#include "lua_script.h"     // L_Call_Terminal_Exit
#include "Packing.h"        // crap, but fixing is for later


// -----------------------------------------------------------------------------------------
// enums

// old Rect crap
#define RECT_WIDTH(r)  ((r).right - (r).left)
#define RECT_HEIGHT(r) ((r).bottom - (r).top)

#define LABEL_INSET 3
#define LOG_DURATION_BEFORE_TIMEOUT (2*TICKS_PER_SECOND)
#define BORDER_HEIGHT 18
#define FUDGE_FACTOR 1


// config-defined terminal header texts and (M1-only logon/logoff title) // TODO: all this shit should disappear behind load_terminal_strings which sets them all as named std::string vars, getting rid of wretched lookup functions
#define strCOMPUTER_LABELS 135
enum
{
    _m1_marathon_name,
    _computer_starting_up,
    _computer_manufacturer,
    _computer_address,
    _computer_terminal,
    _scrolling_message,
    _acknowledgement_message,
    _disconnecting_message,
    _connection_terminated_message,
    _date_format,
};


//typedef uint16_t font_style_t; // bitflag font styles (styleNormal, ::bold, etc) defined in csfonts.h


// -----------------------------------------------------------------------------------------
// config-defined drawing areas, as plotted on original 640x480 screen

Rect get_term_rectangle(int16_t index); // TODO: this goes away once Rect is replaced with SDL_Rect

SDL_Rect get_term_rect(int16_t index);


// -----------------------------------------------------------------------------------------
// calculate end-of-line breaks

bool can_break_after(char* ch, bool is_utf8 = false);

bool calculate_line_end_index(char* base_text, uint16_t font_style, int16_t line_width, int16_t start_index, int16_t text_end_index, int16_t* end_index);

int16_t calculate_lines_per_page();

int16_t count_total_lines(char* base_text, int16_t width, int16_t start_index, int16_t end_index);



// TODO: these functions are only used in Terminal/ so will be replaced/removed when Rect is replaced with SDL_Rect

inline void InsetRect(Rect* r, int32_t dx, int32_t dy)
{
    r->top += dy;
    r->left += dx;
    r->bottom -= dy;
    r->right -= dx;
}


inline void OffsetRect(Rect* r, int32_t dx, int32_t dy)
{
    r->top += dy;
    r->left += dx;
    r->bottom += dy;
    r->right += dx;
}



typedef uint32_t action_flag_t;


// -----------------------------------------------------------------------------------------


inline bool is_line_break(char c)
{
    // TODO: M1 and M2 terminal parsers originally checked for CR only; will accepting LF too be an issue?
    return c == 13 || c == 10;
}


void get_date_string(char* date_string, bool is_m1);


const std::array<std::string, 128> macroman_hi_chars = {
    "\xC3\x84",
    "\xC3\x85",
    "\xC3\x87",
    "\xC3\x89",
    "\xC3\x91",
    "\xC3\x96",
    "\xC3\x9C",
    "\xC3\xA1",
    "\xC3\xA0",
    "\xC3\xA2",
    "\xC3\xA4",
    "\xC3\xA3",
    "\xC3\xA5",
    "\xC3\xA7",
    "\xC3\xA9",
    "\xC3\xA8",
    "\xC3\xAA",
    "\xC3\xAB",
    "\xC3\xAD",
    "\xC3\xAC",
    "\xC3\xAE",
    "\xC3\xAF",
    "\xC3\xB1",
    "\xC3\xB3",
    "\xC3\xB2",
    "\xC3\xB4",
    "\xC3\xB6",
    "\xC3\xB5",
    "\xC3\xBA",
    "\xC3\xB9",
    "\xC3\xBB",
    "\xC3\xBC",
    "\xE2\x80\xA0",
    "\xC2\xB0",
    "\xC2\xA2",
    "\xC2\xA3",
    "\xC2\xA7",
    "\xE2\x80\xA2",
    "\xC2\xB6",
    "\xC3\x9F",
    "\xC2\xAE",
    "\xC2\xA9",
    "\xE2\x84\xA2",
    "\xC2\xB4",
    "\xC2\xA8",
    "\xE2\x89\xA0",
    "\xC3\x86",
    "\xC3\x98",
    "\xE2\x88\x9E",
    "\xC2\xB1",
    "\xE2\x89\xA4",
    "\xE2\x89\xA5",
    "\xC2\xA5",
    "\xC2\xB5",
    "\xE2\x88\x82",
    "\xE2\x88\x91",
    "\xE2\x88\x8F",
    "\xCF\x80",
    "\xE2\x88\xAB",
    "\xC2\xAA",
    "\xC2\xBA",
    "\xCE\xA9",
    "\xE6",
    "\xC3\xB8",
    "\xC2\xBF",
    "\xC2\xA1",
    "\xC2\xAC",
    "\xE2\x88\x9A",
    "\xC6\x92",
    "\xE2\x89\x88",
    "\xE2\x88\x86",
    "\xC2\xAB",
    "\xC2\xBB",
    "\xE2\x80\xA6",
    "\xC2\xA0",
    "\xC3\x80",
    "\xC3\x83",
    "\xC3\x95",
    "\xC5\x92",
    "\xC5\x93",
    "\xE2\x80\x93",
    "\xE2\x80\x94",
    "\xE2\x80\x9C",
    "\xE2\x80\x9D",
    "\xE2\x80\x98",
    "\xE2\x80\x99",
    "\xC3\xB7",
    "\xE2\x97\x8A",
    "\xC3\xBF",
    "\xC5\xB8",
    "\xE2\x81\x84",
    "\xE2\x82\xAC",
    "\xE2\x80\xB9",
    "\xE2\x80\xBA",
    "\xEF\xAC\x81",
    "\xEF\xAC\x82",
    "\xE2\x80\xA1",
    "\xC2\xB7",
    "\xE2\x80\x9A",
    "\xE2\x80\x9E",
    "\xE2\x80\xB0",
    "\xC3\x82",
    "\xC3\x8A",
    "\xC3\x81",
    "\xC3\x8B",
    "\xC3\x88",
    "\xC3\x8D",
    "\xC3\x8E",
    "\xC3\x8F",
    "\xC3\x8C",
    "\xC3\x93",
    "\xC3\x94",
    "\xEF\xA3\xBF",
    "\xC3\x92",
    "\xC3\x9A",
    "\xC3\x9B",
    "\xC3\x99",
    "\xC4\xB1",
    "\xCB\x86",
    "\xCB\x9C",
    "\xC2\xAF",
    "\xCB\x98",
    "\xCB\x99",
    "\xCB\x9A",
    "\xC2\xB8",
    "\xCB\x9D",
    "\xCB\x9B",
    "\xCB\x87",
};


// Caution: This discards all non-printing characters except Space, Tab (replaced with 2 spaces), CR (replaced with LF), and LF.
// The caller is responsible for NUL-terminating the completed std::string if a C string is needed.
inline void append_macroman_char_to_utf8(uint8_t c, std::string& result)
{
    if (c < 0x20)
    {
        switch (c)
        {
            case '\r':
            case '\n':
                result += '\n';
                break;
            case '\t':
                result += "  ";
                break;
        }
    }
    else if (c < 127)
    {
        result += c;
    }
    else if (c >= 128)
    {
        result += macroman_hi_chars[c - 128];
    }
}

#endif /* terminal_support_hpp */
