
#ifndef setup_game_hpp
#define setup_game_hpp

#include "cseries.h"

#include "app_state.hpp"
#include "network.h" // game_info




void start_game(bool changing_level);

ao_err transfer_to_new_level(short level_number);

void finish_game(bool return_to_main_menu);

void clean_up_after_failed_game(bool inNetgame, bool inRecording, bool inFullCleanup);




ao_err load_and_start_game(bool is_coop = false); // TODO: separate load from start


void configure_new_solo_game(int16_t level_number);

void configure_new_network_game(game_info* info);

ao_err configure_replay_game(); // returns replayVersionTooNew if the film file was recorded by newer version of AO






ao_err create_new_game(user_type_t user, int16_t level_number, bool cheat);



ao_err join_networked_resume_game(); // co-op game reloaded from saved game file; obviously this function will rename once it's parted





void handle_save_film();


//ao_err handle_edit_map();




#endif /* setup_game_hpp */
