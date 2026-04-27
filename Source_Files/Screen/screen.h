#ifndef _SCREEN_H_
#define _SCREEN_H_
/*
 SCREEN.H -- display management (in a modern world of 4K super-widescreen
             wonders, 640x480 should be enough for everyone!!!1!1!)

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

#include "cseries.h"


// TODO: there are a couple of use cases where >1 Screen instance will be super-useful for users with multiple monitors: map editing, film editing (I assume SDL can create >1 SDL_Window instance)


struct screen_size_t
{
    int32_t size_id;
    std::string name;
    int32_t w, h, bit_depth;
    bool modern, high_dpi, superwide;
};


struct screen_mode_data
{    
    
    short hud_size; // 0-3 (0=none)
    short terminal_size;
    
};


extern screen_mode_data screen_mode;


extern bool screen_needs_swapped;

#define request_swap() { \
    screen_needs_swapped = true; \
    /*printf("request screen swap\n");*/ \
}


// TODO: include `&& !screen_is_faded_black()` test?

#define swap_screen_if_requested() { \
if (screen_needs_swapped) \
    { \
        current_screen.swap(); \
        screen_needs_swapped = false; \
    } \
}




// (moved here from Input/mouse.h)
void hide_cursor();
void show_cursor();
bool cursor_is_hidden();
void recenter_mouse();



class Screen
{
public:
    Screen() {}
    
    void initialize();
    
    void size_changed(); // replaces change_screen_mode
    
    void swap() { SDL_GL_SwapWindow(m_window); }
    
    bool uses_modern_renderer() { return m_size->modern; }
    
    bool supports_high_dpi();
    
    bool supports_superwide();
    
    std::vector<std::string> get_screen_size_names(); // TODO: vector<{size_id,name}>

    const screen_size_t* get_screen_size() { return m_size; }
    
    int32_t bit_depth() { return m_size->bit_depth; }
    
    // set the screen to the specified size (or next-best size if unavailable)
    void set_screen_size(int32_t size_id);

    bool decrease_size();
    
    bool increase_size();
    
    void set_fullscreen(bool is_fullscreen);
    
    void toggle_fullscreen();
    
    //
    
    SDL_Window* get_window() { return m_window; }
    
    // screen size in SDL coordinates (this ignores HD, e.g. 4K monitor returns 1920,1080px)
    void get_window_coordinates_size(int32_t& w, int32_t& h);
    
    // the true screen size, accounting for hi-res (e.g. 4K monitor returns 3840x2160px)
    void get_window_pixel_size(int32_t& w, int32_t& h);
    
    // the scaling factor from virtual screen to true (pixel) resolution
    float virtual_screen_to_pixel_scale();
    
    SDL_Rect window_rect()         { return m_window_rect; } // 3D view + interface
    
    // the size the renderer thinks the screen is; for Classic, this is 640x480 or 800x600; for modern, it is (OGL viewport rect?); TODO: disentangling UI drawing from this will enable HD/widescreen splash+chapter screens and main menu (caveat the menu needs to work on different monitor widths so should probably keep all buttons within 4:3 center area)
    SDL_Rect virtual_screen_rect() { return {0, 0, m_size->w, m_size->h}; }
    
    SDL_Rect worldview_rect()      { return m_worldview_rect; } // main 3D view
    
    SDL_Rect automap_rect()        { return m_automap_rect; } // think these are in SDL window coordinates
    
    SDL_Rect terminal_rect()       { return m_terminal_rect; }
    
    SDL_Rect hud_rect()            { return m_hud_rect; }
    
    SDL_Rect ogl_viewport_rect()   { return m_viewport_rect; } // TODO: how does this differ to the other rects?
    
    // TODO: move behind set/get methods
    SDL_Rect lua_clip_rect;
    SDL_Rect lua_view_rect;
    SDL_Rect lua_map_rect;
    SDL_Rect lua_term_rect;
    
    void bound_screen(bool in_game = true);
    
    void bound_screen_to_rect(SDL_Rect &r, bool in_game = true);
    
    void set_clipping_rect(SDL_Rect &r);
    
    void clear_clipping_rect();
    
    void convert_window_coordinate_to_virtual_screen_point(int &x, int &y);
    
    // TODO: the HUD should really draw messages / fps / input line itself
    screen_rectangle lua_text_margins;
    
private:
    
    void initialize_available_screen_sizes(SDL_DisplayMode& desktop);
    
    const screen_size_t* m_size;
        
    SDL_Rect m_viewport_rect;
    SDL_Rect m_ortho_rect;
    SDL_Rect m_window_rect;
    SDL_Rect m_worldview_rect;
    SDL_Rect m_automap_rect;
    SDL_Rect m_terminal_rect;
    SDL_Rect m_hud_rect;
    
    SDL_Window* m_window;
    
    // TODO: these are still heinous spaghetti behind scenes and should eventually simplify, but at least now they only run when something changes, not every single frame
    SDL_Rect calculate_window_rect();
    SDL_Rect calculate_worldview_rect();
    SDL_Rect calculate_automap_rect();
    SDL_Rect calculate_terminal_rect();
    SDL_Rect calculate_hud_rect();
    
    void calculate_rects()
    {
        calculate_window_rect();
        calculate_worldview_rect();
        calculate_automap_rect();
        calculate_terminal_rect();
        calculate_hud_rect();
    }
};



extern Screen current_screen;



enum /* screen selection based on game state */ // TODO: these need to go away
{
	_screentype_level,
	_screentype_menu,
	_screentype_chapter
};




void render_game_to_screen(short ticks_elapsed);


void activate_gameworld_screen(void);
void deactivate_gameworld_screen(void);



//void change_screen_mode(struct screen_mode_data *mode, bool redraw, bool resize_hud = false);
//void change_screen_mode(short screentype);


void clear_screen(bool swap = true);

void darken_world_window(); // when paused



SDL_Surface* copy_screen_to_surface(); // used by dump_screen; caller is responsible for freeing the returned Surface

void dump_screen();


#endif
