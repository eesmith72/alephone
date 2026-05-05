/*
 Screen.hpp
 
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

#include "cseries.hpp"


// TODO: the ability to create a second Screen instance will be super-useful for users with multiple monitors: map editing (show 2D editor on one monitor and 3D editor on the other), movie editing (if anyone is crazy enough to build a UI for it: run small 'monitor' views from all available cameras on user's secondary display and editing interface on the main display)

// TODO: for PvP on Modern, need to think about screen aspects that lie between 4:3 and 16:9; should we set everyone to 16:9 (e.g. laptop users with squarer screens get thin black bars at top and bottom; users with Ultrawide screens get big black bars at sides; everyone having the same aspect and the same FOV sees the same amount of the 3D world so no-one has unfair advantage); ofc if everyone in PvP match has ultrawide screen (or doesn't mind competing against others who have) then they can 'allow widest screen' in the netgame config

// TODO: dragging-to-resize window on macOS doesn't generate window events until mouse is released (https://github.com/libsdl-org/SDL/issues/11508), so using fixed-size window for now

// (moved here from Input/mouse.h)
void hide_cursor();
void show_cursor();
bool cursor_is_hidden();


//-----------------------------------------------------------------------------
// Screen

// AO had a myriad modes, most pointless

typedef id_strings_t screen_mode_names_t; // Graphics Preferences dialog needs a vector of {mode,name}


struct screen_mode_definition_t
{
    screen_mode_t mode; // see strScreenSize for UI labels
    int32_t w, h, bit_depth;
    bool modern, high_dpi, ultrawide; // TODO: in SDL2, high_dpi is a bool flag indicating pixel size = window coordinates size * 2 (not sure if it's Mac-only); in SDL3, it's a float indicating the scaling factor (e.g. a 4K screen is typically 2.0) so our code will need redesigned
    
    // TODO: add standard_fov? (calculated from aspect)
    
    float aspect() const { return float(w) / float(h); }
    
    SDL_Point size() const { return {w, h}; }
    
    SDL_Rect rect() const { return {0, 0, w, h}; }
};


//-----------------------------------------------------------------------------


class Screen
{
public:
    Screen() {}
    
    void initialize();
    
    
    // Get information about the user's monitor
    
    //SDL_Point fullscreen_pixel_size();
    //float fullscreen_aspect();

    bool supports_high_dpi();
    bool supports_ultrawide();
    
    
    // Get/set screen mode
    //
    // - We now support 2 Classic modes (640x480, 800x600) and 1-4 Modern modes (native resolution, with/without high-dpi and/or Ultrawide).
    // - Classic modes render in 8-bit or 16-bit color; Modern in 32-bit.
    // - In Classic modes, the original M2 SW renderer draws the 3D gameworld into an emulated video buffer (ClassicScreenBuffer,
    // which wraps an SDL_Surface pixel buffer) which is then copied to GPU texture by ImageBlitter for compositing on screen using OGL.
    // (While the implementation's still a bit wooden-table, this is a paragon of simplicity compared to the old turtles^H^H^H^H^H Surfaces
    // all the way down crapfest.) This preserves the historically significant 2.5D world renderer, minus the 25 years of AO chaos on top.
    // - In Modern modes, the gameworld is drawn by the OGL+Shader renderer+rasterizer.
    // - The HUD, Automap, Terminal are all drawn using OGL-based Canvas class. // TODO: finish these (Q. For Classic, does the OGL automap need rendered to a 640x480/800x600 FBO to replicate its original M2 pixelated appearance?)
    
    bool modern_3D() { return m_mode->modern; } // Is the gameworld being drawn using Modern (OGL) 3D or Classic (SW) renderer?
    
    int32_t bit_depth() { return m_mode->bit_depth; } // e.g. Shapes and main menu + chapter screens need to know this to choose the best available image quality
    
    
    // set the virtual screen's size to one of the predefined sizes (or the next-best size if unavailable)
    // (note: in windowed mode, the window is sized to vscreen's aspect; in fullscreen, the window's horizontal/vertical margins are padded if its aspect is different)
    
    void set_mode(screen_mode_t size);
    
    screen_mode_t mode();
    
    screen_mode_names_t supported_modes(); // for use in Graphics Preferences dialog

    bool decrease_mode();
    bool increase_mode();
    
    bool decrease_gamma();
    bool increase_gamma();
    
    void clear(bool fully = true); // set entire screen black, optionally swapping and clearing back buffer too
    
    
    // in windowed mode, the window's size is determined by the current mode and the size and aspect of the user's display
    
    void set_fullscreen(bool is_fullscreen);
    bool fullscreen();
    void toggle_fullscreen();
    
    
    // when interacting with the Window, most client code should only care about the "virtual screen" we project into it
    // (hopefully this API makes it easy to switch Screen from SDL2 to SDL3, which has improved APIs for high-dpi display)
    
    SDL_Window* window() { return m_window; }
    
    SDL_Point window_pixel_size(); // e.g. in fullscreen, this is the display's native resolution, e.g. {1920,1080} (SD), {3840,2140} (4K HD); in windowed, it is the window's pixel size (this may be same as its coordinates size, 2x its coordinates size in high-dpi, or variable once we move to SDL3)
        
    // the scaling factor from Window's true (pixel) resolution to our virtual screen
    float pixel_to_virtual_scale();
    
    SDL_Point get_mouse_virtual_position(); // cursor's position on the virtual (e.g. 640x480) screen
    
    SDL_Rect virtual_screen_pixel_rect(); // this is what we set the OGL viewport to
    
    SDL_Rect virtual_screen_rect() { return {0, 0, m_virtual_screen_current_size.x, m_virtual_screen_current_size.y}; } // the virtual screen into which the app is currently drawing
    
    // OGL drawing area
    
    // was: bound_screen
    void configure_for_classic_ui(); // Interface/ must use this when displaying legacy splash, main menu, and/or chapter screen images; ditto when displaying legacy dialogs (which, for now, dialogs always are)
    
    void configure_for_modern_ui(const SDL_Point& size); // TODO: Interface/ should call this when modern splash, main menu, and/or chapter screen images are loaded, passing their true dimensions
        
    
    // TODO: update these
    void set_virtual_drawing_rect(SDL_Rect &r, bool drawing_uses_virtual_screen_origin = false);
    
    SDL_Rect virtual_drawing_rect();
    
    void reset_virtual_drawing_rect();
    
    
    // screen size in SDL coordinates (this ignores HD, e.g. 4K monitor returns 1920,1080px)
    void get_window_coordinates_size(int32_t& w, int32_t& h); // TODO: get rid of this
        
    
    // TODO: redo these; worldview this is normally fullscreen in Modern and top 2/3rds in Classic, but it's up to the HUD plugin to calculate these 4 rects - given we're going all-in on Lua HUD now, I think it's reasonable to require existing HUD plugins adopt the new API, though if we can do backwards compatibility (calculated from whatever lua_xxx values the plugin assigns below) then that would be nice
    
    SDL_Rect virtual_world_rect()       { return /*m_virtual_world_rect*/ virtual_screen_rect(); } // the current player's 3D view
    SDL_Rect virtual_automap_rect()     { return m_virtual_automap_rect; }
    SDL_Rect virtual_terminal_rect()    { return m_virtual_terminal_rect; }
    SDL_Rect virtual_classic_hud_rect() { return m_virtual_classic_hud_rect; }
    
    void set_virtual_world_rect(const SDL_Rect& rect) { m_virtual_world_rect = rect; }
    void set_virtual_automap_rect(const SDL_Rect& rect) { m_virtual_automap_rect = rect; }
    void set_virtual_terminal_rect(const SDL_Rect& rect) { m_virtual_terminal_rect = rect; }
    void set_virtual_classic_hud_rect(const SDL_Rect& rect) { m_virtual_classic_hud_rect = rect; }
        
    
    // rendering support
    
    // graphics subsystems that are invoked as part of a running app/game event loop should generally call `request_swap()` to request a buffer swap after everything is drawn...
    void request_swap() // currently only called by ImageBlitter::render_to_screen
    {
        m_needs_swapped = true;
        //printf("request screen swap\n");
    }
    
    void swap_if_needed() // currently only called by main event loop
    {
        if (m_needs_swapped) 
        {
            SDL_GL_SwapWindow(m_window);
            m_needs_swapped = false;
        }
    }
    
    // ...or code which draws everything itself can swap immediately (e.g. main_menu.cpp swaps immediately after updating a button state)
    void swap() { SDL_GL_SwapWindow(m_window); }
    
    
    // what it says on the tin
    void print_debug();
    
    
    // both Classic and Modern in-game renderers require some setup/teardown before and after use
    void start_gameworld_renderer();
    void stop_gameworld_renderer();

private:
    // screen modes
    
    const screen_mode_definition_t* m_mode; // the current screen size definition; for Classic, these are fixed at the original M2 dimensions (640x480 and 800x600); for Modern, the dimensions are calculated from the display
    
    std::vector<screen_mode_definition_t> m_available_screen_sizes;

    void initialize_supported_modes(SDL_DisplayMode& desktop);
    
    // window
    
    SDL_Window* m_window;
    
    SDL_Point m_virtual_screen_current_size; // the size of screen into which the app thinks it is currently drawing, e.g. 640x480 (caution: this is distinct from m_mode->size which primarily describes the in-game screen size; if a Modern UI isn't explicitly defined then the UI must 640x480 for compatibility with existing scenarios)
    
    // SDL mouse uses SDL2 Window coordinates, which don't account for high-dpi, so Screen converts its last position to pixel/virtual coordinates
    SDL_Point convert_coordinate_to_pixel_position(const SDL_Point& point);
    SDL_Point convert_pixel_to_virtual_position(const SDL_Point& point);
    SDL_Point get_mouse_pixel_position();
    
    
    void set_virtual_screen_size(const SDL_Point& size); // size is the virtual screen size we want, e.g. {640,480}, `m_mode->size()`; used by configure_for_ methods
        
    
    
    bool m_needs_swapped = false;
    
    void did_change(); // set_mode and did_change replace the old spaghetti change_screen_mode
    
    
    
 //   SDL_Rect m_viewport_rect; // the pixel coordinates into which the whole vscreen is drawn; TODO: virtual_screen_pixel_rect() currently calculates this each time, which is wasteful
    
    SDL_Rect m_virtual_world_rect; // the 3D world view, as coordinates on the virtual screen rect
    SDL_Rect m_virtual_automap_rect;
    SDL_Rect m_virtual_terminal_rect;
    SDL_Rect m_virtual_classic_hud_rect;
    SDL_Rect m_virtual_drawing_rect;
};



extern Screen main_screen;


//-----------------------------------------------------------------------------
// rendering


void render_game_to_screen(short ticks_elapsed);


void darken_world_window(); // when paused


void dump_screen();


#endif /* __screen_hpp__ */
