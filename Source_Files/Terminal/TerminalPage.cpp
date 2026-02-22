/*
 TerminalPage.hpp
 
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

#include "TerminalPage.hpp"


// -----------------------------------------------------------------------------------------


// TODO:  


void TerminalPage::print_debug()
{
    std::cout << "Page ";
    // TODO: what about object positions?
    switch (type)
    {
        case _undefined_page:
            std::cout << "#undefined";
            break;
        case _logon_page:
            std::cout << "#logon";
            break;
        case _unfinished_page:
            std::cout << "#unfinished";
            break;
        case _success_page:
            std::cout << "#success";
            break;
        case _failure_page:
            std::cout << "#failure";
            break;
        case _information_page:
            std::cout << "#information";
            break;
        case _end_page:
            std::cout << "#end";
            break;
        case _interlevel_teleport_page:
            std::cout << "#interlevel " << permutation;
            break;
        case _intralevel_teleport_page:
            std::cout << "#intralevel " << permutation;
            break;
        case _checkpoint_page:
            std::cout << "#checkpoint " << permutation;
            break;
        case _sound_page:
            std::cout << "#sound " << permutation;
            break;
        case _movie_page:
            std::cout << "#movie " << permutation;
            break;
        case _track_page:
            std::cout << "#track " << permutation;
            break;
        case _pict_page:
            std::cout << "#pict " << permutation;
            break;
        case _logoff_page:
            std::cout << "#logoff";
            break;
        case _camera_page:
            std::cout << "#camera " << permutation;
            break;
        case _static_page:
            std::cout << "#static " << permutation;
            break;
        case _tag_page:
            std::cout << "#tag " << permutation;
            break;
        default:
            break;
    }
    std::cout << " (texts=" << texts.size() << " range=" << mr_start << ".." << mr_end << ")\n";
    for (TerminalText& text : texts) { text.print_debug(); }
    std::cout << "\n";
}


// -----------------------------------------------------------------------------------------

// TODO: horribly knotty
Rect TerminalPage::calculate_bounds_for_object_box(int16_t flags_, Rect* source)
{
    Rect bounds;
    if (source && flags_ & _draw_object_on_center) // && is_connection_screen()?
    {
        bounds = get_term_rectangle(_terminal_logon_graphic_rect);
        if (!(RECT_WIDTH(*source) > RECT_WIDTH(bounds) || RECT_HEIGHT(*source) > RECT_HEIGHT(bounds)))
        {
            // Just return the normal frame.  Aspect ratio will take care of us.
            InsetRect(&bounds, (RECT_WIDTH(bounds) - RECT_WIDTH(*source)) / 2,
                              (RECT_HEIGHT(bounds) - RECT_HEIGHT(*source)) / 2);
        }
    }
    else if (flags_ & _draw_object_on_right)
    {
        bounds = get_term_rectangle(_terminal_right_rect);
    }
    else
    {
        bounds = get_term_rectangle(_terminal_left_rect);
    }
    return bounds;
}


Rect TerminalPage::calculate_bounds_for_text_box()
{
    if (type == _information_page)
    {
        return get_term_rectangle(_terminal_full_text_rect);
    }
    
    Rect bounds;
    if (flags & _draw_object_on_center)
    {
        // dprintf("splitting text not supported!");
        bounds = calculate_bounds_for_object_box(_draw_object_on_right, nullptr);
    }
    else if (flags & _draw_object_on_right)
    {
        bounds = calculate_bounds_for_object_box(_draw_object_on_left, nullptr);
    }
    else // image on left, presumably
    {
        bounds = calculate_bounds_for_object_box(_draw_object_on_right, nullptr);
    }
    
    if (is_m1_screen())
    {
        bounds.top += _get_font_line_height(_computer_interface_font); // presumably there's an extra line of padding
    }
    return bounds;
}

