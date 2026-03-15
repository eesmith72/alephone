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

Thursday, December 30, 1993 9:51:24 PM

Tuesday, May 24, 1994 6:42:32 PM
	functions beginning with underscores are mac-specific and always have a corresponding portable
	function which sets up game-specific information (see update_compass and _update_compass for
	an example).
Friday, June 10, 1994 3:56:57 AM
	gutted, rewritten.  Much cleaner now.
Sunday, September 4, 1994 6:32:05 PM
	made scroll_inventory() non-static so that shell.c can call it.
Saturday, September 24, 1994 10:33:19 AM
	fixed some things, twice...
Friday, October 7, 1994 3:13:22 PM
	New interface.  Draws panels from PICT resources for memory consumption.  Inventory is 
	different panels, which are switched to whenever you grab an item.  There is no scrolling.
Tuesday, June 6, 1995 3:37:50 PM
	Marathon II modifications.
Tuesday, August 29, 1995 4:02:15 PM
	Reestablishing a level of portablility...

Feb 4, 2000 (Loren Petrich):
	Added SMG display stuff
	
Apr 30, 2000 (Loren Petrich): Added XML parser object for all the screen_drawing data,
	and also essentiall all the data here.

May 28, 2000 (Loren Petrich): Added support for buffering the Heads-Up Display

Jul 2, 2000 (Loren Petrich):
	The HUD is now always buffered

Jul 16, 2001 (Loren Petrich):
	Using "temporary" as storage space for count_text and weapon_name;
	it is 256 bytes long, which should be more than enough for most text.
	This fixes the long-weapon-name bug.

Mar 08, 2002 (Woody Zenfell):
    SDL network microphone support
*/

#include "cseries.h"

#include "OGL_Headers.h"

#include "hud_definitions.hpp" // hud_has_changed_data_t (and some other crap, probably)
#include "motion_sensor.hpp"

#include "player.h" // current_player_index

#include "game_window.h"

#include "fonts.hpp"
#include "screen.h"

#include "items.h" // NUMBER_OF_ITEM_TYPES and things

#include "shell.h"
#include "preferences.h"
#include "screen.h"
#include "interface.h" // for INTERFACE_PANEL_BASE (aka M2 SW HUD) (previously `#include "screen_definitions.h"`)
#include "images.h"
#include "InfoTree.h"
#include "interface_menus.h"

#include "screen_drawing.h" // parse_mml_


/* --------- globals */

// TODO: move these to HUD-specific

static void set_current_inventory_screen(short player_index, short screen);


struct hud_has_changed_data_t interface_state;


// TODO: want a vector of 0+ HUD classes

//HUD_Class* activeHUD;


/* --------- code */

void initialize_game_window(void)
{
	initialize_motion_sensor();
   // activeHUD = &HUD_SW;
}


void draw_interface(void)
{
	if (alephone::Screen::instance()->openGL())
		return;

	if (!game_window_is_full_screen())
	{
		/* draw the frame */
	//	draw_hud();
	}
		
    RequestDrawingTerm();
}


// updates only what needs changing (time_elapsed==NONE means redraw everything no matter what, but skip the interface frame)
void update_interface(short time_elapsed)
{
	if (time_elapsed == NONE) reset_motion_sensor(current_player_index);
    
	if (alephone::Screen::instance()->openGL() || alephone::Screen::instance()->lua_hud()) return;

    if (game_window_is_full_screen()) return;
    
    // LP addition: don't force an update unless explicitly requested
    bool force_update = (time_elapsed == NONE);
    
    /*
    ensure_HUD_buffer();

    // LP addition: added support for HUD buffer; twit
    _set_port_to_HUD();
    if (HUD_SW.update_everything(time_elapsed))
        force_update = true;
    _restore_port();
    */
    
    // Draw the whole thing if doing so is requested
    // (may need some smart way of drawing only what has to be drawn)
    if (force_update) RequestDrawingHUD();
}


void mark_interface_collections(bool loading)
{
	loading ? mark_collection_for_loading(_collection_interface) : mark_collection_for_unloading(_collection_interface);
}

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

void mark_player_inventory_screen_as_dirty(short player_index, short screen)
{
	struct player_data *player= get_player_data(player_index);

	set_current_inventory_screen(player_index, screen);
	SET_INVENTORY_DIRTY_STATE(player, true);
}

void mark_player_inventory_as_dirty(short player_index, short dirty_item)
{
	struct player_data *player= get_player_data(player_index);

	/* If the dirty item is not NONE, then goto that item kind display.. */
	if(dirty_item != NONE)
	{
		short item_kind= get_item_kind(dirty_item);
		short current_screen= GET_CURRENT_INVENTORY_SCREEN(player);

		/* Don't change if it is a powerup, or you are in the network statistics screen */
		if(item_kind != _powerup && item_kind != current_screen) // && current_screen!=_network_statistics)
		{
			/* Goto that type of item.. */
			set_current_inventory_screen(player_index, item_kind);
		}
	}
	SET_INVENTORY_DIRTY_STATE(player, true);
}

void mark_player_network_stats_as_dirty(short player_index)
{
	if (GET_GAME_OPTIONS()&_live_network_stats)
	{
		struct player_data *player= get_player_data(player_index);
	
		set_current_inventory_screen(player_index, _network_statistics);
		SET_INVENTORY_DIRTY_STATE(player, true);
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
				if(section_count) break; /* Go tho this one! */
			} else {
				/* Network statistics! */
				break; 
			}
		}
		
		current_inventory_screen= test_inventory_screen;
	} else {
		/* Going down.. */
		for(index= mod_value-1; index>0; --index)
		{
			test_inventory_screen= (current_inventory_screen+index)%mod_value;

			assert_fail(test_inventory_screen>=0 && test_inventory_screen<NUMBER_OF_ITEM_TYPES+1, "");			
			if(test_inventory_screen != NUMBER_OF_ITEM_TYPES)
			{
				calculate_player_item_array(current_player_index, test_inventory_screen,
					section_items, section_counts, &section_count);
				if(section_count) break; /* Go tho this one! */
			} else {
				/* Network statistics! */
				break; 
			}
		}		

		current_inventory_screen= test_inventory_screen;
	}
	set_current_inventory_screen(current_player_index, current_inventory_screen);

	SET_INVENTORY_DIRTY_STATE(current_player, true);	
}

static void set_current_inventory_screen(
	short player_index,
	short screen)
{
	struct player_data *player= get_player_data(player_index);
	
	assert_fail(screen>=0 && screen<7, "");
	
	player->interface_flags&= ~INVENTORY_MASK_BITS;
	player->interface_flags|= screen;
	player->interface_decay= 5*TICKS_PER_SECOND;
}

// From sreen_sdl.cpp
extern SDL_Surface *HUD_Buffer; // TODO: entangled in screen_drawing, game_Window, screen; and our new HUD implementation won't use it





extern short vidmasterLevelOffset;
struct weapon_interface_data *original_weapon_interface_definitions = NULL;





// TODO: absolute shambles: a mismash of UI/HUD/terminal rects + colors

void reset_mml_interface()
{
    reset_mml_menu_item_order();
    reset_mml_interface_rectangles();
    reset_mml_interface_colors();
    reset_mml_interface_fonts();
}



void parse_mml_interface(const InfoTree& root) // TODO: separate UI from gameworld 2D rendering
{
    reset_mml_interface();
	
    // TODO: move this
    bool is_active;
    root.read_attr("motion_sensor", is_active);
    set_motion_sensor_active(is_active);
	
    
    parse_mml_interface_rectangles(root);
    parse_mml_interface_colors(root);
    parse_mml_interface_fonts(root);
    
	
	for (const InfoTree &vid : root.children_named("vidmaster"))
	{
        // EES: Stupid and unsafe. 1. MML mods should not be inventing new string resource IDs, and 2. the given ID isn't adequately bounds-checked so is free to stomp on existing (and future!) string sets.
        int16_t resource_id; //
		vid.read_attr_bounded<int16>("strings_index", resource_id, -1, SHRT_MAX);
        if (resource_id != -1 && resource_id != 0 && resource_id != vidmasterStringSetID)
        {
            // TODO: converting the old non-standard vidmaster stringset into new standard vidmaster stringset can be done here if absolutely necessary, but for now just gonna ignore it. Suffice to say, scenarios *should* be able to include both old and new formats to provide compatibility with old and new AO versions... assuming MML is sensible enough to skip over unrecognized stringsets. (Not that "MML" and "sensibly designed" will ever belong in the same sentence. Or even the same parsec.)
            
            log_warning_f("Found a <vidmaster strings_index=%d ...> tag in MML. Custom string-set IDs are unsafe and no longer permitted. Please update your MML to use string-set %d with 3 strings for dialog heading, vidmaster oath, and 'Start at level' text. The vidmaster dialog currently doesn't auto-wrap text but hopefully inserting &#10; in the strings will tell it where to start the next line.\n", resource_id, vidmasterStringSetID);
        }
        
		vid.read_attr_bounded<int16>("level_offset", vidmasterLevelOffset, 0, 1);
	}
	
    
    parse_mml_hud_definitions(root);
}
