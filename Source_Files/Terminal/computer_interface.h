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
// actions


#define build_terminal_action_flags(key_map)            (build_terminal_state_action_flags((key_map)))

#define update_player_keys_for_terminal(player_index, action_flags) \
    (update_terminal_state_with_action_flags((player_index), (action_flags)))

#define update_player_for_terminal_mode(player_index)   (update_terminal_state_for_player((player_index)))


// -----------------------------------------------------------------------------------------
// state


#define enter_computer_interface(player_index, control_panel_permutation, level_completion_state) \
    (get_terminal_state_for_player(player_index)->enter_computer_terminal((control_panel_permutation), (level_completion_state)))

#define abort_terminal_mode(player_index) \
    (get_terminal_state_for_player((player_index))->exit_computer_terminal(false))

#define initialize_player_terminal_info(player_index) \
    (get_terminal_state_for_player((player_index))->exit_computer_terminal())


#define player_in_terminal_mode(player_index)           (get_terminal_state_for_player((player_index))->is_active)

#define dirty_terminal_view(player_index)               (get_terminal_state_for_player((player_index))->set_dirty())


// -----------------------------------------------------------------------------------------
// packing


#define SIZEOF_player_terminal_data                     (SIZEOF_player_terminal_state)

#define  calculate_packed_terminal_data_length()        (get_bytesize_of_packed_computer_terminals())

#define unpack_map_terminal_data(data, data_length)     (unpack_m2_computer_terminals((data), (data_length)))
#define pack_map_terminal_data(array, count)            (pack_computer_terminals((array), (count)))

#define unpack_player_terminal_data(data, count)        (unpack_player_terminal_state((data), (count)))
#define pack_player_terminal_data(array, count)         (pack_player_terminal_state((array), (count)))


#endif /* computer_interface_h */
