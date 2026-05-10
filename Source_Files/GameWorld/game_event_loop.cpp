

// TODO: clean up includes
#include "game_event_loop.hpp"
#include "movie_screen.hpp" // MML alternative to chapter screens

#include "setup_game.hpp"

#include "gameworld_entrance.hpp"

#include "mouse.h"
#include "joystick.h"

#include "ChaseCam.h" // ChaseCam_IsActive/_SwitchSides

#include "map.h" // dynamic_world
#include "map_wad.h"
#include "preferences.hpp" // player_preferences
#include "player.h" // Player
#include "Plugins.h"
#include "vbl.h" // set_keyboard_controller_status
#include "choose_file_dialogs_os.hpp" // display_read_saved_film_dialog
#include "OpenALManager.h"
#include "FilmExporter.h"
#include "Music.h"
#include "shell_options.h"
#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialogs.h"
#include "hud_manager.h" // scroll_inventory
#include "Screen.hpp" // darken_world_window

#include "lua_script.h" // ExecuteLuaString
#include "visual_effects.hpp" // NUMBER_OF_GAMMA_LEVELS

//#include "ImageBlitter.hpp"

#include "camera.hpp" // zoom_overhead_map_in



#define CLOSE_WITHOUT_WARNING_DELAY (5 * TICKS_PER_SECOND)


// TODO: need to ensure Cmd+Q input isn't handled by Cocoa as we don't want the app to quit on game key presses

// set by game_event_loop and exit_game_event_loop
bool is_running = false;

// these are used in idle_game_state below
extern bool first_frame_rendered; // TODO: yuck; entangled in vbl.cpp and marathon2.cpp; what is it actually doing?
float last_heartbeat_fraction = -1.f; // also in marathon2.cpp, lua_hud_objects.cpp


bool game_loop_is_running()
{
    return is_running;
}


//************************************************************************************************


static void pause_game()
{
    darken_world_window(); // TODO: FIX: IIRC we need to darken the current frame buffer then copy it to FBO for reuse; the dialog widget can then redraw the darkened gameworld screen before drawing the dialog box on top of it
    set_keyboard_controller_status(false);
    show_cursor();
    if (!game_is_networked() && OpenALManager::Get()) OpenALManager::Get()->Pause(true);
}


static void resume_game()
{
    hide_cursor();
    
    //validate_world_window(); // TODO: this just called RequestDrawingTerm; confirm that's no longer needed
    set_keyboard_controller_status(get_user_type() != user_type_t::replay); // TODO: since film replay doesn't pause, just exits, it shouldn't cause a problem always passing `true` here, but this makes the reasoning explicit
    if (OpenALManager::Get()) OpenALManager::Get()->Pause(false);
}


static bool confirm_exit_game() // Esc key pressed; displays a confirmation dialog if there's unsaved game state that will be lost
{
    if (game_is_networked()
        || PLAYER_IS_DEAD(local_player)
        || shell_options.should_output_to_file()
        || dynamic_world.tick_count - local_player->ticks_at_last_successful_save < CLOSE_WITHOUT_WARNING_DELAY) // don't prompt if game has just been saved
    {
        return true;
    }
    else
    {
        // TODO: FIX: use TSE behavior: dim gameworld view and display the dialog centered over it
        pause_game();
        show_cursor();
        bool confirm = display_confirm_exit_game_dialog();
        hide_cursor();
        resume_game();
        
        return confirm;
    }
}


//************************************************************************************************



static void process_game_key(const SDL_Event &event)
{
    //SDL_Keycode key = event.key.keysym.sym;
    SDL_Scancode code = event.key.keysym.scancode;
    //bool changed_screen_mode = false;
    bool changed_prefs = false;
    //bool changed_resolution = false;

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
                if (confirm_exit_game()) { exit_game_event_loop(app_state_t::exit_game); }
                break;
                
               
                
            default:
            {} // TODO: rebindable keys should be a map of procs
        }

        // EES: how NOT to do key bindings map

        if (input_preferences.shell_key_bindings[_key_volume_up].count(code))
        {
            changed_prefs = sound_manager.increase_volume();
        }
        else if (input_preferences.shell_key_bindings[_key_volume_down].count(code))
        {
            changed_prefs = sound_manager.decrease_volume();
        }
        else if (input_preferences.shell_key_bindings[_key_switch_view].count(code))
        {
            change_view_to_next_player();
            render_game_to_screen(NONE);
        }
        else if (input_preferences.shell_key_bindings[_key_zoom_in].count(code))
        {
            if (increase_automap_size())
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (input_preferences.shell_key_bindings[_key_zoom_out].count(code))
        {
            if (decrease_automap_size())
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
            else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }
        else if (input_preferences.shell_key_bindings[_key_inventory_left].count(code))
        {
            if (game_is_live()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                scroll_inventory(-1);
            } else
                decrement_replay_speed(); // TODO: putting live inventory switching on the same keys as replay speed smells
        }
        else if (input_preferences.shell_key_bindings[_key_inventory_right].count(code))
        {
            if (game_is_live()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                scroll_inventory(1);
            } else
                increment_replay_speed();
        }
        else if (input_preferences.shell_key_bindings[_key_toggle_fps].count(code))
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            graphics_preferences.show_fps = !graphics_preferences.show_fps;
        }
        
        
        // TODO: this is kinda weird (surprise!)
        else if (input_preferences.shell_key_bindings[_key_activate_console].count(code))
        {
            if (game_is_networked())
            {
                Console::instance()->activate_input(InGameChatCallbacks::SendChatMessage, InGameChatCallbacks::prompt());
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
        
        
        // TODO: put on inventory?
        else if (input_preferences.shell_key_bindings[_key_show_scores].count(code))
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            {
                extern bool ShowScores;
                ShowScores = !ShowScores;
            }
        }
        
        
        
        
        /*
         F-keys (leftmost = our highest priority)
         
         screen mode // we want users to experiment with Classic and Modern modes
         volume // common user need
         gamma // gamma and hud size are not so high priority
         HUD size (including terminal?); it's up to individual HUD plugins to do what they want with the notification
         
         Console
         
         Preferences dialog // fullscreen/windowed no longer has its own key (unless user/scenario assigns hotkey)
         
         F11/12 must be screenshot (Steam compatibility)

         */
        
        
        else if (code == SDL_SCANCODE_F1) // Decrease screen size
        {
            bool success = main_screen.decrease_mode();
            PlayInterfaceButtonSound(success ? Sound_ButtonSuccess() : Sound_ButtonFailure());
        }
        else if (code == SDL_SCANCODE_F2) // Increase screen size
        {
            bool success = main_screen.increase_mode();
            PlayInterfaceButtonSound(success ? Sound_ButtonSuccess() : Sound_ButtonFailure());
        }
        
        /*
        else if (code == SDL_SCANCODE_F3) // Resolution toggle
        {
            if (!ogl_is_active()) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                if (graphics_preferences.screen_mode.high_resolution) {
                    graphics_preferences.screen_mode.high_resolution = false;
                } else {
                    graphics_preferences.screen_mode.high_resolution = true;
                }
                changed_screen_mode = changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
        }

         // TODO: chasecam, zoom, crosshairs should be hotkey options
         
         
        else if (code == SDL_SCANCODE_F4)        // Reset OpenGL textures
        {
            // Play the button sound in advance to get the full effect of the sound
            //PlayInterfaceButtonSound(Sound_OGL_Reset());
            //OGL_ResetTextures();
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
            if (zoom_is_active())
                deactivate_zoom_vision();
            else
                activate_zoom_vision();
        }
        else if (code == SDL_SCANCODE_F8) // Toggle the crosshairs
        {
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            player_preferences.crosshairs_active = !player_preferences.crosshairs_active;
            set_crosshairs_is_visible(player_preferences.crosshairs_active);
            changed_prefs = true;
        }
        */
        else if (code == SDL_SCANCODE_F9) // Screen dump // TODO: move to F11/F12
        {
            dump_screen();
        }
        
        
        
        
        else if (code == SDL_SCANCODE_F10) // Toggle the position display // TODO: move to Console
        {
            /*
            PlayInterfaceButtonSound(Sound_ButtonSuccess());
            {
                extern bool ShowPosition;
                ShowPosition = !ShowPosition;
            }
             */
        }
        else if (code == SDL_SCANCODE_F11
#ifdef HAVE_STEAM
                 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
                 ) // Decrease gamma level
        {
            /*
            if (graphics_preferences.gamma_level)
            {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences.gamma_level--;
                set_gameworld_gamma(graphics_preferences.gamma_level);
                changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
             */
        }
        else if (code == SDL_SCANCODE_F12
#ifdef HAVE_STEAM
                 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
                 ) // Increase gamma level
        {
            /*
            if (graphics_preferences.gamma_level < NUMBER_OF_GAMMA_LEVELS - 1) {
                PlayInterfaceButtonSound(Sound_ButtonSuccess());
                graphics_preferences.gamma_level++;
                set_gameworld_gamma(graphics_preferences.gamma_level);
                changed_prefs = true;
            } else
                PlayInterfaceButtonSound(Sound_ButtonFailure());
             */
        }
        else
        {
            if (game_is_replay()) { exit_game_event_loop(app_state_t::exit_game); } // so pressing any key causes film replay to stop, yes? TODO: FIX: this won't work as game event loop doesn't do transitions; need `leave_game_in_progress(next_state)`
        }
    }
    /*
    if (changed_screen_mode)
    {
        screen_mode_data temp_screen_mode = graphics_preferences.screen_mode;
        temp_screen_mode.fullscreen = screen_mode.fullscreen;
        change_screen_mode(&temp_screen_mode, true, changed_resolution);
        render_game_to_screen(0);
    }
    if (changed_prefs)
        write_preferences();
    */
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
            exit_game_event_loop(app_state_t::exit_game);
            break;
            
        case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    if (is_vbl_reading_user_inputs() && !FilmExporter::instance()->IsExporting() && shell_options.replay_directory.empty())
                    {
                        pause_game();
                    }
                    break;
                    
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    resume_game();
                    break;
                    
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    log_note("TODO: Window size changed");
                    break;
                    
                // case SDL_WINDOWEVENT_MINIMIZED: // TODO: is it possible to minimize the window without losing focus first? (important: Cmd-M should NOT be treated as 'Window > Minimize'; ditto other OS-standard app shortcuts, at least not while in-game is running)
                // think the rest of these can be ignored
                //case SDL_WINDOWEVENT_EXPOSED: // window was partly obscured but no longer
                //case SDL_WINDOWEVENT_ENTER: // mouse entered/exited window (while paused)
                //case SDL_WINDOWEVENT_LEAVE:
                //case SDL_WINDOWEVENT_MAXIMIZED:
                //case SDL_WINDOWEVENT_RESTORED: // restored from minimized
                    TODO("window events");
                    break;
            }
            break;
    }
    
}



//************************************************************************************************
// event loop


void game_event_loop(bool is_restoring_saved_game)
{
    assert_fail(get_app_state() == app_state_t::game_in_progress, "");
    
    is_running = true; // TODO: ick: enter_gameworld needs this true so `main_screen.synchronize` behaves appropriately; it's a fiddle
    
    enter_gameworld(is_restoring_saved_game); // in marathon2.cpp
    
    uint64_t next_poll_time = 0;
    while (is_running) // TODO: this smells; this should be a bool flag which is initially true and breaking out of game loop performed by a function which sets it to false and also sets the next app state so the main loop will transition itself
    {
        uint64_t current_time = machine_tick_count();
        
        if ((graphics_preferences.current_fps_target() == 0 && is_vbl_reading_user_inputs())
            || current_time >= next_poll_time || Console::instance()->input_active())
        {
            next_poll_time = current_time + TICKS_BETWEEN_EVENT_POLL;
            
            update_audio_on_idle();
            
            SDL_Event event;
            while (SDL_PollEvent(&event)) { process_event(event); }
            
#ifdef HAVE_STEAM
            while (auto steam_event = STEAMSHIM_pump())
            {
                if (steam_event->type == SHIMEVENT_IS_OVERLAY_ACTIVATED && steam_event->okay
                    && is_running && !game_is_networked())
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
        
        // inlined the following from idle_game_state
        // TODO: confirm this is replicated correctly
        
        // if we’re not paused and there’s something to draw (i.e., anything different from last time), render a frame
        // ZZZ change: update_world() whether or not is_vbl_reading_user_inputs() is true. This way we won't fill up
        // queues and stall netgames if one player switches out for a bit.
        int32_t ticks_elapsed;
        bool needs_redraw = update_world(ticks_elapsed);

        if (is_vbl_reading_user_inputs()) // we are reading keyboard inputs, so presumably this means live game is running
        {
            // ZZZ: I don't know for sure that render_game_to_screen works best with the number of _real_ ticks elapsed
            // rather than the number of (potentially predictive) ticks elapsed. This is a guess.
            auto heartbeat_fraction = get_heartbeat_fraction();
            if (needs_redraw || (last_heartbeat_fraction != -1 && last_heartbeat_fraction != heartbeat_fraction))
            {
                last_heartbeat_fraction = heartbeat_fraction;
                
                render_game_to_screen(ticks_elapsed);
                
                first_frame_rendered = ticks_elapsed > 0;
            }
        }
        else
        {
            needs_redraw = true;
        }

        if (needs_redraw)
        {
            static uint64_t last_redraw = 0;
            if (current_player && machine_tick_count() > last_redraw + MACHINE_TICKS_PER_SECOND / 30)
            {
                last_redraw = machine_tick_count();
                render_game_to_screen(ticks_elapsed);
            }
        }
        // end inlined idle_game_state
        
        int16_t fps_target = graphics_preferences.current_fps_target(); // TODO: in Classic mode this should always be 30fps
        if (fps_target != FPS_UNLIMITED)
        {
            uint64_t elapsed_machine_ticks = machine_tick_count() - current_time;
            uint64_t desired_elapsed_machine_ticks = MACHINE_TICKS_PER_SECOND / fps_target;
            
            if (desired_elapsed_machine_ticks - elapsed_machine_ticks > desired_elapsed_machine_ticks / 3)
            {
                sleep_for_machine_ticks(1);
            }
        }
    }
    
    exit_gameworld();
}


void exit_game_event_loop(app_state_t next_state)
{
    set_next_app_state(app_state_t::exit_game);
    is_running = false;
}

