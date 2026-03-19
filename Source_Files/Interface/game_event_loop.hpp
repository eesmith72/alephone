

#ifndef game_event_loop_hpp
#define game_event_loop_hpp

#include "cseries.h"

#include "interface_support.hpp"



void pause_game();
void resume_game();



bool idle_game_state(uint64_t time);



ao_err load_and_start_game(const ao_path& File);

ao_err begin_game(short user, bool cheat);

void start_game(short user, bool changing_level);

void finish_game(bool return_to_main_menu);

void clean_up_after_failed_game(bool inNetgame, bool inRecording, bool inFullCleanup);



void handle_load_game(void);

void handle_replay(bool last_replay);

void handle_load_game();

void handle_save_film();

void handle_network_game(bool gatherer);

ao_err handle_open_replay(const ao_path& File);

ao_err handle_edit_map();


void do_gameworld_command(short menu_item);


#endif /* game_event_loop_hpp */
