/*
 PlayerTerminalState.cpp
 
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

#include "PlayerTerminalState.hpp"

#include "FileHandler.h" // OpenedResourceFile
#include "SoundManager.h" // Sound_TerminalLogon()
#include "screen_drawing.h" // _terminal_full_text_rect
#include "Packing.h"

#include "M1TerminalParser.hpp" // compile_m1_terminal()


// -----------------------------------------------------------------------------------------
// the players' terminal interaction states

// each Player remembers its own terminal interaction: if it is logged in, the terminal ID, current group and line
static std::array<PlayerTerminalState, MAXIMUM_NUMBER_OF_PLAYERS> player_terminals;


void initialize_player_terminal_states()
{
    int16_t player_index = 0; // kludge: due to all the circular references between Player and PlayerTerminalState, some methods below call functions elsewhere which need to know the player_index
    for (PlayerTerminalState& state : player_terminals) { state.initialize(player_index++); }
}


// so, so, so much indirection... TO DO: eventually PlayerTerminalState should probably move onto Player class/struct
PlayerTerminalState* get_terminal_state_for_player(int16_t player_index)
{
    return &player_terminals.at(player_index);
}


// -----------------------------------------------------------------------------------------
// PlayerTerminalState


void PlayerTerminalState::enter_computer_terminal(int16_t terminal_text_id, int16_t completion_flag)
{
    TerminalText* terminal_text = get_terminal_text_for_terminal_id(terminal_text_id);
    if (!terminal_text)
    {
        play_object_sound(get_player_data(player_index)->object_index, Sound_TerminalLogon()); // derp; seems like logging an error message would be more appropriate for a buggy terminal (i.e. the terminal switch's id doesn't have a corresponding terminal text in Maps file [or app/shapes if M1]), but leaving for now
        return;
    }
    
    if (dynamic_world->player_count == 1)
    { // Reset the lines per page to the actual value for whatever fucked up font that they have
        int16_t lines_per_page = calculate_lines_per_page();
        if (lines_per_page != terminal_text->lines_per_page)
        {
            // dprintf("You have one confused font.");
            terminal_text->lines_per_page = lines_per_page;
        }
    }
    
    is_active = true;
    // TODO: is_dirty? presumably 0
    phase = NONE;
    current_group = NONE;
    level_completion_state = completion_flag;
    current_line = 0;
    maximum_line = 1; // any click or keypress will get us out.
    terminal_id = terminal_text_id;
    last_action_flag = -1l; // Eat the first key
    
    goto_next_terminal_group(terminal_text);
}


void PlayerTerminalState::reset()
{
    is_active = false; // And there is no line.
    is_dirty = 0; // = needs_redraw
    phase = NONE; // not using a control panel.
    current_group = NONE;
    level_completion_state = 0;
    current_line = 0;
    maximum_line = 0;
    terminal_id = 0;
    last_action_flag = -1l; // Eat the first key
}


void PlayerTerminalState::exit_computer_terminal(bool reset_state)
{
    if (is_active)
    {
        is_active = false;
        L_Call_Terminal_Exit(terminal_id, player_index);
        if (film_profile.overhead_map_terminal)
        {
            auto player = get_player_data(player_index);
            bool map_was_active = PLAYER_HAD_OVERHEAD_MAP_ACTIVE(player);
            SET_PLAYER_MAP_STATUS(player, map_was_active);
            SET_PLAYER_HAD_OVERHEAD_MAP_STATUS(player, false);
        }
    }
    // this is outside the is_active conditional as that was the original order of operations
    if (reset_state) { reset(); } // logging out normally resets the terminal whereas being interrupted by a monster hit allows user to log back in at where they left off // TO DO: check this comment is correct
}


bool PlayerTerminalState::goto_previous_terminal_group(TerminalText* terminal_text)
{
    bool success = false;
    
    if (is_active)
    {
        int16_t new_group_index = current_group - 1;
        bool use_new_group = true;
        bool done = true;
        
        do
        {
            if (new_group_index >= 0)
            {
                TerminalTextGroup* new_group = terminal_text->get_grouping(new_group_index);
                if (!new_group) return false;
                
                switch (new_group->type)
                {
                    case _logon_group:
                    case _end_group:
                        use_new_group = false;
                        done = true;
                        break;
                        
                    case _interlevel_teleport_group:
                    case _intralevel_teleport_group:
                        // dprintf("This shouldn't happen!");
                        break;

                     case _sound_group:
                    case _tag_group:
                        new_group_index--;
                        done = false;
                        break;
                    
                    case _movie_group:
                    case _track_group:
                    case _checkpoint_group:
                    case _pict_group:
                    case _information_group:
                    case _camera_group:
                        done = true;
                        break;
                        
                    case _unfinished_group:
                    case _success_group:
                    case _failure_group:
                    case _static_group:
                        use_new_group = false;
                        done = true;
                        break;
                    
                    default:
                        break;
                }
            } else {
                use_new_group = false;
                done = true;
            }
        } while (!done);
        
        if (use_new_group)
        {
            goto_terminal_group(terminal_text, new_group_index);
            success = true;
        }
    }
    return success;
}


void PlayerTerminalState::goto_next_terminal_group(TerminalText* terminal_text)
{
    bool update_line_count = false;
    
    if (current_group == NONE)
    {
        update_line_count = true;
        
        switch (level_completion_state)
        {
            case _level_unfinished:
                current_group = terminal_text->find_group_type(_unfinished_group);
                break;
                
            case _level_finished:
                current_group = terminal_text->find_group_type(_success_group);
                if (current_group == NONE)
                { // Fallback.
                    current_group = terminal_text->find_group_type(_unfinished_group);
                    // assert(current_group != terminal_text->groupings.size());
                }
                break;
                
            case _level_failed:
                current_group = terminal_text->find_group_type(_failure_group);
                if (current_group == NONE)
                { // Fallback.
                    current_group = terminal_text->find_group_type(_unfinished_group);
                    //assert(current_group != terminal_text->groupings.size());
                }
                break;
            
            default:
                break;
        }

        if (current_group == NONE && terminal_text->groupings.size() > 0
            && (terminal_text->groupings[0].flags & _group_is_marathon_1)) // Marathon 1 fallback // TODO: why?
        {
            current_group = 0;
        }
        else
        { // Note that the information groups are now keywords, and can have no data associated with them
            goto_next_terminal_group(terminal_text);
        }
    } else {
        current_group++;
        assert(current_group >= 0);
        if ((size_t)current_group >= terminal_text->groupings.size())
        {
            goto_last_terminal_state();
        }
        else
        {
            update_line_count = true;
        }
    }
    
    if (update_line_count) { goto_terminal_group(terminal_text, current_group); }
    
    is_dirty = true;
}


void PlayerTerminalState::goto_terminal_group(TerminalText* terminal_text, int16_t new_group_index)
{
    player_data* player = get_player_data(player_index);
    
    current_group = new_group_index;
    
    TerminalTextGroup* current_group = terminal_text->get_grouping(new_group_index);
    if (!current_group)
    {
        // Copied from _end_group case
        goto_last_terminal_state();
        maximum_line = 1; // any click or keypress will get us out...
        return;
    }
    
    current_line = 0;
        
    switch (current_group->type)
    {
        case _logon_group:
            play_object_sound(player->object_index, Sound_TerminalLogon());
            phase        = LOG_DURATION_BEFORE_TIMEOUT;
            maximum_line = current_group->maximum_line_count;
            break;
            
        case _logoff_group:
            play_object_sound(player->object_index, Sound_TerminalLogoff());
            phase        = LOG_DURATION_BEFORE_TIMEOUT;
            maximum_line = current_group->maximum_line_count;
            break;
            
        case _interlevel_teleport_group:
        case _intralevel_teleport_group:
        case _sound_group:
        case _tag_group:
        case _movie_group:
        case _track_group:
        case _camera_group:
            phase = NONE;
            maximum_line = current_group->maximum_line_count;
            break;

        case _checkpoint_group:
        case _pict_group:
            phase = NONE;
            if (dynamic_world->player_count > 1) // Use what the server told us
            {
                maximum_line = current_group->maximum_line_count;
            }
            else // Calculate this for ourselves
            {
                Rect text_bounds; // The only thing we care about is the width.
                calculate_bounds_for_text_box(current_group->flags, &text_bounds);
                maximum_line = count_total_lines(terminal_text->get_cstr(),
                                                 RECTANGLE_WIDTH(&text_bounds),
                                                 current_group->start_index,
                                                 current_group->start_index + current_group->length);

                if (film_profile.page_up_past_full_width_term_pict && maximum_line == 0)
                {
                    maximum_line = 1;
                }
            }
            break;
            
        case _information_group:
            phase = NONE;
            if (dynamic_world->player_count > 1)
            { // Use what the server told us
                maximum_line = current_group->maximum_line_count;
            }
            else
            { // Calculate this for ourselves.
                Rect bounds = get_term_rectangle(_terminal_full_text_rect);
                maximum_line = count_total_lines(terminal_text->get_cstr(),
                                                 RECTANGLE_WIDTH(&bounds),
                                                 current_group->start_index,
                                                 current_group->start_index + current_group->length);
            }
            break;

        case _static_group:
            phase        = current_group->permutation;
            maximum_line = current_group->maximum_line_count;
            break;

        case _end_group: // Get Out! 
            goto_last_terminal_state();
            maximum_line = 1; // any click or keypress will get us out...
            break;

        case _unfinished_group:
        case _success_group:
        case _failure_group:
            vwarn(0, "You shouldn't be coming to this group");
            break;
            
        default:
            break;
    }
}


// -----------------------------------------------------------------------------------------
// serialization

// each player's current terminal state is recorded, presumably for co-op games where one player may save while another is reading a terminal

uint8_t* unpack_player_terminal_state(uint8_t* Stream, size_t Count)
{
    uint8_t* S = Stream;
    for (PlayerTerminalState obj : player_terminals)
    {
        StreamToValue(S, obj.is_dirty);
        StreamToValue(S, obj.phase);
        StreamToValue(S, obj.is_active);
        StreamToValue(S, obj.current_group);
        StreamToValue(S, obj.level_completion_state);
        StreamToValue(S, obj.current_line);
        StreamToValue(S, obj.maximum_line);
        StreamToValue(S, obj.terminal_id);
        StreamToValue(S, obj.last_action_flag);
    }
    assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_terminal_state));
    return S;
}


uint8_t* pack_player_terminal_state(uint8_t* Stream, size_t Count)
{
    uint8_t* S = Stream;
    for (PlayerTerminalState obj : player_terminals)
    {
        ValueToStream(S, obj.is_dirty);
        ValueToStream(S, obj.phase);
        ValueToStream(S, obj.is_active);
        ValueToStream(S, obj.current_group);
        ValueToStream(S, obj.level_completion_state);
        ValueToStream(S, obj.current_line);
        ValueToStream(S, obj.maximum_line);
        ValueToStream(S, obj.terminal_id);
        ValueToStream(S, obj.last_action_flag);
    }
    assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_terminal_state));
    return S;
}
