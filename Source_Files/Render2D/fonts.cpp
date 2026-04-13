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

#include "fonts.hpp"

#include "find_files.hpp"

#include "resource_manager.h"
#include "DataFile.hpp"

//#include <boost/tokenizer.hpp> // because `w_styled_text` has its own markup scheme that appears to be different terminals' markup scheme; typical

#include "preferences.h" // environment_preferences.smooth_text setting

#include "InfoTree.h"


// built-in fonts embedded
#include "AlephSansMono-Bold.h"
#include "ProFontAO.h"
#include "CourierPrime.h"
#include "CourierPrimeBold.h"
#include "CourierPrimeItalic.h"
#include "CourierPrimeBoldItalic.h"



// From shell_sdl.cpp
extern std::vector<ao_path> scenario_data_search_paths; // idiocy // TODO: Scenario/ needs to provide APIs for finding all assets, including TTL and bitmap fonts



font_t::font_t(font_key_t key, TTF_Font* font, font_size_t adjust_height) : key(key), font(font), adjust_height(adjust_height)
{
    TTF_SetFontHinting(font, environment_preferences.smooth_text ? TTF_HINTING_LIGHT : TTF_HINTING_MONO);
    
    ascent  = TTF_FontAscent(font);
    height  = TTF_FontHeight(font);
    descent = TTF_FontDescent(font);
    
    font_size_t measured_height;
    TTF_SizeText(font, "Ag", nullptr, &measured_height);
    
    line_height = std::max({(font_size_t)TTF_FontLineSkip(font), height, measured_height}); // TODO: should adjust_height be added to line_height here?
    leading = line_height - ascent - descent;
}



// -----------------------------------------------------------------------------------------
// built-in fonts

// note: the .ttf files have been serialized into header file uint8_t[] consts for OS-agnostic embedding (e.g. `courier_prime_italic` in CourierPrimeItalic.h)

// TODO: we really want a general-purpose system for embedding *any* asset as zipped data (preferably a modern, standard, Extension format), at which point built-in fonts go in that; such a system would allow the default Trilogy scenarios (M1-3 minus Music resources, which can be a downloadable Extension) to be embedded too, simplifying packaging and distribution


struct builtin_font_t
{
    std::string   family_name;
    int16_t       font_id;
    font_style_t  style;
    uint8_t*      data;
    uint32_t      data_size;
};


static const std::array<builtin_font_t, 6> builtin_fonts = {
    "mono",                       kFontIDMono,     styleNormal,              aleph_sans_mono_bold,       sizeof(aleph_sans_mono_bold),
    "Monaco",                     kFontIDMonaco,   styleNormal,              pro_font_ao,                sizeof(pro_font_ao),
    "Courier Prime",              kFontIDCourier,  styleNormal,              courier_prime,              sizeof(courier_prime),
    // These should really be "Courier Prime", but it's possible existing MMLs refer to them by these names so leave as-is (all names will return the same font ID).
    "Courier Prime Bold",         kFontIDCourier,  styleBold,                courier_prime_bold,         sizeof(courier_prime_bold),
    "Courier Prime Italic",       kFontIDCourier,  styleItalic,              courier_prime_italic,       sizeof(courier_prime_italic),
    "Courier Prime Bold Italic",  kFontIDCourier,  styleBold | styleItalic,  courier_prime_bold_italic,  sizeof(courier_prime_bold_italic),
};

// This must be greater than kFontIDMono/kFontIDMonaco/kFontIDCourier:
#define MAX_BUILTIN_FONT_ID (32)


const font_id_t default_font_id = kFontIDCourier; // the default font MUST have all 4 real styles in the builtin_fonts table (while SDL_ttf could synthesize bold and/or italic styles, this is already the last-ditch fallback so KISS)


// Copied from M2's original 'finf' resource, which defines the standard UI, HUD, and computer terminal fonts
static const std::array<font_key_t, NUMBER_OF_INTERFACE_FONTS> interface_font_keys_std = {
    kFontIDMonaco,  styleBold,    9, // _interface_font,
    kFontIDMonaco,  styleBold,    9, // _weapon_name_font,
    kFontIDMonaco,  styleBold,    9, // _player_name_font,
    kFontIDMonaco,  styleNormal,  9, // _interface_item_count_font,
    kFontIDCourier, styleNormal, 12, // _computer_interface_font,
    kFontIDCourier, styleBold,   14, // _computer_interface_title_font,
    kFontIDMonaco,  styleNormal,  9, // _net_stats_font,
};


// initially the M2 defaults which may be partially/fully overridden by MML
static std::array<font_key_t, NUMBER_OF_INTERFACE_FONTS> interface_font_keys = interface_font_keys_std;


static std::map<std::string, font_id_t> font_ids_by_family_name;


static font_id_t make_font_id_for_name(const std::string family_name) // called by add_font_specification if the given font spec doesn't have an existing font id
{
    static font_id_t id_count = MAX_BUILTIN_FONT_ID;
    font_id_t font_id;
    auto it = font_ids_by_family_name.find(family_name);
    if (it == font_ids_by_family_name.end())
    {
        font_id = ++id_count;
        font_ids_by_family_name[family_name] = font_id;
    }
    else
    {
        font_id = it->second;
    }
    return font_id;
}


struct hash_font_key_t
{
    size_t operator()(const font_key_t& s) const
    {
        return ((size_t)s.font_id) << 48 | ((size_t)s.style) << 32 | s.size;
    }
};


static std::unordered_map<font_key_t, ao_path, hash_font_key_t> available_font_files; // font keys' size is always 0 here

inline ao_path get_font_file_path(font_key_t& key)
{
    auto it = available_font_files.find(key);
    return it == available_font_files.end() ? ao_path("") : it->second;
}


// all currently loaded TTF_Fonts, e.g. Monaco Bold at 9pt
static std::unordered_map<font_key_t, font_t, hash_font_key_t> active_fonts;


// -----------------------------------------------------------------------------------------
//


static TTF_Font* read_builtin_font_for_key(const font_key_t& key)
{
    for (const auto& builtin_font : builtin_fonts)
    {
        if (builtin_font.font_id == key.font_id && builtin_font.style == key.style)
        {
            return TTF_OpenFontRW(SDL_RWFromConstMem(builtin_font.data, builtin_font.data_size), 0, key.size); // let's assume this never fails
        }
    }
    return nullptr;
}


static TTF_Font* read_font_file_for_key(const font_key_t& key, int32_t& synthesized_style)
{
    font_key_t file_key = key.get_file_key();
    ao_path font_path = get_font_file_path(file_key);
    int32_t synthesized_style_copy = synthesized_style;
    
    if (font_path.empty()) // ...if not, synthesize/substitute as appropriate
    {
        switch (file_key.style)
        {
            case styleBold | styleItalic:
                file_key.style = styleBold;
                synthesized_style |= TTF_STYLE_ITALIC;
                font_path = get_font_file_path(file_key);
                if (!font_path.empty()) break; // if there's a bold font file, synthesize its italic...
                // ...otherwise fall-thru to synthesize bold as well
            case styleBold:
                file_key.style = styleNormal;
                synthesized_style |= TTF_STYLE_BOLD;
                font_path = get_font_file_path(file_key);
                break;
            case styleItalic:
                file_key.style = styleNormal;
                synthesized_style |= TTF_STYLE_ITALIC;
                // fall-thru to synthesize italic from normal
            case styleNormal:
                font_path = get_font_file_path(file_key);
                break;
        }
    }
    TTF_Font* font = nullptr;
    
    if (!font_path.empty())
    {
        DataFile file;
        if (file.open(font_path) == no_err) { font = TTF_OpenFontRW(file.take_rwops(), 1, key.size); }
        
        if (!font) { log_error_f("Failed to read font file at: %s", file.get_path().c_str()); }
    }
    
    if (!font) { synthesized_style = synthesized_style_copy; }
    
    return font;
}


// -----------------------------------------------------------------------------------------


/*
uint16 Font_TTF::text_width(const std::string& text, uint16 style) const
{
    int width = 0;
    const char* label = text.c_str();
    TTF_SizeUTF8(get_ttf(style), label, &width, 0);
    
    return width;
}


int32_t Font_TTF::trunc_text(const std::string& text, int32_t max_width, uint16_t style) const
{
    TODO("overhaul text rendering");
    return -1;
}



int Font_TTF::draw_text(SDL_Surface *s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const
{
    int clip_top, clip_bottom, clip_left, clip_right;
    if (draw_clip_rect_active) {
        clip_top = draw_clip_rect.top;
        clip_right = draw_clip_rect.right;
        clip_bottom = draw_clip_rect.bottom;
        clip_left = draw_clip_rect.left;
    } else {
        clip_top = clip_left = 0;
        clip_right = s->w;
        clip_bottom = s->h;
    }

    SDL_Color c;
    SDL_GetRGB(pixel, s->format, &c.r, &c.g, &c.b);
    c.a = 0xff;
    SDL_Surface *text_surface = 0;
    
    //char *temp = process_printable(text.c_str(), (int32_t)length); // EES: TODO: I'm really throwing caution to the wind here but, really, every external string should be sanitized ONCE, when it's initially read in, not down here in rendering a million times over. So this line is out, but this TODO remains until Files/ gets overhauled to do its job right.
    
    if (environment_preferences.smooth_text) // user can choose modern HD vs classic blocky appearance
    {
        text_surface = TTF_RenderUTF8_Blended(get_ttf(style), text.c_str(), c);
    }
    else
    {
        // TODO: once we unfuck SDL-based screen drawing to use a single SDL_Surface at true screen resolution (not 640x480 or whatever the current atrocious screen.cpp &co uses), the FontRenderer will need to know what size to make the font as if we're drawing on HD Surfaces at true screen resolution, not old-fashioned game lo-res then even _Solid-drawn text will look smooth on a 4K screen (for blocky, we need to draw font at smaller point size then rescale the Surface); OGL screen drawing will need similar considerations
        text_surface = TTF_RenderUTF8_Solid(get_ttf(style), text.c_str(), c);
    }
    if (!text_surface) return 0;
    
    SDL_Rect dst_rect;
    dst_rect.x = x;
    dst_rect.y = y - TTF_FontAscent(get_ttf(style));

    if (draw_clip_rect_active)
    {
        SDL_Rect src_rect;
        src_rect.x = 0;
        src_rect.y = 0;

        if (clip_top > dst_rect.y)
        {
            src_rect.y += dst_rect.y - clip_top;
        }

        if (clip_left > dst_rect.x)
        {
            src_rect.x += dst_rect.x - clip_left;
        }

        src_rect.w = (clip_right > dst_rect.x) ? clip_right - dst_rect.x : 0;
        src_rect.h = (clip_bottom > dst_rect.y) ? clip_bottom - dst_rect.y : 0;

        SDL_BlitSurface(text_surface, &src_rect, s, &dst_rect);
    }
    else
        SDL_BlitSurface(text_surface, NULL, s, &dst_rect);

    if (style & styleUnderline)
    {
        SDL_Rect r = {x, y + 1, text_surface->w, 1};
        if (draw_clip_rect_active)
        {
            r.x = MAX(x, clip_left);
            r.w = MAX(0, MIN(x + text_surface->w, clip_right) - r.x);
            r.y = MAX(y + 1, clip_top);
            r.h = MAX(0, MIN(y + 2, clip_bottom) - r.y);
        }
        SDL_FillRect(s, &r, pixel);
    }
    if (s == MainScreenSurface())
        sw_render_surface_to_screenRect(x, y - TTF_FontAscent(get_ttf(style)), text_width(text, style), TTF_FontHeight(get_ttf(style)));

    int width = text_surface->w;
    SDL_FreeSurface(text_surface);
    return width;
}
*/


// -----------------------------------------------------------------------------------------
// Font management


void initialize_fonts() 
{
    reset_fonts();
}


void reset_fonts()
{
    for (const auto& font : active_fonts)
    {
        TTF_CloseFont(font.second.font);
    }
    
    active_fonts.clear();
    available_font_files.clear();
    font_ids_by_family_name.clear(); // TODO: not sure if this should be cleared or not; can decide when overhauling MML (the root problem is MML IDs aren't unique across plugins, so 2 plugins may define the same ID for different things)
    
    // Register the built-in fonts so they can be looked up by family name ("mono", "Monaco", "Courier Prime").
    for (const auto& font : builtin_fonts)
    {
        font_ids_by_family_name[font.family_name] = font.font_id;
    }
}


ao_err add_font_specification(font_family_t& spec)
{
    // TODO: what if font id is given but conflicts with an existing font
    
    if (spec.font_id == kFontIDUnknown)
    {
        if (spec.family_name.empty()) { return STRID(strERRORS, 99); } // TODO: error code?
        spec.font_id = make_font_id_for_name(spec.family_name);
    }
    if (spec.normal.empty()) return STRID(strERRORS, missingFile);
    
    // TODO: where to store height adjust value if non-zero?
    
    ao_path path = find_file_at_subpath(spec.normal);
    if (path.empty()) return STRID(strERRORS, missingFile);
    
    available_font_files[{spec.font_id, styleNormal}] = path;
    
    if (!spec.bold.empty())
    {
        ao_path path = find_file_at_subpath(spec.bold);
        if (!path.empty()) { available_font_files[{spec.font_id, styleBold}] = path; } // TODO: should these log?
    }
    
    if (!spec.italic.empty())
    {
        ao_path path = find_file_at_subpath(spec.italic);
        if (!path.empty()) { available_font_files[{spec.font_id, styleItalic}] = path; }
    }
    
    if (!spec.bold_italic.empty())
    {
        ao_path path = find_file_at_subpath(spec.bold_italic);
        if (!path.empty()) { available_font_files[{spec.font_id, styleBold | styleItalic}] = path; }
    }
    
    return no_err;
}


const font_t* get_font_for_key(const font_key_t& key)
{
    assert_fail(key.size > 0 && key.size < 120, "Malformed font key."); // TODO: how best to guard? (values should be sanitized when read from MML)
    
    // If there's a font with this exact style and size already active, return it
    auto it = active_fonts.find(key);
    if (it != active_fonts.end()) { return &it->second; } // (Note: If, somehow, a font is requested before its font family specification is defined, the substituted font will always be returned in future, even after the correct font file becomes available. In practice, this shouldn't be an issue as long as MML loading order loads font specs first. If it is an issue, then we could compare font_ids here to determine if the returned font_t is a substitute and attempt reloading it if it is, but KISS for now.)
    
    // otherwise, first see if there's a font file available...
    font_key_t real_key = {key.font_id, get_real_font_style(key.style), key.size};
    int32_t synthesized_style = get_extended_font_style(key.style);
    
    TTF_Font* ttf_font = read_font_file_for_key(real_key, synthesized_style);
    
    // couldn't find a font file, so let's try built-ins
    if (!ttf_font)
    {
        ttf_font = read_builtin_font_for_key(real_key);
        if (!ttf_font && real_key.style != styleNormal)
        {
            if (real_key.style & styleBold)   { synthesized_style |= TTF_STYLE_BOLD; }
            if (real_key.style & styleItalic) { synthesized_style |= TTF_STYLE_ITALIC; }
            real_key.style = styleNormal;
        }
        ttf_font = read_builtin_font_for_key(real_key);
        
        // it wasn't a built-in font either, so return the default font as fallback
        if (!ttf_font)
        {
            real_key = {default_font_id, get_real_font_style(key.style), key.size};
            synthesized_style = get_extended_font_style(key.style);
            ttf_font = read_builtin_font_for_key(real_key);
            assert_fail_f(ttf_font, "Reading a fallback font {%d, %d, %d} should never fail.", real_key.font_id, real_key.style, real_key.size);
        }
    }
    
    // note: SDL_ttf cannot synthesize a shadow style so that must be composed by the text renderer when blitting
    if (synthesized_style) { TTF_SetFontStyle(ttf_font, synthesized_style); }
    active_fonts.emplace(key, font_t(key, ttf_font, 0));
    
    return &active_fonts.at(key);
}


const font_t* get_interface_font(int32_t index)
{
    return get_font_for_key(interface_font_keys.at(index));
}



// MML

void reset_mml_interface_fonts()
{
    interface_font_keys = interface_font_keys_std;
    reset_fonts();
}


void parse_mml_interface_fonts(const InfoTree& root)
{
    for (const InfoTree& font : root.children_named("font"))
    {
        int16 index;
        if (font.read_indexed("index", index, NUMBER_OF_INTERFACE_FONTS))
        {
            font.read_font(interface_font_keys[index]);
        }
    }
}
