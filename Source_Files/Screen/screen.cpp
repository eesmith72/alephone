/*
    screen_sdl.cpp - Screen management, SDL implementation
 
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


#include "cseries.h"

#include "sdl_resize.h"

#include "mouse.h" // recenter_mouse


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
#include "classic_renderer.hpp"

#include "shell.h"
#include "interface.h"
#include "interpolated_world.h"
#include "player.h"
#include "overhead_map.h"
#include "fades.h"
#include "game_window.h"
#include "screen.h"
#include "preferences.h"
#include "computer_interface.h"
#include "Crosshairs.h"
#include "OGL_Render.h"
#include "camera.h"
#include "screen_drawing.h"
#include "mouse.h"
#include "network.h"
#include "images.h"
#include "motion_sensor.hpp"

#include "screen_overlay.h"

#include "fonts.hpp"

#include "lua_script.h"
#include "lua_hud_script.h"
#include "FilmExporter.h"
#include "shell_options.h"


#define DESIRED_SCREEN_WIDTH 640
#define DESIRED_SCREEN_HEIGHT 480

#define DEFAULT_WORLD_WIDTH 640
#define DEFAULT_WORLD_HEIGHT 320



extern bool game_is_running(); // defined in game_event_loop, which is a bit awkward for #includes



screen_mode_data screen_mode; // TODO: why is this defined twice, here [originally screen_shared.cpp] and in graphics_preferences?


bool screen_needs_swapped = false; // set by `request_swap()` macro; allows HUD, terminal, etc subsystems to request a buffer swap after everything is drawn


// static int failed_multisamples = 0; // remember when GL multisample setting didn't succeed // EES: Pepperidge Farm remembers




// Hide/show mouse pointer (moved here from Input/mouse.cpp)

void hide_cursor()
{
    SDL_ShowCursor(SDL_DISABLE);
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
// screen modes; this is greatly simplified from AO and toggles bit-depth, high-DPI, and superwide display options in addition to [virtual] screen resolution

const screen_size_t old_school_screen_size = {-1, "Back to the Future!",  640, 480,  8, false, false, false}; // 320x240 pixel-doubled is the lowest quality we'll


// TODO: if SD performance on low-end machines is poor, add a Low mode that is 50% SD height


const std::array<screen_size_t, 6> screen_sizes = { // definitions must be ascending order of ids and screen sizes
    0, "640×480 (Classic 8-bit)",  640, 480,  8, false, true,  false,
    1, "800×600 (Classic 16-bit)", 800, 600, 16, false, true,  false,
    2, "Standard",                   0,   0, 32, true,  false, false, // 0 = use native screen size (coordinates, not pixel)
    3, "High-Definition",            0,   0, 32, true,  true,  false, // ditto
    4, "Super-Wide",                 0,   0, 32, true,  false, true,
    5, "Super-Wide HD",              0,   0, 32, true,  true,  true,
};


std::vector<screen_size_t> available_screen_sizes;


std::vector<std::string> Screen::get_screen_size_names() // TODO: return vector of {size_id,name}
{
    static std::vector<std::string> result;
    result.clear();
    for (const auto& size : available_screen_sizes) { result.push_back(size.name); }
    return result;
}


// TODO: these may need refining
bool Screen::supports_high_dpi()
{
    assert_fail(m_window, "");
    int32_t cw, ch, dw, dh;
    SDL_GetWindowSize(m_window, &cw, &ch);
    SDL_GL_GetDrawableSize(m_window, &dw, &dh);
    return (dw > cw);
}

bool Screen::supports_superwide()
{
    assert_fail(m_window, "");
    int32_t cw, ch;
    SDL_GetWindowSize(m_window, &cw, &ch);
    return (cw > ch * 2);
}


//-----------------------------------------------------------------------------
//


Screen current_screen;



void Screen::initialize_available_screen_sizes(SDL_DisplayMode& desktop)
{
    bool high_dpi_supported = supports_high_dpi();
    bool superwide_supported = supports_superwide();
    
    available_screen_sizes.clear();
    for (const auto& size : screen_sizes)
    {
        int32_t w = size.w, h = size.h;
        if (!w) { w = desktop.w; h = desktop.h; }
        
        if (w <= desktop.w && h <= desktop.h && (!size.high_dpi || high_dpi_supported) && (!size.superwide || superwide_supported))
        {
            // if the display is wider than 16:9, the SD and HD modes clip to 16:9 ratio
            if (!size.superwide && w * 9 > h * 16) { w = (h * 16) / 9; }
            available_screen_sizes.push_back({size.size_id, size.name, w, h, size.bit_depth, size.modern, size.high_dpi, size.superwide});
        }
    }
    if (available_screen_sizes.empty()) { throw_ao_exception("Unsupported monitor.", STRID(strERRORS, badMonitor)); }
}



void Screen::set_screen_size(int32_t size_id)
{
    if (size_id == old_school_screen_size.size_id)
    {
        m_size = &old_school_screen_size;
    }
    else
    {
        m_size = &available_screen_sizes.front();
        size_id = std::clamp(size_id, screen_sizes.front().size_id, screen_sizes.back().size_id);
        for (const auto& size : available_screen_sizes)
        {
            if (size.size_id > size_id) break;
            m_size = &size;
        }
    }
    log_note_f("Set screen mode to \"%s\"", m_size->name.c_str());
}


bool Screen::decrease_size()
{
    int32_t old_size_id = m_size->size_id;
    for (const auto& size : available_screen_sizes)
    {
        if (size.size_id > old_size_id) { break; }
        m_size = &size;
    }
    if (m_size->size_id != old_size_id)
    {
        current_screen.size_changed();
        return true;
    }
    else
    {
        return false;
    }
}


bool Screen::increase_size()
{
    int32_t old_size_id = m_size->size_id;
    for (const auto& size : available_screen_sizes)
    {
        if (size.size_id > old_size_id)
        {
            m_size = &size;
            break;
        }
    }
    // TODO: if it's changed, need to call sync_screen_mode
    return m_size->size_id != old_size_id;
}


#define WINDOWED_PERCENTAGE_SIZE (50)


/*
 void set_full_screen_enabled(bool is_enabled)
 {
     if (is_enabled != screen_mode.fullscreen)
     {
         screen_mode.fullscreen = is_enabled;
         
         if (game_is_running())
         {
             change_screen_mode(&screen_mode, true);
         }
         else
         {
             change_screen_mode(&screen_mode, true);
             clear_screen();
         }
     }
 }
 */
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
        // need to call sync_screen_mode
    }
}


void Screen::toggle_fullscreen()
{
    set_fullscreen(!graphics_preferences->fullscreen);
}


//-----------------------------------------------------------------------------


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
        window_width = 640; // TODO: need to calculate window size
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
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, Get_OGL_ConfigureData().Multisamples > 0 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, Get_OGL_ConfigureData().Multisamples);
    SDL_GL_SetSwapInterval(Get_OGL_ConfigureData().WaitForVSync ? 1 : 0);
    
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
    
    set_screen_size(graphics_preferences->screen_size_id); // TODO: on April 1, use `old_school_screen_size.size_id` (make sure this is not saved in user's preferences file and the user can switch back in Preferences)
    
    standard_camera_settings.initialize();
    
    size_changed();
}

    
void Screen::size_changed()
{
    static screen_size_t prev_size = {INT32_MIN, "", 0, 0, 0, false, false, false};
    
    if (!(m_size->modern && prev_size.modern))
    {
        unload_all_collections(); // TODO: only call when necessary (I suspect the SW renderer needs reload some/all Shapes collections when changing bit depth; plus we probably want to switch between Classic and Modern Shapes automatically to keep the look authentic)
    }
    prev_size = *m_size;

    if (!graphics_preferences->fullscreen) // TODO: setting windowed size is TBD
    {
        //SDL_SetWindowSize(m_window, window_width, window_height);
        //SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    
    if (game_is_running())
    {
        OGL_StartRun();
        
        if (m_size->modern)
        {
            classic_renderer_buffer.clear();
        }
        else
        {
            classic_renderer_buffer.configure(m_size->w, m_size->h, m_size->bit_depth);
        }
    }
    
    clear_clipping_rect();
    
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    glViewport(0, 0, pixel_w, pixel_h);
    
    calculate_rects();
    
    // TODO: and this?
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    if (game_is_running() && graphics_preferences->hud_size > 0) { L_Call_HUDResize(); }
    
    clear_screen();
    
    recenter_mouse();
    fps_counter.reset();
}




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
    current_screen.get_window_pixel_size(w, h);
    return w / static_cast<float>(virtual_screen_rect().w);
}


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
    SDL_Rect vscreen = current_screen.virtual_screen_rect();
    
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
	switch (screen_mode.terminal_size)
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
    SDL_Rect vscreen = current_screen.virtual_screen_rect();

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





void Screen::bound_screen(bool in_game) // TODO: this POS to go away, once we finish disentangling screen.cpp
{
    SDL_Rect vscreen = virtual_screen_rect();
	SDL_Rect r = { 0, 0, in_game ? vscreen.w : 640, in_game ? vscreen.h : 480 };
	bound_screen_to_rect(r, in_game);
}


void Screen::bound_screen_to_rect(SDL_Rect &r, bool in_game)// TODO: in_game arg should go away
{
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    
    SDL_Rect vscreen = in_game ? virtual_screen_rect() : SDL_Rect({0, 0, 640, 480});
    
    float vscale = MIN(pixel_w / static_cast<float>(vscreen.w), pixel_h / static_cast<float>(vscreen.h));
    
    SDL_Rect viewport;
    viewport.x = static_cast<int>(pixel_w / 2.0f - (vscreen.w * vscale) / 2.0f + (r.x * vscale) + 0.5f);
    viewport.y = static_cast<int>(pixel_h / 2.0f - (vscreen.h * vscale) / 2.0f + (r.y * vscale) + 0.5f);
    viewport.w = static_cast<int>(r.w * vscale + 0.5f);
    viewport.h = static_cast<int>(r.h * vscale + 0.5f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(viewport.x, pixel_h - viewport.h - viewport.y, viewport.w, viewport.h);
    m_viewport_rect.x = viewport.x;
    m_viewport_rect.y = pixel_h - viewport.h - viewport.y;
    m_viewport_rect.w = viewport.w;
    m_viewport_rect.h = viewport.h;
    glOrtho(0, r.w, r.h, 0, -1.0, 1.0);
    m_ortho_rect.x = m_ortho_rect.y = 0;
    m_ortho_rect.w = r.w;
    m_ortho_rect.h = r.h;
}


void Screen::set_clipping_rect(SDL_Rect &r) // called by Canvas_OGL::apply_clip
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(m_viewport_rect.x + (r.x * m_viewport_rect.w / m_ortho_rect.w),
              m_viewport_rect.y + ((m_ortho_rect.h - r.y - r.h) * m_viewport_rect.h / m_ortho_rect.h),
              r.w * m_viewport_rect.w / m_ortho_rect.w,
              r.h * m_viewport_rect.h / m_ortho_rect.h);
}


void Screen::clear_clipping_rect()
{
    int pixel_w, pixel_h;
    get_window_pixel_size(pixel_w, pixel_h);
    glScissor(0, 0, pixel_w, pixel_h);
}


// converts the mouse position (SDL window coordinates) to a point on a virtual 640x480 display (aka main menu and Preferences)
// TODO: it would be better to convert button rects to window coordinates
void Screen::convert_window_coordinate_to_virtual_screen_point(int &x, int &y)
{
    int winw, winh;
    get_window_coordinates_size(winw, winh);
    
    SDL_Rect vscreen = virtual_screen_rect();
    
    float window_aspect = winw / static_cast<float>(winh);
    float virtual_aspect = vscreen.w / static_cast<float>(vscreen.h);
    
    if (window_aspect >= virtual_aspect)
    {
        float scale = winh / static_cast<float>(vscreen.h);
        x -= (winw - (vscreen.w * scale)) / 2;
        x /= scale;
        y /= scale;
    }
    else
    {
        float scale = winw / static_cast<float>(vscreen.w);
        y -= (winh - (vscreen.h * scale)) / 2;
        x /= scale;
        y /= scale;
    }
}




//static void change_screen_mode(int width, int height, int depth, bool nogl, bool force_menu, bool force_resize_hud = false);





// Set-up and tear-down for game rendering; called in enter_gameworld, exit_gameworld

void activate_gameworld_screen()
{
    //reset_messages(); // probably unnecessary here, but need to confirm (overlay messages may end up tying in with notify_user)
    
    reset_screen(); // LP change: reset screen so that extravision will not be persistent // EES: function is now in camera.cpp sort out its behavior and rename

    OGL_StartRun();
    
	
	// Set screen to selected size
    
	//change_screen_mode(_screentype_level);
    current_screen.size_changed();
    
    
	// Reset modifier key status
	SDL_SetModState(KMOD_NONE);
	
    int screen_w, screen_h;
    current_screen.get_window_coordinates_size(screen_w, screen_h);
    SDL_Rect vscreen = current_screen.virtual_screen_rect();
    
    current_screen.lua_clip_rect.x = 0;
    current_screen.lua_clip_rect.y = 0;
    current_screen.lua_clip_rect.w = screen_w;
    current_screen.lua_clip_rect.h = screen_h;
	
    current_screen.lua_view_rect.x = current_screen.lua_map_rect.x = (screen_w - vscreen.w) / 2;
    current_screen.lua_view_rect.y = current_screen.lua_map_rect.y = (screen_h - vscreen.h) / 2;
    current_screen.lua_view_rect.w = current_screen.lua_map_rect.w = vscreen.w;
    current_screen.lua_view_rect.h = current_screen.lua_map_rect.h = vscreen.h;
    
    // not sure; messages overlay?
    current_screen.lua_text_margins.top = 0;
    current_screen.lua_text_margins.left = 0;
    current_screen.lua_text_margins.bottom = 0;
    current_screen.lua_text_margins.right = 0;
	
    SDL_Rect term_rect = get_interface_rect(_terminal_screen_rect);
    current_screen.lua_term_rect.x = (screen_w - term_rect.w) / 2;
    current_screen.lua_term_rect.y = (screen_h - term_rect.h) / 2;
    current_screen.lua_term_rect.w = term_rect.w;
    current_screen.lua_term_rect.h = term_rect.h;

	L_Call_HUDResize();
}


void deactivate_gameworld_screen(void)
{
    reset_messages(); // flush the message overlays
	OGL_StopRun();
}





void render_game_to_screen(short ticks_elapsed)
{
	// Make whatever changes are necessary to the world_view structure based on whichever player is frontmost
	standard_camera_settings.ticks_elapsed = ticks_elapsed;
    standard_camera_settings.tick_count = dynamic_world.tick_count;
    
    // TODO: there are other modes, so why only this one here?
    standard_camera_settings.shading_mode = current_player->infravision_duration ? _shading_infravision : _shading_normal;

    standard_camera_settings.update(); // this is also called in enter_interpolated_world, so can't move the above lines into it
    
    auto heartbeat_fraction = get_heartbeat_fraction();
    standard_camera_settings.heartbeat_fraction = heartbeat_fraction;
	update_interpolated_world(heartbeat_fraction);
    
    interpolate_world_view(heartbeat_fraction);
    
    
//	screen_mode_data *mode = &screen_mode;

    
	//SDL_Rect HUD_DestRect = current_screen.hud_rect();
    SDL_Rect ViewRect = current_screen.worldview_rect();
    
    /*
	bool update_full_screen = false;
    
	if (ViewChangedSize || MapChangedSize || SwitchedModes) // ffs
    {
		clear_screen_margin();
        
		update_full_screen = true;
        
		//if (current_screen.hud() && !hud_is_visible() && !is_network_pregame) update_game_window();
        
		// Reallocate the drawing buffer
        // if (ViewChangedSize) initialize_classic_renderer();
        
		dirty_terminal_view(current_player_index);
	}
    */
    
    
    
    // Is map to be drawn with OpenGL? // answer: always
    //OGL_MapActive = (current_screen.uses_modern_renderer() && automap_is_visible());

    // TODO: FIX
	// Set OpenGL viewport to world view
	current_screen.bound_screen_to_rect(ViewRect);
	OGL_SetWindow(ViewRect);
    // Set OpenGL viewport to whole window (so HUD will be in the right position) // yuck
    current_screen.bound_screen();
    OGL_SetWindow(ViewRect);
    
    // TODO: setting these flags is TBD - obviously with multiplayer films the current_player changes as user switches player views
        
    if (!computer_terminal_is_visible() && (current_screen.uses_modern_renderer() || !automap_is_visible()))
    {
        render_worldview(&standard_camera_settings);
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
    
    /*
    // draw HUD[s] on top
     
    if (hud_is_visible())
    {
        Lua_DrawHUD(ticks_elapsed);
    }
    */

    // TODO: also draw screen overlay

    
    /*
    if (!is_vbl_reading_user_inputs()) // really means 'is vbl reading user inputs?', which it is unless app is backgrounded (what about paused/dead?)
    {
        darken_world_window(); // yeesh, I mean, just call this once and stop redrawing
    }
    */
    
    
    current_screen.swap();
    
    //FilmExporter::instance()->AddFrame(FilmExporter::FRAME_NORMAL); // TODO: I assume this is grabbing from the onscreen buffer
}




//*****************************************************************************
// TODO: these are taken out of render_game_to_screen so we can rebuild it one step at a time



// clear Lua drawing from previous frame
// (SDL is slower if we do this before render_view)
//if (!current_screen.uses_modern_renderer() && (MapIsTranslucent || hud_is_visible())) clear_screen_margin();

/*
if (game_is_networked() && is_network_pregame) // is_network_pregame needs to be an app state
{
    clear_screen(false); // TODO: this clears the backbuffer but does not swap it
    
    // TODO: this 'bound screen' stuff is both a bane and potentially useful; we just have to disentangle it to point where main menu, interstitial screens, and dialogs understand wtf is going on
    if (current_screen.uses_modern_renderer())
    {
        current_screen.bound_screen();
        OGL_SetWindow(sr);
        DisplayNetLoadingScreen(MainScreenSurface());
        current_screen.swap();
    }
    else
    {
        SDL_Rect rect = { (current_screen.window_rect().w - ViewRect.w) / 2, (current_screen.window_rect().h - ViewRect.h) / 2, 0, 0 };
        SDL_FillRect(world_pixels, NULL, SDL_MapRGB(world_pixels->format, 0, 0, 0));
        DisplayNetLoadingScreen(world_pixels);
        update_screen(rect, rect, true);
        sw_render_surface_to_screen();
    }
    return;
}
else
 */
 
/*
// Render crosshairs // HUD plugin only now
if (!automap_is_visible() && !computer_terminal_is_visible())
    if (NetAllowCrosshair() && Crosshairs_IsActive())
            if (!OGL_RenderCrosshairs()) Crosshairs_Render(world_pixels);
*/

// yes, idiocy
/*
 // TODO: these calls need to use Canvas so they can be decoupled and [mostly] moved to Lua HUD plugins
 
 //SDL_Surface *dst_surface = automap_is_visible() ? Map_Buffer : world_pixels;

 // Display FPS and position
if (!computer_terminal_is_visible())
{
    update_fps_display(dst_surface);
    DisplayPosition(dst_surface);
    DisplayScores(dst_surface);
}
DisplayMessages(dst_surface);
DisplayInputLine(dst_surface);
*/



//*****************************************************************************









/*
 *  Blit world view to screen
 */

template <class T>
static inline void quadruple_surface(const T *src, int src_pitch, T *dst, int dst_pitch, const SDL_Rect &dst_rect)
{
	int width = dst_rect.w / 2;
	int height = dst_rect.h / 2;
	dst += dst_rect.y * dst_pitch / sizeof(T) + dst_rect.x;
	T *dst2 = dst + dst_pitch / sizeof(T);

	//uint32 black_pixel = SDL_MapRGB(main_surface->format, 0, 0, 0);
	//bool overlay_active = automap_is_visible() && automap_is_translucent();
	
	while (height-- > 0) {
        for (int x=0; x<width; x++) {
            T p = src[x];
            dst[x * 2] = dst[x * 2 + 1] = p;
            dst2[x * 2] = dst2[x * 2 + 1] = p;
        }

		src += src_pitch / sizeof(T);
		dst += dst_pitch * 2 / sizeof(T);
		dst2 += dst_pitch * 2 / sizeof(T);
	}
}



static inline bool pixel_formats_equal(SDL_PixelFormat* a, SDL_PixelFormat* b)
{
	return (a->BytesPerPixel == b->BytesPerPixel && a->Rmask == b->Rmask && a->Gmask == b->Gmask && a->Bmask == b->Bmask);
}




/*
 *  Draw dithered black pattern over world window
 */

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
	SDL_Rect r = current_screen.window_rect();

	if (current_screen.uses_modern_renderer())
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
        glOrtho(0.0, GLdouble(current_screen.get_screen_size()->w), GLdouble(current_screen.get_screen_size()->h), 0.0, 0.0, 1.0); // TODO: check this
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

//		current_screen.swap();
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



/*
 *  Clear screen
 */

void clear_screen(bool swap)
{
    OGL_ClearScreen();
    
    // TODO: if Classic renderer is active, do we need to fill its Surface black too? or will it be filled automatically next time it's drawn?
	if (!current_screen.uses_modern_renderer())
    {
        classic_renderer_buffer.fill();
	}
    
    if (swap)
    {
        current_screen.swap();
        clear_screen(false);
    }
}


// screenshot



SDL_Surface* copy_screen_to_surface() // used in dump_screen below
{
    int video_w, video_h;
    current_screen.get_window_pixel_size(video_w, video_h);
    
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


