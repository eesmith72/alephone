

#include "app_event_loop.hpp"

#include "shell_options.h"
#include "main_menu.hpp" // display_main_menu
#include "chapter_screens.hpp" //display_splash_screen
#include "movie_screen.hpp" // MML alternative to chapter screens

// TODO: this is a straight copy-paste of all includes from interface.cpp; cleaning it up is for later

#ifdef HAVE_LIBYUV
#include <libyuv/convert.h>
#include <libyuv/scale.h>
#endif

#ifdef PERFORMANCE
#include <perf.h>

extern TP2PerfGlobals perf_globals;
#endif

#include "choose_file_dialogs_os.hpp"

#include "Canvas_SDL.hpp"

#include "mouse.h" // show_cursor
#include "map.h"
#include "shell.h"
#include "interface.h"
#include "player.h"
#include "network.h"
#include "screen_drawing.h"
#include "SoundManager.h"
#include "fades.h"
#include "game_window.h"
#include "Music.h"
#include "images.h"
#include "screen.h"
#include "vbl.h"
#include "preferences.h"
#include "DataFile.hpp"
#include "lua_script.h" // PostIdle
#include "XML_LevelScript.h"
#include "Movie.h"
#include "QuickSave.h"
#include "Plugins.h"
#include "Statistics.h"
#include "shell_options.h"
#include "OpenALManager.h"

#include "InfoTree.h"


#include "render.h"
#include "OGL_Render.h"
#include "image_blitter.hpp"
#include "alephversion.h"

// To tell it to stop playing, and also to run the end-game script
#include "XML_LevelScript.h"

// ZZZ: should the function that uses these (join_networked_resume_game()) go elsewhere?
#include "wad.h"
#include "map_wad.h"

#include "motion_sensor.hpp" // for reset_motion_sensor() // this is also called in map_wad.cpp and, all over the place, really

#include "lua_hud_script.h"

#include <progress.h>

#include "Canvas.hpp"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialog_widgets_sdl.h"


#include "main_menu.hpp" // draw_main_menu_button







struct game_state game_state; // TODO: rename app_state



void initialize_game_state()
{
    game_state.state                = _display_intro_screens;
    game_state.user                 = _single_player;
    game_state.flags                = 0;
    game_state.current_screen       = 0;
    game_state.can_play_intro_music = true;
    
    if (shell_options.insecure_lua) { notify_user(STRID(strDEBUG, db_insecure_lua)); }
    
    if (!shell_options.editor && shell_options.replay_directory.empty())
    {
        if (shell_options.skip_intro)
        {
            display_main_menu();
        }
        else
        {
            display_splash_screen();
        }
    }
}




void update_interface() // TODO: where should this go?
{
    if (game_state.state == _display_main_menu)
    {
        if (game_state.highlighted_main_menu_item >= 0)
        {
            draw_main_menu_button_for_rect(game_state.highlighted_main_menu_item + START_OF_UI_RECTS - 1, true);
        }
    }
    
    swap_screen_if_requested();
}






void set_app_focus_lost()
{
    switch (game_state.state)
    {
        case _display_main_menu:
        case _display_intro_screens_for_demo:
            game_state.ticks_until_next_screen = INFINITE_TIME_DELAY;
            break;
    }
}

void set_app_focus_gained()
{
    switch (game_state.state)
    {
        case _display_main_menu:
            game_state.ticks_until_next_screen = TICKS_UNTIL_DEMO_STARTS;
            break;
        case _display_intro_screens_for_demo:
            game_state.ticks_until_next_screen = DEMO_INTRO_SCREEN_DURATION;
            break;
    }
}





void force_game_state_change()
{
    game_state.ticks_until_next_screen= 0;
}


bool player_controlling_game()
{
    return ((game_state.user == _single_player || game_state.user == _network_player)
        && (game_state.state == _game_in_progress || game_state.state == _switch_demo));
}


short get_user_controlling_game()
{
    return game_state.user;
}






void set_game_state(short new_state)
{
    short old_state= game_state.state;

    switch(old_state)
    {
        case _game_in_progress:
            switch(new_state)
            {
                case _close_game:
                    finish_game(true);
                    break;
                    
                case _quit_game:
                    finish_game(false);
                    display_quit_screens();
                    break;
                    
                case _switch_demo:
                    /* Because Alain's code calls us at interrupt level 1, */
                    /*  we must defer processing of this message until idle */
                    game_state.state= _switch_demo;
                    game_state.ticks_until_next_screen= 0;
                    break;
                    
                case _revert_game:
                    /* Because reverting a game in the middle of the update_world loop sounds */
                    /*  sketchy, this is not done until idle time.. */
                    game_state.state= new_state;
                    game_state.ticks_until_next_screen= 0;
                    break;

                case _change_level:
                    game_state.state= new_state;
                    game_state.ticks_until_next_screen= 0;
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
            break;

        default:
            game_state.state= new_state;
            break;
    }
}


short get_game_state()
{
    return game_state.state;
}


bool current_netgame_allows_microphone()
{
    return game_state.current_netgame_allows_microphone;
}



void set_change_level_destination(short level_number)
{
    assert_fail(game_state.state == _change_level, "");
    game_state.current_screen = level_number;
}

short get_change_level_destination()
{
    return game_state.current_screen;
}






ao_err transfer_to_new_level(short level_number)
{
    ao_err err = no_err;

#if !defined(DISABLE_NETWORKING)
    // Only can transfer if NetUnSync returns true // TODO: which it always does, no?
    if (game_is_networked)
    {
        NetUnSync(); // TODO: wondering if this should return ao_err, e.g. STRID(gameError, errUnsyncOnLevelChange), but right now it doesn't (and there's a comment elsewhere it should never fail) so figure it out later
    }
#endif // !defined(DISABLE_NETWORKING)
    
    stop_fade();
    set_fade_effect(NONE);
    exit_screen(); // Enter_screen will be called again in start_game
    
    set_keyboard_controller_status(false);
    FindLevelMovie(level_number);
    show_movie(level_number);

    // if this is the M2_EPILOGUE_LEVEL_NUMBER, then it is time to get out of here already
    // (as we've just played the epilogue movie, we can move on to the _display_epilogue game state)
    if (level_number == (shapes_file_is_m1() ? M1_EPILOGUE_LEVEL_NUMBER : M2_EPILOGUE_LEVEL_NUMBER))
    {
        finish_game(false);
        show_cursor(); // for some reason, cursor stays hidden otherwise

        if (shell_options.replay_directory.empty()) { set_game_state(_begin_display_of_epilogue); }
        force_game_state_change();
    }
    else
    {
        if (!game_is_networked) try_and_display_chapter_screen(level_number, true, false);
        
        entry_point entry;
        entry.level_number = level_number;
        err = goto_level(&entry, dynamic_world->player_count, nullptr);
        set_keyboard_controller_status(true);
        
        if (err)
        {
            finish_game(true);
        }
        else
        {
            start_game(game_state.user, true);
        }
    }
    return err;
}



