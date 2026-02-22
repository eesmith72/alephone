/*
 terminal_parser_m1.hpp
 
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

#ifndef terminal_parser_m1_hpp
#define terminal_parser_m1_hpp

#include "ComputerTerminal.hpp"


bool unpack_m1_computer_terminal(uint8_t* text, int16_t length, ComputerTerminal& terminal); // returns true on success


#endif /* terminal_parser_m1_hpp */
