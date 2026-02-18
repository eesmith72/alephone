/*
 PlayerTerminalState.hpp -- class containing a single player's current terminal login state
 
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

#ifndef PlayerTerminalState_hpp
#define PlayerTerminalState_hpp

#include "TerminalText.hpp"


// -----------------------------------------------------------------------------------------


// Serialized size of current terminal state (also used in game_wad.cpp)
const int32_t SIZEOF_player_terminal_state = 20;


// -----------------------------------------------------------------------------------------
// PlayerTerminalState -- every Player has one


class PlayerTerminalState
{
private:
    int16_t player_index; // not ideal, but some methods need to reference the Player object so this is assigned on initialization
    
public:
    
    int16_t is_active;
    int16_t is_dirty;
    int16_t phase; // timer for logging in and out
    int16_t current_group;
    int16_t level_completion_state;
    int16_t current_line;
    int16_t maximum_line;
    int16_t terminal_id;
    int32_t last_action_flag;
    
    void reset();
    
    void initialize(int16_t player_index_) 
    {
        player_index = player_index_;
        reset();
    }
    
    // player enters/exits terminal
    
    void enter_computer_terminal(int16_t text_number, int16_t completion_flag);
    
    // - Pass true if user cancels or terminal automatically logs out; this resets the terminal.
    // - Pass false if M2 player was hit by a monster; this disconnects but does not reset
    // TODO: What is point of not resetting after player is hit? It isn't so player can log back in and resume reading where they left off, because enter_ clears the state. About the only thing it seems to do is skip resetting the level_completion_state, although that's re-set by enter_. It's possible there is no behavioral difference, in which case get rid of reset_state arg and always reset.
    void exit_computer_terminal(bool reset_state = true);
    
    // paging; called by terminal_actions.cpp
    
    bool goto_previous_terminal_group(TerminalText* terminal_text);
    
    void goto_next_terminal_group(TerminalText* terminal_text);

    void goto_terminal_group(TerminalText* terminal_text, int16_t new_group_index);
    
    void goto_last_terminal_state()
    {
        if (is_active) { exit_computer_terminal(); }
    }
    
    // request redraw; called by player.cpp, lua_player.cpp, screen.cpp, screen_shared.cpp (as dirty_terminal_view)
    void set_dirty()
    {
        if (is_active) { is_dirty = true; }
    }
};


// -----------------------------------------------------------------------------------------
// access players' terminal states


void initialize_player_terminal_states(); // called by initialize_application in shell.cpp

// TODO: in future, Player object should probably look after its own PlayerTerminalState instance
PlayerTerminalState* get_terminal_state_for_player(int16_t player_index);


// -----------------------------------------------------------------------------------------
// serialization


uint8_t* unpack_player_terminal_state(uint8_t* Stream, size_t Count);

uint8_t* pack_player_terminal_state(uint8_t* Stream, size_t Count);


#endif /* PlayerTerminalState_hpp */
