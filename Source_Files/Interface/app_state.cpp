/*
 app_state.cpp
 
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

#include "app_state.hpp"

#include "vbl.h" // is_vbl_reading_user_inputs

#include "preferences.hpp" // player_preferences

#include "map_wad.h" // get_dynamic_data_from_saved_game_file



static app_state_t current_state = app_state_t::startup;

static app_state_t next_state = app_state_t::shutdown; // will be set to startup_screen or main_menu by shell's initialize_application

static int64_t time_of_next_transition = 0; // set_next_app_state() sets this to a future time (current machine ticks + ticks until change) at which main event loop should transition from current to next state

static int64_t timeout_duration = 0; // we need to hang onto this value so that losing and regaining window focus on main menu doesn't instantly jump into demo film

static int16_t next_level_number = NONE; // new level to teleport to


// used to revert game after player death
static game_configuration_t game_configuration; // if a saved game file doesn't yet exist then revert to start of campaign
static ao_path saved_game_file; // once a pattern buffer is used, revert to the last-saved game
static dynamic_world_t saved_dynamic_world;



void clear_game_configuration()
{
    memset(&game_configuration, 0, sizeof(game_configuration));
    memset(&saved_dynamic_world, 0, sizeof(saved_dynamic_world));
    saved_game_file.clear();
}


void configure_game_for_new_solo_campaign(int16_t level_number)
{
    clear_game_configuration();
    
    game_configuration.new_solo_game(level_number, player_preferences.difficulty_level);
    set_film_profile_for_new_solo_game(player_preferences.solo_profile);
    set_random_seed(game_configuration.initial_random_seed);
}


ao_err configure_game_for_resumed_campaign(const ao_path& saved_game_path)
{
    clear_game_configuration();
    
    saved_game_file = saved_game_path;
    return get_dynamic_data_from_saved_game_file(saved_game_path, saved_dynamic_world); // TODO: where should random seed be restored?
}


game_configuration_t& get_game_configuration()
{
    return game_configuration;
}


int16_t get_initial_level_number()
{
    return game_configuration.initial_level_number;
}


void set_current_saved_game_path(const ao_path& path)
{
    saved_game_file = path;
}

const ao_path& get_current_saved_game_path()
{
    return saved_game_file;
}


// -----------------------------------------------------------------------------------------
// moved here from map_wad.cpp



//void reset_revert_game_file_to_default()
//{
    // euwww
//    saved_game_file = get_saved_games_dir() / get_string(STRID(strFILENAMES, filenameDEFAULT_SAVE_GAME));
//}



void set_next_level_number(int16_t level_number)
{
    assert_fail(current_state == app_state_t::change_level, "");
    next_level_number = level_number;
}


int16_t get_next_level_number()
{
    return next_level_number;
}



user_type_t user_type = user_type_t::solo;


user_type_t get_user_type()
{
    return user_type;
}

void set_user_type(user_type_t type)
{
    user_type = type;
}


// -----------------------------------------------------------------------------------------
// application states


void initialize_app_state()
{
    current_state = app_state_t::startup;
    next_state = app_state_t::startup_screen;
    next_level_number = 0;
}


app_state_t get_app_state()
{
    return current_state;
}


app_state_t advance_app_state()
{
    // TODO: what, if any use cases, where next_state is same as current_state? (this'd be a smell)
    current_state = next_state;
    return current_state;
}


// Sets the new next app state which the event loop will transition to in N ticks (0 = immediately on next loop). Called at end of initialize_application and in main_event_loop.cpp's transition_to_next_app_state.
void set_next_app_state(app_state_t next_state, uint32_t machine_ticks_until_next_state)
{
    ::next_state = next_state;
    timeout_duration = machine_ticks_until_next_state;
    restart_app_state_timeout();
    
    if (machine_ticks_until_next_state > 0)
        printf("state %02i at %05llu; will transition to %02i at %05lld\n", current_state, machine_tick_count(), next_state, time_of_next_transition);
    else
        printf("state %02i at %05llu; will transition to %02i now\n", current_state, machine_tick_count(), next_state);
}


// timeouts

bool app_state_has_timed_out()
{
    return machine_tick_count() >= time_of_next_transition;
}


void suspend_app_state_timeout()
{
    time_of_next_transition = INFINITE_TIME_DELAY;
}


void restart_app_state_timeout()
{
    time_of_next_transition = machine_tick_count() + timeout_duration;
}


void force_app_state_timeout()
{
    time_of_next_transition = 0;
}

