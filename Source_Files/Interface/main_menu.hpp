

#ifndef main_menu_hpp
#define main_menu_hpp

#include "cseries.h"

#include "interface_support.hpp"


enum  // Menus available during the game
{
    mGame       = 128,
    iPause      = 1,
    iSave,
    iRevert,
    iCloseGame,
    iQuitGame,
};

enum // Main menu interface actions
    {
    mInterface          = 129,
    iNewGame            = 1,
    iLoadGame,
    iGatherGame,
    iJoinGame,
    iPreferences,
    iReplayLastFilm,
    iSaveLastFilm,
    iReplaySavedFilm,
    iCredits,
    iQuit,
    iCenterButton,
    iPlaySingletonLevel,
    iAbout,
};


enum // main menu button rects
{
    // M2 HUD rects are now in hud_definitions.hpp
    
    // main menu rects
    START_OF_UI_RECTS           =  7,
    _new_game_button_rect       =  7,
    _load_game_button_rect      =  8,
    _gather_button_rect         =  9,
    _join_button_rect           = 10,
    _prefs_button_rect          = 11,
    _replay_last_button_rect    = 12,
    _save_last_button_rect      = 13,
    _replay_saved_button_rect   = 14,
    _credits_button_rect        = 15,
    _quit_button_rect           = 16,
    _center_button_rect         = 17,
    _singleton_game_button_rect = 18,
    _about_alephone_rect        = 19,
    END_OF_UI_RECTS             = 20,
    
    // M2 computer terminal rects are temporarily in screen_drawing.h
};

#define get_command_id_for_main_menu_rect(rect_index)  ((rect_index) - START_OF_UI_RECTS + 1)
#define get_main_menu_rect_for_command_id(command_id)  ((command_id) + START_OF_UI_RECTS - 1)


void display_main_menu(void);




void do_main_menu_item_command(short menu_item, bool cheat);


bool enabled_item(short item);


void draw_main_menu(void);


void portable_process_screen_click(short x, short y, bool cheatkeys_down);
void process_main_menu_highlight_advance(bool reverse);
void process_main_menu_highlight_select(bool cheatkeys_down);
void draw_main_menu_button_for_command(short index);


void draw_main_menu_button_for_rect(short rect_index, bool pressed);



// MML

void reset_mml_menu_item_order();

void parse_mml_menu_item_order(const InfoTree& root); // <interface>




#endif /* main_menu_hpp */
