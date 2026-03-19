/*
	vbl.h -- process gameworld input events from the user or a film recording

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



// these prototypes were previously in interface.h
void set_keyboard_controller_status(bool active);
bool get_keyboard_controller_status();
void pause_keyboard_controller(bool active);
int32_t get_heartbeat_count();
float get_heartbeat_fraction();
void wait_until_next_frame();
void sync_heartbeat_count();
void process_action_flags(short player_identifier, const uint32_t* action_flags, short count);
ao_err reset_recording();
ao_err stop_recording();
void stop_replay();

void check_recording_replaying();
bool has_recording_file();
void increment_replay_speed();
void decrement_replay_speed();
void set_replay_speed(short);
bool is_saved_game_replay();


void reset_recording_and_playback_queues();
uint32_t parse_keymap();



ao_path get_recording_path();


ao_err setup_replay_from_random_resource();

ao_err setup_for_replay_from_file(const ao_path& film_file, uint32 map_checksum, bool prompt_to_export = false);



ao_err start_recording(); // TODO: while this isn't expected to fail, callers should check and handle any FS errors returned

void set_recording_saved_wad_data(const std::vector<byte>& saved_wad_data);

void set_recording_header_data(short number_of_players, short level_number, uint32 map_checksum,
                               short version, struct player_start_data *starts, struct game_data *game_information);

void get_recording_header_data(short *number_of_players, short *level_number, uint32 *map_checksum,
                               short *version, struct player_start_data *starts, struct game_data *game_information);



void initialize_keyboard_controller();

bool input_controller(void);

void increment_heartbeat_count(int value = 1);


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
