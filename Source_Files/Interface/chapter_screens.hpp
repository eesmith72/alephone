/*
 chapter_screens.hpp
 
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

#ifndef chapter_screens_hpp
#define chapter_screens_hpp

#include "cseries.hpp"
#include "app_state.hpp"
#include "interface_support.hpp"


// TODO: once Canvas is complete and exposed as Lua API, the splash, chapter, main menu, credits, etc screens can/should be drawn by a Lua script so that modders can customize presentation (e.g. scrolling credits with music)


ao_err load_screen_sequence(app_state_t screen_type);

uint32_t display_current_screen(); // returns timeout in ticks

ao_err advance_to_next_screen(); // returns no_err/not found // TODO: FIX: need to implement this (including appropriate error codes)



void display_chapter_screen_for_level(short level, bool text_block); // TODO: this needs to go away



#endif /* chapter_screens_hpp */
