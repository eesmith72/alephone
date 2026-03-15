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


void TerminalPage::write_directive(std::iostream::basic_ostream& result)
{
    switch (type)
    {
        case _undefined_page:
            result << "#undefined";
            break;
        case _logon_page:
            result << "#logon";
            break;
        case _unfinished_page:
            result << "#unfinished";
            break;
        case _success_page:
            result << "#success";
            break;
        case _failure_page:
            result << "#failure";
            break;
        case _information_page:
            result << "#information";
            break;
        case _end_page:
            result << "#end";
            break;
        case _interlevel_teleport_page:
            result << "#interlevel " << permutation;
            break;
        case _intralevel_teleport_page:
            result << "#intralevel " << permutation;
            break;
        case _checkpoint_page:
            result << "#checkpoint " << permutation;
            break;
        case _sound_page:
            result << "#sound " << permutation;
            break;
        case _movie_page:
            result << "#movie " << permutation;
            break;
        case _track_page:
            result << "#track " << permutation;
            break;
        case _pict_page:
            result << "#pict " << permutation;
            break;
        case _logoff_page:
            result << "#logoff";
            break;
        case _camera_page:
            result << "#camera " << permutation;
            break;
        case _static_page:
            result << "#static " << permutation;
            break;
        case _tag_page:
            result << "#tag " << permutation;
            break;
        default:
            result << "#unsupported " << type;
    }
}


void TerminalPage::write(std::iostream::basic_ostream& result)
{
    write_directive(result);
    result << "\n\n";
    
    // TODO: what about flags? append to page directive as optional key-value pairs (use '=' or ':'?), e.g.:
    //
    // #pict 312 align=right
    //
    // Blah-blah-blah
    
    font_style_t current_style = styleNormal;
    font_color_t current_color_id = 0;
    
    for (TerminalText& text : texts) { text.write(current_style, current_color_id, result); }
    
    // reset styles and color
    if (current_style != styleNormal)
    {
        if (current_style & styleBold)      { result << "$b"; }
        if (current_style & styleItalic)    { result << "$i"; }
        if (current_style & styleUnderline) { result << "$u"; }
        if (current_style & styleShadow)    { result << "$s"; }
    }
    if (current_color_id != 0)
    {
        result << "$C0";
    }
    result << "\n\n";
}


void TerminalPage::print_debug()
{
    std::cout << "Page ";
    write_directive(std::cout);
    std::cout << " (texts=" << texts.size() << " range=" << mr_start << ".." << mr_end << ")\n";
    for (TerminalText& text : texts) { text.print_debug(); }
    std::cout << "\n";
}


// -----------------------------------------------------------------------------------------

// TODO: horribly knotty
SDL_Rect TerminalPage::calculate_bounds_for_object_box(int16_t flags_, const SDL_Rect* source)
{
    SDL_Rect bounds;
    if (source && (flags_ & _draw_object_on_center)) // TODO: FIX: this is awfully non-specific, considering next line is specific to logon rect
    {
        bounds = get_term_rect(_terminal_logon_graphic_rect);
        
        if (!(source->w > bounds.w || source->h > bounds.h))
        {
            // Just return the normal frame.  Aspect ratio will take care of us.
            InsetRect(bounds, (bounds.w - source->w) / 2, (bounds.h - source->h) / 2);
        }
    }
    else if (flags_ & _draw_object_on_right)
    {
        bounds = get_term_rect(_terminal_right_rect);
    }
    else
    {
        bounds = get_term_rect(_terminal_left_rect);
    }
    return bounds;
}


SDL_Rect TerminalPage::calculate_bounds_for_text_box()
{
    if (type == _information_page)
    {
        return get_term_rect(_terminal_full_text_rect);
    }
    
    SDL_Rect bounds;
    if (flags & _draw_object_on_center)
    {
        bounds = calculate_bounds_for_object_box(_draw_object_on_right, nullptr); // TODO: not too sure about this one
    }
    else if (flags & _draw_object_on_right)
    {
        bounds = calculate_bounds_for_object_box(_draw_object_on_left, nullptr);
    }
    else // image on left, presumably
    {
        bounds = calculate_bounds_for_object_box(_draw_object_on_right, nullptr);
    }
    
    // TODO: if the drawable text area is different size for M1, it should be adjusted in initialize_terminal_renderer(is_m1), not here, at which point is_m1_screen can go away; hopefully the _terminal_is_m1 flag can be removed too
    if (is_m1_screen())
    {
        const font_t* font = get_interface_font(_computer_interface_font);
        bounds.y += font->line_height; // presumably there's an extra line of padding
    }
    return bounds;
}

