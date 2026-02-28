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

#include "FileHandler.h"    // OpenedResourceFile
#include "SoundManager.h"   // Sound_TerminalLogon()
#include "screen_drawing.h" // _terminal_full_text_rect
#include "Packing.h"

#include "terminal_parser_m1.hpp" // compile_m1_terminal()


// -----------------------------------------------------------------------------------------
// the players' terminal interaction states

// each Player remembers its own terminal interaction: if it is logged in, the terminal ID, current group and line
static std::array<PlayerTerminalState, MAXIMUM_NUMBER_OF_PLAYERS> player_terminals;


void initialize_player_terminal_states()
{
    int16_t player_index = 0; // kludge: due to all the circular references between Player and PlayerTerminalState, some methods below call functions elsewhere which need to know the player_index
    for (PlayerTerminalState& state : player_terminals) { state.initialize(player_index++); }
}


// so, so, so much indirection... TODO: eventually PlayerTerminalState should probably move onto Player class/struct
PlayerTerminalState* get_terminal_state_for_player(int16_t player_index)
{
    return &player_terminals.at(player_index);
}


// -----------------------------------------------------------------------------------------
// PlayerTerminalState


void PlayerTerminalState::enter_computer_terminal(int16_t terminal_id_, int16_t completion_flag)
{
    ComputerTerminal* terminal = get_terminal_for_id(terminal_id_);
    if (!terminal)
    {
        play_object_sound(get_player_data(player_index)->object_index, Sound_TerminalLogon()); // TODO: seems like logging an error message would be more appropriate for a buggy terminal (i.e. the terminal switch's id doesn't have a corresponding terminal text in Maps file [or app/shapes if M1]), but leaving for now
        return;
    }
    
    if (dynamic_world->player_count == 1)
    { // Reset the lines per page to the actual value for whatever fucked up font that they have
        int16_t lines_per_page = calculate_lines_per_page();
        if (lines_per_page != terminal->lines_per_page)
        {
            // ao__dprintf__("You have one confused font->");
            terminal->lines_per_page = lines_per_page;
        }
    }
    
    is_active = true;
    needs_redraw = true; // TODO: not sure about this
    phase = NONE;
    page_id = NONE;
    level_completion_state = completion_flag;
    line_number = 0;
    maximum_line = 1; // any click or keypress will get us out.
    terminal_id = terminal_id_;
    action_flags_mask = ~0;
    
    goto_next_terminal_page(terminal);
}


void PlayerTerminalState::reset()
{
    is_active = false; // And there is no line.
    needs_redraw = 0; // = needs_redraw
    phase = NONE; // not using a control panel.
    page_id = NONE;
    level_completion_state = 0;
    line_number = 0;
    maximum_line = 0;
    terminal_id = 0;
    action_flags_mask = ~0;
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
    if (reset_state) { reset(); } // logging out normally resets the terminal whereas being interrupted by a monster hit allows user to log back in at where they left off // TODO: check this comment is correct
}


bool PlayerTerminalState::goto_previous_terminal_page(ComputerTerminal* terminal)
{
    bool success = false;
    
    if (is_active)
    {
        int16_t new_page_index = page_id - 1;
        bool use_new_page = true;
        bool done = true;
        
        do
        {
            if (new_page_index >= 0)
            {
                TerminalPage* new_page = terminal->get_page_at_index(new_page_index);
                if (!new_page) return false;
                
                switch (new_page->type)
                {
                    case _logon_page:
                    case _end_page:
                        use_new_page = false;
                        done = true;
                        break;
                        
                    case _interlevel_teleport_page:
                    case _intralevel_teleport_page:
                        // ao__dprintf__("This shouldn't happen!");
                        break;

                     case _sound_page:
                    case _tag_page:
                        new_page_index--;
                        done = false;
                        break;
                    
                    case _movie_page:
                    case _track_page:
                    case _checkpoint_page:
                    case _pict_page:
                    case _information_page:
                    case _camera_page:
                        done = true;
                        break;
                        
                    case _unfinished_page:
                    case _success_page:
                    case _failure_page:
                    case _static_page:
                        use_new_page = false;
                        done = true;
                        break;
                    
                    default:
                        break;
                }
            } else {
                use_new_page = false;
                done = true;
            }
        } while (!done);
        
        if (use_new_page)
        {
            goto_terminal_page(terminal, new_page_index);
            success = true;
        }
    }
    return success;
}


void PlayerTerminalState::goto_next_terminal_page(ComputerTerminal* terminal)
{
    bool update_line_count = false;
    
    if (page_id == NONE) // start of terminals, presumably logon-to-first-page transition
    {
        update_line_count = true;
        
        // TODO: An unfinished group should always exist, even if it's empty {logon,logoff,end}; M1 parser synthesizes it, not sure if M2 Maps are guaranteed to have it but this should be checked in unpack_m2_terminals
        switch (level_completion_state)
        {
            case _level_unfinished:
                page_id = terminal->get_index_for_group(_unfinished_group);
                break;
                
            case _level_finished:
                page_id = terminal->get_index_for_group(_success_group);
                if (page_id == NONE) { page_id = terminal->get_index_for_group(_unfinished_group); }
                break;
                
            case _level_failed:
                page_id = terminal->get_index_for_group(_failure_group);
                if (page_id == NONE) { page_id = terminal->get_index_for_group(_unfinished_group); }
                break;
            
            default:
                break;
        }

        if (page_id == NONE && terminal->pages.size() > 0 && (terminal->pages[0].flags & _terminal_is_m1)) // Marathon 1 fallback
        {
            page_id = 0;
        }
        else
        { // Note that the information groups are now keywords, and can have no data associated with them
            goto_next_terminal_page(terminal);
        }
    } else {
        page_id++;
        assert_fail(page_id >= 0, "");
        if ((size_t)page_id >= terminal->pages.size())
        {
            goto_last_terminal_state();
        }
        else
        {
            update_line_count = true;
        }
    }
    
    if (update_line_count) { goto_terminal_page(terminal, page_id); }
    
    needs_redraw = true;
}


void PlayerTerminalState::goto_terminal_page(ComputerTerminal* terminal, int16_t new_page_index)
{
    player_data* player = get_player_data(player_index);
    
    page_id = new_page_index;
    
    TerminalPage* current_page = terminal->get_page_at_index(new_page_index);
    if (!current_page)
    {
        // Copied from _end_page case
        goto_last_terminal_state();
        maximum_line = 1; // any click or keypress will get us out...
        return;
    }
    
    line_number = 0;
        
    switch (current_page->type)
    {
        case _logon_page:
            play_object_sound(player->object_index, Sound_TerminalLogon());
            phase        = LOG_DURATION_BEFORE_TIMEOUT;
            maximum_line = current_page->maximum_line_count;
            break;
            
        case _logoff_page:
            play_object_sound(player->object_index, Sound_TerminalLogoff());
            phase        = LOG_DURATION_BEFORE_TIMEOUT;
            maximum_line = current_page->maximum_line_count;
            break;
            
        case _interlevel_teleport_page:
        case _intralevel_teleport_page:
        case _sound_page:
        case _tag_page:
        case _movie_page:
        case _track_page:
        case _camera_page:
            phase = NONE;
            maximum_line = current_page->maximum_line_count;
            break;

        case _checkpoint_page:
        case _pict_page:
            phase = NONE;
            if (dynamic_world->player_count > 1) // Use what the server told us
            {
                maximum_line = current_page->maximum_line_count;
            }
            else // Calculate this for ourselves
            {
                // TODO: get linecount when rendering to Surface
                Rect text_bounds = current_page->calculate_bounds_for_text_box(); // The only thing we care about is the width.
                maximum_line = 1; //count_total_lines(terminal->get_cstr(), RECTANGLE_WIDTH(&text_bounds), current_page->start_index, current_page->start_index + current_page->length);

                if (film_profile.page_up_past_full_width_term_pict && maximum_line == 0)
                {
                    maximum_line = 1;
                }
            }
            break;
            
        case _information_page:
            phase = NONE;
            if (dynamic_world->player_count > 1)
            { // Use what the server told us
                maximum_line = current_page->maximum_line_count;
            }
            else
            { // Calculate this for ourselves.
                Rect bounds = get_term_rectangle(_terminal_full_text_rect);
                maximum_line = 1; //count_total_lines(terminal->get_cstr(), RECTANGLE_WIDTH(&bounds), current_page->start_index, current_page->start_index + current_page->length); // TODO: FIX
            }
            break;

        case _static_page:
            phase        = current_page->permutation;
            maximum_line = current_page->maximum_line_count;
            break;

        case _end_page: // Get Out! 
            goto_last_terminal_state();
            maximum_line = 1; // any click or keypress will get us out...
            break;

        case _unfinished_page:
        case _success_page:
        case _failure_page:
            assert_warn(0, "You shouldn't be coming to this group");
            break;
            
        default:
            break;
    }
}


// -----------------------------------------------------------------------------------------
// serialization

// each player's current terminal state is recorded, presumably for co-op games where one player may save while another is reading a terminal

enum {
    _reading_terminal  = 0x00, // no idea why these are inverted, but oh well
    _no_terminal_state = 0x01,
};

enum {
    _terminal_is_dirty = 0x01,
};


uint8_t* unpack_player_terminal_state(uint8_t* Stream, size_t Count) // Count = 0-8
{
    if (Count > MAXIMUM_NUMBER_OF_PLAYERS) { Count = MAXIMUM_NUMBER_OF_PLAYERS; } // minimal guard against bad file data // TODO: check all unpack_* functions have adequate guards
    uint8_t* S = Stream;
    for (size_t i = 0; i < Count; i++)
    {
        PlayerTerminalState& obj = player_terminals[i];
        int16_t flags, state;
        int32_t action_flags_mask;
        StreamToValue(S, flags);
        StreamToValue(S, obj.phase);
        StreamToValue(S, state);
        StreamToValue(S, obj.page_id);
        StreamToValue(S, obj.level_completion_state);
        StreamToValue(S, obj.line_number);
        StreamToValue(S, obj.maximum_line);
        StreamToValue(S, obj.terminal_id);
        StreamToValue(S, action_flags_mask);
        obj.needs_redraw = flags & _terminal_is_dirty;
        obj.is_active = state == _reading_terminal;
        obj.action_flags_mask = (action_flag_t)action_flags_mask;
    }
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_terminal_state), "");
    return S;
}


uint8_t* pack_player_terminal_state(uint8_t* Stream, size_t Count)
{
    uint8_t* S = Stream;
    for (size_t i = 0; i < Count; i++)
    {
        PlayerTerminalState& obj = player_terminals[i];
        int16_t flags = obj.needs_redraw ? _terminal_is_dirty : 0;
        int16_t state = obj.is_active ? _reading_terminal : _no_terminal_state;
        int32_t action_flags_mask = (int32_t)obj.action_flags_mask;
        ValueToStream(S, flags); // 0x0000600003ef9600
        ValueToStream(S, obj.phase);
        ValueToStream(S, state);
        ValueToStream(S, obj.page_id);
        ValueToStream(S, obj.level_completion_state);
        ValueToStream(S, obj.line_number);
        ValueToStream(S, obj.maximum_line);
        ValueToStream(S, obj.terminal_id);
        ValueToStream(S, action_flags_mask);
    }
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_terminal_state), "");
    return S;
}
