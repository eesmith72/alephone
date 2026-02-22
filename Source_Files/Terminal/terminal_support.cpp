/*
 terminal_support.cpp
 
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

#include "terminal_support.hpp"

#include "FilmProfile.h"

#include "sdl_fonts.h" // font_info


// TODO: replace Rect with SDL_Rect?


// -----------------------------------------------------------------------------------------
// nasty externs

// implemented in screen_drawing.cpp but not declared in screen_drawing.h
font_info *GetInterfaceFont(short font_index);
uint16_t GetInterfaceStyle(short font_index);


// -----------------------------------------------------------------------------------------
// config-defined drawing areas, as plotted on original 640x480 screen

/*
 _terminal_screen_rect = 20,
 _terminal_header_rect,
 _terminal_footer_rect,
 _terminal_full_text_rect,
 _terminal_left_rect,
 _terminal_right_rect,
 _terminal_logon_graphic_rect,
 _terminal_logon_title_rect,
 _terminal_logon_location_rect,
 */

Rect get_term_rectangle(int16_t index)
{
    screen_rectangle* term_rect = get_interface_rectangle(_terminal_screen_rect);
    screen_rectangle* target_rect = get_interface_rectangle(index);
    Rect bounds;
    bounds.left   = target_rect->left   - term_rect->left;
    bounds.top    = target_rect->top    - term_rect->top;
    bounds.right  = target_rect->right  - term_rect->left;
    bounds.bottom = target_rect->bottom - term_rect->top;
    return bounds;
}

SDL_Rect get_term_rect(int16_t index)
{
    Rect bounds = get_term_rectangle(index);
    SDL_Rect rect = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
    return rect;
}


// -----------------------------------------------------------------------------------------
// calculate end-of-line breaks; this assumes monospace font and some crude non-i18n word-breaking

bool can_break_after(char* ch, bool is_utf8)
{
    switch (ch[0]) // determined empirically on my PowerBook
    {
        case '&':
        case '*':
        case '+':
        case '-':
        case '\\':
        case '<':
        case '=':
        case '>':
        case '/':
        case '^':
        case '|':
            return true;
        default:
            if (is_utf8)
            {
                // from libtextwrap - Text-Wrapping Library with I18N
                // Copyright (C) 2003 by Tomohiro KUBOTA <kubota@debian.org>
                switch (mblen(ch, MB_CUR_MAX))
                {
                    case 3: // U+0800 - U+FFFF
                    {
                        uint32_t u = (*ch&0x0f)*0x1000 + (*(ch+1)&0x3f)*0x40 + (*(ch+2)&0x3f);
                        if (u >= 0x3000 && u <= 0x312f)
                        {
                            return !(u == 0x300a || u == 0x300c || u == 0x300e || u == 0x3010
                                     || u == 0x3014 || u == 0x3016 || u == 0x3018 || u == 0x301a);
                        }
                        else // CJK punctuations, Hiragana, Katakana, Bopomofo
                        {
                            return ((u >= 0x31a0 && u <= 0x31bf)      // Bopomofo
                                    || (u >= 0x31f0 && u <= 0x31ff)   // Katakana extension
                                    || (u >= 0x3400 && u <= 0x9fff)   // Han Ideogram
                                    || (u >= 0xf900 && u <= 0xfaff)); // Han Ideogram
                        }
                    }
                    case 4: // U+10000 - U+1FFFFF
                    {
                        uint32_t u = (*ch&7)*0x40000 + (*(ch+1)&0x3f)*0x1000 + (*(ch+2)&0x3f)*0x40 + (*(ch+3)&0x3f);
                        return (u >= 0x20000 && u <= 0x2ffff);  // Han Ideogram
                    }
                    default:
                    {}
                }
            }
            return false;
    }
}


bool calculate_line_end_index(char* base_text, font_style_t font_style, int16_t line_width, int16_t start_index, int16_t text_end_index, int16_t* end_index)
{
    bool done = false;

    if (base_text[start_index]) // slightly worrisome; presumably it's looking for '\0' that's inserted into the original C string during parsing to split it at group boundaries (this is nice and efficient), though it'd be worth considering structured JSON/XML as future-proof UTF8 terminal text format)
    {
        int32_t index = start_index, running_width = 0;
        
        // terminal_font no longer a global, since it may change
        font_info* terminal_font = GetInterfaceFont(_computer_interface_font);

        while (running_width < line_width && base_text[index] && !is_line_break(base_text[index]))
        {
            running_width += terminal_font->char_width(base_text[index], font_style); // TODO: it is unclear why style is needed when font is supposed to be monospace; OTOH, it won't behave correctly if font is variable-width as styles can change along line
            index++;
        }

        // Now go backwards, looking for a place to split
        if (is_line_break(base_text[index]))
        {
            index++;
        }
        else if (base_text[index])
        {
            if (film_profile.better_terminal_word_wrap)
            {
                int32_t break_point = index - 1;
                while (break_point > start_index)
                {
                    if (base_text[break_point] == ' ')
                    {
                        index = break_point + 1; // eat the space
                        break;
                    }
                    else if (break_point > start_index + 1 && can_break_after(&base_text[break_point - 1]))
                    {
                        index = break_point;
                        break;
                    }
                    --break_point;
                }
            }
            else
            {
                int32_t break_point = index;
                
                while (break_point>start_index)
                {
                    if (base_text[break_point] == ' ') { break; } // Non printing
                    break_point--; // this needs to be in front of the test
                }
                
                if (break_point != start_index) { index = break_point + 1; } // Space at the end of the line
            }
        }
        *end_index = index;
    }
    else
    {
        done = true;
    }
    return done;
}


int16_t count_total_lines(char* base_text, int16_t width, int16_t start_index, int16_t end_index)
{
    font_style_t style = GetInterfaceStyle(_computer_interface_font);
    
    int16_t total_line_count = 0;
    int16_t text_end_index = end_index;
    while (!calculate_line_end_index(base_text, style, width, start_index, text_end_index, &end_index))
    {
        total_line_count++;
        start_index = end_index;
    }
    return total_line_count;
}



int16_t calculate_lines_per_page()
{
    int16_t lines_per_page;
    if (!film_profile.calculate_terminal_lines_correctly)
    {
        Rect bounds = get_term_rectangle(_terminal_screen_rect);
        lines_per_page = (RECTANGLE_HEIGHT(&bounds) - 2 * BORDER_HEIGHT) / _get_font_line_height(_computer_interface_font);
        lines_per_page -= FUDGE_FACTOR;
    }
    else
    {
        Rect bounds = get_term_rectangle(_terminal_full_text_rect);
        lines_per_page = RECTANGLE_HEIGHT(&bounds) / _get_font_line_height(_computer_interface_font);
    }
    return lines_per_page;
}


// -----------------------------------------------------------------------------------------
// generate future date string used in terminal header


void get_date_string(char* date_string, bool is_m1)
{
    char temp_string[101];
    int32_t game_time_passed;
    time_t seconds;
    tm game_time;

    /* Treat the date as if it were recent. */
    game_time_passed = INT32_MAX - dynamic_world->game_information.game_time_remaining;
    
    /* convert the game seconds to machine seconds */
    if (is_m1)
    {
        seconds = 809304137;
        seconds += 7 * 60 * (game_time_passed / TICKS_PER_SECOND);
    }
    else
    {
        seconds = 800070137; // Wednesday, May 10, 1995 1:42:17
        seconds += game_time_passed / TICKS_PER_SECOND;
    }
    game_time = *gmtime(&seconds);
    game_time.tm_year  = 437;
    game_time.tm_yday  = 0;
    game_time.tm_isdst = 0;

    getcstr(temp_string, strCOMPUTER_LABELS, _date_format);
    strftime(date_string, 100, temp_string, &game_time);
}

