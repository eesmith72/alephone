/*
 vbl.h -- process gameworld input events from [solo game] user or a film recording
 
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

#ifndef __vbl_h__
#define __vbl_h__

#include "cseries.h"

#include "DataFile.hpp"
#include "ActionQueues.h" // ActionQueue

#include "player.h" // player_identity_t
#include "map.h" // ame_data


// TODO: film recorder should write an additional file containing timestamp+player/camera ID data of all the exciting gameplay moments (frenetic player movements, kills and deaths); a very watchable MPEG movie could probably be auto-edited together by a Lua script or maybe even AI, good for slinging on YouTube


struct recording_header
{
    int32 length;
    int16 level_number;
    uint32 map_checksum;
    int16 version;
    std::vector<player_identity_t> starts; // will be packed fixed size array
    game_configuration_t game_information;
};
const int SIZEOF_recording_header = 352;

enum class recording_extension_type
{
    none = 0,
    saved_game_wad = 1
};

struct recording_extension_header
{
    recording_extension_type extension_type;
    uint32 length;
};
const int SIZEOF_recording_extension_header = 8;


// these prototypes were previously in interface.h
void set_keyboard_controller_status(bool active); // TODO: rename set_action_inputs_enabled
bool is_vbl_reading_user_inputs();


int32_t get_heartbeat_count();
float get_heartbeat_fraction();
void wait_until_next_frame();
void sync_heartbeat_count();
void process_action_flags(short player_identifier, const uint32_t* action_flags, short count);
ao_err reset_recording();
ao_err stop_recording();
void stop_replay();

void check_recording_replaying();

ao_path get_recording_path();

inline bool has_recording_file()
{
    ao_path path = get_recording_path();
    return std::filesystem::is_regular_file(path);
}


void increment_replay_speed();
void decrement_replay_speed();
void set_replay_speed(short);
bool is_saved_game_replay();


void reset_recording_and_playback_queues();


uint32_t parse_keymap();



ao_err setup_for_replay_from_file(const ao_path& film_file);



// TODO: decomposing film recording should make it easier to automatically record all games automatically (including solo), as collecting and zipping saved game, config, and recorded actions only needs done when exporting a film file for sharing (which, btw, really requires comprehensive config info - the same info that's needed for netgames to support customizations, so best to break compatibility for all at once); for solo gameplay, the Quicksave class can be modified to use subdirectories (save+config+actions); for netgames, just config+actions


ao_err start_recording(); // TODO: while this isn't expected to fail, callers should check and handle any FS errors returned

void set_recording_saved_wad_data(uint8_t* saved_wad_data); // this takes ownership of the wad data and will free it when no longer needed

// TODO: this is kinda dumb; why populate a struct instead of getting/setting directly when reading/writing? (other than obvious answer being LP is a plonker)
void set_recording_header_data(short level_number, uint32 map_checksum, short version,
                               const std::vector<player_identity_t>& player_identities, const game_configuration_t& game_information);

void get_recording_header_data(short& level_number, uint32& map_checksum, short& version,
                               std::vector<player_identity_t>& player_identities, game_configuration_t& game_information);



void initialize_keyboard_controller();

bool input_controller(void);

void increment_heartbeat_count(int value = 1);

void execute_timer_tasks(uint64_t time);

uint32 parse_keymap();


#ifdef DEBUG_REPLAY
struct recorded_flag
{
	uint32 flag;
	int16 player_index;
};

void open_stream_file();
void write_flags(struct recorded_flag *buffer, int32 count);
static void debug_stream_of_flags(uint32 action_flag, short player_index);
static void close_stream_file();

#endif /* DEBUG_REPLAY */

#endif /* __vbl_h__ */
