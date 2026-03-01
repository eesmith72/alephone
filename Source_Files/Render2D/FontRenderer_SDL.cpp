/*
 FontRenderer_SDL.hpp - SDL font handling
 
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

#include "FontRenderer_SDL.hpp"

#include "resource_manager.h"
#include "FileHandler.h"

#include <boost/tokenizer.hpp> // because `w_styled_text` has its own markup scheme that appears to be different terminals' markup scheme; typical

#include "preferences.h" // smooth_font setting

#include "screen.h" // MainScreenSurface, MainScreenUpdateRect (see screen.h, screen_drawing.h, shared_screen.h as that stuff's all over the place)
#include "screen_drawing.h" // screen_rectangle

// From screen_drawing.cpp; see the TODO there re. Cohen/Sutherland
extern bool draw_clip_rect_active;
extern screen_rectangle draw_clip_rect;


// built-in fonts embedded
#include "AlephSansMono-Bold.h"
#include "ProFontAO.h"
#include "CourierPrime.h"
#include "CourierPrimeBold.h"
#include "CourierPrimeItalic.h"
#include "CourierPrimeBoldItalic.h"


// -----------------------------------------------------------------------------------------
// the try_to_load_as_TTF/Pixmap_font functions below reference these

// Global variables
typedef std::pair<int, int> id_and_size_t;
typedef std::map<id_and_size_t, FontRenderer_SDL_Pixmap *> font_list_t;
static font_list_t font_list;				// List of all loaded fonts



typedef std::pair<TTF_Font *, int> ref_counted_ttf_font_t;
typedef std::map<ttf_font_key_t, ref_counted_ttf_font_t> ttf_font_list_t;
static ttf_font_list_t ttf_font_list;



// From shell_sdl.cpp
extern std::vector<DirectorySpecifier> data_search_path; // idiocy // TODO: Scenario/ needs to provide APIs for finding all assets, including TTL and bitmap fonts



// -----------------------------------------------------------------------------------------
// built-in fonts

// note: the .ttf files have been serialized into header file uint8_t[] consts for OS-agnostic embedding (e.g. `courier_prime_italic` in CourierPrimeItalic.h)

// TODO: we really want a general-purpose system for embedding *any* asset as zipped data (preferably a modern, standard, Extension format), at which point built-in fonts go in that; such a system would allow the default Trilogy scenarios (M1-3 minus Music resources, which can be a downloadable Extension) to be embedded too, simplifying packaging and distribution


struct builtin_font_t
{
	std::string name;
	uint8_t* data;
	uint32_t size;
};


static builtin_font_t builtin_fontspecs[] = {
	{ "mono",                       aleph_sans_mono_bold,       sizeof(aleph_sans_mono_bold) },
	{ "Monaco",                     pro_font_ao,                sizeof(pro_font_ao) },
	{ "Courier Prime",              courier_prime,              sizeof(courier_prime) },
	{ "Courier Prime Bold",         courier_prime_bold,         sizeof(courier_prime_bold) },
	{ "Courier Prime Italic",       courier_prime_italic,       sizeof(courier_prime_italic) },
	{" Courier Prime Bold Italic",  courier_prime_bold_italic,  sizeof(courier_prime_bold_italic) }
};
#define NUMBER_OF_BUILTIN_FONTS sizeof(builtin_fontspecs) / sizeof(builtin_font_t)


typedef std::map<std::string, builtin_font_t> builtin_fonts_t;

builtin_fonts_t builtin_fonts;


// -----------------------------------------------------------------------------------------
// used by try_to_load_as_ttf/pixmap_font functions below

// EES: I'm assuming if the font is a built-in, the 'path' will be one of the names defined in builtin_fontspecs above, otherwise it's a path relative to one of the standard search directories; presumably "" translates as "use the default 'mono' font"; TODO: for a simple "find the asset" request, the code itself is stupidly complex and spread all over, but that's a job for Files/
static const std::string locate_font(const std::string& path)
{
    builtin_fonts_t::iterator j = builtin_fonts.find(path);
    if (j != builtin_fonts.end() || path == "")
    {
        return path;
    }
    else //
    {
        static FileSpecifier file;
        return (file.SetNameWithPath(path.c_str())) ? file.GetPath() : "";
    }
}


// -----------------------------------------------------------------------------------------




static TTF_Font *load_ttf_font(const std::string& path, uint16 style, int16 size)
{
    // already loaded? increment reference counter and return pointer
    ttf_font_key_t search_key(path, style, size);
    ttf_font_list_t::iterator it = ttf_font_list.find(search_key);
    if (it != ttf_font_list.end())
    {
        TTF_Font *font = it->second.first;
        it->second.second++;

        return font;
    }

    TTF_Font *font = 0;
    builtin_fonts_t::iterator j = builtin_fonts.find(path);
    if (j != builtin_fonts.end())
    {
        font = TTF_OpenFontRW(SDL_RWFromConstMem(j->second.data, j->second.size), 0, size);
    }
    else
    {
        FileSpecifier fileSpec(path);
        OpenedFile file;
        if (fileSpec.Open(file))
        {
            font = TTF_OpenFontRW(file.TakeRWops(), 1, size);
        }
    }

    if (font)
    {
        int ttf_style = TTF_STYLE_NORMAL;
        if (style & styleBold)
            ttf_style |= TTF_STYLE_BOLD;
        if (style & styleItalic)
            ttf_style |= TTF_STYLE_ITALIC;
        
        TTF_SetFontStyle(font, ttf_style);
#ifdef TTF_HINTING_LIGHT
        if (environment_preferences->smooth_text)
            TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
        else
            TTF_SetFontHinting(font, TTF_HINTING_MONO);
#endif
        
        ttf_font_key_t key(path, style, size);
        ref_counted_ttf_font_t value(font, 1);
        
        ttf_font_list[key] = value;
    }
    
    return font;
}


FontRenderer_SDL_TTF* try_to_load_as_ttf_font(std::string &file, const TextSpec &spec)
{
    FontRenderer_SDL_TTF *info;
    
    TTF_Font *font = load_ttf_font(file, 0, spec.size);
    if (! font) return nullptr;
    
    info = new FontRenderer_SDL_TTF;
    
    info->m_adjust_height = spec.adjust_height;
    info->m_styles[styleNormal] = font;
    info->m_keys[styleNormal] = ttf_font_key_t(file, 0, spec.size);
    
    // SDL_TTF doesn't do a great job determining the height of fonts...
    int height;
    TTF_SizeText(font, "Ag", nullptr, &height);
    
    info->m_line_height = std::max({
        TTF_FontLineSkip(font),
        TTF_FontHeight(font),
        height
    });
    
    
    // load bold face
    file = locate_font(spec.bold);
    font = load_ttf_font(file, styleNormal, spec.size);
    if (font)
    {
        info->m_styles[styleBold] = font;
        info->m_keys[styleBold] = ttf_font_key_t(file, styleNormal, spec.size);
    }
    else
    {
        file = locate_font(spec.normal);
        font = load_ttf_font(file, styleBold, spec.size);
        assert_fail(font, ""); // I loaded you once, you should load again
        info->m_styles[styleBold] = font;
        info->m_keys[styleBold] = ttf_font_key_t(file, styleBold, spec.size);
    }
    
    
    
    // oblique
    file = locate_font(spec.oblique);
    font = load_ttf_font(file, styleNormal, spec.size);
    if (font)
    {
        info->m_styles[styleItalic] = font;
        info->m_keys[styleItalic] = ttf_font_key_t(file, styleNormal, spec.size);
    }
    else
    {
        file = locate_font(spec.normal);
        font = load_ttf_font(file, styleItalic, spec.size);
        assert_fail(font, ""); // same as above
        info->m_styles[styleItalic] = font;
        info->m_keys[styleItalic] = ttf_font_key_t(file, styleItalic, spec.size);
    }
    
    
    
    // bold oblique
    file = locate_font(spec.bold_oblique);
    font = load_ttf_font(file, styleNormal, spec.size);
    if (font)
    {
        info->m_styles[styleBold | styleItalic] = font;
        info->m_keys[styleBold | styleItalic] = ttf_font_key_t(file, styleNormal, spec.size);
    }
    else
    {
        // try boldening the oblique
        file = locate_font(spec.oblique);
        font = load_ttf_font(file, styleBold, spec.size);
        if (font)
        {
            info->m_styles[styleBold | styleItalic] = font;
            info->m_keys[styleBold | styleItalic] = ttf_font_key_t(file, styleBold, spec.size);
        }
        else
        {
            // try obliquing the bold!
            file = locate_font(spec.bold);
            font = load_ttf_font(file, styleItalic, spec.size);
            if (font)
            {
                info->m_styles[styleBold | styleItalic] = font;
                info->m_keys[styleBold | styleItalic] = ttf_font_key_t(file, styleItalic, spec.size);
            }
            else
            {
                file = locate_font(spec.normal);
                font = load_ttf_font(file, styleBold | styleItalic, spec.size);
                assert_fail(font, "");
                info->m_styles[styleBold | styleItalic] = font;
                info->m_keys[styleBold | styleItalic] = ttf_font_key_t(file, styleBold | styleItalic, spec.size);
            }
        }
    }

    return info;
}


void FontRenderer_SDL_TTF::unload()
{
    for (int i = 0; i < styleUnderline; ++i)
    {
        ttf_font_list_t::iterator it = ttf_font_list.find(m_keys[i]);
        if (it != ttf_font_list.end())
        {
            --(it->second.second);
            if (it->second.second <= 0)
            {
                TTF_CloseFont(it->second.first);
                ttf_font_list.erase(m_keys[i]);
            }
        }

        m_styles[i] = 0;
    }

    delete this;
}

uint16 FontRenderer_SDL_TTF::text_width(const std::string& text, uint16 style) const
{
    int width = 0;
    TTF_SizeUTF8(get_ttf(style), text.c_str(), &width, 0);
    
    return width;
}


int32_t FontRenderer_SDL_TTF::trunc_text(const std::string& text, int32_t max_width, uint16_t style) const
{
    TODO("overhaul text rendering");
    return -1;
}



int FontRenderer_SDL_TTF::draw_text(SDL_Surface *s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const
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
    
    if (environment_preferences->smooth_text) // user can choose modern HD vs classic blocky appearance
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
        MainScreenUpdateRect(x, y - TTF_FontAscent(get_ttf(style)), text_width(text, style), TTF_FontHeight(get_ttf(style)));

    int width = text_surface->w;
    SDL_FreeSurface(text_surface);
    return width;
}


// -----------------------------------------------------------------------------------------
// FontRenderer_SDL_Pixmap


FontRenderer_SDL_Pixmap *try_to_load_as_pixmap_font(const TextSpec &spec)
{
    FontRenderer_SDL_Pixmap *info = NULL;

    // Look for ID/size in list of loaded fonts
    id_and_size_t id_and_size(spec.font, spec.size);
    font_list_t::const_iterator it = font_list.find(id_and_size);
    if (it != font_list.end()) {    // already loaded
        info = it->second;
        info->ref_count++;
        return info;
    }

    // Load font family resource
    LoadedResource fond;
    if (!get_resource(FOUR_CHARS_TO_INT('F', 'O', 'N', 'D'), spec.font, fond)) {
        fprintf(stderr, "Font family resource for font ID %d not found\n", spec.font);
        return NULL;
    }
    SDL_RWops *p = SDL_RWFromMem(fond.GetPointer(), (int)fond.GetLength());
    assert_fail(p, "failed to read pixmap font");

    // Look for font size in association table
    SDL_RWseek(p, 52, SEEK_SET);
    int num_assoc = SDL_ReadBE16(p) + 1;
    while (num_assoc--) {
        int size = SDL_ReadBE16(p);
        SDL_ReadBE16(p); // skip style
        int id = SDL_ReadBE16(p);
        if (size == spec.size) {

            // Size found, load bitmap font resource
            info = new FontRenderer_SDL_Pixmap;
            if (!get_resource(FOUR_CHARS_TO_INT('N', 'F', 'N', 'T'), id, info->rsrc))
                get_resource(FOUR_CHARS_TO_INT('F', 'O', 'N', 'T'), id, info->rsrc);
            if (info->rsrc.IsLoaded()) {

                // Found, switch stream to font resource
                SDL_RWclose(p);
                p = SDL_RWFromMem(info->rsrc.GetPointer(), (int)info->rsrc.GetLength());
                assert_fail(p, "failed to read pixmap font");
                void *font_ptr = info->rsrc.GetPointer(true);

                // Read font information
                SDL_RWseek(p, 2, SEEK_CUR);
                info->first_character = static_cast<uint8>(SDL_ReadBE16(p));
                info->last_character = static_cast<uint8>(SDL_ReadBE16(p));
                SDL_RWseek(p, 2, SEEK_CUR);
                info->maximum_kerning = SDL_ReadBE16(p);
                SDL_RWseek(p, 2, SEEK_CUR);
                info->rect_width = SDL_ReadBE16(p);
                info->rect_height = SDL_ReadBE16(p);
                SDL_RWseek(p, 2, SEEK_CUR);
                info->ascent = SDL_ReadBE16(p);
                info->descent = SDL_ReadBE16(p);
                info->leading = SDL_ReadBE16(p);
                int bytes_per_row = SDL_ReadBE16(p) * 2;

                //printf(" first %d, last %d, max_kern %d, rect_w %d, rect_h %d, ascent %d, descent %d, leading %d, bytes_per_row %d\n",
                //    info->first_character, info->last_character, info->maximum_kerning,
                //    info->rect_width, info->rect_height, info->ascent, info->descent, info->leading, bytes_per_row);

                // Convert bitmap to pixmap (1 byte/pixel)
                info->bytes_per_row = bytes_per_row * 8;
                uint8 *src = (uint8 *)font_ptr + SDL_RWtell(p);
                uint8 *dst = info->pixmap = (uint8 *)malloc(info->rect_height * info->bytes_per_row);
                assert_fail(dst, "");
                for (int y=0; y<info->rect_height; y++) {
                    for (int x=0; x<bytes_per_row; x++) {
                        uint8 b = *src++;
                        *dst++ = (b & 0x80) ? 0xff : 0x00;
                        *dst++ = (b & 0x40) ? 0xff : 0x00;
                        *dst++ = (b & 0x20) ? 0xff : 0x00;
                        *dst++ = (b & 0x10) ? 0xff : 0x00;
                        *dst++ = (b & 0x08) ? 0xff : 0x00;
                        *dst++ = (b & 0x04) ? 0xff : 0x00;
                        *dst++ = (b & 0x02) ? 0xff : 0x00;
                        *dst++ = (b & 0x01) ? 0xff : 0x00;
                    }
                }
                SDL_RWseek(p, info->rect_height * bytes_per_row, SEEK_CUR);

                // Set table pointers
                int table_size = info->last_character - info->first_character + 3;    // Tables contain 2 additional entries
                info->location_table = (uint16 *)((uint8 *)font_ptr + SDL_RWtell(p));
                *info->location_table = SDL_SwapBE16(*info->location_table);
                SDL_RWseek(p, table_size * 2, SEEK_CUR);
                info->width_table = (int8 *)font_ptr + SDL_RWtell(p);

                // Add font information to list of known fonts
                info->ref_count++;
                font_list[id_and_size] = info;

            } else {
                delete info;
                info = NULL;
                fprintf(stderr, "Bitmap font resource ID %d not found\n", id);
            }
        }
    }

    // Free resources
    SDL_RWclose(p);
    return info;
}



void FontRenderer_SDL_Pixmap::unload()
{
    // Look for font in list of loaded fonts
    font_list_t::const_iterator i = font_list.begin(), end = font_list.end();
    while (i != end) {
        if (i->second == this) {

            // Found, decrement reference counter and delete
            ref_count--;
            if (ref_count <= 0) {
                delete this; // !
                font_list.erase(i->first);
                return;
            }
        }
        i++;
    }
}




uint16 FontRenderer_SDL_Pixmap::text_width(const std::string& text, uint16 style) const
{
    int width = 0;
    
    TODO("overhaul text rendering");
    
    //while (length--) width += char_width(*text++, style, "");
    
    assert_fail(0 <= width, "");
    assert_fail(width == static_cast<int>(static_cast<uint16>(width)), "");
    return width;
}


int32_t FontRenderer_SDL_Pixmap::trunc_text(const std::string& text, int32_t max_width, uint16_t style) const
{
    TODO("overhaul text rendering");
    return -1;
}



// Draw text at given coordinates, return total width
int FontRenderer_SDL_Pixmap::draw_text(SDL_Surface *s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const
{
    // Get clipping rectangle
    int clip_top, clip_bottom, clip_left, clip_right;
    if (draw_clip_rect_active) {
        clip_top = draw_clip_rect.top;
        clip_right = draw_clip_rect.right - 1;
        clip_bottom = draw_clip_rect.bottom - 1;
        clip_left = draw_clip_rect.left;
    } else {
        clip_top = clip_left = 0;
        clip_right = s->w - 1;
        clip_bottom = s->h - 1;
    }

    if (SDL_MUSTLOCK (s)) {
      if (SDL_LockSurface(s) < 0) return 0;
    }
    int width = 0;
    
    TODO("overhaul text rendering: looks like some right old fuckery; undo all these convolutions");
    /*
    switch (s->format->BytesPerPixel) {
        case 1:
            width = ::draw_text((const uint8*)text, length, x, y, (pixel8*)s->pixels, s->pitch, clip_left, clip_top, clip_right, clip_bottom, pixel, this, style);
            break;
        case 2:
            width = ::draw_text((const uint8*)text, length, x, y, (pixel16*)s->pixels, s->pitch, clip_left, clip_top, clip_right, clip_bottom, pixel, this, style);
            break;
        case 4:
            width = ::draw_text((const uint8*)text, length, x, y, (pixel32*)s->pixels, s->pitch, clip_left, clip_top, clip_right, clip_bottom, pixel, this, style);
            break;
    }*/
    
    if (SDL_MUSTLOCK (s)) {
      SDL_UnlockSurface(s);
    }
    if (s == MainScreenSurface())
        MainScreenUpdateRect(x, y - ascent, text_width(text, style), rect_height);
    return width;
}



// -----------------------------------------------------------------------------------------
// Font management

void initialize_fonts(bool last_chance) // 'last_chance' - oh dear. TODO: extract an `initialize_builtin_fonts` which is called at start of initialize_application so there's always a fallback font available for use in dialogs; beyond that, additional font-loading should be done when a data file/scenario plugin is loaded that declares its fonts in its MML/manifest (as for MMLs in one plugin that request fonts that are in a different plugin, well, that's a larger problem for another day)
{
        log_context("initializing fonts");
    
    // Initialize builtin TTF fonts
    for (int j = 0; j < NUMBER_OF_BUILTIN_FONTS; ++j)
        builtin_fonts[builtin_fontspecs[j].name] = builtin_fontspecs[j];
    
    // Open font resource files
    bool found = false;
    std::vector<DirectorySpecifier>::const_iterator i = data_search_path.begin(), end = data_search_path.end();
    while (i != end) {
        FileSpecifier fonts = *i + "Fonts";

        if (open_file_resource(fonts))
            found = true;

        if (!found)
        {
            fonts = *i + "Fonts.fntA";
            if (open_file_resource(fonts))
                found = true;
        }
        i++;
    }
}


FontRenderer_SDL *load_font(const TextSpec &spec)
{
    if (spec.normal != "") // huh?
    {
        std::string file = locate_font(spec.normal);
        FontRenderer_SDL_TTF* result = try_to_load_as_ttf_font(file, spec);
        if (result) { return result; }
    }
    
    return try_to_load_as_pixmap_font(spec);
}


// TODO: we want an unload_all_fonts which unloads all scenario fonts (if this needs to be efficient then FontRenderer_SDL can add a refcount of all the Extensions that have requested a particular font)
