

#include "game_event_loop.hpp"
//#include "game_window.h"
#include "movie_screen.hpp" // MML alternative to chapter screens

#include "setup_game.hpp"


#include "mouse.h" // hide_cursor
#include "joystick.h"

#include "map.h" // player_start_data, dynamic_world
#include "map_wad.h"
#include "preferences.h" // player_preferences
#include "player.h" // player_data
#include "motion_sensor.hpp" // reset_motion_sensor
#include "Plugins.h"
#include "XML_LevelScript.h" // ResetLevelScript
#include "lua_hud_script.h" // LoadHUDLua
#include "vbl.h" // set_recording_header_data
#include "choose_file_dialogs_os.hpp" // display_read_saved_film_dialog
#include "OpenALManager.h"
#include "MovieExporter.h"
#include "Music.h"
#include "shell_options.h"
#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialogs.h"
#include "game_window.h" // scroll_inventory


#include "image_blitter.hpp"



#define CLOSE_WITHOUT_WARNING_DELAY (5 * TICKS_PER_SECOND)


// TODO: need to ensure Cmd+Q input isn't handled by Cocoa as we don't want the app to quit on game key presses


//************************************************************************************************



static void pause_game()
{
    set_keyboard_controller_status(false);
    show_cursor();
    if (!game_is_networked && OpenALManager::Get()) OpenALManager::Get()->Pause(true);
}


static void resume_game()
{
    hide_cursor();
    if (ogl_is_active()) { alephone::Screen::instance()->bound_screen(true); }
    validate_world_window();
    set_keyboard_controller_status(true);
    if (OpenALManager::Get()) OpenALManager::Get()->Pause(false);
}




static bool confirm_exit_game() // Esc key pressed
{
    if (game_is_networked
        || PLAYER_IS_DEAD(local_player)
        || shell_options.should_output_to_file()
        || dynamic_world->tick_count - local_player->ticks_at_last_successful_save < CLOSE_WITHOUT_WARNING_DELAY) // don't prompt if game has just been saved
    {
        return true;
    }
    else
    {
        pause_game();
        show_cursor();
        bool confirm = display_quit_without_saving_dialog();
        hide_cursor();
        resume_game();
        
        return confirm;
    }
}


//************************************************************************************************
//

// called by main_event_loop() and game_event_loop()
void handle_window_event(const SDL_Event &event)
{
    switch (event.window.event)
    {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            if (!is_game_paused() && !MovieExporter::instance()->IsRecording())
            {
#ifdef TROJAN_SE
                darken_world_window();
#endif
                set_keyboard_controller_status(false);
                show_cursor();
            }
            // TO DO: what about other screens?
            if (get_app_state() == app_state_t::main_menu) // disable timeout until demo
            {
                advance_app_state_queuing_next(app_state_t::main_menu, INFINITE_TIME_DELAY);
            }
            break;
            
        case SDL_WINDOWEVENT_FOCUS_GAINED:
#ifdef __MACOSX__
            // work around Mojave (10.14) issue; TODO: still relevant? (depends on whether AO will still build for 10.13; fairly confident Xcode nowadays won't target <11.0) would've been nice if there'd been an explanation
            static bool is_first_window = true;
            if (is_first_window)
            {
                is_first_window = false;
                SDL_Window *win = SDL_GetWindowFromID(event.window.windowID);
                SDL_Window *w2 = SDL_CreateWindow("Loading", 0, 0, 100, 100, 0);
                SDL_RaiseWindow(w2);
                SDL_RaiseWindow(win);
                SDL_DestroyWindow(w2);
            }
#endif
            if (get_app_state() == app_state_t::main_menu) // restart timeout until demo
            {
                advance_app_state_queuing_next(app_state_t::load_and_play_demo_film, TICKS_UNTIL_DEMO_FILM_STARTS);
            }
            break;
        
        default:
        {}
    }
}


//************************************************************************************************



static void process_game_key(const SDL_Event &event)
{
    SDL_Keycode key = event.key.keysym.sym;
    SDL_Scancode code = event.key.keysym.scancode;
    bool changed_screen_mode = false;
    bool changed_prefs = false;
    bool changed_resolution = false;

    if (Console::instance()->input_active())
    {
        handle_console_key(event);
    }
    else
    {
        switch (code) // TODO: adopt TSE's F-keys (these are not rebindable and some should be available in UI as well as in-game)
        {
            case SDL_SCANCODE_ESCAPE:
            case AO_SCANCODE_JOYSTICK_ESCAPE:
                if (confirm_exit_game()) { advance_app_state_queuing_next(app_state_t::exit_game); }
                break;
                
               
                
            default:
            {} // TODO: rebindable keys should be a map of procs
        }

        // EES: how NOT to do key bindings map

        if (input_preferences->shell_key_bindings[_key_volume_up].count(code))
        {
            changed_prefs = SoundManager::instance()->AdjustVolumeUp(Sound_AdjustVolume());
        }
        else if (input_preferences->shell_key_bindings[_key_volume_down].count(code))
        {
            changed_prefs = SoundManager::instance()->AdjustVolumeDown(Sound_AdjustVolume());
        }
        else if (input_preferences->shell_key_bindings[_key_switch_view].count(code))
        {
            walk_player_list();
            render_game_to_screen(NONE);
        }
        else if (input_preferences->shell_key_bindings[_key_zoom_in].count(code))
        {
            if (zoom_overhead_map_in())
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (input_preferences->shell_key_bindings[_key_zoom_out].count(code))
        {
            if (zoom_overhead_map_out())
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (input_preferences->shell_key_bindings[_key_inventory_left].count(code))
        {
            if (game_is_live()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                scroll_inventory(-1);
            } else
                decrement_replay_speed(); // TODO: putting live inventory switching on the same keys as replay speed smells
        }
        else if (input_preferences->shell_key_bindings[_key_inventory_right].count(code))
        {
            if (game_is_live()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                scroll_inventory(1);
            } else
                increment_replay_speed();
        }
        else if (input_preferences->shell_key_bindings[_key_toggle_fps].count(code))
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            displaying_fps = !displaying_fps;
        }
        else if (input_preferences->shell_key_bindings[_key_activate_console].count(code))
        {
            if (game_is_networked) {
#if !defined(DISABLE_NETWORKING)
                Console::instance()->activate_input(InGameChatCallbacks::SendChatMessage, InGameChatCallbacks::prompt());
#endif
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            }
            else if (Console::instance()->use_lua_console())
            {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                Console::instance()->activate_input(ExecuteLuaString, ">");
            }
            else
            {
                PlayInterfaceButtonSound(Sound_ButtonFailure());
            }
        }
        else if (input_preferences->shell_key_bindings[_key_show_scores].count(code))
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            {
                extern bool ShowScores;
                ShowScores = !ShowScores;
            }
        }
        else if (code == SDL_SCANCODE_F1) // Decrease screen size
        {
            if (!graphics_preferences->screen_mode.hud)
            {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences->screen_mode.hud = true;
                changed_screen_mode = changed_prefs = true;
            }
            else
            {
                int mode = alephone::Screen::instance()->FindMode(get_screen_mode()->width, get_screen_mode()->height);
                if (mode < alephone::Screen::instance()->GetModes().size() - 1)
                {
                    PlayInterfaceButtonSound(Sound_ButtonSuccess());
                    graphics_preferences->screen_mode.width = alephone::Screen::instance()->ModeWidth(mode + 1);
                    graphics_preferences->screen_mode.height = alephone::Screen::instance()->ModeHeight(mode + 1);
                    graphics_preferences->screen_mode.auto_resolution = false;
                    graphics_preferences->screen_mode.hud = false;
                    changed_screen_mode = changed_prefs = changed_resolution = true;
                } else
                    PlayInterfaceButtonSound(Sound_ButtonFailure());
            }
        }
        else if (code == SDL_SCANCODE_F2) // Increase screen size
        {
            if (graphics_preferences->screen_mode.hud)
            {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences->screen_mode.hud = false;
                changed_screen_mode = changed_prefs = true;
            }
            else
            {
                int mode = alephone::Screen::instance()->FindMode(get_screen_mode()->width, get_screen_mode()->height);
                int automode = get_screen_mode()->fullscreen ? 0 : 1;
                if (mode > automode)
                {
                    PlayInterfaceButtonSound(Sound_ButtonSuccess());
                    graphics_preferences->screen_mode.width = alephone::Screen::instance()->ModeWidth(mode - 1);
                    graphics_preferences->screen_mode.height = alephone::Screen::instance()->ModeHeight(mode - 1);
                    if ((mode - 1) == automode)
                        graphics_preferences->screen_mode.auto_resolution = true;
                    graphics_preferences->screen_mode.hud = true;
                    changed_screen_mode = changed_prefs = changed_resolution = true;
                } else
                    PlayInterfaceButtonSound(Sound_ButtonFailure());
            }
        }
        else if (code == SDL_SCANCODE_F3) // Resolution toggle
        {
            if (!ogl_is_active()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                if (graphics_preferences->screen_mode.high_resolution) {
                    graphics_preferences->screen_mode.high_resolution = false;
                    graphics_preferences->screen_mode.draw_every_other_line = false;
                } else if (!graphics_preferences->screen_mode.draw_every_other_line) {
                    graphics_preferences->screen_mode.draw_every_other_line = true;
                } else {
                    graphics_preferences->screen_mode.high_resolution = true;
                    graphics_preferences->screen_mode.draw_every_other_line = false;
                }
                changed_screen_mode = changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (code == SDL_SCANCODE_F4)        // Reset OpenGL textures
        {
#ifdef HAVE_OPENGL
            if (ogl_is_active()) {
                // Play the button sound in advance to get the full effect of the sound
                PlayInterfaceButtonSound(Sound_OGL_Reset());
                OGL_ResetTextures();
            } else
#endif
                PlayInterfaceButtonSound(Sound_ButtonInoperative());
        }
        else if (code == SDL_SCANCODE_F5) // Make the chase cam switch sides
        {
            if (ChaseCam_IsActive())
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            else
                PlayInterfaceButtonSound(Sound_ButtonInoperative());
            ChaseCam_SwitchSides();
        }
        else if (code == SDL_SCANCODE_F6) // Toggle the chase cam
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            ChaseCam_SetActive(!ChaseCam_IsActive());
        }
        else if (code == SDL_SCANCODE_F7) // Toggle zoom (EES: this is an AO-specific feature that should not be on a standard key)
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            set_zoom_is_enabled(!get_zoom_is_enabled());
        }
        else if (code == SDL_SCANCODE_F8) // Toggle the crosshairs
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            player_preferences->crosshairs_active = !player_preferences->crosshairs_active;
            Crosshairs_SetActive(player_preferences->crosshairs_active);
            changed_prefs = true;
        }
        else if (code == SDL_SCANCODE_F9) // Screen dump
        {
            dump_screen();
        }
        else if (code == SDL_SCANCODE_F10) // Toggle the position display
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            {
                extern bool ShowPosition;
                ShowPosition = !ShowPosition;
            }
        }
        else if (code == SDL_SCANCODE_F11
#ifdef HAVE_STEAM
                 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
                 ) // Decrease gamma level
        {
            if (graphics_preferences->screen_mode.gamma_level) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences->screen_mode.gamma_level--;
                change_gamma_level(graphics_preferences->screen_mode.gamma_level);
                changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (code == SDL_SCANCODE_F12
#ifdef HAVE_STEAM
                 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
                 ) // Increase gamma level
        {
            if (graphics_preferences->screen_mode.gamma_level < NUMBER_OF_GAMMA_LEVELS - 1) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences->screen_mode.gamma_level++;
                change_gamma_level(graphics_preferences->screen_mode.gamma_level);
                changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else
        {
            if (game_is_replay()) advance_app_state_queuing_next(app_state_t::exit_game); // TODO: FIX: smelly
        }
    }
    
    if (changed_screen_mode)
    {
        screen_mode_data temp_screen_mode = graphics_preferences->screen_mode;
        temp_screen_mode.fullscreen = get_screen_mode()->fullscreen;
        change_screen_mode(&temp_screen_mode, true, changed_resolution);
        render_game_to_screen(0);
    }

    if (changed_prefs)
        write_preferences();
}






static void process_event(const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_MOUSEMOTION:
            mouse_moved(event.motion.xrel, event.motion.yrel);
            break;
        case SDL_MOUSEWHEEL:
        {
            bool up = (event.wheel.y > 0);
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { up = !up; } // SDL2.0.4+
            mouse_scroll(up);
        }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (is_vbl_reading_user_inputs())
            {
                SDL_Event e2;
                memset(&e2, 0, sizeof(SDL_Event));
                e2.type = SDL_KEYDOWN;
                e2.key.keysym.sym = SDLK_UNKNOWN;
                e2.key.keysym.scancode = (SDL_Scancode)(AO_SCANCODE_BASE_MOUSE_BUTTON + event.button.button - 1);
                process_game_key(e2);
            }
            else
            {
                resume_game();
            }
            break;
            
        case SDL_CONTROLLERBUTTONDOWN:
            if (is_vbl_reading_user_inputs())
            {
                joystick_button_pressed(event.cbutton.which, event.cbutton.button, true);
                SDL_Event e2;
                memset(&e2, 0, sizeof(SDL_Event));
                e2.type = SDL_KEYDOWN;
                e2.key.keysym.sym = SDLK_UNKNOWN;
                e2.key.keysym.scancode = (SDL_Scancode)(AO_SCANCODE_BASE_JOYSTICK_BUTTON + event.cbutton.button);
                process_game_key(e2);
            }
            else
            {
                resume_game();
            }
            break;
            
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
        {
            bool success = joystick_removed(event.jdevice.which);
            if (success) { pause_game(); }
            break;
        }
        case SDL_KEYDOWN:
            process_game_key(event);
            break;
            
        case SDL_TEXTINPUT:
            if (Console::instance()->input_active()) { Console::instance()->textEvent(event); }
            break;
            
        case SDL_QUIT:
            advance_app_state_queuing_next(app_state_t::exit_game);
            break;
            
        case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    if (is_vbl_reading_user_inputs() && !MovieExporter::instance()->IsRecording() && shell_options.replay_directory.empty())
                    {
                        pause_game();
                    }
                    set_app_focus_lost();
                    break;
                    
                case SDL_WINDOWEVENT_FOCUS_GAINED:
#ifdef __MACOSX__
                    // work around Mojave issue // EES: ...which is?
                    static bool gFirstWindow = true;
                    if (gFirstWindow)
                    {
                        gFirstWindow = false;
                        SDL_Window *win = SDL_GetWindowFromID(event.window.windowID);
                        if (!ogl_is_active() && (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP))
                        {
                            SDL_SetWindowFullscreen(win, 0);
                            SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                        else
                        {
                            SDL_Window *w2 = SDL_CreateWindow("Loading", 0, 0, 100, 100, 0);
                            SDL_RaiseWindow(w2);
                            SDL_RaiseWindow(win);
                            SDL_DestroyWindow(w2);
                        }
                    }
#endif
                    set_app_focus_gained();
                    break;
            }
            break;
    }
    
}






//************************************************************************************************
// event loop


// these are used in app_state_t::game_in_progress case but should move to a transitional _game_is_starting state
extern bool first_frame_rendered;
float last_heartbeat_fraction = -1.f;
bool is_network_pregame = false;




static bool idle_game_state(uint64_t time)
{
    // TODO: check this is replicated correctly
    /*
    auto machine_ticks_elapsed = time - game_state.last_ticks_on_idle;

    if (machine_ticks_elapsed || game_state.ticks_until_next_screen==0)
    {
        if(game_state.ticks_until_next_screen != INFINITE_TIME_DELAY)
        {
            game_state.ticks_until_next_screen-= machine_ticks_elapsed;
        }
        
        // Note that we still go through this if we have an indefinate phase.
        if (game_state.ticks_until_next_screen <= 0)
        {
            game_state.ticks_until_next_screen = 15 * MACHINE_TICKS_PER_SECOND;
            //game_state.last_ticks_on_idle= machine_tick_count();

        }
        game_state.last_ticks_on_idle = machine_tick_count();
    }
    */
    // if we’re not paused and there’s something to draw (i.e., anything different from last time), render a frame
    
    // ZZZ change: update_world() whether or not is_vbl_reading_user_inputs() is true
    // This way we won't fill up queues and stall netgames if one player switches out for a bit.
    std::pair<bool, int16> theUpdateResult = update_world();
    short ticks_elapsed= theUpdateResult.second;
    bool redraw = false;

    if (is_vbl_reading_user_inputs()) // we are reading keyboard inputs, so presumably this means live game is running
    {
        // ZZZ: I don't know for sure that render_game_to_screen works best with the number of _real_
        // ticks elapsed rather than the number of (potentially predictive) ticks elapsed.
        // This is a guess.
        auto heartbeat_fraction = get_heartbeat_fraction();
        if (theUpdateResult.first || (last_heartbeat_fraction != -1 && last_heartbeat_fraction != heartbeat_fraction)) {
            last_heartbeat_fraction = heartbeat_fraction;
            
            render_game_to_screen(ticks_elapsed);
            
            first_frame_rendered = ticks_elapsed > 0;
            //is_network_pregame = false;
        }
        //else
        //    redraw = game_is_networked && is_network_pregame;
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




void game_event_loop()
{
    uint64_t last_event_poll = 0;

    while (get_app_state() == app_state_t::game_in_progress)
    {
        uint64_t cur_time = machine_tick_count();
        bool yield_time = false;
        
        if ((get_fps_target() == 0 && is_vbl_reading_user_inputs())
            || Console::instance()->input_active() || cur_time - last_event_poll >= TICKS_BETWEEN_EVENT_POLL)
        {
            // poll event now
            last_event_poll = cur_time;
            
            update_audio_on_idle();
            
            SDL_Event event;
            
            // The app is not in a "hot" state so yield time to other processes but only try for a maximum of 30ms
            // if (SDL_WaitEventTimeout(&event, 30)) { process_event(event); }
            
            while (SDL_PollEvent(&event)) { process_event(event); }
            
#ifdef HAVE_STEAM
            while (auto steam_event = STEAMSHIM_pump())
            {
                if (steam_event->type == SHIMEVENT_IS_OVERLAY_ACTIVATED && steam_event->okay
                    && get_app_state() == app_state_t::game_in_progress && !game_is_networked)
                {
                    pause_game();
                }
            }
#endif
            
        }
        else
        {
            SDL_PumpEvents(); // This ensures a responsive keyboard control
        }

        execute_timer_tasks(machine_tick_count());
        idle_game_state(machine_tick_count());
        
        int16_t fps_target = get_fps_target();
        
        if (fps_target != FPS_UNLIMITED)
        {
            uint64_t elapsed_machine_ticks = machine_tick_count() - cur_time;
            uint64_t desired_elapsed_machine_ticks = MACHINE_TICKS_PER_SECOND / fps_target;
            
            if (desired_elapsed_machine_ticks - elapsed_machine_ticks > desired_elapsed_machine_ticks / 3)
            {
                sleep_for_machine_ticks(1);
            }
        }
    }
}


/* TODO: here are the game_in_progress->STATE transitions for leaving the game loop, as copied (obviously this code isn't valid; this is just here for reference while straightening out)
 
    switch(old_state)
    {
        case app_state_t::game_in_progress:
            switch(new_state)
            {
                case app_state_t::exit_game:
                    finish_game(true);
                    break;
                    
                case app_state_t::exit_game: // actually exit app (but how can the user go directly from inside the gameworld to terminating the process? it smells)
                    finish_game(false);
                    display_shutdown_screen();
                    break;
                    
                case app_state_t::load_demo_film: // presumably switching from end of one demo film immediately to start of another (without going back to main menu for a 30-second wait)
                    // Because Alain's code calls us at interrupt level 1,  we must defer processing of this message until idle
                     current_state= app_state_t::load_and_play_demo_film;
                    game_state.ticks_until_next_screen= 0;
                    break;
                    
                case app_state_t::revert_game:
                    // Because reverting a game in the middle of the update_world loop sounds sketchy, this is not done until idle time
                     current_state= new_state;
                    game_state.ticks_until_next_screen= 0;
                    break;

                case app_state_t::change_level:
                     current_state= new_state;
                    game_state.ticks_until_next_screen= 0;
                    break;
            }
            break;

        default:
             current_state= new_state;
            break;
    }
*/


