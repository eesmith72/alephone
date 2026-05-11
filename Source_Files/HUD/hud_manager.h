/*
 hud_manager.h -- what's left of hud_manager.h

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

// TODO: need a vector of 0+ Lua HUD plugins

// TODO: Classic HUD - Lua script still needs access to the M2 <interface> rects


#ifndef __GAME_WINDOW_H
#define __GAME_WINDOW_H


void initialize_hud_manager();


void scroll_inventory(short dy);

void mark_ammo_display_as_dirty(void);
void mark_shield_display_as_dirty(void);
void mark_oxygen_display_as_dirty(void);
void mark_weapon_display_as_dirty(void);
void mark_player_inventory_screen_as_dirty(short player_index, short screen);
void mark_player_inventory_as_dirty(short player_index, short dirty_item);
void mark_player_network_stats_as_dirty(short player_index);


#endif
