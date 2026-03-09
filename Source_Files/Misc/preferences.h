/*
 preferences.h
 
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

#ifndef __preferences_h__
#define __preferences_h__

#include "cseries.h"

#include "DataFile.hpp"

#include "interface.h"
#include "ChaseCam.h"
#include "Crosshairs.h"
#include "OGL_Setup.h"
#include "shell.h"
#include "SoundManager.h"

#include "wad.h" // read_wad_file_checksum


/* New preferences junk */
const float DEFAULT_MONITOR_REFRESH_FREQUENCY = 60;	// 60 Hz

enum {
	_sw_alpha_off,
	_sw_alpha_fast,
	_sw_alpha_nice,
};
enum {
	_sw_driver_default,
	_sw_driver_none,
	_sw_driver_direct3d,
	_sw_driver_opengl,
};

enum {
	_ephemera_off,
	_ephemera_low,
	_ephemera_medium,
	_ephemera_high,
	_ephemera_ultra
};

struct graphics_preferences_data
{
	struct screen_mode_data screen_mode;
	// LP change: added OpenGL support
	OGL_ConfigureData OGL_Configure;

	int16 software_alpha_blending;
	int16 software_sdl_driver;
	int16 fps_target; // should be a multiple of 30; 0 = unlimited

	int16 movie_export_video_quality;
	int32 movie_export_video_bitrate; // 0 is automatic
    int16 movie_export_audio_quality;

	int16 ephemera_quality;
};

enum {
	_network_game_protocol_star,
	NUMBER_OF_NETWORK_GAME_PROTOCOLS,

	_network_game_protocol_default = _network_game_protocol_star
};

struct network_preferences_data
{
	bool game_is_untimed;
	int16 type; // look in network_dialogs.c for _ethernet, etc...
	int16 game_type;
	int16 difficulty_level;
	uint16 game_options; // Penalize suicide, etc... see map.h for constants
	int32 time_limit;
	int16 kill_limit;
	int16 entry_point;
	bool autogather;
	bool join_by_address;
	std::string join_address;
	uint16 game_port;	// TCP and UDP port number used for game traffic (not player-location traffic)
	uint16 game_protocol; // _network_game_protocol_star, etc.
	bool use_netscript;
	ao_path netscript_file;
	uint16 cheat_flags;
	bool advertise_on_metaserver;
	bool attempt_upnp;
	bool use_remote_hub;
	bool check_for_updates;
	bool verify_https;

	enum {
		kMetaserverLoginLength = 16
	};

	std::string metaserver_login;
	std::string metaserver_password;
	bool use_custom_metaserver_colors;
	rgb_color metaserver_colors[2];
	bool mute_metaserver_guests;
	bool join_metaserver_by_default;
	bool allow_stats;
};

enum SoloProfileType {
	_solo_profile_aleph_one,
	_solo_profile_unused,		// hope springs eternal
	_solo_profile_marathon_2,
	_solo_profile_marathon_infinity,
	NUMBER_OF_SOLO_PROFILE_TYPES
};

struct player_preferences_data
{
	std::string name;
	int16 color;
	int16 team;
	uint32 last_time_ran;
	int16 difficulty_level;
	bool background_music_on;
	bool crosshairs_active;
	struct ChaseCamData ChaseCam;
	struct CrosshairData Crosshairs;

	int solo_profile;
};

// LP addition: input-modifier flags
// run/walk and swim/sink
// LP addition: Josh Elsasser's dont-switch-weapons patch
enum {
	_inputmod_interchange_run_walk = 0x0001,
	_inputmod_interchange_swim_sink = 0x0002,
	_inputmod_dont_switch_to_new_weapon = 0x0004,
	_inputmod_invert_mouse = 0x0008,
	_inputmod_use_button_sounds = 0x0010,
	_inputmod_dont_auto_recenter = 0x0020,   // ZZZ addition
	_inputmod_run_key_toggle = 0x0040,
};

// shell keys
enum {
	_key_inventory_left,
	_key_inventory_right,
	_key_switch_view,
	_key_volume_up,
	_key_volume_down,
	_key_zoom_in,
	_key_zoom_out,
	_key_toggle_fps,
	_key_activate_console,
	_key_show_scores,
	NUMBER_OF_SHELL_KEYS
};

enum {
	_mouse_accel_none,
	_mouse_accel_classic,
	_mouse_accel_symmetric,
	NUMBER_OF_MOUSE_ACCEL_TYPES
};

static constexpr int NUMBER_OF_HOTKEYS = 12;

typedef std::map<int, std::set<SDL_Scancode> > key_binding_map;

struct input_preferences_data
{
	int16 input_device;
	// LP addition: input modifiers
	uint16 modifiers;
	// Mouse-sensitivity parameters (LP: originally ZZZ)
	_fixed sens_horizontal;
	_fixed sens_vertical;
	int16 mouse_accel_type;
	float mouse_accel_scale;
	bool raw_mouse_input;
	bool extra_mouse_precision;
	bool classic_vertical_aim;
	
	// Limit absolute-mode {yaw, pitch} deltas per tick to +/- {32, 8} instead of {63, 15}
	bool classic_aim_speed_limits;
	
	bool controller_analog;
	bool controller_aim_inverted;
	_fixed controller_sensitivity_horizontal;
	_fixed controller_sensitivity_vertical;
	// if an axis reading is taken below this number in absolute
	// value, then we clip it to 0.  this lets people use
	// inaccurate zero points.
	int16 controller_deadzone_horizontal;
	int16 controller_deadzone_vertical;

	key_binding_map key_bindings;
	key_binding_map shell_key_bindings;
	key_binding_map hotkey_bindings;
};


#define MAXIMUM_PATCHES_PER_ENVIRONMENT (32)

struct environment_preferences_data
{
    void reset();
    
    ao_path map_file;
    uint32_t map_checksum; // checksums/modification dates for identity comparisons
    
    void set_map_file(const ao_path& path)
    {
        map_file = path;
        map_checksum = read_wad_file_checksum(map_file);
    }
    
    
    ao_path physics_file;
    uint32_t physics_checksum;
    
    void set_physics_file(const ao_path& path)
    {
        physics_file = path;
        physics_checksum = read_wad_file_checksum(physics_file);
    }
    
    
    ao_path shapes_file;
    std::filesystem::file_time_type shapes_mod_date;
    
    void set_shapes_file(const ao_path& path)
    {
        shapes_file = path;
        // Shapes and Sounds don't have checksums, so use modification date for identity checks; TODO: this is not ideal
        shapes_mod_date = std::filesystem::is_regular_file(shapes_file) ? std::filesystem::last_write_time(shapes_file)
                                                                        : std::filesystem::file_time_type::min();
    }
    
    
    ao_path sounds_file;
    std::filesystem::file_time_type sounds_mod_date;
    
    void set_sounds_file(const ao_path& path)
    {
        sounds_file = path;
        sounds_mod_date = std::filesystem::is_regular_file(sounds_file) ? std::filesystem::last_write_time(sounds_file)
                                                                        : std::filesystem::file_time_type::min();
    }
    
    
    ao_path resources_file; // the Marathon 1 App's extracted resource fork // TODO: what about M2 Images file?

    void set_resources_file(const ao_path& path)
    {
        resources_file = path;
    }
    
    // TODO: API for lua file[s]
    ao_path solo_lua_file;
    bool use_solo_lua;
    bool use_replay_net_lua;
    bool hide_extensions;
    
	uint32_t patches[MAXIMUM_PATCHES_PER_ENVIRONMENT];
    
	// ZZZ: these aren't really environment preferences, but preferences that affect the environment preferences dialog
	bool group_by_directory;	// if not, display popup as one giant flat list
	bool reduce_singletons;		// make groups of a single element part of a larger parent group

	// ghs: are themes part of the environment? they are now
	bool smooth_text;

	FilmProfileType film_profile; // for legacy films

	// how many auto-named save files to keep around (0 is unlimited)
	uint32 maximum_quick_saves;

#ifdef HAVE_NFD
	bool use_native_file_dialogs;
#endif

	bool auto_play_demos;
};


extern struct graphics_preferences_data *graphics_preferences;

extern struct network_preferences_data *network_preferences;

extern struct player_preferences_data *player_preferences;

extern struct input_preferences_data *input_preferences;

//extern struct sound_manager_parameters *sound_preferences;
extern SoundManager::Parameters *sound_preferences;

extern environment_preferences_data environment_preferences;




void initialize_preferences();

void read_preferences();

void write_preferences();

void show_main_preferences_dialog(); // TODO: dialogs belong in their own file


static inline int16 get_fps_target() {
	return graphics_preferences->fps_target;
}

// void transition_preferences(const ao_path& legacy_prefs_dir); // let's assume everyone's transitioned by now and discard this

#endif /* __preferences_h__ */
