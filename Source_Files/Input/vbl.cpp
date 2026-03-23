/*
 vbl.cpp
 
 Tuesday, November 17, 1992 3:53:29 PM
 the new task of the vbl controller is only to move the player.  this is necessary for
 good control of the game.  everything else (doors, monsters, projectiles, etc) will
 be moved immediately before the next frame is drawn, based on delta-time values.
 collisions (including the player with walls) will also be handled at this time.
 
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

#include "vbl.h"

#include "map.h"
#include "map_wad.h" // set_current_map_path_to_file_with_checksum
#include "interface.h"
#include "shell.h"
#include "preferences.h"
#include "mouse.h"
#include "player.h"
#include "key_definitions.h"
#include "tags.h"
#include "DataFile.hpp"
#include "Packing.h"
#include "ActionQueues.h"
#include "computer_interface.h"
#include "Console.h"
#include "joystick.h"
#include "MovieExporter.h"
#include "InfoTree.h"

#include "vbl_definitions.h"

/* ---------- constants */

#define RECORD_CHUNK_SIZE            (MAXIMUM_QUEUE_SIZE/2)
#define END_OF_RECORDING_INDICATOR  (RECORD_CHUNK_SIZE+1)
#define MAXIMUM_TIME_DIFFERENCE     15 // allowed between heartbeat_count and dynamic_world->tick_count
#define MAXIMUM_NET_QUEUE_SIZE       8
#define DISK_CACHE_SIZE             ((sizeof(int16)+sizeof(uint32))*100)
#define MAXIMUM_REPLAY_SPEED         5
#define MINIMUM_REPLAY_SPEED        -5



#define INCREMENT_QUEUE_COUNTER(c) { (c)++; if ((c)>=MAXIMUM_QUEUE_SIZE) (c) = 0; }



static int32 heartbeat_count;
static bool input_task_active;
static timer_task_proc input_task;


static DataFile current_film_file; // both recording and playback, which gets a bit confusing in the implementations below (but since there is only one open just gonna leave it for now)


struct replay_private_data replay;

#ifdef DEBUG
ActionQueue *get_player_recording_queue(short player_index)
{
	assert_fail(replay.recording_queues, "was null");
	assert_fail(player_index>=0 && player_index<MAXIMUM_NUMBER_OF_PLAYERS, "out of range");
	
	return (replay.recording_queues+player_index);
}
#endif

/* ---------- private prototypes */

static void remove_input_controller(void);

static void save_recording_queue_chunk(short player_index);
static void read_recording_queue_chunks(void);
static short pull_flags_from_recording(short count);

// LP modifications for object-oriented file handling; returns a test for end-of-file
static bool vblFSRead(DataFile& File, int32 *count, void *dest, bool& HitEOF);

static void record_action_flags(short player_identifier, const uint32 *action_flags, short count);
static short get_recording_queue_size(short which_queue);

static uint8 *unpack_recording_header(uint8 *Stream, recording_header *Objects, size_t Count);
static uint8 *pack_recording_header(uint8 *Stream, recording_header *Objects, size_t Count);
static uint8* unpack_recording_extension_header(uint8* Stream, recording_extension_header* Objects, size_t Count);
static uint8* pack_recording_extension_header(uint8* Stream, recording_extension_header* Objects, size_t Count);

static ao_err handle_replay_extension();

// #define DEBUG_REPLAY

#ifdef DEBUG_REPLAY
static void open_stream_file(void);
static void debug_stream_of_flags(uint32 action_flag, short player_index);
static void close_stream_file(void);
#endif

/* ---------- code */


void initialize_keyboard_controller()
{
	ActionQueue *queue;
	short player_index;
	
//	assert_fail_f(NUMBER_OF_KEYS == NUMBER_OF_STANDARD_KEY_DEFINITIONS, "NUMBER_OF_KEYS == %d, NUMBER_OF_KEY_DEFS = %d. Not Equal!", NUMBER_OF_KEYS, NUMBER_OF_STANDARD_KEY_DEFINITIONS);
	
	// get globals initialized
	heartbeat_count= 0;
	input_task_active= false;
	obj_clear(replay);

	input_task= install_timer_task(TICKS_PER_SECOND, input_controller);
	assert_fail(input_task, "was null");
	
	atexit(remove_input_controller);
	
	/* Allocate the recording queues */	
	replay.recording_queues = new ActionQueue[MAXIMUM_NUMBER_OF_PLAYERS];
	
	/* Allocate the individual ones */
	for (player_index= 0; player_index<MAXIMUM_NUMBER_OF_PLAYERS; player_index++)
	{
		queue= get_player_recording_queue(player_index);
		queue->read_index= queue->write_index = 0;
		queue->buffer= new uint32[MAXIMUM_QUEUE_SIZE];
	}
	enter_mouse(0);
}


void set_keyboard_controller_status(bool active)
{
	input_task_active= active;

	// flush events when changing game state
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
	SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
	SDL_FlushEvents(SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONUP);

	// We enable/disable mouse control here
	if (active) {
		enter_mouse(input_preferences->input_device);
                enter_joystick();
        } else {
		exit_mouse(input_preferences->input_device);
                exit_joystick();
        }
}

/******************************************************************************************/

bool is_vbl_reading_user_inputs() // temporary (was get_keyboard_controller_status, but that name is unhelpful)
{
	return input_task_active;
}


int32 get_heartbeat_count()
{
	return heartbeat_count;
}

void sync_heartbeat_count()
{
	heartbeat_count= dynamic_world->tick_count;
}

void increment_replay_speed()
{
	if (replay.replay_speed < MAXIMUM_REPLAY_SPEED) replay.replay_speed++;
}

void decrement_replay_speed()
{
	if (replay.replay_speed > MINIMUM_REPLAY_SPEED) replay.replay_speed--;
}

void set_replay_speed(short speed)
{
	replay.replay_speed = speed;
}

int get_replay_speed()
{
	return replay.replay_speed;
}

bool game_is_being_replayed()
{
	return replay.game_is_being_replayed;
}

bool is_saved_game_replay()
{
	return game_is_being_replayed() && 
		replay.extension_header.extension_type == recording_extension_type::saved_game_wad;
}

void increment_heartbeat_count(int value)
{
	heartbeat_count+=value;
}


bool first_frame_rendered = true;

/* Called by the time manager task in vbl_macintosh.c */
bool input_controller(
	void)
{
	if (input_task_active || MovieExporter::instance()->IsRecording())
	{
		if((heartbeat_count-dynamic_world->tick_count) < ((first_frame_rendered || game_is_networked) ? MAXIMUM_TIME_DIFFERENCE : 1))
		{
			if (game_is_networked) // input from network
			{
				; // all handled elsewhere now. (in network.c)
			}
			else if (replay.game_is_being_replayed) // input from recorded game file
			{
				static short phase= 0; /* When this gets to 0, update the world */

				/* Minimum replay speed is a pause. */
				if(replay.replay_speed != MINIMUM_REPLAY_SPEED)
				{
					if (replay.replay_speed > 0 || (--phase<=0))
					{
						short flag_count= MAX(replay.replay_speed, 1);
						flag_count = pull_flags_from_recording(flag_count);
					
						if (!flag_count) // oops. silly me.
						{
							if (replay.have_read_last_chunk)
							{
								assert_fail(get_app_state() == app_state_t::game_in_progress || get_app_state() == app_state_t::load_and_play_demo_film, "film replay failed");
								set_app_state(app_state_t::load_and_play_demo_film);
							}
						}
						else
						{	
							/* Increment the heartbeat.. */
							heartbeat_count+= flag_count;
						}
	
						/* Reset the phase-> doesn't matter if the replay speed is positive */					
						/* +1 so that replay_speed 0 is different from replay_speed 1 */
						phase= -(replay.replay_speed) + 1;
					}
				}
			}
			else // then getting input from the keyboard/mouse
			{
				uint32 action_flags= parse_keymap();
				
				process_action_flags(local_player_index, &action_flags, 1);
				heartbeat_count++; // ba-doom
			}
		} else {
// ao__dprintf__("Out of phase.. (%d);g", heartbeat_count - dynamic_world->tick_count);
		}
	}
	
	return true; // tells the time manager library to reschedule this task
}

void process_action_flags(
	short player_identifier, 
	const uint32 *action_flags, 
	short count)
{
	if (replay.game_is_being_recorded)
	{
		record_action_flags(player_identifier, action_flags, count);
	}
	
	GetRealActionQueues()->enqueueActionFlags(player_identifier, action_flags, count);
}

static void record_action_flags(
	short player_identifier, 
	const uint32 *action_flags, 
	short count)
{
	short index;
	ActionQueue  *queue;
	
	queue= get_player_recording_queue(player_identifier);
	assert_fail(queue && queue->write_index >= 0 && queue->write_index < MAXIMUM_QUEUE_SIZE, "film recording problem");
	for (index= 0; index<count; index++)
	{
		*(queue->buffer + queue->write_index) = *action_flags++;
		INCREMENT_QUEUE_COUNTER(queue->write_index);
		if (queue->write_index == queue->read_index)
		{
			//ao__dprintf__("blew recording queue for player %d", player_identifier);
		}
	}
}

/*********************************************************************************************
 *
 * Function: save_recording_queue_chunk
 * Purpose:  saves one chunk of the queue to the recording file, using run-length encoding.
 *
 *********************************************************************************************/
void save_recording_queue_chunk(
	short player_index)
{
	uint8 *location;
	uint32 last_flag, count, flag = 0;
	int16 i, run_count, num_flags_saved, max_flags;
	static uint8 *buffer= NULL;
	ActionQueue *queue;
	
	// The data format is (run length (int16)) + (action flag (uint32))
	int DataSize = sizeof(int16) + sizeof(uint32);
	
	if (buffer == NULL)
		buffer = new byte[RECORD_CHUNK_SIZE * DataSize];
	
	location= buffer;
	count= 0; // keeps track of how many bytes we'll save.
	last_flag= (uint32)NONE;

	queue= get_player_recording_queue(player_index);
	
	// don't want to save too much stuff
	max_flags= MIN(RECORD_CHUNK_SIZE, get_recording_queue_size(player_index)); 

	// save what's in the queue
	run_count= num_flags_saved= 0;
	for (i = 0; i<max_flags; i++)
	{
		flag = queue->buffer[queue->read_index];
		INCREMENT_QUEUE_COUNTER(queue->read_index);
		
		if (i && flag != last_flag)
		{
			ValueToStream(location,run_count);
			ValueToStream(location,last_flag);
			count += DataSize;
			num_flags_saved += run_count;
			run_count = 1;
		}
		else
		{
			run_count++;
		}
		last_flag = flag;
	}
	
	// now save the final run
	ValueToStream(location,run_count);
	ValueToStream(location,last_flag);
	count += DataSize;
	num_flags_saved += run_count;
	
	if (max_flags<RECORD_CHUNK_SIZE)
	{
		short end_indicator = END_OF_RECORDING_INDICATOR;
		ValueToStream(location,end_indicator);
		int32 end_flag = 0;
		ValueToStream(location,end_flag);
		count += DataSize;
		num_flags_saved += RECORD_CHUNK_SIZE-max_flags;
	}
	
	current_film_file.write(count,buffer);
	replay.header.length+= count;
		
	assert_warn_f(num_flags_saved == RECORD_CHUNK_SIZE, "bad recording: %d flags, max=%d, count = %u;dm #%p #%u",
                                                num_flags_saved, max_flags, count, buffer, count);
}

/*********************************************************************************************
 *
 * Function: pull_flags_from_recording
 * Purpose:  remove one flag from each queue from the recording buffer.
 * Returns:  number of flags actually pulled
 *
 *********************************************************************************************/
static short pull_flags_from_recording(
	short count)
{
	short true_count = count;
	for (short player_index = 0; player_index < dynamic_world->player_count; player_index++)
	{
		ActionQueue* queue = get_player_recording_queue(player_index);
		for (short index = 0; index < count; index++)
		{
			if (get_recording_queue_size(player_index) != 0)
			{
#ifdef DEBUG_REPLAY
				debug_stream_of_flags(*(queue->buffer + queue->read_index), player_index);
#endif
				GetRealActionQueues()->enqueueActionFlags(player_index, queue->buffer + queue->read_index, 1);
				INCREMENT_QUEUE_COUNTER(queue->read_index);
			}
			else {
				true_count = index;
				break;
			}
		}
	}
	
	return true_count;
}

static short get_recording_queue_size(
	short which_queue)
{
	short size;
	ActionQueue *queue= get_player_recording_queue(which_queue);

	/* Note that this is a circular queue */
	size= queue->write_index-queue->read_index;
	if(size<0) size+= MAXIMUM_QUEUE_SIZE;
	
	return size;
}

void set_recording_header_data(short number_of_players, short level_number, uint32 map_checksum,
                               short version, player_start_data* starts, game_data* game_information)
{
	assert_fail(!replay.valid, "something's wrong with it");
	obj_clear(replay.header);
    
    replay.header.version       = version;
	replay.header.num_players   = number_of_players;
	replay.header.level_number  = level_number;
	replay.header.map_checksum  = map_checksum;
	objlist_copy(replay.header.starts, starts, MAXIMUM_NUMBER_OF_PLAYERS);
	obj_copy(replay.header.game_information, *game_information);
	// Use the packed size here!!!
	replay.header.length= SIZEOF_recording_header;
}


void get_recording_header_data(short& number_of_players, short& level_number, uint32& map_checksum,
                               short& version, player_start_data* starts, game_data* game_information)
{
	assert_fail(replay.valid, "nope");
	number_of_players   = replay.header.num_players;
	level_number        = replay.header.level_number;
	map_checksum        = replay.header.map_checksum;
 	version             = replay.header.version;
	objlist_copy(starts, replay.header.starts, MAXIMUM_NUMBER_OF_PLAYERS);
	obj_copy(*game_information, replay.header.game_information);
}


extern int movie_export_phase;
extern bool load_saved_game_from_flat_data(byte* saved_flat_data);

ao_err setup_for_replay_from_file(const ao_path& path, uint32 map_checksum)
{
	(void)(map_checksum);
	
    ao_err err = current_film_file.open(path);
    if (err) return err;
    
    replay.valid                  = true;
    replay.have_read_last_chunk   = false;
    replay.game_is_being_replayed = true;
    replay.resource_data          = NULL;
    replay.resource_data_size     = 0;
    replay.film_resource_offset   = NONE;
    movie_export_phase = 0;
    
    assert_fail(!replay.resource_data, "can't be null");
    
    uint8_t header[SIZEOF_recording_header];
    current_film_file.read(SIZEOF_recording_header, header);
    unpack_recording_header(header, &replay.header, 1);
    replay.header.game_information.cheat_flags = default_cheat_flags;
    replay.extension_header.extension_type = recording_extension_type::none;
    replay.extension_header.length = 0;

    int64_t file_length = current_film_file.get_length();

    // Set to the mapfile this replay came from
    err = file_length > replay.header.length ? handle_replay_extension()
                                             : set_current_map_path_to_file_with_checksum(replay.header.map_checksum);
    if (!err)
    {
        replay.fsread_buffer     = new char[DISK_CACHE_SIZE];
        replay.location_in_cache = NULL;
        replay.bytes_in_cache    = 0;
        replay.replay_speed      = 1;
        
#ifdef DEBUG_REPLAY
        open_stream_file();
#endif
    }
    else // map not found
    {
        replay.valid                  = false;
        replay.game_is_being_replayed = false;
        current_film_file.close();
        
        err = STRID(strERRORS, cantFindReplayMap);
    }
	
	return err;
}


void set_recording_saved_wad_data(const std::vector<byte>& saved_wad_data)
{
	replay.saved_wad_data = saved_wad_data;
}


// set the header information at start of file before we start recording
ao_err start_recording()
{
	assert_fail(!replay.valid, "nope");
	replay.valid = true;
	
    ao_path film_path = get_recording_path();
    std::filesystem::remove(film_path); // fairly sure this is unnecessary
    
    ao_err err = current_film_file.open(film_path, DataFile::mode_binary_write);
    if (err) return err;

    replay.game_is_being_recorded = true;
        
    uint8_t header[SIZEOF_recording_header];
    pack_recording_header(header, &replay.header, 1);
    current_film_file.write(SIZEOF_recording_header, header);
	
    return err;
}


ao_err stop_recording()
{
    ao_err err = no_err;
    
	if (replay.game_is_being_recorded)
	{
		replay.game_is_being_recorded = false;

		assert_fail(replay.valid, "nope");
		for (int32_t player_index = 0; player_index < dynamic_world->player_count; player_index++)
		{
			save_recording_queue_chunk(player_index);
		}

		// Rewrite the header, since it has the new length
        current_film_file.set_position(0);
		uint8_t header[SIZEOF_recording_header];
		pack_recording_header(header, &replay.header, 1);

		// ZZZ: removing code that does stuff from assert_fail() argument.  BUT...
		// should we really be asserting on this anyway?  I mean, the write could fail
		// in 'normal operation' too, not just when we screwed something up in writing the program?
        current_film_file.write(SIZEOF_recording_header, header);
        
		bool has_extension_header = false;
		replay.extension_header.length = 0;
		replay.extension_header.extension_type = recording_extension_type::none;

		int extension_data_length = 0;
		byte* extension_data = nullptr;

		if (replay.saved_wad_data.size())
		{
			extension_data_length = (int32_t)replay.saved_wad_data.size();
			extension_data = replay.saved_wad_data.data();
			replay.extension_header.extension_type = recording_extension_type::saved_game_wad;
			replay.extension_header.length = SIZEOF_recording_extension_header + (int32_t)replay.saved_wad_data.size();
			has_extension_header = true;
		}

		if (has_extension_header)
		{
            current_film_file.set_position(replay.header.length);
			byte extension_header[SIZEOF_recording_extension_header];
			pack_recording_extension_header(extension_header, &replay.extension_header, 1);

            current_film_file.write(SIZEOF_recording_extension_header, extension_header);
            current_film_file.set_position(replay.header.length + SIZEOF_recording_extension_header);
            current_film_file.write(extension_data_length, extension_data);
		}

        int64_t total_length = current_film_file.get_length();
		assert_fail(total_length==replay.header.length + replay.extension_header.length, "film file length is inconsistent"); // TODO: why is this assert, not permanent check?
		
		current_film_file.close();
	}

	replay.saved_wad_data.clear();
	replay.valid = false;
    return err;
}


ao_err handle_replay_extension()
{
    ao_err err = no_err;
    
    current_film_file.set_position(replay.header.length);
	uint8_t extension_header[SIZEOF_recording_extension_header];
    current_film_file.read(SIZEOF_recording_extension_header, extension_header);
	unpack_recording_extension_header(extension_header, &replay.extension_header, 1);

	switch (replay.extension_header.extension_type)
	{
		case recording_extension_type::saved_game_wad:
		{
			auto saved_wad = (uint8_t*)malloc(replay.extension_header.length);
            current_film_file.read(replay.extension_header.length, saved_wad);
			bool successful = load_saved_game_from_flat_data(saved_wad);
            if (!successful) err = STRID(strERRORS, cantReadFile);
			break;
		}

		default:
            throw_ao_exception("unrecognized replay extension type: %x", 1, replay.extension_header.extension_type);
			break;
	}

    current_film_file.set_position(SIZEOF_recording_header);
	return err;
}

ao_err reset_recording()
{
    ao_err err = no_err;
	if (replay.game_is_being_recorded)
	{
        current_film_file.set_position(0);
		byte header[SIZEOF_recording_header];
        current_film_file.read(SIZEOF_recording_header, header);
		current_film_file.close();
        
        std::filesystem::remove(current_film_file.get_path());
        
        ao_return_if_err(current_film_file.reopen());
        current_film_file.write(SIZEOF_recording_header, header);
		replay.header.length = SIZEOF_recording_header;
	}
    return no_err;
}

void check_recording_replaying(
	void)
{
	short player_index, queue_size;

	if (replay.game_is_being_recorded)
	{
		bool enough_data_to_save= true;
	
		// it's time to save the queues if all of them have >= RECORD_CHUNK_SIZE flags in them.
		for (player_index= 0; enough_data_to_save && player_index<dynamic_world->player_count; player_index++)
		{
			queue_size= get_recording_queue_size(player_index);
			if (queue_size < RECORD_CHUNK_SIZE)	enough_data_to_save= false;
		}
		
		if(enough_data_to_save)
		{
			for (player_index= 0; player_index<dynamic_world->player_count; player_index++)
			{
				save_recording_queue_chunk(player_index);
			}
		}
	}
	else if (replay.game_is_being_replayed)
	{
		bool load_new_data= true;
	
		// it's time to refill the requeues if they all have < RECORD_CHUNK_SIZE flags in them.
		for (player_index= 0; load_new_data && player_index<dynamic_world->player_count; player_index++)
		{
			queue_size= get_recording_queue_size(player_index);
			if(queue_size>= RECORD_CHUNK_SIZE) load_new_data= false;
		}
		
		if(load_new_data)
		{
			// at this point, we've determined that the queues are sufficently empty, so
			// we'll fill 'em up.
			read_recording_queue_chunks();
		}
	}
}

void reset_recording_and_playback_queues(
	void)
{
	short index;
	
	for(index= 0; index<MAXIMUM_NUMBER_OF_PLAYERS; ++index)
	{
		replay.recording_queues[index].read_index= replay.recording_queues[index].write_index= 0;
	}
}

void stop_replay(
	void)
{
	if (replay.game_is_being_replayed)
	{
		assert_fail(replay.valid, "");

		replay.game_is_being_replayed= false;
		if (replay.resource_data)
		{
			delete []replay.resource_data;
			replay.resource_data= NULL;
		}
		else
		{
			current_film_file.close();
			assert_fail(replay.fsread_buffer, "failed to close film file");
			delete []replay.fsread_buffer;
		}
#ifdef DEBUG_REPLAY
		close_stream_file();
#endif
	}

	/* Unecessary, because reset_player_queues calls this. */
	replay.valid= false;
}

static void read_recording_queue_chunks(
	void)
{
	log_context("reading recording queue chunks");

	int32 i, sizeof_read;
	uint32 action_flags; 
	int16 count, player_index, num_flags;
	ActionQueue *queue;
	
	for (player_index = 0; player_index < dynamic_world->player_count; player_index++)
	{
		queue= get_player_recording_queue(player_index);
		for (count = 0; count < RECORD_CHUNK_SIZE; )
		{
			if (replay.resource_data)
			{
				bool hit_end= false;
				
				if (replay.film_resource_offset >= replay.resource_data_size)
				{
					hit_end = true;
				}
				else
				{
					uint8* S;
					S = (uint8 *)(replay.resource_data + replay.film_resource_offset);
					StreamToValue(S,num_flags);
					replay.film_resource_offset += sizeof(num_flags);
					S = (uint8 *)(replay.resource_data + replay.film_resource_offset);
					StreamToValue(S,action_flags);
					replay.film_resource_offset+= sizeof(action_flags);
				}
				
				if (hit_end || num_flags == END_OF_RECORDING_INDICATOR)
				{
					replay.have_read_last_chunk= true;
					break;
				}
			}
			else
			{
				sizeof_read = sizeof(num_flags);
				uint8 NumFlagsBuffer[sizeof(num_flags)];
				bool HitEOF = false;
				if (vblFSRead(current_film_file, &sizeof_read, NumFlagsBuffer, HitEOF))
				{
					uint8 *S = NumFlagsBuffer;
					StreamToValue(S,num_flags);
					sizeof_read = sizeof(action_flags);
					uint8 ActionFlagsBuffer[sizeof(action_flags)];
					bool status = vblFSRead(current_film_file, &sizeof_read, ActionFlagsBuffer, HitEOF);
					S = ActionFlagsBuffer;
					StreamToValue(S,action_flags);
					assert_fail(status || (HitEOF && sizeof_read == sizeof(action_flags)), "screwed up action flags");
				}
				else
				{
                    log_error("film file read error");
					replay.have_read_last_chunk = true;
					break;
				}
				
				if ((HitEOF && sizeof_read != sizeof(action_flags)) || num_flags == END_OF_RECORDING_INDICATOR)
				{
					replay.have_read_last_chunk = true;
					break;
				}
			}

			if (!(replay.have_read_last_chunk || num_flags))
			{
                log_anomaly("chunk contains no flags");
			}

			count += num_flags;

			for (i = 0; i < num_flags; i++)
			{
				*(queue->buffer + queue->write_index) = action_flags;
				INCREMENT_QUEUE_COUNTER(queue->write_index);
				assert_fail(queue->read_index != queue->write_index, "problem reading circular action queue");
			}
		}
		assert_fail(replay.have_read_last_chunk || count == RECORD_CHUNK_SIZE, "mismatched film data length");
	}
}

/* This is gross, (Alain wrote it, not me!) but I don't have time to clean it up */
static bool vblFSRead(
	DataFile& File,
	int32 *count, 
	void *dest,
	bool& HitEOF)
{
	int32 fsread_count;
	bool status = true;
	
	assert_fail(replay.fsread_buffer, "failed to read film data");
	
	// LP: way for testing whether hitting end-of-file;
	// doing that by testing for whether a read was complete.
	HitEOF = false;

	if (replay.bytes_in_cache < *count)
	{
		assert_fail(replay.bytes_in_cache + *count < int(DISK_CACHE_SIZE), "film stuff");
		if (replay.bytes_in_cache)
		{
			memcpy(replay.fsread_buffer, replay.location_in_cache, replay.bytes_in_cache);
		}
		replay.location_in_cache = replay.fsread_buffer;
		fsread_count= DISK_CACHE_SIZE - replay.bytes_in_cache;
		int64_t PrevPos = File.get_position();
		int64_t replay_left= replay.header.length - PrevPos;
		if (replay_left < fsread_count) fsread_count= replay_left;
		if (fsread_count > 0)
		{
			assert_fail(fsread_count > 0, "film stuff");
			// LP: wrapped the routines with some for finding out the file positions;
			// this finds out how much is read indirectly
			File.read(fsread_count,replay.fsread_buffer+replay.bytes_in_cache);
			int64_t CurrPos = File.get_position();
			int32 new_fsread_count = CurrPos - PrevPos;
			int64_t FileLen = File.get_length();
			HitEOF = (new_fsread_count < fsread_count) && (CurrPos == FileLen);
			fsread_count = new_fsread_count;
			if(status) replay.bytes_in_cache += fsread_count;
		}
	}

	// If we're still low, then we've consumed the disk cache
	if(replay.bytes_in_cache < *count)
	{
		HitEOF = true;
	}

	// Ignore EOF if we still have cache
	if (HitEOF && replay.bytes_in_cache < *count)
	{
		*count= replay.bytes_in_cache;
	}
	else
	{
		status = true;
		HitEOF = false;
	}
	
	memcpy(dest, replay.location_in_cache, *count);
	replay.bytes_in_cache -= *count;
	replay.location_in_cache += *count;
	
	return status;
}

static void remove_input_controller(
	void)
{
	remove_timer_task(input_task);
	if (replay.game_is_being_recorded)
	{
		stop_recording();
	}
	else if (replay.game_is_being_replayed)
	{
		if (replay.resource_data)
		{
			delete []replay.resource_data;
			replay.resource_data= NULL;
			replay.resource_data_size= 0l;
			replay.film_resource_offset= NONE;
		}
		else
		{
			current_film_file.close();
		}
	}

	replay.valid= false;
}

static void StreamToPlayerStart(uint8* &S, player_start_data& Object)
{
	StreamToValue(S,Object.team);
	StreamToValue(S,Object.identifier);
	StreamToValue(S,Object.color);
    char tmp[MAXIMUM_PLAYER_START_NAME_LENGTH];
	StreamToBytes(S, tmp, sizeof(tmp));
    Object.name = convert_macroman_cstr_to_utf8_string(tmp, sizeof(tmp));
    
    S += 2;
}

static void PlayerStartToStream(uint8* &S, player_start_data& Object)
{
	ValueToStream(S,Object.team);
	ValueToStream(S,Object.identifier);
	ValueToStream(S,Object.color);
    char tmp[MAXIMUM_PLAYER_START_NAME_LENGTH];
    convert_utf8_string_to_macroman_cstr(Object.name, tmp, sizeof(tmp));
    BytesToStream(S, tmp,sizeof(tmp));
    S += 2;
}


static void StreamToGameData(uint8* &S, game_data& Object)
{
	StreamToValue(S,Object.game_time_remaining);
	StreamToValue(S,Object.game_type);
	StreamToValue(S,Object.game_options);
	StreamToValue(S,Object.kill_limit);
	StreamToValue(S,Object.initial_random_seed);
	StreamToValue(S,Object.difficulty_level);
	StreamToList(S,Object.parameters,2);
}

static void GameDataToStream(uint8* &S, game_data& Object)
{
	ValueToStream(S,Object.game_time_remaining);
	ValueToStream(S,Object.game_type);
	ValueToStream(S,Object.game_options);
	ValueToStream(S,Object.kill_limit);
	ValueToStream(S,Object.initial_random_seed);
	ValueToStream(S,Object.difficulty_level);
	ListToStream(S,Object.parameters,2);
}

uint8* pack_recording_extension_header(uint8* Stream, recording_extension_header* Objects, size_t Count)
{
	uint8* S = Stream;
	recording_extension_header* ObjPtr = Objects;

	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S, ObjPtr->length);
		ValueToStream(S, static_cast<int>(ObjPtr->extension_type));
	}

	assert_fail(static_cast<size_t>(S - Stream) == (Count * SIZEOF_recording_extension_header), "");
	return S;
}

uint8* unpack_recording_extension_header(uint8* Stream, recording_extension_header* Objects, size_t Count)
{
	uint8* S = Stream;
	recording_extension_header* ObjPtr = Objects;

	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S, ObjPtr->length);
		int extension_type;
		StreamToValue(S, extension_type);
		ObjPtr->extension_type = static_cast<recording_extension_type>(extension_type);
	}

	assert_fail(static_cast<size_t>(S - Stream) == (Count * SIZEOF_recording_extension_header), "");
	return S;
}

uint8 *unpack_recording_header(uint8 *Stream, recording_header *Objects, size_t Count)
{
	uint8* S = Stream;
	recording_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->length);
		StreamToValue(S,ObjPtr->num_players);
		StreamToValue(S,ObjPtr->level_number);
		StreamToValue(S,ObjPtr->map_checksum);
		StreamToValue(S,ObjPtr->version);
		for (int m = 0; m < MAXIMUM_NUMBER_OF_PLAYERS; m++)
			StreamToPlayerStart(S,ObjPtr->starts[m]);
		StreamToGameData(S,ObjPtr->game_information);
	}
	
	assert_fail(static_cast<size_t>(S - Stream) == (Count*SIZEOF_recording_header), "");
	return S;
}

uint8 *pack_recording_header(uint8 *Stream, recording_header *Objects, size_t Count)
{
	uint8* S = Stream;
	recording_header* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->length);
		ValueToStream(S,ObjPtr->num_players);
		ValueToStream(S,ObjPtr->level_number);
		ValueToStream(S,ObjPtr->map_checksum);
		ValueToStream(S,ObjPtr->version);
		for (size_t m = 0; m < MAXIMUM_NUMBER_OF_PLAYERS; m++)
			PlayerStartToStream(S,ObjPtr->starts[m]);
		GameDataToStream(S,ObjPtr->game_information);
	}
	
	assert_fail(static_cast<size_t>(S - Stream) == (Count*SIZEOF_recording_header), "");
	return S;
}

// Constants
#define MAXIMUM_FLAG_PERSISTENCE    15
#define DOUBLE_CLICK_PERSISTENCE    10
#define FILM_RESOURCE_TYPE          FOUR_CHARS_TO_INT('f', 'i', 'l', 'm')

#define NUMBER_OF_SPECIAL_FLAGS (sizeof(special_flags)/sizeof(struct special_flag_data))
static struct special_flag_data special_flags[]=
{
	{_double_flag, _look_dont_turn, _looking_center},
	{_double_flag, _run_dont_walk, _action_trigger_state},
	{_latched_flag, _action_trigger_state},
	{_latched_flag, _cycle_weapons_forward},
	{_latched_flag, _cycle_weapons_backward},
	{_latched_flag, _toggle_map}
};



ao_path get_recording_path()
{
    return get_local_storage_dir() / get_string(STRID(strFILENAMES, filenameMARATHON_RECORDING));
}





static uint32_t hotkey_sequence[3] {0};
static constexpr uint32_t hotkey_used = 0x80000000;

void encode_hotkey_sequence(int hotkey)
{
	hotkey_sequence[0] =
		(3 << _cycle_weapons_forward_bit) |
		hotkey_used;
	
	hotkey_sequence[1] =
		((hotkey / 4 + 1) << _cycle_weapons_forward_bit) |
		hotkey_used;
	
	hotkey_sequence[2] =
		((hotkey % 4) << _cycle_weapons_forward_bit) |
		hotkey_used;
}

/*
 *  Poll keyboard and return action flags
 */

uint32_t last_input_update;

uint32 parse_keymap(void)
{
  uint32 flags = 0;

  if(is_vbl_reading_user_inputs())
    {
		Uint8 key_map[SDL_NUM_SCANCODES];
      if (Console::instance()->input_active()) {
	memset(key_map, 0, sizeof(key_map));
      } else {
		  memcpy(key_map, SDL_GetKeyboardState(NULL), sizeof(key_map));
		  auto mod_state = SDL_GetModState();
		  key_map[SDL_SCANCODE_CAPSLOCK] = mod_state & KMOD_CAPS ? 1 : 0;
      }
      
      // ZZZ: let mouse code simulate keypresses
      mouse_buttons_become_keypresses(key_map);
      joystick_buttons_become_keypresses(key_map);
      
      // Parse the keymap
		for (int i = 0; i < NUMBER_OF_STANDARD_KEY_DEFINITIONS; ++i)
		{
			for (const SDL_Scancode& code : input_preferences->key_bindings[i])
			{
				if (key_map[code])
					flags |= standard_key_definitions[i].action_flag;
			}
		}
		
      // Post-process the keymap
		struct special_flag_data *special = special_flags;
      for (unsigned i=0; i<NUMBER_OF_SPECIAL_FLAGS; i++, special++) {
	if (flags & special->flag) {
	  switch (special->type) {
	  case _double_flag:
	    // If this flag has a double-click flag and has been hit within
	    // DOUBLE_CLICK_PERSISTENCE (but not at MAXIMUM_FLAG_PERSISTENCE),
	    // mask on the double-click flag */
	    if (special->persistence < MAXIMUM_FLAG_PERSISTENCE
		&&	special->persistence > MAXIMUM_FLAG_PERSISTENCE - DOUBLE_CLICK_PERSISTENCE)
	      flags |= special->alternate_flag;
	    break;
	    
	  case _latched_flag:
	    // If this flag is latched and still being held down, mask it out
	    if (special->persistence == MAXIMUM_FLAG_PERSISTENCE)
	      flags &= ~special->flag;
	    break;
	    
	  default:
              throw_ao_exception("bad special action flags: %x", 1, special->type);
	    break;
	  }
	  
	  special->persistence = MAXIMUM_FLAG_PERSISTENCE;
	} else
	  special->persistence = FLOOR(special->persistence-1, 0);
      }

	  if (!hotkey_sequence[0])
	  {
		  for (auto i = 0; i < NUMBER_OF_HOTKEYS; ++i)
		  {
			  auto& hotkey = input_preferences->hotkey_bindings[i];
			  for (auto it : hotkey)
			  {
				  if (key_map[it])
				  {
					  encode_hotkey_sequence(i);
					  break;
				  }
			  }
		  }
	  }

	  if (hotkey_sequence[0])
	  {
		  flags &= ~(_cycle_weapons_forward | _cycle_weapons_backward);
		  flags |= (hotkey_sequence[0] & ~hotkey_used);
		  hotkey_sequence[0] = hotkey_sequence[1];
		  hotkey_sequence[1] = hotkey_sequence[2];
		  hotkey_sequence[2] = 0;
	  }

	  if (input_preferences->input_device == _mouse_yaw_pitch) {
		  flags = process_aim_input(flags, pull_mouselook_delta());
	  }

	  flags = process_joystick_axes(flags);

      // if the user prefers to toggle run/swim, the flag becomes latched
      if (input_preferences->modifiers & _inputmod_run_key_toggle)
      {
          static bool persistence = false;
          if (flags & _run_dont_walk)
          {
              if (persistence)
              {
                  flags &= ~_run_dont_walk;
              }

              persistence = true;
          }
          else
          {
              persistence = false;
          }
      }

      if (input_preferences->modifiers & _inputmod_run_key_toggle)
      {
          static bool run_swim = false;
          if (flags & _run_dont_walk)
          {
              run_swim = !run_swim;
          }

          if (run_swim)
          {
              flags |= _run_dont_walk;
          }
          else
          {
              flags &= ~_run_dont_walk;
          }
      }
      else
      {
          bool do_interchange =
              (local_player->variables.flags & _HEAD_BELOW_MEDIA_BIT) ?
              (input_preferences->modifiers & _inputmod_interchange_swim_sink) != 0:
              (input_preferences->modifiers & _inputmod_interchange_run_walk) != 0;

           if (do_interchange)
           {
               flags ^= _run_dont_walk;
           }
      }
		
      
      if (player_in_terminal_mode(local_player_index))
	flags = build_terminal_action_flags((char *)key_map);
    } // if(is_vbl_reading_user_inputs())
  
  return flags;
}



/*
 *  Periodic task management
 */

typedef bool (*timer_func)(void);

static timer_func tm_func = NULL;	// The installed timer task
static uint64_t tm_period;			// Ticks between two calls of the timer task
static uint64_t tm_last = 0, tm_accum = 0;

timer_task_proc install_timer_task(short tasks_per_second, timer_func func)
{
	// We only handle one task, which is enough
	tm_period = 1000 / tasks_per_second;
	tm_func = func;
	tm_last = machine_tick_count();
	tm_accum = 0;
	return (timer_task_proc)tm_func;
}

void remove_timer_task(timer_task_proc proc)
{
	tm_func = NULL;
}

void execute_timer_tasks(uint64_t time)
{
	if (tm_func)
    {
		if (MovieExporter::instance()->IsRecording())
        {
			if (get_fps_target() == 0 || movie_export_phase++ % (get_fps_target() / 30) == 0) { tm_func(); }
			return;
		}
		
		auto now = time;
		tm_accum += now - tm_last;
		tm_last = now;
		bool first_time = true;
		while (tm_accum >= tm_period)
        {
			tm_accum -= tm_period;
			if (first_time) // ick
            {
                if (is_vbl_reading_user_inputs()) { mouse_idle(input_preferences->input_device); }
				first_time = false;
			}
			tm_func();
		}
	}
}


