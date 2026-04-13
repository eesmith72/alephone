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


struct screen_mode_data;


SDL_Renderer* get_sw_renderer(); // need this to create Texture

void sw_render_texture_to_screen(SDL_Texture* texture, const SDL_Rect* dst_rect, const SDL_Rect* src_rect);


extern bool screen_needs_swapped;

#define request_swap() { \
    screen_needs_swapped = true; \
    printf("request screen swap\n"); \
}


// TODO: include `&& !screen_is_faded_black()` test?

#define swap_screen_if_requested() { \
if (screen_needs_swapped) \
    { \
        MainScreenSwap(); \
        screen_needs_swapped = false; \
    } \
}



extern SDL_Surface *world_pixels; // gameworld view; the original M2 SW renderer draws into this now, presumably seeing it no differently to a 1995-era Mac screen buffer



namespace alephone
{
    // EES: absolute nonsense on stilts; TODO: what do we actually need? Screen_SDL and Screen_OGL, each managing its own Renderer, presumably with SDL_Window managed either in base Screen class or just plain old static functions
	class Screen
	{
	public:
		static inline Screen* instance() {
			return &m_instance;
		}

		void Initialize(screen_mode_data* mode);
		
        const std::vector<std::pair<int, int> >& GetModes() { return m_modes; };
		
        int FindMode(int width, int height) {
			for (int i = 0; i < m_modes.size(); ++i)
			{
				if (m_modes[i].first == width &&
				    m_modes[i].second == height)
				{
					return i;
				}
			}
			return -1;
		}
        
        // TODO: isn't this same as GameResolutionHeight/Width? it is very confusing
		int ModeHeight(int mode) { return m_modes[mode].second; }
		int ModeWidth(int mode) { return m_modes[mode].first; }

		bool hud();
		bool lua_hud();
		bool openGL();
		bool fifty_percent();
		bool seventyfive_percent();
		SDL_Rect window_rect(); // 3D view + interface
		SDL_Rect view_rect(); // main 3D view
		SDL_Rect map_rect();
		SDL_Rect term_rect();
		SDL_Rect hud_rect();
		SDL_Rect OpenGLViewPort();

		void bound_screen(bool in_game = true);
		void bound_screen_to_rect(SDL_Rect &r, bool in_game = true);
		void scissor_screen_to_rect(SDL_Rect &r);
		void window_to_screen(int &x, int &y);
		
		SDL_Rect lua_clip_rect;
		SDL_Rect lua_view_rect;
		SDL_Rect lua_map_rect;
		SDL_Rect lua_term_rect;

		// TODO: the HUD should really draw messages / fps / input line itself
		screen_rectangle lua_text_margins;

	private:
		Screen() : m_initialized(false) { }
		static Screen m_instance;
		bool m_initialized;
		SDL_Rect m_viewport_rect;
		SDL_Rect m_ortho_rect;

		std::vector<std::pair<int, int> > m_modes;
	};
}

extern SDL_PixelFormat pixel_format_16;
extern SDL_PixelFormat pixel_format_32;

/* ---------- constants */

// Original screen-size definitions
enum /* screen sizes */
{
	_50_percent,
	_75_percent,
	_100_percent,
	_full_screen,
};


enum /* screen selection based on game state */
{
	_screentype_level,
	_screentype_menu,
	_screentype_chapter
};


/* ---------- globals */

extern struct color_table *world_color_table, *visible_color_table, *interface_color_table;

/* ---------- prototypes/SCREEN.C */

void initialize_gamma(void);

void change_screen_clut(struct color_table *color_table);
void change_interface_clut(struct color_table *color_table);
void animate_screen_clut(struct color_table *color_table, bool full_screen);

void build_direct_color_table(struct color_table *color_table, short bit_depth);


void render_game_to_screen(short ticks_elapsed);

void toggle_overhead_map_display_status(void);

// Returns whether the size scale had been changed
bool zoom_overhead_map_out(void);
bool zoom_overhead_map_in(void);

bool map_is_translucent(void);

void enter_screen(void);
void exit_screen(void);

void validate_world_window(void);


color_table* calculate_picture_clut();
color_table* build_8bit_system_color_table();

void change_gamma_level(short gamma_level);

void assert_world_color_table(struct color_table *world_color_table, struct color_table *interface_color_table);


// LP change: added function for resetting the screen state when starting a game
void reset_screen();

// CP addition: added function to return the the game size
screen_mode_data *get_screen_mode(void);

void change_screen_mode(struct screen_mode_data *mode, bool redraw, bool resize_hud = false);
void change_screen_mode(short screentype);

void set_full_screen_enabled(bool fs);
void toggle_fullscreen();

void clear_screen(bool swap = true);



void ReloadViewContext();


bool ogl_is_active();

void MainScreenSwap();

void MainScreenCenterMouse();


SDL_Window* MainScreenWindow(); // returns borrowed pointer

SDL_Surface* MainScreenSurface(); // returns borrowed pointer

SDL_Surface* get_main_screen_surface_OGL(); // used by dump_screen; unlike MainScreenSurface which returns a borrowed pointer, the caller owns this one is responsible for disposing it when done


// if surface is not given, uses main_surface (which should go away shortly)
void sw_render_surface_to_screen(SDL_Surface* surface = nullptr, const SDL_Rect* dst_rect = nullptr);


// screen size, ignoring hi-res (e.g. 4K monitor returns 1920,1080px)
void MainScreenWindowSize(int32_t& w, int32_t& h);

// the true screen size, accounting for hi-res (e.g. 4K monitor returns 3860,2140px)
void MainScreenPixelSize(int32_t& w, int32_t& h);

// the user's in-game resolution setting
int GameResolutionWidth();
int GameResolutionHeight();

// the size of the SDL_Surface used to draw full-screen images
void MainScreenSurfaceSize(int* w, int* h);

// scale factor between screen's true resolution and the game's effective resolution
float MainScreenPixelScale();




void dump_screen();


#endif
