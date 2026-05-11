/*
 items.h
 
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

#ifndef __ITEMS_H
#define __ITEMS_H


#include "map.h" // NUMBER_OF_GAME_DIFFICULTY_LEVELS
#include "shapes.h" // shape_descriptor


enum // item categories
{
	_weapon,
	_ammunition,
	_powerup,
	_item,
	_weapon_powerup,
	_ball,
	
	NUMBER_OF_ITEM_CATEGORIES,
	_network_statistics = NUMBER_OF_ITEM_CATEGORIES, // Used in game_window.c; ick
};


enum // item types
{
	_i_knife,
	_i_magnum,
	_i_magnum_magazine,
	_i_plasma_pistol,
	_i_plasma_magazine,
	_i_assault_rifle,
	_i_assault_rifle_magazine,
	_i_assault_grenade_magazine,
	_i_missile_launcher,
	_i_missile_launcher_magazine,
	_i_invisibility_powerup,
	_i_invincibility_powerup,
	_i_infravision_powerup,
	_i_alien_shotgun,
	_i_alien_shotgun_magazine,
	_i_flamethrower,
	_i_flamethrower_canister,
	_i_extravision_powerup,
	_i_oxygen_powerup,
	_i_energy_powerup,
	_i_double_energy_powerup,
	_i_triple_energy_powerup,
	_i_shotgun,
	_i_shotgun_magazine,
	_i_spht_door_key,
	_i_uplink_chip,

	BALL_ITEM_BASE,
	_i_light_blue_ball= BALL_ITEM_BASE,
	_i_red_ball,
	_i_violet_ball,
	_i_yellow_ball,
	_i_brown_ball,
	_i_orange_ball,
	_i_blue_ball, // heh heh
	_i_green_ball,
	
	_i_smg,
	_i_smg_ammo,
	
	NUMBER_OF_ITEM_TYPES // must be <= MAXIMUM_OBJECT_TYPES
};


struct item_definition
{
    int16 item_kind;
    int16 singular_name_id;
    int16 plural_name_id;
    shape_descriptor base_shape;
    int16 maximum_count_per_player;
    int16 invalid_environments;

    // extension to support per-difficulty maximums
    int16 extended_maximum_count[NUMBER_OF_GAME_DIFFICULTY_LEVELS] = { NONE, NONE, NONE, NONE, NONE };

    int16 get_maximum_count_per_player(bool is_m1, int difficulty_level) const;
};


short new_item(struct object_location *location, short item_type);

void calculate_player_item_array(short player_index, short type, short *items, short *counts, short *array_count);
const std::string get_header_name(int16_t type);
const std::string get_item_name(int16_t item_id, bool plural);
bool new_item_in_random_location(short item_type);
short count_inventory_lines(short player_index);
void swipe_nearby_items(short player_index);

short get_item_kind(short item_id);

bool unretrieved_items_on_map(void);
bool item_valid_in_current_environment(short item_type);

void trigger_nearby_items(short polygon_index);
short find_player_ball_color(short player_index); // returns the color of the ball or NONE if they don't have one

short get_item_shape(short item_id);
bool try_and_add_player_item(short player_index, short type);

item_definition *get_item_definition_external(const short type);


short find_player_ball_color(short player_index); // Returns NONE if this player is not carrying a ball

void initialize_items(void);
void animate_items(void);

class InfoTree;
void parse_mml_items(const InfoTree& root);
void reset_mml_items();

#endif

