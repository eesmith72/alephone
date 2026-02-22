/*
 TerminalText.hpp -- a single styled string, UTF8-encoded with NUL terminator
 
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


class TerminalText // a styled string
{
public:
        
    font_style_t style;
    int16_t color_id;
    
    uint16_t mr_start, mr_end; // the character on which this style begins (the end index is the start_index of the next range); unpack_m2_computer_terminals sets this as it unpacks the WAD data; once unpacking is finished it should not be used
    
    std::string utf8_string;
    
    TerminalText(font_style_t style = styleNormal, int16_t color_id = 0,
                 uint16_t start_index = 0, uint16_t end_index = 0, std::string s = "")
            : style(style), color_id(color_id), mr_start(start_index), mr_end(end_index), utf8_string(s)
    {
        //std::cout << "NEW TEXT: ";
        //print_debug();
    } // end_index is set when unpacking M2 data
    
    TerminalText(const TerminalText& t)
            : style(t.style), color_id(t.color_id), mr_start(t.mr_start), mr_end(t.mr_end), utf8_string(t.utf8_string)
    {
        //std::cout << "COPY TEXT: ";
        //print_debug();
    }
    
    void unpack_m2_data(uint8_t*& ptr)
    {
        StreamToValue(ptr, mr_start);
        mr_end = 0;
        StreamToValue(ptr, style); // uint16_t
        StreamToValue(ptr, color_id);
        utf8_string = "";
    }
    
    void print_debug();
    
};
const int32_t SIZEOF_m2_terminal_text = 6;


#endif /* TerminalText_hpp */
