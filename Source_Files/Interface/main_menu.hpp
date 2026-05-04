

#ifndef main_menu_hpp
#define main_menu_hpp

#include "cseries.hpp"

#include "app_state.hpp"
#include "interface_support.hpp"


// TODO: confirm this now works with HD widescreen images and MML button rects plotted on those images


enum class direction_t : int32_t
{
    left,
    up,
    down,
    right,
};



enum // main menu button rects // TODO: these enums are no longer used in the CPP code, except for the START and END offsets which must be kept for legacy Lua+MML support
{
    // M2 HUD rects are now in hud_definitions.hpp
    
    // main menu rects
    START_OF_MAIN_MENU_RECTS    =  7,
    /* these rects are now defined in main_menu_buttons_std of main_menu.cpp and use app_state_t enums which correspond to main menu buttons as lookup keys
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
     */
    END_OF_MAIN_MENU_RECTS      = 20,
    
    // M2 computer terminal rects are temporarily in screen_drawing.h but will move to terminal_support.hpp
};


// images.cpp needs main menu button rects to generate M1's main menu bitmaps from Shapes.shps collection 10
const SDL_Rect& get_main_menu_button_rect_for_action(app_state_t button_action);


// this should only be called
void display_main_menu();


// Called by app event loop when user presses keyboard key/controller button
void handle_main_menu_keyboard_input(const SDL_Event &event);

void handle_main_menu_mouse_input(const SDL_Event &event);

void handle_main_menu_controller_input(const SDL_Event &event);


// -----------------------------------------------------------------------------------------
// MML <interface> can define main menu's button rects and/or the order in which cursor keys select buttons

void reset_mml_main_menu();

void parse_mml_main_menu(const InfoTree& root);



#endif /* main_menu_hpp */
