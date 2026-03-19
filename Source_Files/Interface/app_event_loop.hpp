

#ifndef app_event_loop_hpp
#define app_event_loop_hpp

#include "cseries.h"

#include "interface_support.hpp"




struct game_state
{
    int16_t  state;
    int16_t  flags; // TODO: what are flags?
    int16_t  user;
    int32_t  ticks_until_next_screen;
    uint64_t last_ticks_on_idle;
    int16_t  current_screen;
    bool     current_netgame_allows_microphone;
    bool     can_play_intro_music;   
    int16_t  highlighted_main_menu_item;
};


extern game_state game_state; // temporary


// called from shell.cpp's process_game_key
void set_app_focus_lost();
void set_app_focus_gained();



void initialize_game_state(void);
void force_game_state_change(void);

void set_game_state(short new_state); // sets/gets game_state.state; TODO: poorly named
short get_game_state();
short get_user_controlling_game(); // returns game_state.user
bool player_controlling_game(void);





void set_change_level_destination(short level_number);

short get_change_level_destination();

ao_err transfer_to_new_level(short level_number);



void update_interface(void);





#endif /* app_event_loop_hpp */
