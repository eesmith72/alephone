/*
 terminal_actions.hpp -- process key presses for any players currently in terminal
 
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

#ifndef terminal_actions_hpp
#define terminal_actions_hpp


#include "PlayerTerminalState.hpp"


typedef uint32_t action_flag_t;


// -----------------------------------------------------------------------------------------


action_flag_t build_terminal_state_action_flags(char* keymap); // called by vbl.cpp when the user (local player) is viewing terminal; translates key inputs into terminal actions (scrolling, paging, cancel)

void update_terminal_state_with_action_flags(int16_t player_index, action_flag_t action_flags); // called by player.cpp in update_m1_solo_player_in_terminal and update_players

// called by player.cpp when updating players' current state; e.g. login screen will automatically advance to first text screen after a delay
void update_terminal_state_for_player(int16_t player_index);


#endif /* terminal_actions_hpp */
