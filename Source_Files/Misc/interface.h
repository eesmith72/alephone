/*
INTERFACE.H -- a real mess

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

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "cseries.h"

#include "shapes.h"


// moved this enum here from screen_definitions.h; mostly (but not entirely) 2D UI resource IDs: main menu
// this should not be its permanent home, but converting old M2 hardcoded rect ids to modern extensible ids is TODO
//
// 'pict' resource ids for the 8 bit picts
// the 16 bit versions are these ids + 10000
// the 32 bit versions are these ids + 20000
enum {
    INTRO_SCREEN_BASE       = 1000, // splash screen[s] (included in Images.img2)
    MAIN_MENU_BASE          = 1100, // main menu screen (ditto)
    
    PROLOGUE_SCREEN_BASE    = 1200, // the remaining SCREEN ids are for 'pict' resources stored in Map.sce2
    EPILOGUE_SCREEN_BASE    = 1300,
    CREDIT_SCREEN_BASE      = 1400,
    CHAPTER_SCREEN_BASE     = 1500,
    //COMPUTER_INTERFACE_BASE = 1600,
    INTERFACE_PANEL_BASE    = 1700, // just to be awkward, the M2 SW HUD's background image was stored in Images.img2 as (iirc) 1700 + 2700 'pict' resources
    FINAL_SCREEN_BASE       = 1800,
};



enum /* animation types */
{
	_animated1= 1,
	_animated2to8= 2, /* ?? */
	_animated3to4= 3,
	_animated4= 4,
	_animated5to8= 5,
	_animated8= 8,
	_animated3to5= 9,
	_unanimated= 10,
	_animated5= 11
};

enum /* shading tables */
{
	_darkening_table
};


/* ---------- structures */

// TODO: these ought to be in shapes.h

enum /* shape types (this is for the editor) */
{
    _wall_shape, /* things designated as walls */
    _floor_or_ceiling_shape, /* walls in raw format */
    _object_shape, /* things designated as objects */
    _other_shape /* anything not falling into the above categories (guns, interface elements, etc) */
};


#define _X_MIRRORED_BIT 0x8000
#define _Y_MIRRORED_BIT 0x4000
#define _KEYPOINT_OBSCURED_BIT 0x2000


struct shape_information_data
{
	uint16 flags; /* [x-mirror.1] [y-mirror.1] [keypoint_obscured.1] [unused.13] */

	_fixed minimum_light_intensity; /* in [0,FIXED_ONE] */

	short unused[5];

	short world_left, world_right, world_top, world_bottom;
	short world_x0, world_y0;
};


struct shape_animation_data // Also used in high_level_shape_definition
{
	int16 number_of_views; /* must be 1, 2, 5 or 8 */
	
	int16 frames_per_view, ticks_per_frame;
	int16 key_frame;
	
	int16 transfer_mode;
	int16 transfer_mode_period; /* in ticks */
	
	int16 first_frame_sound, key_frame_sound, last_frame_sound;

	int16 pixels_to_world;
	
	int16 loop_frame;

	int16 unused[14];

	/* N*frames_per_view indexes of low-level shapes follow, where
	   N = 1 if number_of_views = _unanimated/_animated1,
	   N = 4 if number_of_views = _animated3to4/_animated4,
	   N = 5 if number_of_views = _animated3to5/_animated5,
	   N = 8 if number_of_views = _animated2to8/_animated5to8/_animated8 */
	int16 low_level_shape_indexes[1];
};


enum { /* controllers */
	_single_player,
	_network_player,
	_demo,
	_replay,
	_replay_from_file,
	NUMBER_OF_PSEUDO_PLAYERS
};

enum { /* states. */
	_display_intro_screens,
	_display_main_menu,
	_display_chapter_heading,
	_display_prologue,
	_display_epilogue,
	_display_credits,
	_display_intro_screens_for_demo,
	_display_quit_screens,
	NUMBER_OF_SCREENS,
	_game_in_progress= NUMBER_OF_SCREENS,
	_quit_game,
	_close_game,
	_switch_demo,
	_revert_game,
	_change_level,
	_begin_display_of_epilogue,
	_displaying_network_game_dialogs,
	NUMBER_OF_GAME_STATES
};

void set_game_state(short new_state);
short get_game_state(void);

void set_change_level_destination(short level_number);
short get_change_level_destination();
ao_err transfer_to_new_level(short level_number);


bool game_window_is_full_screen(void);

void initialize_game_state(void);
void force_game_state_change(void);
bool player_controlling_game(void);

void toggle_suppression_of_background_tasks(void);
bool suppress_background_events(void);

short get_game_controller(void);

void display_loading_map_error(ao_err err);


void pause_game(void);
void resume_game(void);
void portable_process_screen_click(short x, short y, bool cheatkeys_down);
void process_main_menu_highlight_advance(bool reverse);
void process_main_menu_highlight_select(bool cheatkeys_down);
void draw_menu_button_for_command(short index);
void update_interface_display(void);
bool idle_game_state(uint64_t time);
void display_main_menu(void);
void do_menu_item_command(short menu_id, short menu_item, bool cheat);
bool interface_fade_finished(void);
void stop_interface_fade(void);
bool enabled_item(short item);
void paint_window_black(void);

void set_game_focus_lost();
void set_game_focus_gained();

/* ---------- prototypes/INTERFACE_MACINTOSH.C */
void do_preferences(void);
void toggle_menus(bool game_started);


void show_movie(short index);

void load_main_menu_buffers(short base_id);
bool main_menu_buffers_loaded(void);
void main_menu_bit_depth_changed(short base_id);
void free_main_menu_buffers(void);
void draw_main_menu(void);
void draw_menu_button(short index, bool pressed);

/* ---------- prototypes/INTERFACE_MACINTOSH.C- couldn't think of a better place... */
void hide_cursor(void);
void show_cursor(void);
void set_clipping_rectangle(short top, short left, short bottom, short right);

/* ---------- prototypes/SHAPES.C */


// LP additions:
// Whether or not collection is present
bool is_collection_present(short collection_index);
// Number of texture frames in a collection (good for wall-texture error checking)
short get_number_of_collection_frames(short collection_index);
// Number of bitmaps in a collection (good for allocating texture information for OpenGL)
short get_number_of_collection_bitmaps(short collection_index);
// Which bitmap index for a frame (good for OpenGL texture rendering)
short get_bitmap_index(short collection_index, short low_level_shape_index);
// Get CLUT for collection
struct rgb_color_value *get_collection_colors(short collection_index, short clut_number, int &num_colors);

// ZZZ: made these visible
struct low_level_shape_definition *get_low_level_shape_definition(short collection_index, short low_level_shape_index);


/* ---------- prototypes/PREPROCESS_MAP_MAC.C */
void setup_revert_game_info(struct game_data *game_info, struct player_start_data *start, struct entry_point *entry);
ao_err revert_game(void);
bool load_game(bool use_last_load);
void restart_game(void);

/* ---------- prototypes/GAME_WAD.C */
/* Map transferring fuctions */

void process_net_map_data(uint8_t* flat_data); // Note that this frees it as well

ao_err get_map_for_net_transfer(entry_point* entry, uint8_t*& flat_data);

/* ---------- prototypes/VBL.C */

void set_keyboard_controller_status(bool active);
bool get_keyboard_controller_status(void);
void pause_keyboard_controller(bool active);
int32 get_heartbeat_count(void);
float get_heartbeat_fraction(void);
void wait_until_next_frame(void);
void sync_heartbeat_count(void);
void process_action_flags(short player_identifier, const uint32 *action_flags, short count);
ao_err reset_recording(void);
ao_err stop_recording(void);
void stop_replay(void);

void check_recording_replaying(void);
bool has_recording_file(void);
void increment_replay_speed(void);
void decrement_replay_speed(void);
void set_replay_speed(short);
void reset_recording_and_playback_queues(void);
uint32 parse_keymap(void);

/* ---------- prototypes/GAME_DIALOGS.C */

bool handle_preferences_dialog(void);
void handle_load_game(void);
void handle_quicksave_game(void);
bool handle_start_game(void);
bool quit_without_saving(void);

/* ---------- prototypes/GAME_WINDOW.C */
void scroll_inventory(short dy);

/* ---------- prototypes/NETWORK.C */

enum network_join_result_t {	// Results for network_join
    kNetworkJoinFailedUnjoined,
    kNetworkJoinFailedJoined,
    kNetworkJoinedNewGame,
    kNetworkJoinedResumeGame,
};

ao_err network_gather(bool inResumingGame, bool& outUseRemoteHub);

network_join_result_t network_join();

/* ---------- prototypes/PHYSICS.C */

void reset_absolute_positioning_device(_fixed yaw, _fixed pitch, _fixed velocity);

/* ---------- prototypes/IMPORT_DEFINITIONS.C */


/* ---------- prototypes/KEYBOARD_DIALOG.C */
bool configure_key_setup(short *keycodes);

/* --------- from PREPROCESS_MAP_MAC.C */



ao_err load_and_start_game(const ao_path& File);

ao_err handle_open_replay(const ao_path& File);

ao_err handle_edit_map();



// LP change: resets field of view to whatever the player had had when reviving
void ResetFieldOfView();

// LP change: modification of Josh Elsasser's dont-switch-weapons patch
bool dont_switch_to_new_weapon();

bool dont_auto_recenter();

// ZZZ: let code disable (standardize)/enable behavior modifiers like
// dont_switch
void standardize_player_behavior_modifiers();
void restore_custom_player_behavior_modifiers();

// ZZZ: return whether the user's behavior matches standard behavior (either by being forced so or by chosen that way)
bool is_player_behavior_standard();

void ReloadViewContext();

class InfoTree;
void parse_mml_infravision(const InfoTree& root); // in fucking shapes.cpp
void reset_mml_infravision();
void parse_mml_control_panels(const InfoTree& root); // in fucking devices.cpp
void reset_mml_control_panels();


// these are now in this file, controlling button order (aggravatingly separate to button rects, but sorting that's for later)
void reset_mml_menu_item_order();

void parse_mml_menu_item_order(const InfoTree& root); // <interface>



#endif
