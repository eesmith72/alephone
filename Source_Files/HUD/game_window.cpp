/*
GAME_WINDOW.C

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

#include "game_window.h"

//#include "OGL_Headers.h"

#include "hud_definitions.hpp" // hud_has_changed_data_t (and some other crap, probably)
#include "motion_sensor.hpp"

#include "player.h" // current_player_index


//#include "fonts.hpp"
//#include "screen.h"

#include "items.h" // NUMBER_OF_ITEM_TYPES and things

//#include "shell.h"
//#include "preferences.h"
//#include "screen.h"
//#include "interface.h" // for M2_HUD_BACKGROUND_BASE (aka M2 SW HUD) (previously `#include "screen_definitions.h"`)
//#include "images.h"
//#include "InfoTree.h"

//#include "screen_drawing.h" // parse_mml_



// TODO: want a vector of 0+ HUD classes so user can activate Lua plugins for radar+health+manifest, screen messages, crosshairs



void initialize_game_window(void)
{
	initialize_motion_sensor();
   // activeHUD = &HUD_SW;
}


/* EES: Just like that. Pfttt. Gone.
 

void update_interface(short time_elapsed) // really update_hud
{
	if (time_elapsed == NONE) reset_motion_sensor(current_player_index);
    
	if (alephone::Screen::instance()->openGL() || alephone::Screen::instance()->lua_hud() || !alephone::Screen::instance()->hud()) return;
    
    // LP addition: don't force an update unless explicitly requested
    bool force_update = (time_elapsed == NONE);
    
    ensure_HUD_buffer();

    // LP addition: added support for HUD buffer; twit
    _set_port_to_HUD();
    if (HUD_SW.update_everything(time_elapsed))
        force_update = true;
    _restore_port();
    
    // Draw the whole thing if doing so is requested (may need some smart way of drawing only what has to be drawn)
    if (force_update) RequestDrawingHUD();
    
    RequestDrawingTerm(); // temporarily dumped here while straightening out drawing
}
 */


static void set_current_inventory_screen(short player_index, short screen)
{
    assert_fail(screen >= 0 && screen < 7, "");
    
    struct player_data *player= get_player_data(player_index);
    
    player->hud_flags &= ~INVENTORY_MASK_BITS;
    player->hud_flags |= screen;
    player->hud_decay  = 5 * TICKS_PER_SECOND;
}


void mark_player_inventory_screen_as_dirty(short player_index, short screen) // called by recreate_player in player.cpp when entering a level
{
    player_data* player = get_player_data(player_index);

    set_current_inventory_screen(player_index, screen);
    SET_INVENTORY_DIRTY_STATE(player);
}


void mark_interface_collections(bool loading)
{
	loading ? mark_collection_for_loading(_collection_interface) : mark_collection_for_unloading(_collection_interface);
}



struct hud_has_changed_data_t interface_state; // 'dirty' flags; currently unused, though could be exposed to Lua HUD

void mark_weapon_display_as_dirty(void)
{
	interface_state.weapon_is_dirty = true;
}


void mark_ammo_display_as_dirty(void)
{
	interface_state.ammo_is_dirty = true;
}


void mark_shield_display_as_dirty(void)
{
	interface_state.shield_is_dirty = true;
}


void mark_oxygen_display_as_dirty(void)
{
	interface_state.oxygen_is_dirty = true;
}


void mark_player_inventory_as_dirty(short player_index, short dirty_item)
{
	struct player_data *player= get_player_data(player_index);

	// If the dirty item is not NONE, then goto that item kind display.
	if(dirty_item != NONE)
	{
		short item_kind= get_item_kind(dirty_item);
		short current_screen= GET_CURRENT_INVENTORY_SCREEN(player);

		// Don't change if it is a powerup, or you are in the network statistics screen
		if(item_kind != _powerup && item_kind != current_screen) // && current_screen!=_network_statistics)
		{
			// Goto that type of item.
			set_current_inventory_screen(player_index, item_kind);
		}
	}
	SET_INVENTORY_DIRTY_STATE(player);
}



void mark_player_network_stats_as_dirty(short player_index)
{
	if (GET_GAME_OPTIONS()&_live_network_stats)
	{
		struct player_data *player= get_player_data(player_index);
	
		set_current_inventory_screen(player_index, _network_statistics);
		SET_INVENTORY_DIRTY_STATE(player);
	}
}


void scroll_inventory(short dy)
{
	short mod_value, index, current_inventory_screen, section_count, test_inventory_screen = 0;
	short section_items[NUMBER_OF_ITEMS];
	short section_counts[NUMBER_OF_ITEMS];
	
	current_inventory_screen= GET_CURRENT_INVENTORY_SCREEN(current_player);

	if(dynamic_world->player_count>1)
	{
		mod_value= NUMBER_OF_ITEM_TYPES+1;
	} else {
		mod_value= NUMBER_OF_ITEM_TYPES;
	}

	if(dy>0)
	{
		for(index= 1; index<mod_value; ++index)
		{
			test_inventory_screen= (current_inventory_screen+index)%mod_value;
			
			assert_fail(test_inventory_screen>=0 && test_inventory_screen<NUMBER_OF_ITEM_TYPES+1, "");			
			if(test_inventory_screen != NUMBER_OF_ITEM_TYPES)
			{
				calculate_player_item_array(current_player_index, test_inventory_screen,
					section_items, section_counts, &section_count);
				if(section_count) break; // Go tho this one!
			} else {
				// Network statistics!
				break;
			}
		}
		
		current_inventory_screen= test_inventory_screen;
	} else {
		// Going down
		for(index= mod_value-1; index>0; --index)
		{
			test_inventory_screen= (current_inventory_screen+index)%mod_value;

			assert_fail(test_inventory_screen>=0 && test_inventory_screen<NUMBER_OF_ITEM_TYPES+1, "");			
			if(test_inventory_screen != NUMBER_OF_ITEM_TYPES)
			{
				calculate_player_item_array(current_player_index, test_inventory_screen,
					section_items, section_counts, &section_count);
				if(section_count) break; // Go tho this one!
			} else {
				// Network statistics!
				break;
			}
		}		

		current_inventory_screen= test_inventory_screen;
	}
	set_current_inventory_screen(current_player_index, current_inventory_screen);
	SET_INVENTORY_DIRTY_STATE(current_player); // Q. Why is there a dirty inventory flag stored on Player object too? A. replays presumably allow observer to see the original user switch inventory screens while playing (TODO: this'd be better encoded in action state, once it can be increased from 32 bits)
}

