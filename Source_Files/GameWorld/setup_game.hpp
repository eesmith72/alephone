
#ifndef setup_game_hpp
#define setup_game_hpp

#include "cseries.h"


// TODO: game_info is defined in network.h and is almost identical to game_configuration_t defined in map.h; merge these structs into one and relocate here

void match_starts_with_existing_players(); // fuckwittery from network.h, put here till we consolidate the 2 functions in setup_game.cpp




//void configure_game_for_new_solo_campaign();

void configure_new_coop_game();

void configure_new_pvp_game();

ao_err configure_replay_game(); // returns replayVersionTooNew if the film file was recorded by newer version of AO



//void create_network_player_identities();


void synchronize_restored_solo_player();

void synchronize_restored_coop_players();




ao_err load_saved_game_from_flat_data(uint8_t* saved_flat_data);


ao_err load_level(int16_t level_number, bool is_new_game);


void finish_game();


#endif /* setup_game_hpp */
