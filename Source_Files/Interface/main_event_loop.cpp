/*
 main_event_loop.cpp
 
 Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
 and the "Aleph One" developers.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 This license is contained in the file "COPYING",
 which is included with this source code; it is available online at
 http://www.gnu.org/licenses/gpl.html
 */

// TODO: never hide the cursor when in windowed mode

// TODO: clean up #includes

#include "main_event_loop.hpp"

#include "app_state.hpp"
#include "main_menu.hpp" // display_main_menu
#include "about_ao_dialog.hpp"
#include "chapter_screens.hpp" //display_startup_screen
#include "choose_file_dialogs_os.hpp"

#include "setup_game.hpp"
#include "game_event_loop.hpp"

#include "preferences.h"

#include "mouse.h" // joystick_button_pressed
#include "joystick.h" // AO_SCANCODE_BASE_JOYSTICK_BUTTON

#include "shell_options.h" // shell_options.film_files

#include "vbl.h" // execute_timer_tasks
#include "FilmExporter.h"

#include "lua_script.h" // run_lua_scripts


#include "Music.h"
#include "QuickSave.h"
#include "Console.h"
#include "Statistics.h"




#include "fades.h"
#include "SoundManager.h"
#include "Music.h"


#include "InfoTree.h"

#include "alephversion.h"

// To tell it to stop playing, and also to run the end-game script
#include "XML_LevelScript.h"

#include "wad.h"
#include "map_wad.h"

//#include "lua_hud_script.h"



#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialog_widgets_sdl.h"


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
            if (get_app_state() == app_state_t::main_menu) // keys perform menu navigation+shortcuts
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
            switch (event.window.event)
            {
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    show_cursor();
                    // TODO: what about other screens? deleting these conditional tests might be tricky: we don't want to trigger a new, unintended, state transition by restarting timeout below
                    if (get_app_state() == app_state_t::main_menu) { suspend_app_state_timeout(); } // disable timeout until demo
                    break;
                    
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    // TODO: don't re-hide cursor without checking state first, as state _might've_ have changed since window focus was lost (depending on how robust, or not, our suspend timeout function is, as there is probably race potential between GUI events and state transitions, i.e. in testing, the cursor's gotten hidden in a way it doesn't reappear on main menu); one option is to define an auto_showhide_cursor function that switches on app_states but, for now, leave cursor visible
                    show_cursor();
                    if (get_app_state() == app_state_t::main_menu) { restart_app_state_timeout(); } // restart timeout until demo
                    break;
                
                default:
                {}
            }
    }
}


//************************************************************************************************
// state transitions


// TODO: start new game, etc. called `force_system_colors(true)` so need to review once Fades are rearchitected properly

// TODO: adopt standard convention for dialog calls: always return ao_err and pass back any values (e.g. level_number) via out args; if user cancels the dialog, it should return STRID(strERRORS,errUserCanceled) which may be ignored


// called by main_event_loop() when it is time for the next transition
static ao_err transition_to_next_app_state()
{
    static app_state_t state_after_screen = app_state_t::undefined; // bit bodgy, but allows all screens to use the same display+advance states; once a screen sequence is exhausted, transition to this state
    
    ao_err err = no_err; // TODO: make sure no_err/errUserCanceled is returned appropriately as any other error code will call notify_user
    
    app_state_t old_state = get_app_state();
    app_state_t new_state = advance_app_state();
    if (new_state == old_state) { return no_err; } // not sure about this; however, advance_app_state doesn't clear the next_state value so after advancing both vars will be the same until a new next state is set
    
    printf("transition_to_next_app_state: %i -> %i\n", old_state, new_state);
    
    main_screen.print_debug();
    
    
    switch (new_state)
    {
            // The Big Kahuna
            
        case app_state_t::main_menu:
            
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

            // TODO: modern main menu should have "Begin Story" (Solo/Co-op) and "Multiplayer" (PvP) buttons; presumably these will have new states

            // TODO: all of these cases should ensure that scenario files exist (if scenario files are missing, clicking a button should take user to scenario chooser first, then proceed as normal)
            
            // TODO: all cases that lead to a game must check for required scenario files first and prompt if any are missing (caveat tests/ must run headless); we don't want to disable New/Continue Game buttons as that doesn't match user expectations
            
        case app_state_t::start_new_campaign:
        {
            clear_game_configuration(); // shouldn't be necessary, but put it in for now

            configure_game_for_new_solo_campaign();
            set_next_app_state(app_state_t::load_new_solo_game);
            break;
        }
            
        case app_state_t::choose_vidmaster_level: // normally on a cheat key, but MML can assign it to a main menu button
        {
            clear_game_configuration();

            // get the level number to start on
            int16_t level_number;
            
            clear_screen();
            show_cursor();
            err = display_vidmaster_dialog(level_number);
            hide_cursor();
            clear_screen();
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            configure_game_for_new_solo_campaign(level_number);
            set_next_app_state(app_state_t::load_new_solo_game);
            break;
        }
            
        case app_state_t::load_new_solo_game: // configure_game_for_new_solo_campaign
        {
            assert_fail(!get_current_map_path().empty(), "");
            
            set_user_type(user_type_t::solo);
            
            int16_t level_number = get_game_configuration().initial_level_number;
            
            load_base_and_default_scripts(level_number); // loads all non-HUD(?) scripts defined as .mml/.lua files (I think) and in Map's 'text' resource #128
            err = load_level_from_map_wad_file(get_current_map_path(), level_number, false); // loads scripts defined in level's wad data
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            // once the level is loaded, the dynamic world must be initialized for a new game
            dynamic_world.initialize_for_new_game(get_game_configuration());
            // once the dynamic world is ready, initialize the player
            initialize_player_for_solo_game();
            
            // ZZZ: until film files store player behavior flags, all films recorded must use standard behavior. // TODO: FIX (this requires expanding action flags so states like run/swim and optional behaviors are encoded in film stream, plus extending film header to include scripts and other customizations)
            //record_game = is_player_behavior_standard(); // TODO: proper customization management
            
            // finish initializing the level; TODO: can these 3 calls move to enter_game?
            run_lua_scripts(); // run all the Lua scripts which were loaded above // ghs: this runs very early now: we want to be before initialize_items_and_monsters, and before MarkLuaCollections; EES: it would be nice to know why (e.g. so they can modify object placement frequencies before those objects are placed?)
            initialize_items_and_monsters();
            initialize_control_panels(); // set the initial states of all switches based on the objects they control
            
            set_next_app_state(app_state_t::enter_game);
            break;
        }
            
            // continue saved game
            
        case app_state_t::choose_saved_game:
        {
            clear_game_configuration();

            ao_path saved_game_path;
            
            clear_screen();
            show_cursor();
            err = display_load_saved_game_dialog(saved_game_path);
            hide_cursor();
            clear_screen();
            if (err) // user cancelled
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            err = configure_game_for_resumed_campaign(saved_game_path); // returns error if the file can't be read
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::load_saved_game);
            break;
        }
            
            
        case app_state_t::load_saved_game: // chosen via dialog above or drag-n-drop; configure_game_for_resumed_campaign must have been called first
        {
            clear_game_configuration();
            const ao_path saved_game_path = get_current_saved_game_path();
            
            // get the original Map file's checksum from saved game and
            err = set_current_map_path_to_file_with_checksum(read_wad_file_parent_checksum(saved_game_path));
            if (err) { return err; } // The original Map file wasn't found. The original M2 behavior was to continue playing the saved game file, then fail when exiting the level, but this is the right time to bail.
            if (err)
            {
                log_error_f("Can't find original Map for saved game: %s", saved_game_path.c_str()); // TODO: the error strings should have access to info like scenario file paths, so error reporting can generate this descriptive error message given error code only
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            bool is_coop = false;
            
            // get the saved game's level_number and player_count
            dynamic_world_t saved_dynamic_world;
            err = get_dynamic_data_from_saved_game_file(saved_game_path, saved_dynamic_world);
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            if (saved_dynamic_world.player_count > 1)
            {
                // TODO: it would be much better if the 'choose saved game' dialog showed which saves are solo and which are co-op, and provided an option to restore the latter as solo or co-op (presumably dragging-and-dropping a co-op saved game file onto app will still need to display this separate dialog)
                clear_screen();
                show_cursor();
                err = display_restore_saved_game_as_coop_dialog(saved_game_path, is_coop);
                hide_cursor();
                clear_screen();
                if (err)
                {
                    set_next_app_state(app_state_t::main_menu);
                    break;
                }
            }
            
            if (is_coop)
            {
                set_user_type(user_type_t::coop);
                
                clear_screen();
                show_cursor();
                err = display_network_gather_dialog(true); // TODO: what is current UI/UX for restoring co-op vs starting PvP and how can it be modernized/improved? (e.g. starting a co-op game should be done in Begin New Game and it'd be nice if an ongoing solo game could be converted to co-op at any time too - better for casual gaming)
                hide_cursor();
                clear_screen();
                
                set_next_app_state(err ? app_state_t::main_menu : app_state_t::await_network_game);
            }
            else //
            {
                set_user_type(user_type_t::solo);
                
                // some weirdness here: #128 scripts come from Map file while level-embedded scripts come from saved game; it may be an idea to move reading the level-embedded scripts out of initialize_level_from_wad_data and into load_base_and_default_scripts, which will be much easier to do once there's a nice WAD class for reading and writing Map and saved game files
                load_base_and_default_scripts(saved_dynamic_world.current_level_number);
                err = load_level_from_map_wad_file(saved_game_path, 0, false);
                if (err)
                {
                    set_next_app_state(app_state_t::main_menu);
                    break;
                }
                set_current_saved_game_path(saved_game_path); // Setup for a revert
                
                // the saved game's dynamic world is restored when level is loaded, so proceed to remaining initializations
                
                initialize_player_for_solo_game();
                
                // TODO: can these 3 calls move to enter_game[world]?
                run_lua_scripts();
                initialize_items_and_monsters();
                initialize_control_panels();
                
                set_next_app_state(app_state_t::enter_game);
            }
            /*
             // old crap to check and discard
             
            err = load_level_from_saved_game_file(saved_game_path); // doesn't run map's MML scripts (why? what about Restore? check against original code)
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
             
            // TODO: FIX
            // err = load_and_start_game(); // ugh; should be load_saved_game_file(), then transition to enter_game
            
            // TODO: review how AO sets up co-op game before sharing it
            // old comment: In co-op, this will be used only on the machine that picked "Continue Saved Game".
            //create_player_identities_for_new_solo_game();
            //match_starts_with_existing_players(); // herp...
            
            // was: make_restored_game_relevant
            run_lua_scripts();
            synchronize_restored_solo_player(); // ...de-derp
             
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
             */
            break;
        }
        
            // EES: implement case app_state_t::start_network_game for a unified MULTIPLAYER dialog
            
        case app_state_t::gather_network_game: // gather_pvp_game, I think
        {
            // EES: life's too short to deal with AO's complexity fetish, so let's assume everyone uses remote hub nowadays (if anyone wants to play over local network, they should spawn their own hub process)
            clear_screen();
            show_cursor();
            err = display_network_gather_dialog(false); // TODO: FIX: currently crashing as NetGetNetworkInterface is nullptr
            hide_cursor();
            clear_screen();
            if (err)
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            
            set_user_type(user_type_t::pvp);
            configure_new_pvp_game();
            
            set_next_app_state(app_state_t::await_network_game);
            break;
        }
            
            
        case app_state_t::join_network_game:
        {
            bool resume_coop_game;
            show_cursor();
            err = display_network_join_dialog(resume_coop_game);
            hide_cursor();
            
            if (resume_coop_game)
            {
                set_user_type(user_type_t::coop);
                set_next_app_state(app_state_t::join_resumed_coop_game);
            }
            else
            {
                set_user_type(user_type_t::pvp);
                //err = create_new_game();
            }
            if (err) { NetExit(); }

            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::await_network_game);
            
            break;
        }
            
        case app_state_t::join_resumed_coop_game:
        {
            uint8_t* flat_data;
            ao_err err = NetReceiveGameData(false, flat_data); // what's the deal with do_physics=false?
            if (err) return err;
            
            err = load_saved_game_from_flat_data(flat_data);
            if (err) return err;
            
            
        //    create_network_player_identities();
            //make_restored_game_relevant(true);
            run_lua_scripts();
            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::await_network_game);
            break;
        }
            
        case app_state_t::await_network_game:
            // TODO: netsync, lobby screen
            
            set_next_app_state(app_state_t::enter_game);
            
            
            // TODO: pulled this chunk out of render_screen in screen.cpp; it should be called by main event loop during the app_state_t::await_network_game state (see also is_network_pregame flag in the old code)
            /*
             if (game_is_networked && is_network_pregame)
             {
                 clear_screen(false);

                 Screen::instance()->bound_screen();
                 OGL_SetWindow(sr, sr, true);
                 DisplayNetLoadingScreen(MainScreenSurface());
                 OGL_SwapBuffers();
             }
             */


            break;
            
            
            // load saved games/films
            
        case app_state_t::load_and_play_saved_film:
        {
            bool prompt_to_export = has_cheat_keys_modifier(); // manky but redesign UI later so there's a proper 'Export' button in the films dialog
            
            // TODO: may be best if dialogs always returned ao_err, and other values via out args
            show_cursor();
            ao_path film_file;
            err = display_read_saved_film_dialog(film_file);
            hide_cursor();
            
            if (film_file.empty()) // user canceled
            {
                set_next_app_state(app_state_t::main_menu);
                break;
            }
            if (!std::filesystem::is_regular_file(get_scenario_map_path()))
            {
                err = STRID(strERRORS, missingFile); // TODO: need specific error codes (missingMapFile, missingShapesFile, etc) so notify_user reports meaningful error message
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
                
                // FilmExporter::instance()->StartExportingToFile(dst_file); // TODO: where should this be called automatically on/prior to entering game? (it probably doesn't matter in terms of implementation since only screen frames explicitly sent to Exporter get written, but it does muddly the program flow); thing is, StartExportingToFile immediately calls StopExporting (unless ALMgr is busted, in which case it silently fails [and leaves recording active?]; like I say, logic is a real mess
            }
            
            // problem: this may or may not load the level
            err = setup_for_replay_from_file(film_file);
            
            // TODO: pretty sure there's more to do before entering game
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
            
            break;
        }
            
            
        case app_state_t::load_and_play_dropped_film:
        {
            ao_path path = shell_options.pull_film_path();
            
            if (path.empty()) { throw_bug_report_f("No film files found (state %d)", new_state); }
            
            // TODO: setup
            
            err = setup_for_replay_from_file(path); // TODO: this is flawed: it may or may not load level
            
            
            set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
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
                err = setup_for_replay_from_file(path); // TODO: ditto
                
                // TODO: pretty sure there's more to do before entering game
                
                set_next_app_state(err ? app_state_t::main_menu : app_state_t::enter_game);
                
            }
            break;
        }
            
        case app_state_t::load_and_play_last_film:
            
            TODO("");
            // TODO: what we really want is to auto-save each (non-trivial) film file with quicksave
            
            break;
            
            
            // TODO: [non-trivial] last films should be automatically saved (both campaign and netmatch); if files are large we can add more aggressive auto-pruning but, in general, recent films should be available to user to review, playback, edit, export (a simple timeline editor would be lovely but, like map editor, will need to be a Lua plugin)
            
        case app_state_t::save_last_film:
        {
            //force_system_colors(false);
            show_cursor();
            
            ao_path src_path = get_recording_path();
            
            if (!src_path.empty())
            {
                // Ask user for destination file
                ao_path dst_path = display_write_saved_film_dialog(); // TODO: level name and timecode would be better default name (does recording header contain this info?)
                if (!dst_path.empty())
                {
                    ao_err err = rename_file(src_path, dst_path);
                    if (err)
                    {
                        notify_user(STRID(strERRORS, fileError), "Filesystem error $code$", {
                            {"$code$", [err]{ return std::to_string(err); }},
                            //{"$text$", [code]{ return code.message(); }}, // TODO: find out what error strings are and put them into strings
                        });
                    }
                }
            }
            
            hide_cursor();
            set_next_app_state(app_state_t::main_menu);
            break;
        }
            
            // game transitions
            
        case app_state_t::enter_game: // TODO: this state is probably redundant
            
            // TODO: do film setup here? or in an earlier state?
            
            // TODO: checking we have everything needs done early on, when user clicks New Game/Load Saved Game/etc
            if(!std::filesystem::is_regular_file(get_scenario_map_path()))
            {
                err = STRID(strERRORS, missingFile); // error codes should be more specific, e.g. mapFileNotFound
                break;
            }
            
            //hide_cursor();
            
            set_next_app_state(app_state_t::game_in_progress);
            break;
            
            
        case app_state_t::game_in_progress:
        {
           // if (get_user_type() == user_type_t::pvp)
           // {
           //     initialize_net_game(); // TODO: this should be in an earlier state (which one[s]?)
           // }

            // TODO: anything that needs to be done here?

            bool is_restoring_saved_game = false; // TODO: get this from app_state; it should be true when loading a saved game file/restoring coop game over network/restoring saved game embedded in film file
            
            game_event_loop(is_restoring_saved_game); // on return, the app's current state should already be set to one of the following (change_level/revert_to_saved_game/exit_game) // TODO: confirm this
                        
            break;
        }
            
            // temporarily/permanently exit game event loop
            
        case app_state_t::change_level:
        {
            int16_t level_number = get_next_level_number(); // the level to jump to (or epilogue number = end of game)
            bool can_show_chapter_screen = get_user_type() == user_type_t::solo; // TODO: what about co-op and film replay/export?
            
            if (level_number == get_epilogue_screen_number()) // teleported out of final level
            {
                finish_game();
                set_next_app_state(can_show_chapter_screen ? app_state_t::epilogue_screen : app_state_t::main_menu);
            }
            else // teleported to a new level
            {
                err = load_level(level_number, true); // TODO: 
                
                if (err)
                {
                    finish_game();
                    break;
                }

                sync_heartbeat_count(); // TODO: where is appropriate place for this? (it sets heartbeat count to dynamic_world.tick_count); given that load_level_ below will set dynamic_world, it needs to come before; but it smells - surely vbl's heartbeat count is current and correct and the tick_count in dynamic_world is stale (either 0 if new game or the saved game/film's stored heartbeat count); might be simplest if dynamic_world.pack_stream gets the value to store directly from vbl
                
                load_base_and_default_scripts(level_number); // let's put this here
                err = load_level_from_map_wad_file(get_current_map_path(), level_number, false);
                if (!err)
                {
                    set_next_app_state(can_show_chapter_screen ? app_state_t::chapter_screen : app_state_t::enter_game);
                }
            }
            break;
        }
            
        case app_state_t::revert_to_saved_game:
            // TODO: what needs to be done here?
            
            set_next_app_state(app_state_t::enter_game);
            break;
            
        case app_state_t::exit_game:
            // TODO: what needs to be done here? (gameworld cleanup must be done in game_event_loop); we must be able to transition from game_in_progress to revert_to_saved_game, change_level, (and, ideally, prefs dialog would be accessible in-game too); also map editor needs to toggle between 2D and 3D (unless the automap-based 2D editor is built inside gameworld too)
            
            //change_screen_mode(_screentype_menu);
            
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
            // this starts a solo game, but with texture editor hud and maybe other stuff activated
            
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
            // TODO: what about scenario movies? seems those should be their own state, playing before the screen if movie found; should probably separate show_movie into load_movie and play_movie functions
            
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
            
            // used by startup_screen, chapter_screen, etc
            
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


//************************************************************************************************
// main event loop


// handles UI events for main menu and interstitial screens (dialogs, UI fades, and in-game world have their own event loops)
void main_event_loop()
{
    //change_screen_mode(_screentype_menu); // mucky
    
    while (is_running)
    {
        SDL_Event event;
        while (SDL_WaitEventTimeout(&event, 30)) { process_ui_event(event); } // 30ms
       
        if (app_state_has_timed_out())
        {
            ao_err err = no_err;
            
            // TODO: also need try-catch block to handle any CPP exceptions that propagate this far (exceptions should always terminate process after reporting the problem, e.g. AO code bug, corrupted data file)
            err = transition_to_next_app_state();
            if (err != no_err && err != err_user_canceled)
            {
                // Reset the system colors, since the screen clut is all black
                //force_system_colors(false);
                show_cursor();
                notify_user(err);
                
                // TODO: assuming there's only 2 states which can follow an error, this should be sufficient; otoh, if there are cleanup states (e.g. for film recording) then
                set_next_app_state(get_app_state() == app_state_t::main_menu ? app_state_t::shutdown : app_state_t::main_menu);
                
                // TODO: if fatal error, need to exit process (preferably via shutdown), otherwise transition to...what? main_menu?
            }
            
        }
        
        update_audio_on_idle();
       
        execute_timer_tasks(machine_tick_count()); // TODO: 99% sure this is only needed in game_event_loop; confirm and, if correct about this, remove it from here
       // idle_game_state(machine_tick_count()); // TODO: this is in game_event_loop now but need to check if there's any behaviors in it that ought to be here instead/as well

        static uint64_t next_redraw = 0;
        if (machine_tick_count() >= next_redraw) // cap screen redraws at 30fps
        {
            main_screen.swap_if_needed();
            next_redraw = machine_tick_count() + TICKS_PER_SECOND / 30;
        }
    }
    
    show_cursor();
}

