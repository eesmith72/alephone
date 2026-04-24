/*
 computer_interface.h -- legacy header providing old API shims used by rest of AO
 
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

#ifndef computer_interface_h
#define computer_interface_h


#include "computer_terminal.hpp"


// -----------------------------------------------------------------------------------------
// state


#define enter_computer_interface(player_index, control_panel_permutation, level_completion_state) \
    (get_terminal_state_for_player(player_index)->enter_computer_terminal((control_panel_permutation), (level_completion_state)))

#define abort_terminal_mode(player_index) \
    (get_terminal_state_for_player((player_index))->exit_computer_terminal(false))

#define initialize_player_terminal_info(player_index) \
    (get_terminal_state_for_player((player_index))->exit_computer_terminal())


#define player_in_terminal_mode(player_index)   (get_terminal_state_for_player((player_index))->is_active)

#define dirty_terminal_view(player_index)       (get_terminal_state_for_player((player_index))->set_dirty())



#endif /* computer_interface_h */
