/*
 StyledFont.cpp
 
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

#include "fonts.hpp"


// TODO: blech. Time to standardize. Let's use '\' as the escape char going forwards as '\' is easier than '$' for modders to see in a block of terminal or other text. It also allows "\$appName$" to be supported without having to change the existing string_resources names or implementation. (Obviously legacy M1+2 terms will continue using their legacy '$'-escape syntax; exporting those terms to plaintext file will use the new syntax, and so should future new scenarios.)



// simplifying FontRenderer to draw a single font face at a single size will greatly simplify things; where styles are needed (e.g. computer terminals), they should use StyledFontRenderer
/*

static inline bool style_code(char c)
{
    switch(tolower(c)) {
    case 'p':
    case 'b':
    case 'i':
    case 'l':
    case 'r':
    case 'c':
    case 's':
        return true;
    default:
        return false;
    }
}



class style_separator
{
public:
    bool operator() (std::string::const_iterator& next, std::string::const_iterator end, std::string& token)
    {
        if (next == end) return false;

        token = std::string();

        // if we start with a token, return it
        if (*next == '|' && next + 1 != end && style_code(*(next + 1)))
        {
            token += *next;
            ++next;
            token += *next;
            ++next;
            return true;
        }

        token += *next;
        ++next;

        // add characters until we hit a token
        for (;next != end && !(*next == '|' && next + 1 != end && style_code(*(next + 1))); ++next)
        {
            token += *next;
        }

        return true;
    }

    void reset() {}

};



static inline bool is_style_token(const std::string& token)
{
    return (token.size() == 2 && token[0] == '|' && style_code(token[1]));
}


static void update_style(uint16& style, const std::string& token)
{
    if (tolower(token[1]) == 'p')
        style &= ~(styleBold | styleItalic);
    else if (tolower(token[1]) == 'b')
    {
        style |= styleBold;
        style &= ~styleItalic;
    }
    else if (tolower(token[1]) == 'i')
    {
        style |= styleItalic;
        style &= ~styleBold;
    }
}


int Font::draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const
{
    if (style & styleShadow)
    {
        _draw_text(s, text, x + 1, y + 1, SDL_MapRGB(s->format, 0x0, 0x0, 0x0), style);
    }
    return _draw_text(s, text, x, y, pixel, style);
}
 
 uint16 Font::text_width(const std::string& text, uint16 style) const
 {
     return _text_width(text, style) + (style & styleShadow ? 1 : 0);
 }
 
 int Font::trunc_text(const std::string& text, int max_width, uint16 style) const
 {
    return _trunc_text_muckroman(text, max_width - (style & styleShadow ? 1 : 0), style);
 }



int StyledFont::draw_styled_text(SDL_Surface *s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const
{
     int width = 0;
     
    boost::tokenizer<style_separator> tok(text.begin(), text.begin() + length);
    for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
    {
        if (is_style_token(*it))
        {
            update_style(style, *it);
        }
        else
        {
            if (style & styleShadow)
            {
                _draw_text(s, it, it->size(), x + width + 1, y + 1, SDL_MapRGB(s->format, 0x0, 0x0, 0x0), style);
            }
            width += _draw_text(s, it, it->size(), x + width, y, pixel, style);
        }
    }
     return width;
}


int StyledFont::styled_text_width(const std::string& text, uint16 style) const
{
     int width = 0;
     
    boost::tokenizer<style_separator> tok(text.begin(), text.begin() + length);
    for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
    {
        if (is_style_token(*it))
        {
            update_style(style, *it);
        }
        else
        {
            width += _text_width(*it, style);
        }
    }

    return (style & styleShadow) ? width + 1 : width;
}


int StyledFont::trunc_styled_text(const std::string& text, int max_width, uint16 style) const
{
     int length = 0;
     
    if (style & styleShadow)
    {
        max_width -= 1;
        style &= (~styleShadow);
    }

    boost::tokenizer<style_separator> tok(text);
    for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
    {
        if (is_style_token(*it))
        {
            update_style(style, *it);
            length += 2;
        }
        else
        {
            int additional_length = _trunc_text(it->c_str(), max_width, style);
            max_width -= _text_width(it->c_str(), additional_length, style);
            length += additional_length;
            if (additional_length < it->size())
                return length;
        }
    }
    return length;
}


std::string StyledFont::style_at(const std::string& text, std::string::const_iterator pos, uint16 style) const
{
    boost::tokenizer<style_separator> tok(text.begin(), pos);
    for (boost::tokenizer<style_separator>::iterator it = tok.begin(); it != tok.end(); ++it)
    {
        if (is_style_token(*it))
            update_style(style, *it);
    }
    
    if (style & styleBold)
        return std::string("|b");
    else if (style & styleItalic)
        return std::string("|i");
    else
        return std::string();
}



*/
