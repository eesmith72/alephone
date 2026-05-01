/*
    screen.cpp
 
    Written in 2000 by Christian Bauer

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

#include "Screen.hpp"

#include "sdl_resize.h"

#include "mouse.h" // recenter_mouse


// EES: Completely rebuilt so it's 1. simple, 2. comprehensible, 3. modernizable. Still, there was in the old implementation the nugget of a good idea: the engine should model a "virtual screen" which emulates the 640x480 screen buffer of 1995 M2, independent of the size and aspect of SDL_Window it draws into. e.g. Classic M2's splash+chapter+main menu screens all use 640x480 images and the main menu's button rects (hardcoded and legacy MML) use matching 640x480 coordinates system.


// TODO: finish straightening out m_viewport_rect, m_ortho_rect

// TODO: if the user modifies screen settings outside the app (or e.g. user disconnects external main monitor so laptop switches to its internal screen), how can we get notifications? (see SDL_WINDOWEVENT_)


/*
 TODO: make OGL a required dependency (caveat the Hub, which shouldn't be rendering anything).

 This allows us to clean up AO's OGL_ APIs so that, at some point, the OGL implementation can migrate to SDL_gpu.
 */

#include "OGL_Headers.h"
#include "ImageBlitter.hpp"
#include "OGL_Faders.h"
#include "OGL_Textures.h"

#include "vbl.h" // get_heartbeat_fraction
#include "world.h"
#include "map.h"
#include "render.h"
#include "Renderer_SW_ScreenBuffer.hpp" // classic_renderer_buffer

#include "shell.h"
#include "interface.h"
#include "interpolated_world.h"
#include "player.h"
#include "overhead_map.h"
#include "fades.h"
#include "game_window.h"
#include "Screen.hpp"
#include "preferences.h"
#include "computer_interface.h"
#include "OGL_Render.h"
#include "camera.h"
#include "screen_drawing.h"
#include "mouse.h" // recenter_mouse // (it needs to know when mouse_active, which is defined in mouse.cpp)
#include "network.h"
#include "images.h"
#include "motion_sensor.hpp"

#include "screen_overlay.h"

#include "fonts.hpp"

#include "lua_script.h"
#include "lua_hud_script.h"
#include "FilmExporter.h"
#include "shell_options.h"



// static int failed_multisamples = 0; // remember when GL multisample setting didn't succeed // EES: Pepperidge Farm remembers

//static void change_screen_mode(int width, int height, int depth, bool nogl, bool force_menu, bool force_resize_hud = false); // EES: representative line


//-----------------------------------------------------------------------------
// Hide/show mouse pointer (moved here from Input/mouse.cpp)


void hide_cursor()
{
    if (graphics_preferences.fullscreen || get_app_state() == app_state_t::game_in_progress)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
}

void show_cursor()
{
    SDL_ShowCursor(SDL_ENABLE);
}

bool cursor_is_hidden()
{
    return SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE;
}


//-----------------------------------------------------------------------------
// rebuilt Screen class for managing main SDL window and its display


Screen main_screen;


// TODO: if Standard-definition performance on low-end machines proves poor, add an 'Economy' mode that is ~half SD height (SDL_SetWindowDisplayMode presumably sets the display resolution in fullscreen; we might want to call it for fullscreen SD and classic as well)

// supported screen sizes: original M2 resolutions or the SDL_Window's (coordinate) size (clipped to 16:9 if display is ultrawide but the extra width is disallowed for PvP)

// TODO: for scenarios that *require* OGL (e.g. they use 3D models) we need a way to override the classic modes' modern=false setting

// TODO: for ultrawide screens the FOV needs automatically calculated from screen aspect (we can get rid of the manual FOV slider)

// w=h=0 = calculate virtual screen size based on window size (this doesn't include high-dpi)
const std::array<screen_mode_definition_t, 6> screen_modes = { // definitions must be ascending order of ids and screen sizes
    screen_mode_t::classic_8,    640, 480,  8, false, true,  false, // 640×480 (Classic 8-bit)
    screen_mode_t::classic_16,   800, 600, 16, false, true,  false, // 800×600 (Classic 16-bit)
    screen_mode_t::sd,             0,   0, 32, true,  false, false, // Standard
    screen_mode_t::hd,             0,   0, 32, true,  true,  false, // High-Definition
    screen_mode_t::sd_ultrawide,   0,   0, 32, true,  false, true,  // Ultrawide = wider than 2:1
    screen_mode_t::hd_ultrawide,   0,   0, 32, true,  true,  true,  // Ultrawide HD
    // one option we don't provide is an ultrawide ('letterbox') ratio on a standard 16:9 display; I think if one PvP user has a standard 16:9 and another 32:9, making both 32:9 would leave the standard-display user unhappy at small size of view
};


const screen_mode_definition_t old_school_screen_size = {screen_mode_t::classic_8, 640, 480,  8, false, false, false}; // 320x240 pixel-doubled 256-color is the lowest quality we'll go, and only on 4/1 (while M2 also supported 75%, 50%, and/or every-other-line, those were really only intended to get MacII users to buy the game, not to make it pleasant/playable)


//-----------------------------------------------------------------------------


void Screen::print_debug()
{
    printf("------------------------------------\n");
    printf("SCREEN MODE %i\n", m_mode->mode);
    SDL_Rect r = {0, 0};
    //get_window_coordinates_size(r.w, r.h);
    //printf(" window:    {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    SDL_Point p = window_pixel_size();
    printf(" window:       {%3d, %3d, %3d, %3d} %0.1f\n", 0, 0, p.x, p.y, float(p.x) / p.y);
    r = virtual_screen_pixel_rect();
    printf(" viewport px:  {%3d, %3d, %3d, %3d} %0.1f\n", r.x, r.y, r.w, r.h, float(r.w) / r.h);
    p = m_mode->size();
    printf(" virtual:      {%3d, %3d, %3d, %3d}\n", 0, 0, p.x, p.y);
    /*
    r = m_worldview_rect;
    printf(" worldview: {%3d, %3d, %3d, %3d}\n", r.x, r.y, r.w, r.h);
    r = m_automap_rect;
    printf(" automap:   {%3d, %3d, %3d, %3d}\n", r.x, r.y, r.w, r.h);
    r = m_terminal_rect;
    printf(" terminal:  {%3d, %3d, %3d, %3d}\n", r.x, r.y, r.w, r.h);
    r = m_hud_rect;
    printf(" hud:       {%3d, %3d, %3d, %3d}\n", r.x, r.y, r.w, r.h);
    r = virtual_screen_pixel_rect();
    printf(" virtual:   {%3d, %3d, %3d, %3d}\n", r.x, r.y, r.w, r.h);
     */
    printf("------------------------------------\n");
}


//-----------------------------------------------------------------------------
// configure


void Screen::initialize() // TODO: this is called in initialize_application and again in display_main_preferences_dialog (the latter needs reworking)
{
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    
    SDL_DisplayMode desktop; // widths are in screen co-ordinates
    ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
    if (err || desktop.w < 640 || desktop.h < 480) { throw_ao_exception("Failed to initialize screen.", err); }
    
    int window_width = 0, window_height = 0;
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI; // SDL_WindowFlags // SDL_WINDOW_RESIZABLE
    
    if (graphics_preferences.fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        window_width = 640; // temporary values; the did_change call below will set to calculated size
        window_height = 480;
    }
    
    m_window = SDL_CreateWindow(get_application_name().c_str(),
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                window_width, window_height, flags);
    
    SDL_SetWindowTitle(m_window, get_application_name().c_str());
    
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, ogl_preferences.Multisamples > 0 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, ogl_preferences.Multisamples);
    SDL_GL_SetSwapInterval(1); // EES: WaitForVSync always on
    
    SDL_GL_CreateContext(m_window);
    
#if defined (__WIN32__)
    glewInit();
#endif
    
    initialize_supported_modes(desktop);
    
    
    // a little something for 4/1 // TODO: as another Easter egg, play JingleBobs on 12/25
    time_t seconds = time(nullptr);
    tm now = *gmtime(&seconds);
    if (now.tm_mon  == 4 && now.tm_mday == 1 && now.tm_hour < 12)
    {
        m_mode = &old_school_screen_size; // TODO: also use M1 HUD
    }
    else
    {
        set_mode(graphics_preferences.screen_mode);
        //set_size(screen_size_t::classic_16); // DEBUG // TODO: FIX: SW renderer is crash
    }
    
    did_change();

    //configure_for_classic_ui();
}


void Screen::initialize_supported_modes(SDL_DisplayMode& desktop)
{
    bool high_dpi_supported = supports_high_dpi();
    bool ultrawide_supported = supports_ultrawide();
    
    m_available_screen_sizes.clear();
    for (const auto& mode : screen_modes)
    {
        int32_t w = mode.w, h = mode.h;
        
        if (w == 0) { w = desktop.w; h = desktop.h; }
        
        if (w <= desktop.w && h <= desktop.h && (!mode.high_dpi || high_dpi_supported) && (!mode.ultrawide || ultrawide_supported))
        {
            // if the display is wider than 16:9, the SD and HD modes use a 16:9 virtual screen with black bars at sides
            if (!mode.ultrawide && (w * 9 > h * 16)) { w = (h * 16) / 9; }
            
            m_available_screen_sizes.push_back({mode.mode, w, h, mode.bit_depth, mode.modern, mode.high_dpi, mode.ultrawide});
        }
    }
    if (m_available_screen_sizes.empty()) { throw_ao_exception("Unsupported monitor.", STRID(strERRORS, badMonitor)); }
}


screen_mode_names_t Screen::supported_modes()
{
    screen_mode_names_t result;
    for (const auto& size : m_available_screen_sizes)
    {
        result.push_back({(int32_t)size.mode, get_string(STRID(strScreenSize, (int32_t)size.mode))});
    }
    return result;
}


// methods that change the window size/bit-depth/virtual screen size MUST call did_change after
void Screen::did_change()
{
    static const screen_mode_definition_t* prev_size = nullptr;
    bool was_modern = prev_size && prev_size->modern;
    bool is_modern = m_mode->modern;
    prev_size = m_mode;
    
    int32_t window_w = 0, window_h = 0;

    if (graphics_preferences.fullscreen)
    {
        // TODO: what about hiding mouse in fullscreen?
    }
    else // windowed mode
    {
        if (get_app_state() != app_state_t::game_in_progress) { show_cursor(); } // except for in-game, where mouse controls player, always keep mouse cursor visible
        
        SDL_DisplayMode desktop; // widths are in screen co-ordinates
        ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
        // let's roll with reasonable minimum size of computer monitor
        if (err || desktop.w < 640 || desktop.h < 480) { throw_ao_exception("Failed to initialize screen.", err); }
        
        int32_t scale;
        if (desktop.w >= desktop.h) // landscape
        {
            scale = (m_mode->h == 480) ? 40 : 50; // make window smaller on classic_8 than classic_16, or users will think size hasn't changed
        }
        else // portrait
        {
            scale = (m_mode->h == 480) ? 80 : 100; // 640px uses 80% width, >=800px uses full width
        }
        
        if (m_mode->ultrawide) // the whole enchillada, 23:9, 32:9, whatever
        {
            window_w = desktop.w * scale / 100;
            window_h = desktop.h * scale / 100;
            // TODO: FOV will need auto-adjusted too
        }
        if (m_mode->modern) // this is the display's native aspect up to 16:9 // TODO: kinda tempted to fix at 16:9
        {
            window_w = std::min((desktop.w * scale / 100), (desktop.h * scale * 16 / 900));
            window_h = desktop.h * scale / 100;
        }
        else // classic; this is always 4:3 aspect and always uses SW renderer
        {
            window_w = desktop.h * scale * 4 / 300;
            window_h = desktop.h * scale / 100;
        }
        
        SDL_SetWindowFullscreen(m_window, graphics_preferences.fullscreen);
        SDL_SetWindowSize(m_window, window_w, window_h);
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        
    }
    
    SDL_Rect viewport = virtual_screen_pixel_rect();
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (game_is_running())
    {
        glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
        glOrtho(0, m_mode->w, m_mode->h, 0, -1.0, 1.0);
        
        main_camera_settings.initialize(m_mode->w, m_mode->h);
        
        // unload_all_collections(); // TODO: this should be called appropriately in the following start_/stop_ functions (note: we need to reload Shapes when switching to/from/between Classic modes; switching between modern modes shouldn't reload)
        
        if (is_modern)
        {
            if (!was_modern) { stop_classic_renderer(); }
            start_modern_renderer(); // always reconfigure the 3D OGL renderer for the new screen size
        }
        else
        {
            if (was_modern) { stop_modern_renderer(); }
            classic_renderer_buffer.configure(m_mode->w, m_mode->h, m_mode->bit_depth);
        }
    }
    else // UI
    {
        // kludge: for now, use legacy M2 640x480; TODO: Interface/ modules need to tell Screen what size of vscreen to use; the default should be legacy 640x480; however, this will be overrideable once we deal with MML
        
        int32_t vw = viewport.h * 4 / 3;
        int32_t vx = (window_pixel_size().x - vw) / 2;
        glViewport(vx, viewport.y, vw, viewport.h);
        log_note_f("Screen::mode_changed set OGL viewport: {%d, %d, %d, %d} (aspect: %.2f)\n", vx, viewport.y, vw, viewport.h, float(vw) / viewport.h);
        log_note_f(" window pixel size: {%d, %d} (aspect: %.2f)", window_pixel_size().x, window_pixel_size().y, window_pixel_size().x / window_pixel_size().y);
        glOrtho(0, 640, 480, 0, -1.0, 1.0);
        
    }
    
    
    // TODO: update these; for Classic, set automatically; for Modern, the main HUD plugin presumably dictates
    
    m_worldview_rect = calculate_worldview_rect();
    m_automap_rect   = calculate_automap_rect();
    m_terminal_rect  = calculate_terminal_rect();
    m_hud_rect       = calculate_hud_rect();
    
#ifdef DEBUG
    //print_debug();
#endif
    
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    if (game_is_running() && graphics_preferences.hud_size > 0) { L_Call_HUDResize(); }
    
    clear_screen();
    
    recenter_mouse();
    fps_counter.reset();
    
}


//-----------------------------------------------------------------------------
// screen settings


// TODO: these may need refining
bool Screen::supports_high_dpi()
{
    // when updating for SDL3, see: https://wiki.libsdl.org/SDL3/README-highdpi
    assert_fail(m_window, "");
    int32_t cw, ch, dw, dh;
    SDL_GetWindowSize(m_window, &cw, &ch);
    SDL_GL_GetDrawableSize(m_window, &dw, &dh);
    return (dw > cw);
}


bool Screen::supports_ultrawide() // is display wider than 2:1? (ultrawide = >16:9; use 2:1, which is slightly wider, to be sure)
{
    assert_fail(m_window, "");
    int32_t cw, ch;
    SDL_GetWindowSize(m_window, &cw, &ch);
    return (cw > ch * 2);
}


void Screen::set_mode(screen_mode_t new_size)
{
    screen_mode_t old_size = m_mode ? m_mode->mode : screen_mode_t::classic_8;
    // easiest way to set to a supported size is to start with smallest and increase till it can't increase any more
    m_mode = &m_available_screen_sizes.front();
    
    new_size = std::clamp(new_size, screen_modes.front().mode, screen_modes.back().mode);
    for (const auto& size : m_available_screen_sizes)
    {
        if (size.mode > new_size) break;
        m_mode = &size;
    }
    log_note_f("Set screen mode to \"%s\"", get_string(STRID(strScreenSize, (int32_t)m_mode->mode)).c_str());
    
    if (m_mode->mode != old_size)
    {
        graphics_preferences.screen_mode = m_mode->mode; // bit of a bodge till we improve Preferences/
        did_change();
    }
}


screen_mode_t Screen::mode()
{
    return graphics_preferences.screen_mode;
}


bool Screen::decrease_mode()
{
    screen_mode_t old_size_id = m_mode->mode;
    for (const auto& size : m_available_screen_sizes)
    {
        if (size.mode > old_size_id) { break; }
        m_mode = &size;
    }
    bool changed = m_mode->mode != old_size_id;
    if (changed) { did_change(); }
    return changed;
}


bool Screen::increase_mode()
{
    screen_mode_t old_size_id = m_mode->mode;
    for (const auto& size : m_available_screen_sizes)
    {
        if (size.mode > old_size_id)
        {
            m_mode = &size;
            break;
        }
    }
    bool changed = m_mode->mode != old_size_id;
    if (changed) { did_change(); }
    return changed;
}


#define WINDOWED_PERCENTAGE_SIZE (50)


void Screen::set_fullscreen(bool is_fullscreen)
{
    if (is_fullscreen != graphics_preferences.fullscreen)
    {
        graphics_preferences.fullscreen = is_fullscreen;
        if (is_fullscreen)
        {
            SDL_SetWindowSize(m_window, m_mode->w, m_mode->h);
            SDL_SetWindowFullscreen(m_window, 0);
        }
        else
        {
            SDL_SetWindowSize(m_window, m_mode->w * WINDOWED_PERCENTAGE_SIZE / 100, m_mode->h * WINDOWED_PERCENTAGE_SIZE / 100);
            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        did_change();
    }
}


void Screen::toggle_fullscreen()
{
    set_fullscreen(!graphics_preferences.fullscreen);
}


bool is_fullscreen()
{
    return graphics_preferences.fullscreen;
}



//-----------------------------------------------------------------------------


/*
SDL_Point Screen::fullscreen_pixel_size()
{
    SDL_DisplayMode desktop; // widths are in screen co-ordinates
    ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
    if (err) { throw_ao_exception("Failed to initialize screen.", err); }
    return convert_coordinate_to_pixel_position({desktop.w, desktop.h});
}


float Screen::fullscreen_aspect() // TODO: if the user's display is portrait orientation this method returns that aspect (e.g. 9:16); should Modern adopt portrait camera (with narrowed FOV) or letterbox the display to a 4:3 or 16:9 viewport?
{
    SDL_DisplayMode desktop; // widths are in screen co-ordinates
    ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
    if (err) { throw_ao_exception("Failed to initialize screen.", err); }
    return float(desktop.w) / float(desktop.h);
}
*/


SDL_Point Screen::window_pixel_size()
{
    int32_t w, h;
    SDL_GL_GetDrawableSize(m_window, &w, &h);
    return {w, h};
}


float Screen::pixel_to_virtual_scale()
{
    return m_mode->w / virtual_screen_pixel_rect().w;
}


SDL_Point Screen::convert_coordinate_to_pixel_position(const SDL_Point& point)
{
    int32_t coord_h, pixel_h;
    SDL_GetWindowSize(m_window, nullptr, &coord_h);
    SDL_GL_GetDrawableSize(m_window, nullptr, &pixel_h);
    return {point.x * pixel_h / coord_h, point.y * pixel_h / coord_h};
}


SDL_Point Screen::convert_pixel_to_virtual_position(const SDL_Point& point)
{
    SDL_Rect pixel_rect = virtual_screen_pixel_rect();
    SDL_Rect vrect = virtual_screen_rect();
    
    return {((point.x - pixel_rect.x) * vrect.w / pixel_rect.w),
            ((point.y - pixel_rect.y) * vrect.h / pixel_rect.h)};
}


SDL_Point Screen::get_mouse_pixel_position() 
{
    SDL_Point p;
    SDL_GetMouseState(&p.x, &p.y);
    return convert_coordinate_to_pixel_position(p);
}


SDL_Point Screen::get_mouse_virtual_position()
{
    return convert_pixel_to_virtual_position(get_mouse_pixel_position());
}


// the location of the virtual screen in the SDL Window
// To set up for 2D drawing (UI and Classic in-game): 1. pass this rect to glViewport to set the OGL drawing area for the full game screen, 2. pass the virtual screen size to glOrtho to project into it.
SDL_Rect Screen::virtual_screen_pixel_rect()
{
    int32_t w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    float window_aspect = float(w) / float(h), target_aspect = float(m_virtual_screen_current_size.x) / float(m_virtual_screen_current_size.y); //m_mode->aspect();

    SDL_Rect rect;
    if (target_aspect >= window_aspect) // fill window width; pad top and bottom
    {
        int32_t screen_h;
        rect.x = 0;
        SDL_GL_GetDrawableSize(m_window, &rect.w, &screen_h);
        rect.h = rect.w / target_aspect;
        rect.y = (screen_h - rect.h) / 2;
    }
    else // fill window height; pad left and right
    {
        int32_t screen_w;
        rect.y = 0;
        SDL_GL_GetDrawableSize(m_window, &screen_w, &rect.h);
        rect.w = rect.h * target_aspect;
        rect.x = (screen_w - rect.w) / 2;
    }
   // log_note_f("virtual_screen_pixel_rect: {%3d, %3d, %3d, %3d} (aspect: %.2f)\n", rect.x, rect.y, rect.w, rect.h, float(rect.w) / rect.h);
    return rect;
}


//-----------------------------------------------------------------------------
// map the virtual screen to the Window's OGL viewport


void Screen::set_virtual_screen_size(const SDL_Point& size)
{
    m_virtual_screen_current_size = size;
    
    SDL_Rect viewport = virtual_screen_pixel_rect();
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
    glOrtho(0, size.x, size.y, 0, -1.0, 1.0);
}


// chapter_screen and main_menu should use this with legacy images; for HD/widescreen, if an image is 640 wide OR 480 high we can assume it's legacy but things get messy since we can't assume Modern images will be 16:9 (they probably will be, but images up to 21:9 or even 32:9 would look very impressive on Ultrawide displays)
void Screen::configure_for_classic_ui() // also Classic in-game rendering, which uses the original M2 SW renderer to draw the gameworld into an SDL_Surface pixel buffer and uses 2D OGL to composite that with the Classic M2 HUD (which is now drawn by Lua plugin)
{
    set_virtual_screen_size({640, 480});
}


void Screen::configure_for_modern_ui(const SDL_Point& size)
{
    set_virtual_screen_size(m_virtual_screen_current_size);
}


void Screen::configure_for_game()
{
    set_virtual_screen_size(m_mode->size());
}


//-----------------------------------------------------------------------------
// TODO: these need redone


void Screen::get_window_coordinates_size(int32_t& w, int32_t& h) // SD dimensions (e.g. mouse coordinates)
{
    SDL_GetWindowSize(m_window, &w, &h);
}


// TODO: update these 2 (needed for automap, terminal, maybe HUD)

void Screen::set_virtual_drawing_rect(SDL_Rect &rect, bool drawing_uses_vscreen_origin) // called by Canvas_OGL::apply_clip
{
    //m_vscreen_clip_rect = rect;
    
    SDL_Rect pixel_rect = virtual_screen_pixel_rect();
    pixel_rect.x = 0; pixel_rect.y = 0;
    
    if (drawing_uses_vscreen_origin) //
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(pixel_rect.x, pixel_rect.y, pixel_rect.w, pixel_rect.h);
    }
    else
    {
        glViewport(pixel_rect.x, pixel_rect.y, pixel_rect.w, pixel_rect.h);
        glOrtho(pixel_rect.x, (pixel_rect.x + pixel_rect.w), (pixel_rect.y + pixel_rect.h), pixel_rect.y, -1.0, 1.0); // TODO: is this appropriate?
    }
}


void Screen::unset_virtual_drawing_rect()
{
    SDL_Rect r = {0, 0, m_vscreen_w, m_vscreen_h};
    set_virtual_drawing_rect(r);
}


//-----------------------------------------------------------------------------
// rects are recalculated when did_change is called


SDL_Rect Screen::calculate_worldview_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    
    SDL_Rect vscreen = virtual_screen_pixel_rect();
    
	SDL_Rect r;
    
    // TODO: this obviously needs a rethink: all rects should be calculated at enter_gameworld and whenever user changes resolution or resizes hud while in-game
    
	if (hud_is_visible())
	{

		r.x = lua_view_rect.x + (screen_w - vscreen.w) / 2;
		r.y = lua_view_rect.y + (screen_h - vscreen.h) / 2;
		r.w = MIN(lua_view_rect.w, vscreen.w - lua_view_rect.x);
		r.h = MIN(lua_view_rect.h, vscreen.h - lua_view_rect.y);
	}
	else
	{
        int available_height = vscreen.h; // - hud_rect().h;
		if (vscreen.w > available_height * 2)
		{
			r.w = available_height * 2;
			r.h = available_height;
		}
		else
		{
			r.w = vscreen.w;
			r.h = vscreen.w / 2;
		}
		r.x = (screen_w - r.w) / 2;
		r.y = (screen_h - vscreen.h) / 2 + (available_height - r.h) / 2;
	}
    
	return r;
}


SDL_Rect Screen::calculate_automap_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = virtual_screen_pixel_rect();
    
	SDL_Rect r;
	if (hud_is_visible())
    {
		r.x = lua_map_rect.x + (screen_w - vscreen.w) / 2;
		r.y = lua_map_rect.y + (screen_h - vscreen.h) / 2;
		r.w = MIN(lua_map_rect.w, vscreen.w - lua_map_rect.x);
		r.h = MIN(lua_map_rect.h, vscreen.h - lua_map_rect.y);
        return r;
    }
	if (automap_is_translucent())
		return worldview_rect();
	
	r.w = vscreen.w;
	r.h = vscreen.h;
	if (hud_is_visible())
		r.h -= hud_rect().h;

	r.x = (screen_w - vscreen.w) / 2;
	r.y = (screen_h - vscreen.h) / 2;

	return r;
}


SDL_Rect Screen::calculate_terminal_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = main_screen.virtual_screen_pixel_rect();
    
    vscreen.x = (screen_w - vscreen.w) / 2;
    vscreen.y = (screen_h - vscreen.h) / 2;
	
	if (hud_is_visible())
	{
        vscreen.x += lua_term_rect.x;
        vscreen.y += lua_term_rect.y;
        vscreen.w = MIN(lua_term_rect.w, vscreen.w - lua_term_rect.x);
        vscreen.h = MIN(lua_term_rect.h, vscreen.h - lua_term_rect.y);
	}
	
	int available_height = vscreen.h;
    //if (hud() && !lua_hud()) { available_height -= hud_rect().h; }
	
	SDL_Rect term_rect = get_interface_rect(_terminal_screen_rect);

    SDL_Rect r = {0, 0, term_rect.w, term_rect.h};

	float aspect = r.w / static_cast<float>(r.h);
    switch (graphics_preferences.terminal_size)
	{
		case 1:
            if (available_height >= (r.h * 2) && vscreen.w >= (r.w * 2)) { r.w *= 2; }
			break;
		case 2:
			r.w = std::min(vscreen.w, std::max(static_cast<int>(r.w), static_cast<int>(aspect * available_height)));
			break;
	}
	r.h = r.w / aspect;
	r.x = vscreen.x + (vscreen.w - r.w) / 2;
	r.y = vscreen.y + (available_height - r.h) / 2;

	return r;
}


SDL_Rect Screen::calculate_hud_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = main_screen.virtual_screen_pixel_rect();

	SDL_Rect r;
	r.w = 640;
	switch (graphics_preferences.hud_size)
	{
		case 1:
            if (vscreen.h >= 960 && vscreen.w >= 1280)
				r.w *= 2;
			break;
		case 2:
            r.w = std::min(vscreen.w, std::max(640, 4 * vscreen.h / 3));
			break;
	}
	r.h = r.w / 4;
	r.x = (screen_w - r.w) / 2;
    r.y = vscreen.h - r.h + (screen_h - vscreen.h) / 2;

	return r;
}


//-----------------------------------------------------------------------------
// rect-related




/*
void Screen::set_clipping_rect(SDL_Rect &r) // called by Canvas_OGL::apply_clip
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(m_viewport_rect.x + (r.x * m_viewport_rect.w / m_ortho_rect.w),
              m_viewport_rect.y + ((m_ortho_rect.h - r.y - r.h) * m_viewport_rect.h / m_ortho_rect.h),
              r.w * m_viewport_rect.w / m_ortho_rect.w,
              r.h * m_viewport_rect.h / m_ortho_rect.h);
}


void Screen::reset_clipping_rect()
{
    SDL_Point p = window_pixel_size();
    glScissor(0, 0, p.x, p.y);
}
*/


//-----------------------------------------------------------------------------
// game rendering entrypoint

// TODO: more cleanup; aside from the lua rects, which belong in their own methods, these three functions should relocate to Render3D/



// Set-up and tear-down for game rendering; called in enter_gameworld, exit_gameworld

void set_lua_rects()
{
    	
    // TODO: move the rest of this code into its own method (Q. if user resizes the screen in-game, do these rects need recalcuated?)
    // TODO: more thought needed, especially as loading multiple HUD plugins could have them stomping on each others' screen settings/drawing areas if they can't coordinate effectively
    int screen_w, screen_h;
    main_screen.get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = main_screen.virtual_screen_pixel_rect();
    
    main_screen.lua_clip_rect.x = 0;
    main_screen.lua_clip_rect.y = 0;
    main_screen.lua_clip_rect.w = screen_w;
    main_screen.lua_clip_rect.h = screen_h;
	
    main_screen.lua_view_rect.x = main_screen.lua_map_rect.x = (screen_w - vscreen.w) / 2;
    main_screen.lua_view_rect.y = main_screen.lua_map_rect.y = (screen_h - vscreen.h) / 2;
    main_screen.lua_view_rect.w = main_screen.lua_map_rect.w = vscreen.w;
    main_screen.lua_view_rect.h = main_screen.lua_map_rect.h = vscreen.h;
    
    // not sure; messages overlay?
    main_screen.lua_text_margins.top = 0;
    main_screen.lua_text_margins.left = 0;
    main_screen.lua_text_margins.bottom = 0;
    main_screen.lua_text_margins.right = 0;
	
    SDL_Rect term_rect = get_interface_rect(_terminal_screen_rect);
    main_screen.lua_term_rect.x = (screen_w - term_rect.w) / 2;
    main_screen.lua_term_rect.y = (screen_h - term_rect.h) / 2;
    main_screen.lua_term_rect.w = term_rect.w;
    main_screen.lua_term_rect.h = term_rect.h;

	L_Call_HUDResize();
}


void Screen::start_gameworld_renderer()
{
    configure_for_game();
    
    if (modern_3D())
        start_modern_renderer();
    else
        start_classic_renderer(m_mode->w, m_mode->h, m_mode->bit_depth);
}


void Screen::stop_gameworld_renderer()
{
    // they should never both be active, but...
    if (modern_renderer_is_active()) { stop_modern_renderer(); }
    if (classic_renderer_is_active()) { stop_classic_renderer(); }
    configure_for_classic_ui();
}




// EES: originally `render_screen`, which was absolutely full of shite
void render_game_to_screen(short ticks_elapsed)
{
    update_main_camera(ticks_elapsed); // currently defined in interpolated_world.cpp (EES: moved view-updating code from here into interpolate_world_view and renamed it update_main_camera)
    
    // TODO: finish rebuilding this function; do we need to clear_screen before we start drawing? what about the classic_renderer_buffer?
    
    SDL_Rect vscreen = main_screen.virtual_screen_pixel_rect();
    
    main_camera_settings.initialize(vscreen.w, vscreen.h); // TODO: not sure if this is right, but we're missing something in our 3D rendering setup

	// Set OpenGL viewport to world view
    SDL_Rect ViewRect = main_screen.worldview_rect();
	main_screen.set_virtual_drawing_rect(ViewRect);
    // Set OpenGL viewport to whole window (so HUD will be in the right position) // yuck; unknotting this crap is WIP
	OGL_SetWindow(ViewRect);
    
    // TODO: setting these flags is TBD - obviously with multiplayer films the current_player changes as user switches player views
        
    if (!computer_terminal_is_visible() && (modern_renderer_is_active() || !automap_is_visible()))
    {
        render_gameworld_view(&main_camera_settings);
    }
    
    if (computer_terminal_is_visible()) // player_in_terminal_mode(current_player_index)
    {
        // TODO: render terminal in its rect
    }
    
    if (automap_is_visible())
    {
        // TODO: render automap
        
#ifdef AUTOMAP_DEBUG
         clear_automap();
#endif
        ResetOverheadMap();
        render_overhead_map();
    }
    
    if (hud_is_visible())
    {
       // Lua_DrawHUD(ticks_elapsed); // once there'a a vector of active HUD plugins, we'll need to decide how best to trigger them, e.g. 1. send them all a draw() and let them query app state to determine what, if anything, they should draw (radar, inventory, crosshairs, messages); or, 2. let HUD plugins declare what feature[s] they support (e.g. radar + inventory + crosshairs) and let the app decide which to trigger (e.g. a user/scenario might install their own crosshairs plugin, in which case that should override the crosshairs feature of preceding plugins) [inclined to #2 as it shouldn't draw more than one of each thing; obviously existing plugins will need updated to support multi-HUD, though a 'legacy' HUD should still work]

        
         // TODO: also draw screen overlay
        
        /*
        // Display FPS and position
        if (!computer_terminal_is_visible())
        {
           DisplayPosition(dst_surface);
           DisplayScores(dst_surface);
        }
        DisplayMessages(dst_surface);
        DisplayInputLine(dst_surface);
         */
    }

    //update_fps_display(dst_surface);

    main_screen.swap();
    
    // reset the full-screen viewport (pixel size, excluding any black padding at sides)
    main_screen.unset_virtual_drawing_rect();
    
    FilmExporter::instance()->AddFrame(FilmExporter::FRAME_NORMAL);
}


//*****************************************************************************
// clear/darken window


void clear_screen(bool swap)
{
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (swap)
    {
        main_screen.swap();
        clear_screen(false);
    }
}



// TODO: this needs to be called between active_renderer.begin() and active_renderer.end()
void darken_world_window()
{
	if (modern_renderer_is_active())
    {
        // for Modern, shade the entire in-game view (including floating HUD, messages, etc)
        
        SDL_Rect rect = main_screen.virtual_screen_pixel_rect(); // TODO: appropriate?
        
		// Save current state
		glPushAttrib(GL_ALL_ATTRIB_BITS);

		// Disable everything but alpha blending
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_FOG);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);

		// Direct projection
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
        glOrtho(0.0, GLdouble(rect.w), GLdouble(rect.h), 0.0, 0.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		// Draw 50% black rectangle
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.0, 0.0, 0.0, 0.5);
		OGL_RenderRect(rect);

		// Restore projection and state
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();
	}
    else // for Classic, overdraw the gameworld view (not M2 HUD) with dithered black pixels
    {
        // TODO: need to call classic_renderer_buffer.darken()
    }

    //        main_screen.swap();
}


//-----------------------------------------------------------------------------
// screenshot


SDL_Surface* copy_screen_to_surface() // used in dump_screen below
{
    SDL_Point window_size = main_screen.window_pixel_size();
    int32_t video_w = window_size.x, video_h = window_size.y;
    
    SDL_Surface *surface = SDL_CreateRGBSurface(SDL_SWSURFACE, video_w, video_h, 24, SDLRGBSurfaceBitmask);
    if (!surface) return nullptr;
    
    // Read OpenGL frame buffer
    void *pixels = ao_malloc(video_w * video_h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, video_w, video_h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);  // return to default
    
    // Copy pixel buffer (which is upside-down) to surface
    for (int y = 0; y < video_h; y++)
    {
        memcpy((uint8 *)surface->pixels + surface->pitch * y, (uint8 *)pixels + video_w * 3 * (video_h - y - 1), video_w * 3);
    }
    free(pixels);
    return surface;
}


void dump_screen()
{
    // Find suitable file name; TODO: YYYY-MM-DD HH-MM-SS datestamp might be better; it's longer but it's effectively unique and it's more descriptive
    ao_path path;
    int i = 0;
    do
    {
        const char* suffix;
#if defined(HAVE_SDL_IMAGE) && defined(HAVE_PNG)
        suffix = "png";
#else
        suffix = "bmp";
#endif
        char name[256];
        if (get_app_state() == app_state_t::game_in_progress)
        {
            std::string level_name = static_world.level_name;
            make_string_filesystem_safe(level_name);
            snprintf(name, sizeof(name), "%s_%04d.%s", level_name.c_str(), i, suffix);
        }
        else
        {
            snprintf(name, sizeof(name), "Screenshot_%04d.%s", i, suffix);
        }

        path = get_screenshots_dir() / name;
        i++;
    }
    while (std::filesystem::exists(path));

    SDL_Surface * surface = copy_screen_to_surface();
    
    // TODO: is there any earthly combination where both libraries aren't included?
#if defined(HAVE_SDL_IMAGE) && defined(HAVE_PNG)
    IMG_SavePNG(surface, path.c_str());
#else
    SDL_SaveBMP(surface, path.c_str());
#endif
    SDL_FreeSurface(surface);
}


