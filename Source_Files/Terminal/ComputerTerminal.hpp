/*
 ComputerTerminal.hpp -- class containing one terminal's text
 
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

#ifndef ComputerTerminal_hpp
#define ComputerTerminal_hpp

#include "TerminalPage.hpp"


// TODO: function for exporting M1/M2/AO+ terminals from current scenario to .txt files


// -----------------------------------------------------------------------------------------
// ComputerTerminal -- holds text, formatting, and page information for a single terminal


class ComputerTerminal
{
public:
    
    int16_t lines_per_page = 0; // config; not clear why it's attached here; it's specific to terminal font's height; we need to redo paging anyway
    
    // A terminal will display one of the three following sequences: unfinished, success, failure
    TerminalPages pages;
    
    ComputerTerminal() {}
    
    size_t get_bytesize();
    
    TerminalPage* get_page_at_index(int16_t page_id);
    
    int16_t get_index_for_group(int16_t group_type) // used by PlayerTerminalState to get start of unfinished/success/failure group
    {
        /*
        for (uint32_t i = 0; i < groups.size(); i++)
        {
            TerminalPage* group = get_page_at_index(i);
            if (!group) return NONE;
            if (group->type == group_type) return i;
        }*/
        return NONE;
    }
    
    void print_debug();
    
    void write(std::iostream::basic_ostream& result);
    
};


// -----------------------------------------------------------------------------------------
// get a terminal


ComputerTerminal* get_terminal_for_id(int16_t id); // returns nullptr if not found

int16_t number_of_terminals();


// -----------------------------------------------------------------------------------------
// serialization


void load_m1_computer_terminals_for_level(int16_t level_number); // will be read from App/Shapes resource fork

void unpack_m2_computer_terminals(uint8_t* Stream, size_t Count); // read from WAD

// always packs in M2 format
void pack_computer_terminals(uint8_t* Stream, size_t Count);

size_t get_bytesize_of_packed_computer_terminals(); // number of packed bytes


#endif /* ComputerTerminal_hpp */
