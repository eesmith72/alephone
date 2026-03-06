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


struct Rect;

struct screen_mode_data;
namespace alephone
{
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
		Rect lua_text_margins;

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

enum /* hardware acceleration codes */
{
	_no_acceleration,
	_opengl_acceleration
};

enum /* screen selection based on game state */
{
	_screentype_level,
	_screentype_menu,
	_screentype_chapter
};

/* ---------- missing from QUICKDRAW.H */

#define deviceIsGrayscale 0x0000
#define deviceIsColor 0x0001

/* ---------- structures */

/* ---------- globals */

extern struct color_table *world_color_table, *visible_color_table, *interface_color_table;

/* ---------- prototypes/SCREEN.C */

void initialize_gamma(void);

void change_screen_clut(struct color_table *color_table);
void change_interface_clut(struct color_table *color_table);
void animate_screen_clut(struct color_table *color_table, bool full_screen);

void build_direct_color_table(struct color_table *color_table, short bit_depth);

void start_teleporting_effect(bool out);
void start_extravision_effect(bool out);

void render_screen(short ticks_elapsed);

void toggle_overhead_map_display_status(void);

// Returns whether the size scale had been changed
bool zoom_overhead_map_out(void);
bool zoom_overhead_map_in(void);

bool map_is_translucent(void);

void enter_screen(void);
void exit_screen(void);

void validate_world_window(void);

void change_gamma_level(short gamma_level);

void assert_world_color_table(struct color_table *world_color_table, struct color_table *interface_color_table);

// LP change: added function for resetting the screen state when starting a game
void reset_screen();

// CP addition: added function to return the the game size
screen_mode_data *get_screen_mode(void);

// LP: when initing, ask whether to show the monitor-frequency dialog
//void initialize_screen(struct screen_mode_data *mode, bool ShowFreqDialog);
void change_screen_mode(struct screen_mode_data *mode, bool redraw, bool resize_hud = false);
void change_screen_mode(short screentype);

void toggle_fullscreen(bool fs);
void toggle_fullscreen();
void update_screen_window(void);
void clear_screen(bool update = true);

void calculate_destination_frame(short size, bool high_resolution, Rect *frame);

// For getting and setting tunnel-vision mode
bool GetTunnelVision();
bool SetTunnelVision(bool TunnelVisionOn);

// Request for drawing the HUD
void RequestDrawingHUD();
// Request for drawing the terminal
void RequestDrawingTerm();
// Request for drawing (or redrawing) a menu or intro screen
void draw_intro_screen();

// Corresponding with-and-without-HUD sizes for some view-size index,
// for the convenience of Pfhortran scripting;
// the purpose is to get a similar size of display with the HUD status possibly changed
short SizeWithHUD(short _size);
short SizeWithoutHUD(short _size);

// Displays a message on the screen for a second or so; may be good for debugging
void ShowMessage(char *Text);

/* SB: Custom Blizzard-style overlays */
#define MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS 6
bool IsScriptHUDNonlocal();
void SetScriptHUDNonlocal(bool nonlocal = true);
/* color is a terminal color */
void SetScriptHUDColor(int player, int idx, int color);
/* text == NULL or "" removes that HUD element
   to turn HUD elements off, set all elements NULL or "" */
void SetScriptHUDText(int player, int idx, const char* text);
/* icon == NULL turns the icon off
   someday I'll document the format */
bool SetScriptHUDIcon(int player, int idx, const char* icon, size_t length);
/* sets the icon for that HUD to a colored square (same colors as SetScriptHUDColor) */
void SetScriptHUDSquare(int player, int idx, int color);


bool MainScreenVisible();

bool MainScreenIsOpenGL();

void MainScreenSwap();

void MainScreenCenterMouse();


SDL_Window* MainScreenWindow(); // returns borrowed pointer

SDL_Surface* MainScreenSurface(); // returns borrowed pointer

SDL_Surface* get_main_screen_surface_OGL(); // used by dump_screen; unlike MainScreenSurface which returns a borrowed pointer, the caller owns this one is responsible for disposing it when done


void MainScreenUpdateRect(int x, int y, int w, int h);
void MainScreenUpdateRects(size_t count, const SDL_Rect *rects);


// the true screen size
void MainScreenWindowSize(int* w, int* h);

// the user's in-game resolution setting
int GameResolutionWidth();
int GameResolutionHeight();

// the size of the SDL_Surface used to draw full-screen images
void MainScreenSurfaceSize(int* w, int* h);

void MainScreenPixelSize(int32_t* w, int32_t* h);

// scale factor between screen's true resolution and the game's effective resolution
float MainScreenPixelScale();




void dump_screen();


#endif
