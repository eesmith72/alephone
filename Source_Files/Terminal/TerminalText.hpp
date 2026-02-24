/*
 TerminalText.hpp -- a UTF8-encoded std::string with style and color info
 
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


class TerminalText
{
public:
    // TODO: check if the original M2 int types are signed and/or unsigned; we should upgrade to [u]int32s

    font_style_t style; // bitwise BIUS flags; see csfonts.h
    font_color_t color_id; // 0-7 (0 = green); TODO: check if RGB values can be MML defined; TODO: would be better to store SDL_Color (RGB[A]) here and allow "$c...$" modifier to specify any hex color
    
    std::string utf8_string;
    
    // only used by unpack_m2_computer_terminals
    uint16_t mr_start; // the MacRoman character on which this style begins
    uint16_t mr_end; // the MacRoman character on which this style ends (i.e. the start_index of next range/end of string)
    
    TerminalText(font_style_t style = styleNormal, font_color_t color_id = 0, std::string s = "",
                 uint16_t start_index = 0, uint16_t end_index = 0)
            : style(style), color_id(color_id), utf8_string(s), mr_start(start_index), mr_end(end_index)
    {
        //std::cout << "NEW TEXT: ";
        //print_debug();
    } // end_index is set when unpacking M2 data
    
    TerminalText(const TerminalText& t)
            : style(t.style), color_id(t.color_id), utf8_string(t.utf8_string), mr_start(t.mr_start), mr_end(t.mr_end)
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
    
    void write(font_style_t& current_style, font_color_t& current_color_id, std::iostream::basic_ostream& result);
    
};
const int32_t SIZEOF_m2_terminal_text = 6; // 3x [u]int16 = style, color_id, start_index


#endif /* TerminalText_hpp */
