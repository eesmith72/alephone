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

#ifndef __FontRenderer_SDL_hpp__
#define __FontRenderer_SDL_hpp__

#include "cseries.h"

#include "DataFile.hpp"

#include "resource_manager.h"

#include <SDL2/SDL_ttf.h>

/*
 TODO: may be worth consolidating on SDL_fox's pixmap mechanics and glyph metrics core, with Pixmap- and TTF-loading frontends and SDL- and OGL-rendering backends. Crucially, this will give us a single text drawing engine which works everywhere.

 Notes:

 1. If our goal is to migrate all of AO's high-level (theme-aware) UI to ImGui+Lua bindings, it's probably not worth effort to rearchitect all this. ImGui will have its own text renderer for its widgets, so we'll only need to support in-world OGL/SW rendering.
 
 2. LP's old OGL font renderer is almost certainly no longer needed. While drawing text to SDL_Surface and transferring that to GPU texture (Image_Blitter.cpp) to put on screen is relatively slow, the SW world renderer already does this anyway when playing without OGL enabled. In-world text renderering is currently needed for computer terminal, HUD, and on-screen message display (in future it'd be nice to add signage/grafitti to maps, in which case drawing text to wall textures too) but I really don't see any of these taxing post-2010 hardware (like a 2008 hotrod box could run Dead Space, so Moore's Law has already solved most of our need for us). Therefore it makes a lot of sense to ditch LP's OGL font renderer entirely and use TextRenderer_SDL to draw TTF and Pixmap fonts to Surface.
 
 */


// -----------------------------------------------------------------------------------------
// abstract base class: FontRenderer_SDL can draw text using either TTF fonts or glyph bitmaps

// TODO: this implementation will simplify further as `styles` moves completely to StyledFontRenderer

class FontRenderer_SDL
{
public:
    FontRenderer_SDL() = default;
    
    virtual ~FontRenderer_SDL() = default;
    
    virtual void unload() = 0; // should be protected friend called by `reset_fonts` below, which is called whenever a scenario/theme changes (it'd be nice to unload more granularly, but that'd require reference counting each loaded font since some will be shared dependencies; we can worry about that when overhauling Scenario/), but right now unload() is being called from sdl_dialogs.cpp and FontRenderer_OGL.cpp (which is ridiculous and almost certainly doesn't account for sharing),
    
    virtual uint16 get_ascent(void) const      = 0;
    virtual uint16 get_height(void) const      = 0;
    virtual uint16 get_line_height(void) const = 0;
    virtual uint16 get_descent(void) const     = 0;
    virtual int16  get_leading(void) const     = 0;
    
    virtual int draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const = 0;
    
    virtual uint16 text_width(const std::string& text, uint16 style) const = 0;
    
    virtual int32_t trunc_text(const std::string& text, int32_t max_width, uint16_t style) const = 0;
        
};


// -----------------------------------------------------------------------------------------
// concrete classes

// TODO: use SDL_foxxed as the backend to all FontRenderers (it generates pixmaps of TTF fonts for fast blitting so it should accommodate bitmap-based fonts nicely)


class FontRenderer_SDL_Pixmap : public FontRenderer_SDL
{
    friend FontRenderer_SDL_Pixmap *try_to_load_as_pixmap_font(const TextSpec &spec);
    
public:
    FontRenderer_SDL_Pixmap() : first_character(0), last_character(0),
                            ascent(0), descent(0), leading(0), pixmap(nullptr), ref_count(0) {}
    
    ~FontRenderer_SDL_Pixmap() { if (pixmap) free(pixmap); }
    
    void unload();
    
    uint16 get_ascent()      const { return ascent; }
    uint16 get_height()      const { return ascent + descent; }
    uint16 get_line_height() const { return ascent + descent + leading; }
    uint16 get_descent()     const { return descent; }
    int16 get_leading()      const { return leading; }
    
    int draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const;
    
    uint16 text_width(const std::string& text, uint16 style) const;
    
    int32_t trunc_text(const std::string& text, int32_t max_width, uint16_t style) const;
    
    // these need to be public while try_to_load_as_ttf_font is a separate function
    uint8 first_character, last_character;
    int16 maximum_kerning;
    int16 rect_width, rect_height;
    uint16 ascent, descent;
    int16 leading;
    
    uint8* pixmap;            // Font image (1 byte/pixel)
    int32_t bytes_per_row;    // Bytes per row in pixmap
    
    uint16_t* location_table; // Table of byte-offsets into pixmap (points into resource)
    int8_t* width_table;      // Table of kerning/width info (points into resource)

private:
    int ref_count;
    LoadedResource rsrc;
};


// -----------------------------------------------------------------------------------------
// FontRenderer_SDL_TTF -- draw a TTF font using SDL's TTF_
//

typedef std::tuple<std::string, uint16, int16> ttf_font_key_t; // font name, style, and size // TODO: this should be used as key in global font registry


// TODO: there should be one font style+size per one FontRenderer_SDL_TTF instance


class FontRenderer_SDL_TTF : public FontRenderer_SDL
{
    friend FontRenderer_SDL_TTF* try_to_load_as_ttf_font(std::string &file, const TextSpec &spec);

public:
    FontRenderer_SDL_TTF()
    {
        for (int i = 0; i < styleUnderline; i++) { m_styles[i] = 0; }
    }
    
    virtual ~FontRenderer_SDL_TTF() = default;
    
    virtual void unload();
    
    // metrics
    uint16 get_ascent()      const { return TTF_FontAscent(m_styles[styleNormal]);            }
    uint16 get_height()      const { return TTF_FontHeight(m_styles[styleNormal]);            }
    uint16 get_line_height() const { return m_line_height + m_adjust_height;                  }
    uint16 get_descent()     const { return -TTF_FontDescent(m_styles[styleNormal]);          }
    int16 get_leading()      const { return get_line_height() - get_ascent() - get_descent(); }

    int draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const;
    
    uint16_t text_width(const std::string& text, uint16 style) const;
    
    int32_t trunc_text(const std::string& text, int32_t max_width, uint16_t style) const;
    
    // these need to be public while try_to_load_as_pixmap_font is a separate function
    int m_adjust_height; // TODO: these could stand some clarification
    int m_line_height;
    
    TTF_Font* m_styles[NUMBER_OF_REAL_FONT_STYLES];
    ttf_font_key_t m_keys[NUMBER_OF_REAL_FONT_STYLES];
    
private:
    TTF_Font *get_ttf(uint16 style) const { return m_styles[style & (styleBold | styleItalic)]; }
};




/*
 *  Functions
 */

// Initialize font management
void initialize_fonts(bool last_chance);

// Load font, return pointer to font info; remember to call its unload()
FontRenderer_SDL *load_font(const TextSpec &spec);



extern FontRenderer_SDL_TTF default_font;



#endif /* __FontRenderer_SDL_hpp__ */
