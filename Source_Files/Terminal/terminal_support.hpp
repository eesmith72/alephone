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

#include "player.h"     // action flag enums for keyboard input (page up/down/etc)
#include "lua_script.h" // L_Call_Terminal_Exit
#include "Packing.h"    // crap, but fixing is for later


// -----------------------------------------------------------------------------------------
// enums

#define LABEL_INSET 3
#define LOG_DURATION_BEFORE_TIMEOUT (2*TICKS_PER_SECOND)
#define BORDER_HEIGHT 18
#define FUDGE_FACTOR 1


// config-defined terminal header texts and (M1-only logon/logoff title)
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


enum {
    _logon_group,
    _unfinished_group,
    _success_group,
    _failure_group,
    _information_group,
    _end_group,
    _interlevel_teleport_group, // permutation is level to go to
    _intralevel_teleport_group, // permutation is polygon to go to
    _checkpoint_group, // permutation is the goal to show
    _sound_group,      // permutation is the sound id to play
    _movie_group,      // permutation is the movie id to play
    _track_group,      // permutation is the track to play
    _pict_group,       // permutation is the pict to display
    _logoff_group,
    _camera_group,     //  permutation is the object index
    _static_group,     // permutation is the duration of static
    _tag_group,        // permutation is the tag to activate

    NUMBER_OF_GROUP_TYPES
};


enum { // TerminalTextGroup flags
    _draw_object_on_right = 0x01,  // for drawing checkpoints, picts, movies
    _center_object        = 0x02,
    _group_is_marathon_1  = 0x100,
};


struct TerminalTextGroup
{
    int16_t flags;       // flags enum above
    int16_t type;        // group types enum above
    int16_t permutation; // see group enum comments above
    int16_t start_index; // position of this group's text in the parsed string
    int16_t length;
    int16_t maximum_line_count;
};
const int32_t SIZEOF_terminal_groupings = 12;


typedef uint16_t font_style_t; // bitflag font styles (styleNormal, styleBold, etc) defined in csfonts.h


struct TerminalTextStyleRange
{
    int16_t start_index = 0; // the character on which this style begins (the end index is the start_index of the next range)
    font_style_t style  = 0;
    int16_t color_id    = 0;
    
    void unpack_m2_data(uint8_t*& ptr)
    {
        StreamToValue(ptr, start_index);
        StreamToValue(ptr, style); // uint16_t
        StreamToValue(ptr, color_id);
    }
};
const int32_t SIZEOF_text_face_data = 6;


#define MAC_LINE_END 13


// -----------------------------------------------------------------------------------------
// config-defined drawing areas, as plotted on original 640x480 screen

Rect get_term_rectangle(int16_t index);

SDL_Rect get_term_rect(int16_t index);


// -----------------------------------------------------------------------------------------
// calculate end-of-line breaks

bool calculate_line_end_index(char* base_text, uint16_t font_style, int16_t line_width, int16_t start_index, int16_t text_end_index, int16_t* end_index);


int16_t calculate_lines_per_page();

void calculate_maximum_lines_for_groups(TerminalTextGroup* groups, int16_t group_count, char* text_base); // used by M1TerminalParser

void calculate_bounds_for_object(int16_t flags, Rect* bounds, Rect* source);

void calculate_bounds_for_text_box(int16_t flags, Rect* bounds);

int16_t count_total_lines(char* base_text, int16_t width, int16_t start_index, int16_t end_index);


// -----------------------------------------------------------------------------------------

void get_date_string(char* date_string, int16_t flags);


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


#endif /* terminal_support_hpp */
