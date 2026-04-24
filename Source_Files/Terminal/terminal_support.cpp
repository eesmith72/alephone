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

#include "fonts.hpp" // Font

#include "map.h" // dynamic_world.game_time_remaining

// TODO: replace screen_rectangle with SDL_Rect?


// -----------------------------------------------------------------------------------------
// nasty externs


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


SDL_Rect get_term_rect(int32_t index)
{
    // the target rects are relative to origin 640x480 screen, but we want them relative to the terminal screen rect's origin
    SDL_Rect term_rect = get_computer_terminal_rect(_terminal_screen_rect); // the computer terminal canvas' origin and size (assuming 640x480 screen)
    SDL_Rect target_rect = get_computer_terminal_rect(index); // header/footer bars, logo position
    target_rect.x -= term_rect.x;
    target_rect.y -= term_rect.y;
    return target_rect;
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
    TODO("redo this once Render2D/ is done");
    
    bool done = false;
/*
    if (base_text[start_index]) // slightly worrisome; presumably it's looking for '\0' that's inserted into the original C string during parsing to split it at group boundaries (this is nice and efficient), though it'd be worth considering structured JSON/XML as future-proof UTF8 terminal text format)
    {
        int32_t index = start_index, running_width = 0;
        
        // terminal_font no longer a global, since it may change
        Font* terminal_font = get_interface_font(_computer_interface_font);

        
        while (running_width < line_width && base_text[index] && !is_line_break(base_text[index]))
        {
            running_width += terminal_font->char_width_muckroman(base_text[index], font_style); // TODO: it is unclear why style is needed when font is supposed to be monospace; OTOH, it won't behave correctly if font is variable-width as styles can change along line
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
    }*/
    return done;
}


int16_t count_total_lines(char* base_text, int16_t width, int16_t start_index, int16_t end_index)
{
    
    int16_t total_line_count = 0;
    int16_t text_end_index = end_index;
    // TODO: FIX
    /*
    font_style_t style = GetInterfaceStyle(_computer_interface_font);
    while (!calculate_line_end_index(base_text, style, width, start_index, text_end_index, &end_index))
    {
        total_line_count++;
        start_index = end_index;
    }
     */
    return total_line_count;
}



int16_t calculate_lines_per_page()
{
    int16_t lines_per_page;
    
    const font_t* font = get_interface_font(_computer_interface_font);
    
    if (!film_profile.calculate_terminal_lines_correctly)
    {
        SDL_Rect bounds = get_term_rect(_terminal_screen_rect);
        lines_per_page = (bounds.h - 2 * BORDER_HEIGHT) / font->line_height;
        lines_per_page -= FUDGE_FACTOR;
    }
    else
    {
        SDL_Rect bounds = get_term_rect(_terminal_full_text_rect);
        lines_per_page = bounds.h / font->line_height;
    }
    return lines_per_page;
}


// -----------------------------------------------------------------------------------------
// generate future date string used in terminal header

const std::string pad_2(uint64_t n)
{
    return n < 10 ? "0" + std::to_string(n) : std::to_string(n);
}


const std::string get_date_string(bool is_m1)
{
    // Treat the date as if it were recent
    int32_t game_time_passed = (INT32_MAX - dynamic_world.game_information.game_time_remaining) / TICKS_PER_SECOND;
    
    // convert the game seconds to machine seconds
    time_t seconds = is_m1 ? (809304137 + 7 * 60 * game_time_passed)
                           : (800070137 + game_time_passed); // Wednesday, May 10, 1995 1:42:17
    
    tm game_time = *gmtime(&seconds);
    game_time.tm_year  = 437; // TODO: why is this being replaced?
    game_time.tm_yday  = 0;   // TODO: ditto
    game_time.tm_isdst = 0;
    
    return get_string(STRID(strCOMPUTER_TERMINAL_LABELS, _date_format), {
        {"$year$",   [game_time]{ return pad_2(game_time.tm_year); }},
        {"$month$",  [game_time]{ return pad_2(game_time.tm_mon);  }},
        {"$day$",    [game_time]{ return pad_2(game_time.tm_mday); }},
        {"$hour$",   [game_time]{ return pad_2(game_time.tm_hour); }},
        {"$minute$", [game_time]{ return pad_2(game_time.tm_min);  }},
        {"$second$", [game_time]{ return pad_2(game_time.tm_sec);  }},
    });
}

