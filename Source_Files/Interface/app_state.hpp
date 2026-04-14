

#ifndef app_state_hpp
#define app_state_hpp

#include "cseries.h"

#include "interface_support.hpp"


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
    
    start_solo_game,
    start_solo_game_choosing_level, // TODO: rename: start_solo_game_choosing_level
    
    start_network_game,
    gather_network_game,
    join_network_game,
    await_network_game,
    
    load_and_resume_saved_game,
    
    load_and_play_saved_film,
    
    load_and_play_dropped_films,
    
    load_and_play_last_film,
    load_and_play_demo_film,
    save_last_film,
    
    
    enter_game, // TODO: rename enter_gameworld?
    game_in_progress,
    change_level, // inter-level teleport
    revert_to_saved_game, // reload last savepoint after dying
    exit_game, // TODO: probably want separate states for user explicitly quitting an in-progress game vs exit_game[world]
        
};




enum class user_type_t
{
    // live
    solo_player,
    network_player,
    // replay
    film_player,
};


user_type_t get_user_type();

void set_user_type(user_type_t type);


inline bool game_is_live()   { return get_user_type() != user_type_t::film_player; }

inline bool game_is_replay() { return get_user_type() == user_type_t::film_player; }




// TODO: may be simpler if set_next_app_state is split into separate advance_app_state and set_next_app_state, and transition_to_next_app_state calls advance at start and gets back the new current state

app_state_t get_app_state();


bool app_state_has_timed_out();


app_state_t advance_app_state(); // this assumes that set_next_app_state has previously been called; returns the new app state


inline bool is_interstitial_screen() // ick
{
    return get_app_state() >= app_state_t::startup_screen && get_app_state() <= app_state_t::advance_to_next_screen;
}



void set_next_app_state(app_state_t new_state, uint32_t machine_ticks_until_next_state = 0); // 0 = transition now

void set_app_state(app_state_t new_state);

void suspend_app_state_timeout(); // sets timeout to "infinite" so app does not auto-transition to next state

void restart_app_state_timeout(); // resets the timeout to its original delay (e.g. to prevent a Demo film starting while user is navigating main menu buttons using cursor keys)

void force_app_state_timeout(); // sets the timeout to 0 so app state will transition immediately on the next main event loop


void initialize_app_state();



void set_next_level_number(int16_t level_number);
int16_t get_next_level_number();


// called from shell.cpp's process_game_key
void set_app_focus_lost();
void set_app_focus_gained();



#endif /* app_state_hpp */
