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

#include "screen.hpp"

#include "sdl_resize.h"

#include "mouse.h" // recenter_mouse


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
#include "screen.hpp"
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
    if (!graphics_preferences->fullscreen && get_app_state() != app_state_t::game_in_progress)
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
const std::array<screen_size_definition_t, 6> screen_sizes = { // definitions must be ascending order of ids and screen sizes
    screen_size_t::classic8,  640, 480,  8, false, true,  false, // 640×480 (Classic 8-bit)
    screen_size_t::classic16, 800, 600, 16, false, true,  false, // 800×600 (Classic 16-bit)
    screen_size_t::sd,          0,   0, 32, true,  false, false, // Standard
    screen_size_t::hd,          0,   0, 32, true,  true,  false, // High-Definition
    screen_size_t::sd_wide,     0,   0, 32, true,  false, true, // Ultrawide = wider than 2:1
    screen_size_t::hd_wide,     0,   0, 32, true,  true,  true, // Ultrawide HD
    // one option we don't provide is an ultrawide ('letterbox') ratio on a standard 16:9 display; I think if one PvP user has a standard 16:9 and another 32:9, making both 32:9 would leave the standard-display user unhappy at small size of view
};


const screen_size_definition_t old_school_screen_size = {screen_size_t::classic8, 640, 480,  8, false, false, false}; // 320x240 pixel-doubled 256-color is the lowest quality we'll go, and only on 4/1 (while M2 also supported 75%, 50%, and/or every-other-line, those were really only intended to get MacII users to buy the game, not to make it pleasant/playable)


std::vector<screen_size_definition_t> available_screen_sizes;


//-----------------------------------------------------------------------------
// configuration


void Screen::initialize() // TODO: this is called in initialize_application and again in display_main_preferences_dialog (the latter needs reworking)
{
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    
    SDL_DisplayMode desktop; // widths are in screen co-ordinates
    ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
    if (err || desktop.w < 640 || desktop.h < 480) { throw_ao_exception("Failed to initialize screen.", err); }
    
    int window_width = 0, window_height = 0;
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI; // SDL_WindowFlags
    
    if (graphics_preferences->fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        window_width = 640; // temporary values; the size_changed call below will set to calculated size
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
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, graphics_preferences->OGL_Configure.Multisamples > 0 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, graphics_preferences->OGL_Configure.Multisamples);
    SDL_GL_SetSwapInterval(graphics_preferences->OGL_Configure.WaitForVSync ? 1 : 0);
    
    SDL_GL_CreateContext(m_window);
    
#if defined (__WIN32__)
    glewInit();
#endif
    
    if (!(OGL_CheckExtension("GL_ARB_vertex_shader")  && OGL_CheckExtension("GL_ARB_fragment_shader") &&
          OGL_CheckExtension("GL_ARB_shader_objects") && OGL_CheckExtension("GL_ARB_shading_language_100")))
    {
        throw_ao_exception("Failed to initialize screen.", 2);
    }
    
    
    initialize_available_screen_sizes(desktop);
    
    
    // a little something to celebrate 4/1 // TODO: also include JingleBobs for 12/25
    time_t seconds = time(nullptr);
    tm now = *gmtime(&seconds);
    if (now.tm_mon  == 4 && now.tm_mday == 1 && now.tm_hour < 12)
    {
        m_size = &old_school_screen_size;
    }
    else
    {
        set_size(graphics_preferences->screen_size);
        //set_size(screen_size_t::classic16); // DEBUG // TODO: FIX: SW renderer is crash
    }
    
    size_changed();

    set_viewport_for_ui(); // TODO: the old change_screen_mode tickled so many things; there is a core of a good idea there, that we should set the drawing area so that the original M2 4:3 splash+main+game screens are sized to fill the height of a 16:9 window, centered with black on each side and code that assumes a 1995 640x480 display Just Works (e.g. main menu button rects); but disentangling AO's convolutions so its code expresses this both legibly and simply is a huge slog, so WIP
}


void Screen::initialize_available_screen_sizes(SDL_DisplayMode& desktop)
{
    bool high_dpi_supported = supports_high_dpi();
    bool ultrawide_supported = supports_ultrawide();
    
    available_screen_sizes.clear();
    for (const auto& size : screen_sizes)
    {
        int32_t w = size.w, h = size.h;
        if (!w) { w = desktop.w; h = desktop.h; }
        
        if (w <= desktop.w && h <= desktop.h && (!size.high_dpi || high_dpi_supported) && (!size.ultrawide || ultrawide_supported))
        {
            // if the display is wider than 16:9, the SD and HD modes use a 16:9 virtual screen
            if (!size.ultrawide && (w * 9 > h * 16)) { w = (h * 16) / 9; }
            
            available_screen_sizes.push_back({size.size, w, h, size.bit_depth, size.modern, size.high_dpi, size.ultrawide});
        }
    }
    if (available_screen_sizes.empty()) { throw_ao_exception("Unsupported monitor.", STRID(strERRORS, badMonitor)); }
}


// methods that change the screen (window) size/bit-depth MUST call size_changed after to update everything
void Screen::size_changed()
{
    static const screen_size_definition_t* prev_size = m_size;
    bool was_modern = prev_size->modern;
    
    if (!(m_size->modern && prev_size->modern))
    {
        unload_all_collections(); // TODO: only call when necessary (I suspect the SW renderer needs reload some/all Shapes collections when changing bit depth; plus we probably want to switch between Classic and Modern Shapes automatically to keep the look authentic)
    }
    prev_size = m_size;
    
    SDL_Rect vscreen;
    
    if (graphics_preferences->fullscreen) // TODO: setting windowed size is TBD
    {
        main_camera_settings.initialize(m_size->w, m_size->h); // TODO: what about hi-dpi?
        vscreen = {0, 0, m_size->w, m_size->h};
    }
    else
    {
        if (get_app_state() != app_state_t::game_in_progress) { show_cursor(); } // keep mouse visible when in windowed mode
        
        SDL_DisplayMode desktop; // widths are in screen co-ordinates
        ao_err err = SDL_GetCurrentDisplayMode(0, &desktop);
        // TODO: we should be able to support smaller
        if (err || desktop.w < 640 || desktop.h < 480) { throw_ao_exception("Failed to initialize screen.", err); }
        
        // TODO: if Classic, use 4:3 ratio
        int32_t window_width = std::max(desktop.w / 2, 320);
        int32_t window_height = std::max(desktop.h / 2, 240);
        SDL_SetWindowSize(m_window, window_width, window_height);
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        main_camera_settings.initialize(window_width, window_height); // TODO: what about hi-dpi?
        vscreen = {0, 0, window_width, window_height};
    }
    
    SDL_Rect window_rect = {0, 0};
    get_window_coordinates_size(window_rect.w, window_rect.h);
    
    main_screen.set_viewport_rect(window_rect, vscreen);

    if (game_is_running())
    {
        if (m_size->modern)
        {
            if (!was_modern) { stop_classic_renderer(); }
            start_modern_renderer(); // always reconfigure the 3D OGL renderer for the new screen size
        }
        else
        {
            if (was_modern) { stop_modern_renderer(); }
            classic_renderer_buffer.configure(m_size->w, m_size->h, m_size->bit_depth);
        }
    }
    
    
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    glViewport(0, 0, pixel_w, pixel_h);
    reset_clipping_rect();
    
    
    
    m_window_rect    = calculate_window_rect();
    m_worldview_rect = calculate_worldview_rect();
    m_automap_rect   = calculate_automap_rect();
    m_terminal_rect  = calculate_terminal_rect();
    m_hud_rect       = calculate_hud_rect();
    
#ifdef DEBUG
    print_debug();
#endif
    
    
    // TODO: and this?
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    if (game_is_running() && graphics_preferences->hud_size > 0) { L_Call_HUDResize(); }
    
    clear_screen();
    
    recenter_mouse();
    fps_counter.reset();
    
}


void Screen::print_debug()
{
    printf("------------------------------------\n");
    printf("SCREEN RECTS\n");
    SDL_Rect r = m_window_rect;
    printf(" window:    {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_viewport_rect;
    printf(" viewport:  {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_ortho_rect;
    printf(" ortho:     {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_worldview_rect;
    printf(" worldview: {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_automap_rect;
    printf(" automap:   {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_terminal_rect;
    printf(" terminal:  {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = m_hud_rect;
    printf(" hud:       {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    r = virtual_screen_rect();
    printf(" virtual:   {%03d, %03d, %03d, %03d}\n", r.x, r.y, r.w, r.h);
    printf("------------------------------------\n");
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


void Screen::set_size(screen_size_t new_size)
{
    screen_size_t old_size = m_size ? m_size->size : screen_size_t::classic8;
    // easiest way to set to a supported size is to start with smallest and increase till it can't increase any more
    m_size = &available_screen_sizes.front();
    
    new_size = std::clamp(new_size, screen_sizes.front().size, screen_sizes.back().size);
    for (const auto& size : available_screen_sizes)
    {
        if (size.size > new_size) break;
        m_size = &size;
    }
    log_note_f("Set screen mode to \"%s\"", get_string(STRID(strScreenSize, (int32_t)m_size->size)).c_str());
    
    if (m_size->size != old_size)
    {
        graphics_preferences->screen_size = m_size->size; // bit of a bodge till we improve Preferences/
        size_changed();
    }
}


screen_size_t Screen::size()
{
    return graphics_preferences->screen_size;
}


bool Screen::decrease_size()
{
    screen_size_t old_size_id = m_size->size;
    for (const auto& size : available_screen_sizes)
    {
        if (size.size > old_size_id) { break; }
        m_size = &size;
    }
    bool changed = m_size->size != old_size_id;
    if (changed) { size_changed(); }
    return changed;
}


bool Screen::increase_size()
{
    screen_size_t old_size_id = m_size->size;
    for (const auto& size : available_screen_sizes)
    {
        if (size.size > old_size_id)
        {
            m_size = &size;
            break;
        }
    }
    bool changed = m_size->size != old_size_id;
    if (changed) { size_changed(); }
    return changed;
}


#define WINDOWED_PERCENTAGE_SIZE (50)


void Screen::set_fullscreen(bool is_fullscreen)
{
    if (is_fullscreen != graphics_preferences->fullscreen)
    {
        graphics_preferences->fullscreen = is_fullscreen;
        if (is_fullscreen)
        {
            SDL_SetWindowSize(m_window, m_size->w, m_size->h);
            SDL_SetWindowFullscreen(m_window, 0);
        }
        else
        {
            SDL_SetWindowSize(m_window, m_size->w * WINDOWED_PERCENTAGE_SIZE / 100, m_size->h * WINDOWED_PERCENTAGE_SIZE / 100);
            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        size_changed();
    }
}


void Screen::toggle_fullscreen()
{
    set_fullscreen(!graphics_preferences->fullscreen);
}


//-----------------------------------------------------------------------------
// rects are recalculated when size_changed is called


SDL_Rect Screen::calculate_window_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = virtual_screen_rect();
	vscreen.x = (screen_w - vscreen.w) / 2;
	vscreen.y = (screen_h - vscreen.h) / 2;
	return vscreen;
}


SDL_Rect Screen::calculate_worldview_rect()
{
    int screen_w, screen_h;
    get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = virtual_screen_rect();
    
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
    SDL_Rect vscreen = virtual_screen_rect();
    
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
    SDL_Rect vscreen = main_screen.virtual_screen_rect();
    
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
    switch (graphics_preferences->terminal_size)
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
    SDL_Rect vscreen = main_screen.virtual_screen_rect();

	SDL_Rect r;
	r.w = 640;
	switch (graphics_preferences->hud_size)
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


void Screen::get_window_coordinates_size(int32_t& w, int32_t& h) // SD dimensions (e.g. mouse coordinates)
{
    SDL_GetWindowSize(m_window, &w, &h);
}


void Screen::get_window_pixel_size(int32_t& w, int32_t& h) // SD/HD dimensions (TODO: not sure how these relate to OGL drawing)
{
    SDL_GL_GetDrawableSize(m_window, &w, &h);
}


// scale factor between screen's true resolution and the game's effective resolution
float Screen::virtual_screen_to_pixel_scale()
{
    int w, h;
    main_screen.get_window_pixel_size(w, h);
    return w / static_cast<float>(virtual_screen_rect().w);
}



// TODO: update this; it is useful inasmuch as OGL makes it super easy to set the part of the window we want to draw to, typically the whole window in Modern (unless user has an ultrawide display, in which case chapter and main screen typically fill 16:9 center area only) and 4:3 area in Classic; best provide separate methods for UI screen and in-game

void Screen::set_viewport_rect(SDL_Rect &new_rect, SDL_Rect& vscreen)
{
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    
    float vscale = MIN(pixel_w / static_cast<float>(vscreen.w), pixel_h / static_cast<float>(vscreen.h));
    
    SDL_Rect viewport;
    viewport.x = static_cast<int>(pixel_w / 2.0f - (vscreen.w * vscale) / 2.0f + (new_rect.x * vscale) + 0.5f);
    viewport.y = static_cast<int>(pixel_h / 2.0f - (vscreen.h * vscale) / 2.0f + (new_rect.y * vscale) + 0.5f);
    viewport.w = static_cast<int>(new_rect.w * vscale + 0.5f);
    viewport.h = static_cast<int>(new_rect.h * vscale + 0.5f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(viewport.x, pixel_h - viewport.h - viewport.y, viewport.w, viewport.h);
    m_viewport_rect.x = viewport.x;
    m_viewport_rect.y = pixel_h - viewport.h - viewport.y;
    m_viewport_rect.w = viewport.w;
    m_viewport_rect.h = viewport.h;
    glOrtho(0, new_rect.w, new_rect.h, 0, -1.0, 1.0);
    m_ortho_rect.x = m_ortho_rect.y = 0;
    m_ortho_rect.w = new_rect.w;
    m_ortho_rect.h = new_rect.h;
}


/*
void Screen::reset_viewport_rect()
{
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    glViewport(0, 0, pixel_w, pixel_h);
    // TODO: what about glOrtho, m_viewport_rect, m_ortho_rect?
}
*/


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
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    glScissor(0, 0, pixel_w, pixel_h);
}


// converts the mouse position (SDL window coordinates) to a point on a virtual 640x480 display; used by main menu and dialogs (while it could also be used in map editor when mouse cursor is active, I suspect that should always run in Modern mode so no conversion needed)
// TODO: it would be tidier to convert button rects to window coordinates, though that table would need to be rebuilt when window size changes
void Screen::convert_window_coordinate_to_virtual_screen(int32_t &x, int32_t &y, const SDL_Rect& vscreen)
{
    int window_w, window_h;
    get_window_coordinates_size(window_w, window_h);
        
    float window_aspect = window_w / static_cast<float>(window_h);
    float virtual_aspect = vscreen.w / static_cast<float>(vscreen.h);
    
    if (window_aspect >= virtual_aspect)
    {
        float scale = window_h / static_cast<float>(vscreen.h);
        x -= (window_w - (vscreen.w * scale)) / 2;
        x /= scale;
        y /= scale;
    }
    else
    {
        float scale = window_w / static_cast<float>(vscreen.w);
        y -= (window_h - (vscreen.h * scale)) / 2;
        x /= scale;
        y /= scale;
    }
}


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
    SDL_Rect vscreen = main_screen.virtual_screen_rect();
    
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
    set_viewport_for_game();
    if (modern_3D())
    {
        start_modern_renderer();
    }
    else
    {
        start_classic_renderer(m_size->w, m_size->h, m_size->bit_depth);
    }
}


void Screen::stop_gameworld_renderer()
{
    // they should never both be active, but...
    if (modern_renderer_is_active()) { stop_modern_renderer(); }
    if (classic_renderer_is_active()) { stop_classic_renderer(); }
    set_viewport_for_ui();
}




// EES: originally `render_screen`, which was absolutely full of shite
void render_game_to_screen(short ticks_elapsed)
{
    update_main_camera(ticks_elapsed); // currently defined in interpolated_world.cpp (EES: moved view-updating code from here into interpolate_world_view and renamed it update_main_camera)
    
    // TODO: finish rebuilding this function; do we need to clear_screen before we start drawing? what about the classic_renderer_buffer?
    
    SDL_Rect vscreen = main_screen.virtual_screen_rect();
    
    main_camera_settings.initialize(vscreen.w, vscreen.h); // TODO: not sure if this is right, but we're missing something in our 3D rendering setup

	// Set OpenGL viewport to world view
    SDL_Rect ViewRect = main_screen.worldview_rect();
	main_screen.set_viewport_rect(ViewRect, vscreen);
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
    
    FilmExporter::instance()->AddFrame(FilmExporter::FRAME_NORMAL);
}


//*****************************************************************************
// TODO: these are taken out of render_[game_to_]screen so we can rebuild it one step at a time


// clear Lua drawing from previous frame
// (SDL is slower if we do this before render_view)
//if (!modern_renderer_is_active() && (MapIsTranslucent || hud_is_visible())) clear_screen_margin();

// yes, idiocy
/*
 // TODO: these calls need to use Canvas so they can be decoupled and [mostly] moved to Lua HUD plugins
 
 //SDL_Surface *dst_surface = automap_is_visible() ? Map_Buffer : world_pixels;


*/



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


// Draw dithered black pattern over world window (Classic?)
template <class T>
static inline void draw_pattern_rect(T *p, int pitch, uint32 pixel, const SDL_Rect &r)
{
	p += r.y * pitch / sizeof(T) + r.x;
	for (int y=0; y<r.h; y++) {
		for (int x=y&1; x<r.w; x+=2)
			p[x] = pixel;
		p += pitch / sizeof(T);
	}
}


void darken_world_window()
{
	// Get world window bounds
	SDL_Rect r = main_screen.window_rect();

	if (modern_renderer_is_active())
    {
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
        glOrtho(0.0, GLdouble(main_screen.window_rect().w), GLdouble(main_screen.window_rect().h), 0.0, 0.0, 1.0); // TODO: check this
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		// Draw 50% black rectangle
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.0, 0.0, 0.0, 0.5);
		OGL_RenderRect(r);

		// Restore projection and state
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();

//		main_screen.swap();
	}
    else // TODO: check if the original MacOS9 M2 draws a dithered black pixel effect
    {
        // TODO: this needs redone as there is no longer a `main_surface` to draw to (quick-n-nasty is to create a Surface here using screen_size.w+h+bit_depth, then use ImageBlitter to throw it on screen; clean and elegant is to use a shader)
        /*
        // Get black pixel value
        uint32 pixel = SDL_MapRGB(main_surface->format, 0, 0, 0);
        
        // Lock surface
        if (SDL_MUSTLOCK(main_surface))
            if (SDL_LockSurface(main_surface) < 0)
                return;
        
        // Draw pattern
        switch (main_surface->format->BytesPerPixel) {
            case 1:
                draw_pattern_rect((pixel8 *)main_surface->pixels, main_surface->pitch, pixel, r);
                break;
            case 2:
                draw_pattern_rect((pixel16 *)main_surface->pixels, main_surface->pitch, pixel, r);
                break;
            case 4:
                draw_pattern_rect((pixel32 *)main_surface->pixels, main_surface->pitch, pixel, r);
                break;
        }
        
        // Unlock surface
        if (SDL_MUSTLOCK(main_surface)) SDL_UnlockSurface(main_surface);
        
        sw_render_surface_to_screen();
         */
    }
}


//-----------------------------------------------------------------------------
// screenshot


SDL_Surface* copy_screen_to_surface() // used in dump_screen below
{
    int video_w, video_h;
    main_screen.get_window_pixel_size(video_w, video_h);
    
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


