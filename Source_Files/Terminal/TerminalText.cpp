/*
 TerminalText.cpp -- a single styled string, UTF8-encoded with NUL terminator
 
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

#include "TerminalText.hpp"


void TerminalText::print_debug()
{
    std::cout << "Text range=" << mr_start << ".." << mr_end << " ";
    std::cout << "style='";
    if (style & styleBold)      std::cout << "b";
    if (style & styleItalic)    std::cout << "i";
    if (style & styleUnderline) std::cout << "u";
    if (style & styleShadow)    std::cout << "s";
    std::cout << "' color=" << color_id << " string={{" << utf8_string << "}}\n";
}


void TerminalText::write(font_style_t& current_style, font_color_t& current_color_id, std::iostream::basic_ostream& result)
{
    if (style != current_style)
    {
        if ((style & styleBold)      != (current_style & styleBold))      { result << (style & styleBold      ? "$B" : "$b"); }
        if ((style & styleItalic)    != (current_style & styleItalic))    { result << (style & styleItalic    ? "$I" : "$i"); }
        if ((style & styleUnderline) != (current_style & styleUnderline)) { result << (style & styleUnderline ? "$U" : "$u"); }
        if ((style & styleShadow)    != (current_style & styleShadow))    { result << (style & styleShadow    ? "$S" : "$s"); }
        current_style = style;
    }
    if (color_id != current_color_id)
    {
        result << "$C" << color_id;
        current_color_id = color_id;
    }
    for (char c : utf8_string)
    {
        if (c == '$') { result << '$'; } // TODO: what about '%' escapes?
        result << c;
    }
}
