/*
 game_event_loop.hpp -- handles gameworld input events
 
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

#ifndef game_event_loop_hpp
#define game_event_loop_hpp

#include "cseries.h"

#include "app_state.hpp"


// in-game event-handling loop
// (extracted from AO's original main event loop, which performed both UI and in-game operations)
void game_event_loop(bool is_restoring_saved_game);


// exit the game event loop and return to app event loop (finished game/revert after dying/jump to new level)
void exit_game_event_loop(app_state_t next_state);


#endif /* game_event_loop_hpp */
