

#include "main_event_loop.hpp"


#include "main_menu.hpp" // display_main_menu
#include "chapter_screens.hpp" //display_startup_screen

#include "setup_game.hpp"
#include "game_event_loop.hpp"

#include "about_ao_dialog.hpp"

#include "preferences.h"
#include "mouse.h" // show_cursor
#include "joystick.h" // AO_SCANCODE_JOYSTICK_ESCAPE

#include "shell_options.h" // shell_options.film_files

#include "vbl.h" // execute_timer_tasks

#include "interface.h"

#include "Music.h"
#include "QuickSave.h"
#include "Console.h"
#include "Statistics.h"

#include "MovieExporter.h"

#include "choose_file_dialogs_os.hpp"


#include "fades.h"
#include "SoundManager.h"
#include "Music.h"

/*
#include "player.h"
#include "network.h"
#include "screen_drawing.h"
#include "game_window.h"
#include "images.h"
#include "screen.h"
#include "preferences.h"
#include "DataFile.hpp"
#include "lua_script.h" // PostIdle
#include "XML_LevelScript.h"
#include "MovieExporter.h"
#include "Plugins.h"
#include "Statistics.h"
#include "shell_options.h"
#include "OpenALManager.h"
*/
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

//#include "motion_sensor.hpp" // for reset_motion_sensor() // this is also called in map_wad.cpp and, all over the place, really

#include "lua_hud_script.h"


#include "Canvas.hpp"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialog_widgets_sdl.h"


//#include "main_menu.hpp" // draw_main_menu_button




void update_interface() // TODO: where should this go?
{
    /*
    if (get_app_state() == app_state_t::main_menu)
    {
        if (game_state.highlighted_main_menu_item >= 0)
        {
            draw_main_menu_button_for_rect(game_state.highlighted_main_menu_item + START_OF_UI_RECTS - 1, true);
        }
    }
    */
    swap_screen_if_requested();
}



static bool is_running = true;


//*****************************************************************************
// process mouse click and keyboard inputs


static void process_ui_event(const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_MOUSEBUTTONDOWN:
        {
            if (get_app_state() == app_state_t::main_menu)
            {
                handle_main_menu_mouse_input(event);
            }
            else if (is_interstitial_screen())
            {
                force_app_state_timeout();
            }
            break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
        {
            joystick_button_pressed(event.cbutton.which, event.cbutton.button, true);
            // remap joystick/controller buttons to emulate keyboard keys; TODO: why?
            SDL_Event e2;
            memset(&e2, 0, sizeof(SDL_Event));
            e2.type = SDL_KEYDOWN;
            e2.key.keysym.sym = SDLK_UNKNOWN;
            e2.key.keysym.scancode = (SDL_Scancode)(AO_SCANCODE_BASE_JOYSTICK_BUTTON + event.cbutton.button);
            if (get_app_state() == app_state_t::main_menu)
            {
                handle_main_menu_controller_input(e2);
            }
            else if (is_interstitial_screen()) // pressing any key advances current screen; TO DO: check chapter/epilogue
            {
                force_app_state_timeout();
            }
            break;
        }
        case SDL_CONTROLLERBUTTONUP:
            joystick_button_pressed(event.cbutton.which, event.cbutton.button, false);
            break;
            
        case SDL_CONTROLLERAXISMOTION:
            joystick_axis_moved(event.caxis.which, event.caxis.axis, event.caxis.value);
            break;
            
        case SDL_JOYDEVICEADDED:
            joystick_added(event.jdevice.which);
            break;
            
        case SDL_JOYDEVICEREMOVED:
            break;
            
        case SDL_KEYDOWN:
            if (get_app_state() == app_state_t::main_menu)
            {
                handle_main_menu_keyboard_input(event);
            }
            else if (is_interstitial_screen()) // pressing any key advances current screen; TO DO: check chapter/epilogue
            {
                force_app_state_timeout();
            }
            break;
            
        case SDL_TEXTINPUT: // TO DO: can console be activated outside of game world? if not, get rid of this
            if (Console::instance()->input_active()) Console::instance()->textEvent(event);
            break;
            
        case SDL_QUIT:
            set_next_app_state(app_state_t::shutdown);
            
        case SDL_WINDOWEVENT:
            handle_window_event(event);
    }
}




/*

// these are used in app_state_t::game_in_progress case but should move to a transitional _game_is_starting state
extern bool first_frame_rendered;
float last_heartbeat_fraction = -1.f;
bool is_network_pregame = false;

bool idle_app_state(uint64_t time)
{
    auto machine_ticks_elapsed = time - game_state.last_ticks_on_idle;

    if(machine_ticks_elapsed || game_state.ticks_until_next_screen == 0)
    {
        if (game_state.ticks_until_next_screen != INFINITE_TIME_DELAY)
        {
            game_state.ticks_until_next_screen -= machine_ticks_elapsed;
        }
        
        // Note that we still go through this if we have an infinite phase.
        if (game_state.ticks_until_next_screen <= 0)
        {
            ao_err err = no_err;
            
            switch (get_app_state())
            {
                    // these are irrelevant since idle_game_state is called in game loop, not app loop
                case app_state_t::shutdown_screen:
                case app_state_t::startup_screen:
                case app_state_t::prologue_screen:
                case app_state_t::epilogue_screen:
                case app_state_t::credit_screen:
                    next_game_screen();
                    break;

               // case app_state_t::startup_screen_for_demo:
                case app_state_t::main_menu:
                    // Start the demo.
                    if(!environment_preferences.auto_play_demos || !create_new_game(_demo, false))
                    {
                        // This means that there was not a valid demo to play
                        game_state.ticks_until_next_screen= TICKS_UNTIL_DEMO_FILM_STARTS;
                    }
                    break;

                case app_state_t::exit_game:
                    display_main_menu();
                    break;

                case app_state_t::load_and_play_demo_film:
                    // This is deferred to the idle task because it occurs at interrupt time.
                    switch(game_state.user)
                    {
                        case _replay:
                            finish_game(true);
                            break;
                            
                        case _demo:
                            finish_game(false);
                            display_introduction_screen_for_demo();
                            break;
                            
                        default:
                            assert_fail(false, "");
                            break;
                    }
                    break;
                
                case app_state_t::chapter_screen:
                    //ao__dprintf__("Chapter heading...");
                    break;

                //case _quit_game:
                    // About to quit, but can still hit this through order of ops..
                //    break;

                case app_state_t::revert_to_saved_game:
                    // Reverting while in the update loop sounds sketchy.
                    err = revert_game();
                    if (!err)
                    {
                        set_app_state(app_state_t::game_in_progress);
                        game_state.ticks_until_next_screen = 15 * MACHINE_TICKS_PER_SECOND;
                        game_state.last_ticks_on_idle= machine_tick_count();
                        SoundManager::instance()->UpdateListener();
                        reset_motion_sensor(current_player_index);
                    }
                    else
                    {
                        display_loading_map_error(err);
                        finish_game(true);
                    }
                    break;
                    
                case app_state_t::epilogue_screen:
                    load_epilogue_screen();
                    break;

                case app_state_t::game_in_progress:
                    game_state.ticks_until_next_screen = 15 * MACHINE_TICKS_PER_SECOND;
                    //game_state.last_ticks_on_idle= machine_tick_count();
                    break;

                case app_state_t::change_level:
                case app_state_t::gather_network_game:
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
        }
        game_state.last_ticks_on_idle= machine_tick_count();
    }

    // if we’re not paused and there’s something to draw (i.e., anything different from last time), render a frame
    if (get_app_state() == app_state_t::game_in_progress)
    {
        // ZZZ change: update_world() whether or not is_vbl_reading_user_inputs() is true
        // This way we won't fill up queues and stall netgames if one player switches out for a bit.
        std::pair<bool, int16> theUpdateResult= update_world();
        short ticks_elapsed= theUpdateResult.second;
        bool redraw = false;

        if (is_vbl_reading_user_inputs())
        {
            // ZZZ: I don't know for sure that render_game_to_screen works best with the number of _real_
            // ticks elapsed rather than the number of (potentially predictive) ticks elapsed.
            // This is a guess.
            auto heartbeat_fraction = get_heartbeat_fraction();
            if (theUpdateResult.first || (last_heartbeat_fraction != -1 && last_heartbeat_fraction != heartbeat_fraction)) {
                last_heartbeat_fraction = heartbeat_fraction;
                render_game_to_screen(ticks_elapsed);
                first_frame_rendered = ticks_elapsed > 0;
                is_network_pregame = false;
            }
            else
                redraw = game_is_networked && is_network_pregame;
        }
        else
            redraw = true;

        if (redraw)
        {
            static uint64_t last_redraw = 0;
            if (current_player && machine_tick_count() > last_redraw + MACHINE_TICKS_PER_SECOND / 30)
            {
                last_redraw = machine_tick_count();
                render_game_to_screen(ticks_elapsed);
                if (ticks_elapsed) is_network_pregame = false;
            }
        }
        
        return theUpdateResult.first;
    }
    else
    {
        swap_screen_if_requested();
        update_interface_fades();
        return false;
    }
}
*/


void display_prologue_screen();

void enter_level();



//************************************************************************************************
// state transitions


// TODO: start new game, etc. called `force_system_colors(true)` so need to review once Fades are rearchitected properly


// called by main_event_loop() when it is time for the next transition
static ao_err transition_to_next_app_state()
{
    static app_state_t state_after_screen = app_state_t::undefined; // bit bodgy, but allows all screens to use the same display+advance states; once a screen sequence is exhausted, transition to this state
    
    ao_err err = no_err;
    
    app_state_t old_state = get_app_state();
    app_state_t new_state = advance_app_state();
    if (new_state == old_state) { return no_err; } // not sure about this; however, advance_app_state doesn't clear the next_state value so after advancing both vars will be the same until a new next state is set
    
    printf("transition_to_next_app_state: %i -> %i\n", old_state, new_state);
    
    switch (new_state)
    {
        case app_state_t::main_menu:
            // TODO: these will need disentangled later
            Plugins::instance()->set_mode(Plugins::kMode_Menu);
            
            // TSE loads the Credits music, which we don't want playing here; TODO: rejig Music class so credits music is separate to intro music; the bool flag could also be avoided by having intermediate state after startup screens, or by having music start on startup screen #N even when there's no image
#ifndef TSE
            // Q. which startup screen does it normally start playing? if it's the first, could move this to beginning of main_event_loop function (which also gets rid of the conditional)
            static bool can_main_menu_play_music = true; // TO DO: presumably false by default for M1 scenarios
            if (!Music::instance()->Playing() && can_main_menu_play_music) { Music::instance()->RestartIntroMusic(); } // TO DO: check startup screen behavior (but presumably skipping splash screens on shell shouldn't prevent music playing here)
            can_main_menu_play_music = false;
#endif
            set_next_app_state(app_state_t::load_and_play_demo_film, TICKS_UNTIL_DEMO_FILM_STARTS);
            display_main_menu();
            
            show_cursor();
            break;
            
            
            // WOULD YOU LIKE TO PLAY A GAME?
            
        case app_state_t::start_solo_game:
        {
            // TODO: create new game, then immediately save it to create current saved game file for reverting
            
            // TODO: for solo/coop/replay, always start from the saved game file? would simplify implementation
            
            configure_new_solo_game(0); // level 0
            
            //bool success = create_new_game(user_type_t::solo_player, 0, false);
            

            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            break;
        }
        case app_state_t::start_solo_game_choosing_level: // normally on a cheat key, but a scenario may assign it a main menu button
        {
            clear_screen();
            hide_cursor();
            int16_t level_number = display_vidmaster_dialog();
            show_cursor();
            clear_screen();
            
            if (level_number == NONE) // user canceled
            {
                set_next_app_state(app_state_t::main_menu);
            }
            else
            {
                configure_new_solo_game(level_number);
                
                //err = create_new_game(user_type_t::solo_player, 0, false);
                
                
                set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            }
            break;
        }
            
            
        case app_state_t::load_and_resume_saved_game:
        {
            //force_system_colors(false);
            show_cursor();
            
            ao_path path = display_load_saved_game_dialog();
            if (path.empty()) { err = errUserCanceled; }
           
            bool is_coop = false;
            if (!err)
            {
                dynamic_data dynamic_data;
                err = get_dynamic_data_from_saved_game_file(path, dynamic_data);
                if (!err && dynamic_data.player_count > 1)
                {
                    if (!display_restore_saved_game_as_coop_dialog(path, is_coop)) { err = errUserCanceled; }
                }
            }
            hide_cursor();
            
            if (!err)
            {
                set_user_type(is_coop ? user_type_t::network_player : user_type_t::solo_player);
                Plugins::instance()->set_mode(is_coop ? Plugins::kMode_Net : Plugins::kMode_Solo);
                
                err = load_game_from_file(path, false); // doesn't run map's MML scripts (why? can we [eventually] separate out scripts? so they always run after scenario files are all loaded? [depends on what scripts can/need to do to alter the loading process])

                
                // TODO: FIX
                err = load_and_start_game(); // ugh; should be load_saved_game_file(), then transition to enter_game
                

            }

            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            break;
        }
            
        
            // EES: implement case app_state_t::start_network_game for a unified MULTIPLAYER dialog
                        
        case app_state_t::gather_network_game:
        {            
            bool use_remote_hub;
            show_cursor();
            err = display_network_gather_dialog(false, use_remote_hub);
            hide_cursor();
            
            set_user_type(user_type_t::network_player);
            Plugins::instance()->set_mode(Plugins::kMode_Net);

            if (!err)
            {
                // order of operations is confusing
                
                if (!use_remote_hub) { NetStart(); } // EES: does anyone still play LAN matches? TODO: Architecturally it'd be simpler and cleaner if all net games used a Hub now, and just run the Hub server as a subprocess when gathering a LAN game. A true standalone server could be extended as a robust foundation for all package distribution (both netgame dependencies and solo scenarios).
                
                game_info* info = NetGetGameData(); // this gets level number
                configure_new_network_game(info);
                
                
                //create_new_game(user_type_t::network_player, 0, false);
            }
            
            
            
            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::await_network_game);
            
            break;
        }
            
            
        case app_state_t::join_network_game:
        {
            bool resume_coop_game;
            show_cursor();
            err = display_network_join_dialog(resume_coop_game);
            hide_cursor();
            
            set_user_type(user_type_t::network_player);
            Plugins::instance()->set_mode(Plugins::kMode_Net);

            
            if (resume_coop_game)
            {
                err = join_networked_resume_game(); // absolute fricking shambles
                if (err) { NetExit(); }
            }
            else
            {
                //err = create_new_game(user_type_t::network_player, false);
            }
            
            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::await_network_game);
            
            break;
        }
            
            
            
        case app_state_t::await_network_game:
            
            
            set_next_app_state(app_state_t::enter_game);
            
            break;
            
            
            
        case app_state_t::load_and_play_saved_film:
        {
            bool prompt_to_export = has_cheat_keys_modifier(SDL_GetModState()); // manky but redesign UI later
            
            show_cursor();
            ao_path film_file = display_read_saved_film_dialog();
            hide_cursor();
            
            if (film_file.empty()) // user canceled
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            if (!std::filesystem::is_regular_file(get_default_map_path()))
            {
                err = STRID(strERRORS, missingFile); // TODO: need specific error code so notify_user reports meaningful error message
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            if (prompt_to_export)
            {
                show_cursor();
                // TODO: default filename should be film's level name and timestamp
                ao_path dst_file = display_write_exported_film_dialog("Untitled Movie.webm");
                hide_cursor();
                
                if (dst_file.empty()) // user canceled
                {
                    set_next_app_state(app_state_t::main_menu);
                    break;
                }
                
                MovieExporter::instance()->StartRecording(dst_file); // ? should this be SetFile/Setup, with Start being called automatically on/prior to entering game? (it probably doesn't matter in terms of implementation since only screen frames explicitly sent to Exporter get written, but it does muddly the program flow); thing is, StartRecording immediately calls StopRecording (unless ALMgr is busted, in which case it silently fails [and leaves recording active?]; like I say, logic is a real mess)
            }

            
            err = setup_for_replay_from_file(film_file, get_current_map_checksum());
            
            // TODO: pretty sure there's more to do before entering game
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            
            break;
        }
            
            
        case app_state_t::load_and_play_dropped_films:
        {
            if (shell_options.film_files.empty())
            {
                warn_bug_report_f("No film files found (state %d)", new_state);
                set_next_app_state(app_state_t::main_menu);
            }
            else
            {
                ao_path path = shell_options.film_files.front();
                shell_options.film_files.erase(shell_options.film_files.begin());
                
                err = setup_for_replay_from_file(path, get_current_map_checksum());
                
                // TODO: pretty sure there's more to do before entering game
                set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            }
            break;
        }
            
            
        case app_state_t::load_and_play_demo_film:
        {
            ao_path path = get_random_demo_file();
            
            if (path.empty()) // didn't find a Demos/ directory or any film files in it
            {
                set_next_app_state(app_state_t::main_menu);
            }
            else
            {
                err = setup_for_replay_from_file(path, 0);
                
                // TODO: pretty sure there's more to do before entering game
                set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
                
            }
            break;
        }
            
        case app_state_t::load_and_play_last_film:
            // TODO: what we really want is to auto-save each (non-trivial) film file, integrated with quicksave, but that will require additional development
            
            break;
            
        case app_state_t::save_last_film:
            // TODO: finish
            break;
            
            
            // game transitions
            
        case app_state_t::enter_game:
            hide_cursor();
            
            // TODO: finish
            //enter_game();
            
            
            set_prediction_wanted(get_user_type() == user_type_t::network_player);
            
            set_next_app_state(app_state_t::game_in_progress);
            break;
            
        case app_state_t::game_in_progress:
            // TODO: anything that needs to be done here?
            game_event_loop(); // on return, the app's current state should already be set to one of the following (change_level/revert_to_saved_game/exit_game) // TODO: confirm this
            break;
            
        case app_state_t::change_level:
            
            // TODO: this needs work:
            
            err = load_level_from_map(get_next_level_number());
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
            }
            else
            {
                set_next_app_state(app_state_t::enter_game);
            }
            break;
            
        case app_state_t::revert_to_saved_game:
            // TODO: what needs to be done here?
            
            set_next_app_state(app_state_t::enter_game);
            break;
            
        case app_state_t::exit_game:
            // TODO: what needs to be done here?
            
            set_next_app_state(app_state_t::main_menu);
            break;
            
            
            // app transitions
            
        case app_state_t::preferences:
            show_cursor();
            display_main_preferences_dialog(); // blocks until done
            set_next_app_state(app_state_t::main_menu);
            break;
            
        case app_state_t::quit:
            hide_cursor();
            set_next_app_state(app_state_t::shutdown_screen);
            break;
            
        case app_state_t::credits:
            hide_cursor();
            set_next_app_state(app_state_t::credit_screen);
            break;
            
        case app_state_t::map_editor:
            show_cursor();
            
            TODO("enter map editor");
            
            break;
            
        case app_state_t::about_ao:
            show_cursor();
            display_about_ao_dialog();
            set_next_app_state(app_state_t::main_menu);
            break;
            
        case app_state_t::center: // M1 center button (Easter egg)
            set_next_app_state(app_state_t::main_menu);
            SoundManager::instance()->PlaySound(Sound_Center_Button(), 0, NONE);
            break;
            
            // interstitial screens
            
        case app_state_t::display_current_screen:
        {
            hide_cursor();
            uint32_t ticks_until_next_state = display_current_screen();
            set_next_app_state(app_state_t::advance_to_next_screen, ticks_until_next_state);
            break;
        }
        case app_state_t::advance_to_next_screen:
            err = advance_to_next_screen();
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::startup_screen:
            state_after_screen = app_state_t::main_menu;
            err = load_screen_sequence(app_state_t::startup_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::prologue_screen:
            state_after_screen = app_state_t::chapter_screen;
            err = load_screen_sequence(app_state_t::prologue_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::chapter_screen:
            state_after_screen = app_state_t::enter_game;
            err = load_screen_sequence(app_state_t::chapter_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::epilogue_screen:
            state_after_screen = app_state_t::main_menu;
            err = load_screen_sequence(app_state_t::epilogue_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::credit_screen:
            state_after_screen = app_state_t::main_menu;
            err = load_screen_sequence(app_state_t::credit_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
        case app_state_t::shutdown_screen:
            state_after_screen = app_state_t::shutdown;
            err = load_screen_sequence(app_state_t::shutdown_screen);
            set_next_app_state(err ? state_after_screen : app_state_t::display_current_screen);
            err = no_err;
            break;
            
            
            // Open the pod bay doors, please, HAL.
            
        case app_state_t::shutdown:
            show_cursor();
            StatsManager::instance()->Finish(); // utterly bizarrely, this was called in shutdown screens when there was no screen to show; TODO: if this hasn't finished uploading stats, it will display a blocking dialog that continues uploading stats till it's complete or user cancels
            hide_cursor();
            animate_ui_fade_out_blocking(true);
            is_running = false;
            break;
            
        default:
            throw_bug_report_f("Invalid app state transition: %d -> %d", get_app_state(), new_state);
    }
    
    return err;
}



    
    /*
    // MC shite
    switch (new_state)
    {
        case app_state_t::main_menu:
            / *
            // DEBUG: test new MultilineTextRenderer
            test_text_renderer();
            set_next_app_state(app_state_t::shutdown, 10000);
            return;
             * /
            show_cursor();
            goto_main_menu();
            // TO DO: where to put the following 3 lines?
        //#ifndef TROJAN_SE
            // TSE loads the Credits music, which we don't want playing here; TO DO: rejig Music class so credits music is separate to intro music; the bool flag could also be avoided by having intermediate state after startup screens, or by having music start on startup screen #N even when there's no image
            static bool can_main_menu_play_music = true; // TO DO: presumably false by default for M1 scenarios
            
            if (!Music::instance()->IsPlaying() && can_main_menu_play_music) Music::instance()->RestartIntroMusic(); // TO DO: check startup screen behavior (but presumably skipping splash screens on shell shouldn't prevent music playing here)
            can_main_menu_play_music = false;
            
        //#endif
            set_next_app_state(app_state_t::load_and_play_demo_film, TICKS_UNTIL_DEMO_FILM_STARTS);
            break;
            
        // TO DO: may need sStartMainMenu (with fade_ui_to_black) and sResumeMainMenu (without fade), if states which transition to main menu don't fade out themselves
            
        // ------- new/resume solo/co-op
            
        case app_state_t::new_solo_game:
        {
            set_level_number(0);
            reset_compatibility_version(); // enable all the newest features
            setup_solo_game();
            bool success = setup_new_game(); // TO DO: badly named function
            set_next_app_state(success ? app_state_t::enter_game : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::vidmaster_dialog:
        {
            show_cursor();
            short level_number;
            bool success = display_vidmaster_dialog(level_number); // blocks until done
            if (success)
            {
                set_level_number(level_number);
                reset_compatibility_version();
                setup_solo_game();
                success = setup_new_game(); // TO DO: badly named function
            }
            //clear_screen(); // TO DO: need to sort out who fades/clears to black
            set_next_app_state(success ? app_state_t::enter_game : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::choose_saved_game_dialog:
        {
            fade_ui_to_black(DO_NOT_FADE_MUSIC);
            bool success = display_load_game_dialog(); // on successful return, the saved game has been set in game_state.cpp // TO DO: issue is whether or not dialog has option for selecting external saved game files; if it's limited to quicksave files only then the main menu button should be disabled when no files exist
            set_next_app_state(success ? app_state_t::load_game_from_file : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::load_and_resume_saved_game: // shell.cpp also sets this state when a saved game file is drag-n-dropped/passed on command line; TO DO: what about restoring after player died?
        {
            hide_cursor();
            animate_ui_fade_out_blocking();
            ao_path path;
            bool success = display_load_saved_game_dialog(path); // if saved game is co-op, user will be prompted to resume as solo or co-op; TO DO: this might be problematic if launching from shell
            set_next_app_state(success ? app_state_t::enter_game : app_state_t::failed_game);
            break;
        }
            
        // ------- gather/join multiplayer

        case app_state_t::gather_net_game:
        {
            assert(false); // TO DO: Gather and Join buttons combine into Multiplayer button, always use remote hub; Q. use native dialog or open hub server homepage/user page in user's web browser?
            / *
            fade_ui_to_black();
            set_game_user(game_user_t::network_player);
            // TO DO: set_compatibility_version(???) - we need to find out the oldest version of AO in this netgame and set everyone's version to that
            setup_network_game();
            bool use_remote_hub;
            bool success = display_gather_network_game_dialog(false, use_remote_hub);
            if (success && !use_remote_hub) success = NetStart();
            set_next_app_state(success ? app_state_t::net_game_loading_screen : app_state_t::main_menu); // TO DO: why doesn't this go to app_state_t::failed_game on failure?
             * /
            break;
        }
            
        case app_state_t::join_net_game:
        {
            fade_ui_to_black();
            set_game_user(game_user_t::network_player);
            setup_network_game();
            bool success;
            network_join_result_t join_result = display_join_network_game_dialog();
            switch (join_result)// TO DO: this is a bit weird
            {
                case network_join_result_t::joined_new_game:
                    success = setup_new_game(); // TO DO: this...
                    break;
                case network_join_result_t::joined_resume_game:
                    success = join_network_game(); // TO DO: ...and this don't seem balanced behavior; need to straighten out gameworld setup so there is one code path for all new and resumed solo, co-op, netplay and film-replay games
                    break;
                default:
                    success = false;
            }
            // TO DO: set_compatibility_version(???) - we need to find out the oldest version of AO in this netgame and set everyone's version to that
            set_next_app_state(success ? app_state_t::net_game_loading_screen : app_state_t::failed_game);
            break;
        }
            
        case app_state_t::net_game_loading_screen:
            force_ui_to_black(false);
            DisplayNetLoadingScreen();
            //swap_main_window(); // TO DO: confirm DisplayNetLoadingScreen draws itself to screen
            set_next_app_state(app_state_t::wait_for_net_players_to_join);
            break;
            
        case app_state_t::wait_for_net_players_to_join:
            / * TO DO: FIX:
             
             There is some sort of wait state between displaying the screen and starting gameplay; my guess is while waiting for everyone to join the game world time does not advance, and once everyone is joined the game world time starts advancing automatically, at which point the pregame flag was cleared.
             
             Now that we have a proper FSM managing app states outside of the gameworld, we need to transition to a 'waiting for everyone to join' state, where it remains until the network code advances it to app_state_t::enter_game (or app_state_t::failed_game if something barfed/bailed/otherwise went wrong)
             
             Note: the following chunk of code for updating the wait screen was previously in game_event_loop() but [unless there's a fundamental need for the full game loop to be running while waiting] the wait screen should run its own minimal event loop, same as other dialogs and blocking screens already do:
             
                     else if (game_is_networked && is_network_pregame)
                     {
                         static auto last_redraw = 0;
                         if (current_player && machine_tick_count() > last_redraw + MACHINE_TICKS_PER_SECOND / 30)
                         {
                             last_redraw = machine_tick_count();
                             render_screen(ticks_elapsed);
                             if (ticks_elapsed) is_network_pregame = false;
                         }
                     }
            * /
            assert(false);
            set_next_app_state(app_state_t::enter_game);
            break;
            
        
        // ------- load film to replay
        
        case app_state_t::load_and_play_last_film:
        {
            FileSpecifier film_file; // film to replay
            bool success = get_recorded_film_spec(film_file);
            if (success) set_film_file(film_file);
            set_next_app_state(success ? app_state_t::play_film : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::load_and_play_saved_film:
        {
            fade_ui_to_black();
            FileSpecifier film_file; // film to replay
            bool success = display_load_film_dialog(film_file);
            if (success) set_film_file(film_file);
            set_next_app_state(success ? app_state_t::play_film : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::load_and_play_demo_film:
        {
            FileSpecifier film_file; // film to replay
            bool success = get_random_demo_film_spec(film_file);
            if (success) set_film_file(film_file);
            set_next_app_state(success ? app_state_t::play_film : app_state_t::main_menu);
            break;
        }
            
        case app_state_t::play_film:
        {
            bool success = setup_replay_game(); // TO DO: this doesn't work RN: for solo/co-op films we need to load the saved game, and that will be easiest once the new file format is implemented as we can put both saved game and film data in the same zipfile; no need to fuck about trying to write both into one file; in turn, the film file header becomes just another JSON manifest
            if (success) success = start_replay();
            set_next_app_state(success ? app_state_t::enter_game : app_state_t::main_menu);
            break;
        }
        
        // ------- enter editor // TO DO: WIP (it is not clear how AO puts itself into editing mode; presumably it's all done by plugin)
            
        case app_state_t::new_editor:
        {
            set_game_user(game_user_t::solo_player);
            // TO DO: what else needs set?
            fade_ui_to_black();
            set_next_app_state(app_state_t::enter_game);
            break;
        }
            
        // ------- enter game world
            
        case app_state_t::enter_game: // from New Game, Load Game, Gather/Join, Play Last/Saved/Demo Film, New Editor; precedes app_state_t::game_in_progress
        {
            assert(dynamic_world != NULL);
            
            hide_cursor();

            // This has already been done to get to gather/join // TO DO: sort this out: fades should ideally be performed at points of transition
            fade_ui_to_black();
            
            // TO DO: what about resuming from saved game? presumably that shouldn't re-display a chapter screen, so this line needs moved somewhere else
            if (get_game_user() != game_user_t::network_player) display_chapter_screen_for_current_level();
            
            if (get_game_user() != game_user_t::film_player)
            {
        //        new_film_for_game(); // TO DO: FIX: what if film is already recording from previous level? we want to pause it then resume when changing level, not start a new film (which breaks); yet another problem for solo films
        //        start_recording(); // TO DO: re-enable this
            }
            
            set_next_app_state(app_state_t::game_in_progress);

            // TO DO: what else should be done here?
            
            break;
        }
            
        case app_state_t::game_in_progress:
            game_event_loop(); // on return, the next transition is already set
            break;
            
        case app_state_t::revert_to_saved_game:
        {
            bool success = revert_game();
            // TO DO: user_alert() if failed?
            set_next_app_state(success ? app_state_t::enter_game : app_state_t::main_menu);
            break;
        }
        case app_state_t::change_level:
        {
            if (is_network_game()) NetUnSync();
            
            if (is_epilogue_level_number(get_level_number()))
            {
                finish_game();
                set_next_app_state(app_state_t::epilogue_screen);
            }
            else
            {
                bool success = change_level();
                
                // TO DO: user_alert() if failed? (presumably term text gave level index for non-existent level)
                set_next_app_state(success ? app_state_t::enter_game : app_state_t::exit_game);
            }
            break;
        }
            
        case app_state_t::exit_game:
            finish_game();
            set_next_app_state(app_state_t::main_menu);
            break;
            
        case app_state_t::failed_game: // was clean_up_after_failed_game() // TO DO: not sure if this should be function or state (a function can take error code as local arg; state needs it to be set somewhere)
            stop_recording();
            set_local_player_index(NONE);
            set_current_player_index(NONE);
            
            show_cursor();
            if (get_game_user() == game_user_t::network_player)
                NetExit(); // the network code displays its own errors // TO DO: presumably error dialog has already been displayed as NetExit doesn't appear to display errors itself
            else
                printf("TODO: display error");
            
            set_next_app_state(app_state_t::main_menu);
            break;
        
        // ------- dialogs and screens
            
        case app_state_t::save_last_film_dialog: // TO DO: this should move to films dialog
        {
            show_cursor();
            FileSpecifier dst_file;
            bool success = display_save_film_dialog(dst_file);
            if (success) success = save_film_to_file(dst_file) == 0;
            set_next_app_state(app_state_t::main_menu);
            break;
        }
     
     
        case app_state_t::credit_screen:
            // TO DO: how to generalize this so it displays TSE Credits if available or traditional M2 credits if not?
#ifdef TROJAN_SE
            hide_cursor();
            display_scrolling_credits(); // this blocks until fadeout is completed
            set_next_app_state(app_state_t::main_menu); // transition now
#else // original AO credits
            // M1 only has 1 credit screen, M2 allows up to 7; TO DO: if M1 doesn't use pict ids 1001...1007 then leave M2_NUMBER_OF_CREDIT_SCREENS as 7, else need to set it to M2_NUMBER_OF_CREDIT_SCREENS_M1
            goto_first_interstitial_screen(is_scenario_m1() ? CREDIT_SCREEN_BASE_M1 : CREDIT_SCREEN_BASE,
                                           M2_NUMBER_OF_CREDIT_SCREENS, CREDIT_SCREEN_DURATION, app_state_t::main_menu);
#endif
            break;
            
        case app_state_t::help_screen: // TROJAN
            hide_cursor();
            // TO DO: FIX: this doesn't fade out the first time Help button is pressed, but does fade out on subsequent presses; check this once fades are reinstated
            goto_first_interstitial_screen(HELP_SCREEN_BASE, NUMBER_OF_HELP_SCREENS, HELP_SCREEN_DURATION, app_state_t::main_menu);
            break;
            
    }
    */



//************************************************************************************************
// main event loop


// handles UI events for main menu and interstitial screens (dialogs, UI fades, and in-game world have their own event loops)
void main_event_loop()
{
    change_screen_mode(_screentype_menu); // mucky
    
    while (is_running)
    {
        SDL_Event event;
        while (SDL_WaitEventTimeout(&event, 30)) { process_ui_event(event); } // 30ms
       
        if (app_state_has_timed_out())
        {
            ao_err err = no_err;
            
            // TODO: also need try-catch block to handle any CPP exceptions that propagate this far (exceptions should always terminate process after reporting the problem, e.g. AO code bug, corrupted data file)
            err = transition_to_next_app_state();
            
            if (err && err != errUserCanceled)
            {
                // Reset the system colors, since the screen clut is all black
                force_system_colors(false);
                show_cursor();
                //display_loading_map_error(err); // TODO: move to caller
                
                notify_user(err);
                // TODO: if fatal error, need to exit process (preferably via shutdown)
            }
            
        }
        
        update_audio_on_idle();

        execute_timer_tasks(machine_tick_count());
        
       // idle_game_state(machine_tick_count()); // TODO: this is in game_event_loop now but need to check if there's any behaviors in it that ought to be here instead/as well

        static uint64_t next_redraw = 0;
        if (machine_tick_count() > next_redraw) // cap screen redraws at 30fps
        {
            update_interface();
            next_redraw = machine_tick_count() + TICKS_PER_SECOND / 30;
        }
    }
    
    show_cursor();
}






/*
void main_event_loop()
{
    uint64_t last_event_poll = 0;
    enum app_state_t app_state;

    while ((app_state = get_app_state()) != app_state_t::exit_game)
    {
        uint64_t cur_time = machine_tick_count();
        bool yield_time = false;
        bool poll_event = false;

        switch (app_state)
        {
            case app_state_t::game_in_progress:
            case app_state_t::change_level:
                game_event_loop();
                break;
                / *
                if ((get_fps_target() == 0 && is_vbl_reading_user_inputs()) || Console::instance()->input_active() || cur_time - last_event_poll >= TICKS_BETWEEN_EVENT_POLL)
                {
                    poll_event = true;
                    last_event_poll = cur_time;
                }
                else
                {
                    SDL_PumpEvents ();    // This ensures a responsive keyboard control
                }
                break;
                * /
                
                
            case app_state_t::startup_screen:
            case app_state_t::main_menu:
            case app_state_t::chapter_screen:
            case app_state_t::prologue_screen:
            case app_state_t::epilogue_screen:
            case app_state_t::start_epilogue:
            case app_state_t::credit_screen:
            case app_state_t::shutdown_screen:
            case app_state_t::start_network_game_gather:
                yield_time = interface_fade_finished();
                poll_event = true;
                break;

            //case _close_game:
            case app_state_t::load_demo_film:
            case app_state_t::revert_game:
                yield_time = poll_event = true;
                break;
        }

        if (poll_event)
        {
            update_audio_on_idle();

            SDL_Event event;
            if (yield_time)
            {
                // The game is not in a "hot" state, yield time to other
                // processes but only try for a maximum of 30ms
                if (SDL_WaitEventTimeout(&event, 30)) { process_event(event); }
            }

            while (SDL_PollEvent(&event)) { process_event(event); }

        }

        execute_timer_tasks(machine_tick_count());
        idle_game_state(machine_tick_count());

        static uint64_t last_redraw = 0U;
        if (machine_tick_count() > last_redraw + TICKS_PER_SECOND / 30)
        {
            update_interface();
            last_redraw = machine_tick_count();
        }
    }
}
*/
