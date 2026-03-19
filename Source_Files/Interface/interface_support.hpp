
#ifndef interface_support_hpp
#define interface_support_hpp

#include "cseries.h"




enum // source of player actions; see also vbl
{
    // live
    _single_player,
    _network_player,
    // film
    _demo,
    _replay,
    _replay_from_file,
    NUMBER_OF_PSEUDO_PLAYERS
};


// TODO: the switch that uses these is in app_event_loop.cpp; the display_SCREEN method sets them

enum  // app states
{
    _display_intro_screens,
    _display_main_menu,
    _display_chapter_heading,
    _display_prologue,
    _display_epilogue,
    _display_credits,
    _display_intro_screens_for_demo,
    _display_quit_screens,
    NUMBER_OF_SCREENS,
    
    _game_in_progress= NUMBER_OF_SCREENS,
    _quit_game,
    _close_game,
    _switch_demo,
    _revert_game,
    _change_level,
    _begin_display_of_epilogue,
    _displaying_network_game_dialogs,
    NUMBER_OF_GAME_STATES
};



void initialize_ui();
void shutdown_ui();


// TODO: move into dialog class
struct Canvas_SDL;
Canvas_SDL* get_ui_canvas();
struct Blitter;
Blitter* get_ui_blitter();



// To be called regularly during event loops
void global_idle_proc();


// dumped here till we decide where best to put it

// returns false if cancelled
bool show_quit_without_saving_dialog();

// returns false if cancelled
bool show_vidmaster_dialog(int16_t& selected_level_number);


// MML

struct InfoTree;
void reset_mml_vidmaster_dialog_strings();
void parse_mml_vidmaster_dialog_strings(const InfoTree& root);


#endif /* interface_support_hpp */
