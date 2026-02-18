/*
 computer_terminal.hpp -- in-game computer terminals
 
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
 
 Tuesday, August 23, 1994 11:25:40 PM (ajr)
 Thursday, May 25, 1995 5:18:03 PM- rewriting.
 
 New paradigm:
 Groups each start with one of the following groups:
 #UNFINISHED, #SUCCESS, #FAILURE
 
 First is shown the:
 #LOGON XXXXX
 
 Then there are any number of groups with:
 #INFORMATION, #CHECKPOINT, #SOUND, #MOVIE, #TRACK
 
 And a final:
 #INTERLEVEL TELEPORT, #INTRALEVEL TELEPORT
 
 Each group ends with:
 #END
 
 Groupings:
 #logon XXXX- login message (XXXX is shape for login screen)
 #unfinished- unfinished message
 #success- success message
 #failure- failure message
 #information- information
 #briefing XX- briefing, then load XX
 #checkpoint XX- Checkpoint xx (associated with goal)
 #sound XXXX- play sound XXXX
 #movie XXXX- play movie XXXX (from Movie file)
 #track XXXX- play soundtrack XXXX (from Music file)
 #interlevel teleport XXX- go to level XXX
 #intralevel teleport XXX- go to polygon XXX
 #pict XXXX- diplay the pict resource XXXX
 
 Special embedded keys:
 $B- Bold on
 $b- bold off
 $I- Italic on
 $i- italic off
 $U- underline on
 $u- underline off
 $- anything else is passed through unchanged
 
*/

#ifndef computer_terminal_hpp
#define computer_terminal_hpp


#include "terminal_actions.hpp"    // process user key presses (scroll, page, cancel)
#include "terminal_renderer.hpp"   // draws to SDL_Surface and sets flag for game loop to render to screen
#include "PlayerTerminalState.hpp" // each player's current terminal login state


#endif /* computer_terminal_hpp */
