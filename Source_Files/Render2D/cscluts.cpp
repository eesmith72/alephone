/*
 cscluts_sdl.cpp - CLUT handling, SDL implementation
 
 Written in 2000 by Christian Bauer
 
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

#include "cseries.h"


const rgb_color rgb_black = {0x0000, 0x0000, 0x0000};
const rgb_color rgb_white = {0xffff, 0xffff, 0xffff};


// TODO: why aren't these static-allocated? are they variable length?

struct color_table* uncorrected_color_table = nullptr; // the pristine color environment of the game (can be 16bit)
struct color_table* world_color_table       = nullptr; // the gamma-corrected color environment of the game (can be 16bit)
struct color_table* interface_color_table   = nullptr; // always 8bit, for mixed-mode (i.e., valkyrie) fades
struct color_table* visible_color_table     = nullptr; // the color environment the player sees (can be 16bit)


// EES: saints preserve us... these do eventually get initialized right, way over in Screen::Initialize, but keeping 
short bit_depth             = NONE;
short interface_bit_depth   = NONE;

