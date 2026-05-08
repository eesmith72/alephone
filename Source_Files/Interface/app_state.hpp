/*
 app_state.hpp
 
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

#ifndef app_state_hpp
#define app_state_hpp

#include "cseries.hpp"

#include "interface_support.hpp"

#include "map.h" // game_configuration_t


// 60 Hz
#define TICKS_BETWEEN_EVENT_POLL  (16)


// TODO: the switch that uses these is in main_event_loop.cpp; the display_SCREEN method sets them

enum class app_state_t // app states
{
    undefined           = 0,
    shutdown            = 0,
    startup             = 1,
    
    // interstitial screens
    startup_screen, // TODO: consistency in naming; e.g. 'show_startup_screen'
    chapter_screen,
    prologue_screen,
    epilogue_screen,
    credit_screen,
    shutdown_screen,
    
    display_current_screen, // 8
    advance_to_next_screen,
    
    // main menu
    main_menu, // = 10
    
    // the following enums are mostly re-used as main menu's actions
    preferences, // TODO: consistency in naming; e.g. 'show_preferences'
    quit, // -> shutdown_screen -> shutdown
    credits, // 13
    about_ao,
    center,
    //help,
    map_editor,
    
    start_new_campaign,
    choose_vidmaster_level,
    load_new_solo_game,
    
    //start_network_game, // TODO: implement for unified Multiplayer button
    
    new_pvp_game,
    
    gather_network_game,
    join_network_game,
    join_resumed_coop_game,
    await_network_game,
    
    choose_saved_game,
    load_saved_game,
    
    load_and_play_saved_film,
    load_and_play_dropped_film,
    load_and_play_last_film,
    load_and_play_demo_film,
    
    save_last_film,
    
    
    enter_game,
    game_in_progress,
    change_level, // inter-level teleport
    revert_to_saved_game, // reload last savepoint after dying
    exit_game, // TODO: probably want separate states for user explicitly quitting an in-progress game vs exit_game[world]
        
};



#define user_type_multiplayer_mask (0x02)

enum class user_type_t : uint32_t
{
    solo    = 0x01,
    coop    = user_type_multiplayer_mask | 0x00,
    pvp     = user_type_multiplayer_mask | 0x01,
    replay  = 0x04,
};


user_type_t get_user_type();

void set_user_type(user_type_t type);


inline bool game_is_live()      { return get_user_type() != user_type_t::replay; }

inline bool game_is_replay()    { return !game_is_live(); }

inline bool game_is_networked() { return (uint32_t)get_user_type() & user_type_multiplayer_mask; }


// TODO: may be simpler if set_next_app_state is split into separate advance_app_state and set_next_app_state, and transition_to_next_app_state calls advance at start and gets back the new current state

app_state_t get_app_state();


// TODO: defined in game_event_loop, which is not ideal: problem occurs when exiting gameworld as app state is still `game_in_progress` but `main_screen.did_change` needs to know that it's returning to UI in order to set correct size
bool game_is_running(); // { return get_app_state() == app_state_t::game_in_progress; }


bool app_state_has_timed_out();


app_state_t advance_app_state(); // this assumes that set_next_app_state has previously been called; returns the new app state


inline bool is_interstitial_screen() // ick
{
    return get_app_state() >= app_state_t::startup_screen && get_app_state() <= app_state_t::advance_to_next_screen;
}



void set_next_app_state(app_state_t new_state, uint32_t machine_ticks_until_next_state = 0); // 0 = transition now

void suspend_app_state_timeout(); // sets timeout to "infinite" so app does not auto-transition to next state

void restart_app_state_timeout(); // resets the timeout to its original delay (e.g. to prevent a Demo film starting while user is navigating main menu buttons using cursor keys)

void force_app_state_timeout(); // sets the timeout to 0 so app state will transition immediately on the next main event loop


void initialize_app_state();


// configure campaign/pvp match

void configure_game_for_new_solo_campaign(int16_t level_number = 0);

ao_err configure_game_for_resumed_campaign(const ao_path& saved_game_path);


game_configuration_t& get_game_configuration(); // not consted for now as get_recording_header_data writes it

void clear_game_configuration(); // clears old state (probably unnecessary) before restoring from file


int16_t get_initial_level_number();


void set_next_level_number(int16_t level_number);
int16_t get_next_level_number();


void set_current_saved_game_path(const ao_path& path);

const ao_path& get_current_saved_game_path();




#endif /* app_state_hpp */
