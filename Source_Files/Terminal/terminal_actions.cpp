/*
 terminal_actions.hpp
 
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

#include "terminal_actions.hpp"

#include "joystick.h" // for AO_SCANCODE_BASE_JOYSTICK_BUTTON
#include "platforms.h" // for tagged platforms
#include "lightsource.h" // for tagged lightsources
#include "SoundManager.h" // login/logout/paging sounds
#include "lua_script.h" // L_Call_Terminal_Exit


// -----------------------------------------------------------------------------------------
// Terminal key definitions


enum { // original keyboard input bitflags (from action_flags enum in player.h) are remapped here to terminal-specific actions
    _terminal_exit       = _action_trigger_state,
    _terminal_up_arrow   = _moving_forward,
    _terminal_down_arrow = _moving_backward,
    _terminal_page_up    = _turning_left,
    _terminal_page_down  = _turning_right,
    _terminal_next_state = _left_trigger_state,
};


struct TerminalAction
{
    int16_t keycode;
    action_flag_t action_flag;
};


// controller inputs; TODO: controller doesn't seem to support per-line scrolling; is this deliberate?
static const std::array<TerminalAction, 16> terminal_keys = { // user may press >1 button at a time (although it'd be silly to do so)
    SDL_SCANCODE_UP,                                                    _terminal_page_up,     // arrow up
    SDL_SCANCODE_DOWN,                                                  _terminal_page_down,   // arrow down
    SDL_SCANCODE_PAGEUP,                                                _terminal_page_up,     // page up
    SDL_SCANCODE_PAGEDOWN,                                              _terminal_page_down,   // page down
    SDL_SCANCODE_TAB,                                                   _terminal_next_state,  // tab
    SDL_SCANCODE_KP_ENTER,                                              _terminal_next_state,  // enter
    SDL_SCANCODE_RETURN,                                                _terminal_next_state,  // return
    SDL_SCANCODE_SPACE,                                                 _terminal_next_state,  // space
    SDL_SCANCODE_ESCAPE,                                                _terminal_exit,        // escape
    AO_SCANCODE_JOYSTICK_ESCAPE,                                        _terminal_exit,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_UP,   _terminal_page_up,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_DOWN, _terminal_page_down,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_A,         _terminal_next_state,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_X,         _terminal_next_state,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_Y,         _terminal_next_state,
    AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_B,         _terminal_exit,
};


// -----------------------------------------------------------------------------------------
// TODO: these should be methods on Player (update_world presumably polls teleporting_destination to see if it's changed)


static void teleport_to_level(int16_t level_number, int16_t delay_before_teleport)
{
    // It doesn't matter which player we get: when one player teleports out, the co-op player teleports too
    player_data* player = get_player_data(0);
    player->teleporting_destination = -level_number - 1; // LP change: moved down by 1 so that level 0 will be valid
    player->delay_before_teleport = delay_before_teleport;
}


static void teleport_to_polygon(int16_t player_index, int16_t polygon_index)
{
    player_data* player = get_player_data(player_index);
    player->teleporting_destination = polygon_index;
    assert(!player->delay_before_teleport);
}


// -----------------------------------------------------------------------------------------
// Process key presses for local/all players currently viewing terminals.


// Translates the user's key presses into terminal-specific action flags used below. Called by parse_keymap in vbl.cpp.
action_flag_t build_terminal_state_action_flags(char* keymap)
{
    PlayerTerminalState* terminal_state = get_terminal_state_for_player(local_player_index);
    
    action_flag_t raw_flags = 0;
    for (const TerminalAction& key : terminal_keys)
    {
        if (keymap[key.keycode]) { raw_flags |= key.action_flag; }
    }
    
    // Only catch the key the first time. // TODO: what does this mean? probably that keys are sticky so pressing and holding doesn't scroll constantly
    action_flag_t flags = raw_flags ^ terminal_state->action_flags_mask;
    flags &= raw_flags;
    terminal_state->action_flags_mask = raw_flags;
    return flags;
}


#define play_sound_at_player(player_index, sound_id)  (play_object_sound(get_player_data((player_index))->object_index, (sound_id)))


// Called (via player.cpp) from update_world[_elements_one_tick] in marathon2.cpp.
void update_terminal_state_with_action_flags(int16_t player_index, action_flag_t action_flags)
{
    PlayerTerminalState* terminal_state = get_terminal_state_for_player(player_index);
    
    if (terminal_state->is_active)
    {
        ComputerTerminal* terminal = get_terminal_for_id(terminal_state->terminal_id);
        if (!terminal) return;
        
        int16_t initial_page = terminal_state->page_id;
        int16_t initial_line  = terminal_state->line_number;
        int16_t line_delta = 0; // scroll or page up/down
        bool forces_state_change = false;
        bool aborted = false;
        
        TerminalPage* current_page = terminal->get_page_at_index(terminal_state->page_id);
        
        switch (current_page ? current_page->type : _end_page)
        {
            case _logon_page:
            case _logoff_page:
            case _unfinished_page:
            case _success_page:
            case _failure_page:
            case _information_page:
            case _checkpoint_page:
            case _pict_page:
            case _camera_page:
            case _static_page:
                if (action_flags & _terminal_up_arrow)   { line_delta = -1; }
                if (action_flags & _terminal_down_arrow) { line_delta = +1; }
                
                if (action_flags & _terminal_page_down)
                {
                    play_sound_at_player(player_index, Sound_TerminalPage());
                    line_delta = terminal->lines_per_page;
                }
                if (action_flags & _terminal_page_up)
                {
                    play_sound_at_player(player_index, Sound_TerminalPage());
                    line_delta = -terminal->lines_per_page;
                }
                
                if (action_flags & _terminal_next_state) // this one should change state, if necessary
                {
                    play_sound_at_player(player_index, Sound_TerminalPage());
                    line_delta = terminal->lines_per_page;
                    forces_state_change = true;
                }
                
                if (action_flags & _terminal_exit)
                {
                    terminal_state->exit_computer_terminal();
                    aborted = true;
                }
                break;
            
            case _movie_page:
            case _track_page:
                break; // TODO: does this mean these aren't implemented yet?
                
            case _end_page:
                terminal_state->goto_last_terminal_state();
                aborted = true;
                break;
                
            case _interlevel_teleport_page: // permutation = level to go to
            {
                bool is_m1_terminal = film_profile.m1_teleport_without_delay && (current_page->flags & _terminal_is_m1);
                teleport_to_level(current_page->permutation, (is_m1_terminal ? 0 : TICKS_PER_SECOND / 2));
                terminal_state->exit_computer_terminal();
                aborted = true;
                break;
            }
            case _intralevel_teleport_page: // permutation = polygon to go to
                teleport_to_polygon(player_index, current_page->permutation);
                terminal_state->exit_computer_terminal();
                aborted = true;
                break;
                
            case _sound_page: // permutation = sound id to play. Start playing the sound and go to the next group immediately.
                play_sound_at_player(player_index, current_page->permutation);
                terminal_state->goto_next_terminal_page(terminal);
                aborted = true;
                break;
            
            case _tag_page:
                set_tagged_light_statuses(current_page->permutation, true);
                try_and_change_tagged_platform_states(current_page->permutation, true);
                terminal_state->goto_next_terminal_page(terminal);
                aborted = true;
                break;
                
            default:
                break;
        }
        
        // If terminal display has changed (e.g. user has scrolled view), update state and request redraw.
        terminal_state->line_number += line_delta;
        if (!aborted && (initial_page != terminal_state->page_id || initial_line != terminal_state->line_number))
        {
            if (terminal_state->line_number < 0 && !terminal_state->goto_previous_terminal_page(terminal))
            {
                terminal_state->line_number = 0;
            }
            
            if (terminal_state->line_number >= terminal_state->maximum_line)
            {
                assert(terminal_state->page_id >= 0);
                if (static_cast<size_t>(terminal_state->page_id) + 1 >= terminal->pages.size())
                {
                    if (forces_state_change)
                    {
                        terminal_state->goto_next_terminal_page(terminal); // let the terminal group deal with it
                    }
                    else
                    {
                        terminal_state->line_number -= line_delta; // renumber the lines
                    }
                }
                else
                {
                    terminal_state->goto_next_terminal_page(terminal);
                }
            }
            
            terminal_state->needs_redraw = true;
        }
    }
}


// called (via player.cpp) from update_world[_elements_one_tick] in marathon2.cpp
void update_terminal_state_for_player(int16_t player_index)
{
    PlayerTerminalState* terminal_state = get_terminal_state_for_player(player_index);
    
    if (terminal_state->is_active && terminal_state->phase != NONE && --terminal_state->phase <= 0)
    {
        ComputerTerminal* terminal = get_terminal_for_id(terminal_state->terminal_id);
        if (terminal) { terminal_state->goto_next_terminal_page(terminal); }
    }
}

