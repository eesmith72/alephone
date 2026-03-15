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

#include "font_t.hpp"

#include "find_files.hpp"

#include "resource_manager.h"
#include "DataFile.hpp"

//#include <boost/tokenizer.hpp> // because `w_styled_text` has its own markup scheme that appears to be different terminals' markup scheme; typical

#include "preferences.h" // smooth_font setting

#include "screen.h" // MainScreenSurface, MainScreenUpdateRect (see screen.h, screen_drawing.h, shared_screen.h as that stuff's all over the place)
#include "screen_drawing.h" // screen_rectangle

// From screen_drawing.cpp; see the TODO there re. Cohen/Sutherland
extern bool draw_clip_rect_active;
extern screen_rectangle draw_clip_rect;




// -----------------------------------------------------------------------------------------
// the try_to_load_as_TTF/Pixmap_font functions below reference these


struct font_key_t
{
    //std::string name; // string or ID?
    uint32_t font_id;
    font_style_t style;
    uint16_t size;
};



/*
// Global variables
typedef std::pair<int, int> id_and_size_t;
typedef std::map<id_and_size_t, Font_Pixmap *> font_list_t;
static font_list_t font_list;				// List of all loaded fonts



typedef std::pair<TTF_Font *, int> ref_counted_ttf_font_t;
typedef std::map<ttf_font_key_t, ref_counted_ttf_font_t> ttf_font_list_t;
static ttf_font_list_t ttf_font_list;
*/


// From shell_sdl.cpp
extern std::vector<ao_path> scenario_data_search_paths; // idiocy // TODO: Scenario/ needs to provide APIs for finding all assets, including TTL and bitmap fonts



// -----------------------------------------------------------------------------------------
// used by try_to_load_as_ttf/pixmap_font functions below

// EES: I'm assuming if the font is a built-in, the 'path' will be one of the names defined in builtin_fontspecs above, otherwise it's a path relative to one of the standard search directories; presumably "" translates as "use the default 'mono' font"; TODO: for a simple "find the asset" request, the code itself is stupidly complex and spread all over, but that's a job for Files/

static const std::string find_font(const std::string& name) //
{
    if (name.empty()) return name;
    
    // is it a built-in font?
    builtin_fonts_t::iterator it = builtin_fonts.find(name);
    if (it != builtin_fonts.end()) return name;
    
    return find_file_at_subpath(name);
}




// -----------------------------------------------------------------------------------------
// Font management

void initialize_fonts(bool last_chance) // 'last_chance' - oh dear. TODO: extract an `initialize_builtin_fonts` which is called at start of initialize_application so there's always a fallback font available for use in dialogs; beyond that, additional font-loading should be done when a data file/scenario plugin is loaded that declares its fonts in its MML/manifest (as for MMLs in one plugin that request fonts that are in a different plugin, well, that's a larger problem for another day)
{
        log_context("initializing fonts");
    
    /*
    // Initialize builtin TTF fonts
    for (int j = 0; j < NUMBER_OF_BUILTIN_FONTS; ++j)
        builtin_fonts[builtin_fontspecs[j].name] = builtin_fontspecs[j];
    
    // Open font resource files
    bool found = false;
    for (const auto& it : scenario_data_search_paths)
    {
        ao_path fonts = it / "Fonts";

        if (open_resource_file(fonts))
            found = true;

        if (!found)
        {
            fonts = it / "Fonts.fntA";
            if (open_resource_file(fonts))
                found = true;
        }
    }
     */
}


font_t *load_font(const font_spec_t &spec)
{
    if (spec.normal != "") // huh?
    {
        std::string file = find_font(spec.normal);
        Font_TTF* result = try_to_load_as_ttf_font(file, spec);
        if (result) { return result; }
    }
    
    return try_to_load_as_pixmap_font(spec);
}


// TODO: we want an unload_all_fonts which unloads all scenario fonts (if this needs to be efficient then Font can add a refcount of all the Extensions that have requested a particular font)
