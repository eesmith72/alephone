/*
 TerminalText.hpp -- class containing one terminal's text
 
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

#ifndef TerminalText_hpp
#define TerminalText_hpp

#include "terminal_support.hpp"


// -----------------------------------------------------------------------------------------
// TerminalText -- holds text, formatting, and page information for a single terminal


class TerminalText
{
public:
    
    int16_t lines_per_page = 0;
    std::vector<TerminalTextGroup> groupings;
    std::vector<TerminalTextStyleRange> style_ranges;
    std::vector<uint8_t> text; // yeesh
    
    TerminalText() {}
    
    size_t get_bytesize();
    
    TerminalTextGroup* get_grouping(int16_t index)
    {
        return index >= 0 || index < int32_t(groupings.size()) ? &groupings[index] : NULL; // TODO: inclined towards chucking exceptions and catch them in main draw function (i.e. these are Map bugs so can and will happen but there's nothing we can do except report error and leave terminal)
    }
    
    int16_t find_group_type(int16_t group_type)
    {
        for (uint32_t i = 0; i < groupings.size(); i++)
        {
            TerminalTextGroup* group = get_grouping(i);
            if (!group) return NONE;
            if (group->type == group_type) return i;
        }
        return NONE;
    }

    char* get_cstr()
    {
        return (char*)text.data();
    }
    
    TerminalTextStyleRange* get_indexed_font_changes(int16_t index) // fairly sure this is the indices of \b, \i, etc modifiers in terminal text string
    {
        return index >= 0 && index < int32_t(style_ranges.size()) ? &style_ranges[index] : nullptr;
    }
};


// -----------------------------------------------------------------------------------------
// get a terminal's text


TerminalText* get_terminal_text_for_terminal_id(int16_t id);

int16_t number_of_terminal_texts();


// -----------------------------------------------------------------------------------------
// serialization


void unpack_computer_terminal_text(uint8_t* Stream, size_t Count);

void pack_computer_terminal_text(uint8_t* Stream, size_t Count);

size_t get_bytesize_of_packed_computer_terminals(); // number of packed bytes


#endif /* TerminalText_hpp */
