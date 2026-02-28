/*
 TerminalPage.hpp -- a single terminal 'screen', e.g. with scrolling text and optional pict/checkpoint
 
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

#ifndef TerminalPage_hpp
#define TerminalPage_hpp

#include "TerminalText.hpp"


// -----------------------------------------------------------------------------------------


enum { // TerminalPage types // TODO: type it, move it onto class
    _undefined_page, // e.g. the terminal doesn't have an unfinished/success/failure group
    _logon_page, // TODO: was 0
    _unfinished_page,
    _success_page,
    _failure_page,
    _information_page,
    _end_page,
    _interlevel_teleport_page, // permutation is level to go to
    _intralevel_teleport_page, // permutation is polygon to go to
    _checkpoint_page,          // permutation is the goal to show
    _sound_page,               // permutation is the sound id to play
    _movie_page,               // permutation is the movie id to play
    _track_page,               // permutation is the track to play
    _pict_page,                // permutation is the pict to display
    _logoff_page,
    _camera_page,              // permutation is the object index
    _static_page,              // permutation is the duration of static
    _tag_page,                 // permutation is the tag to activate
    
    _unfinished_group = _unfinished_page,
    _success_group = _success_page,
    _failure_group = _failure_page,
};


enum { // TerminalPage flags
    _draw_object_on_left   = 0x00, // these indicate which rect to use for non-text element (picture/checkpoint)
    _draw_object_on_right  = 0x01,
    _draw_object_on_center = 0x02,
    _terminal_is_m1        = 0x100, // caution: this flag affects teleport delay, text top-padding, pseudo-date, something hinkey in goto_next_terminal_page // TODO: putting this flag on every Page instead of on ComputerTerminal is bloody annoying (basically there are some APIs that only take ComputerPage) but leave it for now as it's entangled in saved/net-game data
};


// -----------------------------------------------------------------------------------------
// a single 'screen' in a computer terminal


class TerminalPage // was terminal_groupings
{
public:
    
    int16_t type;               // see TerminalPage types above
    int16_t flags;              // see TerminalPage flags above
    int16_t permutation;        // TerminalPage types comments above describe if and how this is used
    
    int16_t mr_start, mr_end;   // MacRoman string range
    
    int16_t maximum_line_count; // TODO: what is this? presumably it's the page's total line_count, badly named
    
    std::vector<TerminalText> texts; // styled text chunks
    
    std::shared_ptr<SDL_Surface> text_surface = std::shared_ptr<SDL_Surface>(nullptr, SDL_FreeSurface);
    
    TerminalPage(int16_t type = _undefined_page, int16_t flags = 0, int16_t permutation = 0,
                 int16_t start_index = 0, int16_t end_index = 0, int16_t maximum_line_count = 0,
                 SDL_Surface* text_surface = nullptr)
            : type(type), flags(flags), permutation(permutation),
              mr_start(start_index), mr_end(end_index), maximum_line_count(maximum_line_count),
              text_surface(text_surface)
    {
        texts.clear();
    }
    
    // TODO: shouldn't need an explicit declaration for Copy
    TerminalPage(const TerminalPage& t)
            : type(t.type), flags(t.flags), permutation(t.permutation),
              mr_start(t.mr_start), mr_end(t.mr_end),
              maximum_line_count(t.maximum_line_count), texts(t.texts),
              text_surface(t.text_surface) { }
    
    //
    
    void unpack_m2_data(uint8_t*& ptr) // TODO: could make an argument for these being constructors that take a stream-like object; problem is, there can be >1 type (M1,M2,AO,AO+) so probably best as named class/instance methods
    {
        StreamToValue(ptr, flags);
        StreamToValue(ptr, type);
        type++; // _logon_page is 0 in M2 'term' but we want 0 to indicate undefined so all types are budged up
        StreamToValue(ptr, permutation);
        StreamToValue(ptr, mr_start);
        int16_t length = 0;
        StreamToValue(ptr, length);
        mr_end = mr_start + length;
        StreamToValue(ptr, maximum_line_count);
        texts.clear();
    }
    
    void write(std::iostream::basic_ostream& result);

    
    void print_debug();
    
    Rect calculate_bounds_for_text_box();
    
    Rect calculate_bounds_for_object_box(int16_t flags_, Rect* source);
    
    Rect calculate_bounds_for_object_box(Rect* source)
    {
        return calculate_bounds_for_object_box(flags, source);
    }

    bool is_connection_screen() // logon/logoff // probably unused
    {
        return type == _logon_page || type == _logoff_page;
    }
    
    bool is_m1_screen()
    {
        return flags & _terminal_is_m1;
    }
    
private:
    
    void write_directive(std::iostream::basic_ostream& result);
    
};

const int32_t SIZEOF_m2_terminal_page = 12;


typedef std::vector<TerminalPage> TerminalPages;


#endif /* TerminalPage_hpp */
