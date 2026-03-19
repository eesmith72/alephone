/*
 Font.hpp - SDL font handling
 
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

#ifndef __Font_hpp__
#define __Font_hpp__

#include "cseries.h"

#include "DataFile.hpp"

#include "resource_manager.h"

#include <SDL2/SDL_ttf.h>



typedef int16_t font_id_t;
typedef uint16_t font_style_t; // TODO: proper typing will have to wait
typedef int32_t font_size_t;

typedef int16_t font_color_t; // 0-7 // TODO: `color_id_t`? (Q. Where are 8-color schemes used? terminals, team colors; anything else? Are they the same, or do they vary?)


#define get_real_font_style(font_style)     ((font_style_t)(font_style & (styleBold | styleItalic)))
#define get_extended_font_style(font_style) ((font_style_t)(font_style & ~(styleBold | styleItalic)))


enum styles : uint16_t
{
    // real font styles (most TTF font families will provide Bold, Italic, and BoldItalic variants to their Normal style)
    // (SDL_ttf can synthesize bold and italic styles if font files aren't available)
    styleNormal        = TTF_STYLE_NORMAL,
    styleBold          = TTF_STYLE_BOLD,
    styleItalic        = TTF_STYLE_ITALIC,
    // synthesized styles (these are achieved in the code using extra drawing tricks)
    styleUnderline     = TTF_STYLE_UNDERLINE,
    styleStrikethrough = TTF_STYLE_STRIKETHROUGH, // originally styleOutline (which isn't supported); EES: don't recall outlined text ever appearing in the game, so we should be fine repurposing this bitflag
    styleShadow        = 0x10, // TTF_Font can't synthesize this itself
};


enum {
    kFontIDUnknown =  0,
    kFontIDMono    =  1,
    kFontIDMonaco  =  4,
    kFontIDCourier = 22,
};

enum { /* justification flags for screen_drawing___draw_screen_text */
    _no_justification  = 0x00,
    _center_horizontal = 0x01,
    _center_vertical   = 0x02,
    _right_justified   = 0x04,
    _top_justified     = 0x08,
    _bottom_justified  = 0x10,
    _wrap_text         = 0x20,
};

enum {
    // HUD
    _interface_font,
    _weapon_name_font,
    _player_name_font,
    _interface_item_count_font,
    // terminal
    _computer_interface_font,
    _computer_interface_title_font,
    // ?
    _net_stats_font,
    NUMBER_OF_INTERFACE_FONTS
};


// lookup key for active_fonts table
struct font_key_t
{
    font_id_t    font_id = kFontIDUnknown;
    font_style_t style   = styleNormal;
    font_size_t  size    = 12;
    
    font_key_t get_file_key() const { return {font_id, get_real_font_style(style), 0}; }
    
    const bool operator==(const font_key_t& other) const
    {
        return (font_id == other.font_id && style == other.style && size == other.size);
    }
};


struct font_t;
const font_t* get_font_for_key(const font_key_t& key);

#define flip_bitflag(value, flag)  ((value) & (flag) ? (value) & ~(flag) : (value) | (flag))

struct font_t
{
    font_key_t key;
    TTF_Font* font;
    font_size_t adjust_height; // TODO: ignore this for now
    
    // in FontSpecifier:
    // Monaco's size = size * 1.34
    // Courier's adjust_height -= size * 0.084 -- i.e. looks like reduced leading
    
    font_size_t ascent, height, line_height, descent, leading;
    
    // TODO: check where adjust_height was being supplied
    font_t(font_key_t key, TTF_Font* font, font_size_t adjust_height);
    
    int32_t measure_width(const std::string text) const
    {
        int32_t width;
        TTF_SizeUTF8(font, text.c_str(), &width, nullptr); // returns -1 on error (why would it fail?)
        if (key.style & styleShadow) { width += 1; } // TODO: this needs to be % to scale properly
        return width;
    }
    
    int32_t measure_styled_width(const std::string text) const // TODO: implement (it'd be best if all styled text was pre-parsed into same data structures as terminal text; each Text object can implement its own width method)
    {
        return measure_width(text);
    }
    
    const font_t* embolden() const { return get_font_for_key({key.font_id, (font_style_t)flip_bitflag(key.style, styleBold), key.size}); }
    
    const font_t* italicize() const { return get_font_for_key({key.font_id, (font_style_t)flip_bitflag(key.style, styleItalic), key.size}); }
    
    const font_t* shadowed() const { return get_font_for_key({key.font_id, (font_style_t)flip_bitflag(key.style, styleShadow), key.size}); }
};


struct font_family_t
{
    std::string family_name;
    font_id_t   font_id       = kFontIDUnknown; // font family; if Unknown, a new id will be auto-generated for this family
    font_size_t adjust_height = 0; // leading adjustment; TODO: it'd be more logical if MML specified a leading value, but leaving at this time

    // paths to fonts; normal is required, others are optional
    ao_path normal;
    ao_path bold;
    ao_path italic;
    ao_path bold_italic;
};


// -----------------------------------------------------------------------------------------
// font management


// Called once on startup.
void initialize_fonts();


// Whenever the user switches scenario, it's simplest to yeet everything and reload clean.
void reset_fonts();


// Register a font family defined in MML. The specification must contain a font family name and/or id.
// If a font_id wasn't given, on return the spec contains the generated id to use in font keys.
ao_err add_font_specification(font_family_t &spec);


// Get a font with the specified family, style, and size.
// This will always return a valid font_t* pointer. Caution: fonts.cpp retains ownership so, while a caller can retain that borrowed pointer for efficiency, it MUST not be used after reset_fonts is called.
const font_t* get_font_for_key(const font_key_t& key);

const font_t* get_interface_font(int32_t index); // standard M2 or defined in <interface>


// -----------------------------------------------------------------------------------------
// MML

class InfoTree;
void reset_mml_interface_fonts();

void parse_mml_interface_fonts(const InfoTree& root);


#endif /* __Font_hpp__ */
