/*
 screen.hpp -- display management
 
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

#ifndef __screen_hpp__
#define __screen_hpp__

#include "cseries.h"


// TODO: the ability to create a second Screen instance will be super-useful for users with multiple monitors: map editing (show 2D editor on one monitor and 3D editor on the other), movie editing (if anyone is crazy enough to build a UI for it: run small 'monitor' views from all available cameras on user's secondary display and editing interface on the main display)




// (moved here from Input/mouse.h)
void hide_cursor();
void show_cursor();
bool cursor_is_hidden();


//-----------------------------------------------------------------------------
// Screen


struct screen_size_definition_t
{
    screen_size_t size; // see strScreenSize for UI labels
    int32_t w, h, bit_depth;
    bool modern, high_dpi, ultrawide;
};

class Screen
{
public:
    Screen() {}
    
    void initialize();
    
    bool supports_high_dpi();
    bool supports_ultrawide();
    
    bool modern_3D() { return m_size->modern; }
    
    float aspect() { return m_window_rect.h / m_window_rect.w; }
    int32_t bit_depth() { return m_size->bit_depth; }
    
    // set the screen to the specified size (or next-best size if unavailable)
    void set_size(screen_size_t size);
    
    screen_size_t size();

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
    
    // the size the renderer thinks the screen is; for Classic, this is 640x480 or 800x600; for Modern, it is the full window's coordinates size (not counting high-dpi)
    SDL_Rect virtual_screen_rect() { return {0, 0, m_size->w, m_size->h}; }
    
    SDL_Rect worldview_rect()      { return m_worldview_rect; } // main 3D view
    
    SDL_Rect automap_rect()        { return m_automap_rect; } // think these are in SDL window coordinates
    
    SDL_Rect terminal_rect()       { return m_terminal_rect; }
    
    SDL_Rect hud_rect()            { return m_hud_rect; }
    
    // TODO: move all this stuff behind methods as changing size of one rect may require recalculating others
    SDL_Rect lua_clip_rect;
    SDL_Rect lua_view_rect;
    SDL_Rect lua_map_rect;
    SDL_Rect lua_term_rect;
    screen_rectangle lua_text_margins;
    
    // OGL drawing area
    
    // was: bound_screen(bool in_game)
    void set_viewport_for_ui()
    {
        SDL_Rect r = {0, 0, 640, 480};
        set_viewport_rect(r, r);
    }

    void set_viewport_for_game()
    {
        SDL_Rect r = virtual_screen_rect();
        set_viewport_rect(r, r);
    }
    
    // the active drawing area; the virtual screen is mapped onto this
    void set_viewport_rect(SDL_Rect &rect, SDL_Rect& vscreen);
    
    SDL_Rect viewport_rect() { return m_viewport_rect; }
    
    //void reset_viewport_rect();
    
    void set_clipping_rect(SDL_Rect &r);
    
    void reset_clipping_rect();
    
    // used to map mouse clicks onto the virtual screen (e.g. when clicking on main menu and dialog buttons)
    void convert_window_coordinate_to_virtual_screen(int32_t &x, int32_t &y, const SDL_Rect& vscreen);
    
    
    // rendering support
    
    // graphics subsystems should call `request_swap()` to request a buffer swap after everything is drawn
    void request_swap()
    {
        needs_swapped = true;
        /*printf("request screen swap\n");*/
    }
    
    void swap_if_needed()
    {
        if (needs_swapped) 
        {
            swap();
            needs_swapped = false;
        }
    }
    
    void swap() { SDL_GL_SwapWindow(m_window); }

    void print_debug();
    
    void start_gameworld_renderer();
    
    void stop_gameworld_renderer();

private:

    bool needs_swapped = false;

    void initialize_available_screen_sizes(SDL_DisplayMode& desktop);
    
    void size_changed(); // replaces change_screen_mode
    
    const screen_size_definition_t* m_size;
        
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
    
};



extern Screen main_screen;


//-----------------------------------------------------------------------------
// rendering


void set_lua_rects(); // kludge til we disentangle properly


void render_game_to_screen(short ticks_elapsed);



void clear_screen(bool swap = true); // set entire screen black

void darken_world_window(); // when paused



SDL_Surface* copy_screen_to_surface(); // used by dump_screen; caller is responsible for freeing the returned Surface

void dump_screen();


#endif /* __screen_hpp__ */
