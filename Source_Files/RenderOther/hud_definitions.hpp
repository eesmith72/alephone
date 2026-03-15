/*
 hud_definitions.hpp
 
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


#ifndef _HUD_RENDERER_H_
#define _HUD_RENDERER_H_

#include "cseries.h"

#include "shapes.h"




#define TEXT_INSET 2
#define NAME_OFFSET 23
#define TOP_OF_BAR_WIDTH 8

#define DELAY_TICKS_BETWEEN_OXYGEN_REDRAW   (2*TICKS_PER_SECOND)
#define RECORDING_LIGHT_FLASHING_DELAY      (TICKS_PER_SECOND)

#define MICROPHONE_STOP_CLICK_SOUND         ((short) 1250)
#define MICROPHONE_START_CLICK_SOUND        ((short) 1280)

#define TOP_OF_BAR_HEIGHT 4

/* ---------- flag macros */

#define INVENTORY_MASK_BITS 0x0007
#define INVENTORY_DIRTY_BIT 0x0010
#define INTERFACE_DIRTY_BIT 0x0020

#define GET_CURRENT_INVENTORY_SCREEN(p) ((p)->interface_flags & INVENTORY_MASK_BITS)

#define INVENTORY_IS_DIRTY(p) ((p)->interface_flags & INVENTORY_DIRTY_BIT)
#define SET_INVENTORY_DIRTY_STATE(p, v) ((void)((v)?((p)->interface_flags|=(uint16)INVENTORY_DIRTY_BIT):((p)->interface_flags&=(uint16)~INVENTORY_DIRTY_BIT)))

#define INTERFACE_IS_DIRTY(p) ((p)->interface_flags & INTERFACE_DIRTY_BIT)
#define SET_INTERFACE_DIRTY_STATE(p, v) ((v)?((p)->interface_flags |= INTERFACE_DIRTY_BIT):(p)->interface_flags &= ~INTERFACE_DIRTY_BIT)

/* ---------- enums */


enum // M2 rsrc-defined colors
{
    _energy_weapon_full_color           = 0,
    _energy_weapon_empty_color          = 1,
    _black_color                        = 2,
    _inventory_text_color               = 3,
    _inventory_header_background_color  = 4,
    _inventory_background_color         = 5,
};


enum // HUD rects (from screen_drawing.h)
    {
    // Replacing the old hardcoded M2 HUD with Hopper's Default_HUD_1.2 means this enum is no longer used in CPP (except for _weapon_display_rect, used twice in lua_hud_objects.cpp). However, Default_HUD_1.2 and other Lua HUDs can still use these built-in/MML-defined rects so these values are still 100% relevant.
    _player_name_rect       = 0,
    _oxygen_rect            = 1,
    _shield_rect            = 2,
    _motion_sensor_rect     = 3,
    _microphone_rect        = 4,
    _inventory_rect         = 5,
    _weapon_display_rect    = 6,
};


enum // these are M2 shape descriptor texture ids
{
	_empty_energy_bar = 0,
	_energy_bar,
	_energy_bar_right,
	_double_energy_bar,
	_double_energy_bar_right,
	_triple_energy_bar,
	_triple_energy_bar_right,
	_empty_oxygen_bar,
	_oxygen_bar,
	_oxygen_bar_right,
	_motion_sensor_mount,
	_motion_sensor_virgin_mount,
	_motion_sensor_alien,
	_motion_sensor_friend = _motion_sensor_alien  + 6,
	_motion_sensor_enemy  = _motion_sensor_friend + 6,
	_network_panel        = _motion_sensor_enemy  + 6,
    
    // weapon_interface_definitions table references these
	_magnum_bullet,
	_magnum_casing,
	_assault_rifle_bullet,
	_assault_rifle_casing,
	_alien_weapon_panel,
	_flamethrower_panel,
	_magnum_panel,
	_left_magnum,
	_zeus_panel,
	_assault_panel,
	_missile_panel,
	_left_magnum_unusable,
	_assault_rifle_grenade,
	_assault_rifle_grenade_casing,
	_shotgun_bullet,
	_shotgun_casing,
	_single_shotgun,
	_double_shotgun,
	_missile,
	_missile_casing,
	
	_network_compass_shape_nw,
	_network_compass_shape_ne,
	_network_compass_shape_sw,
	_network_compass_shape_se,

	_skull,
	
	// LP additions:
	_smg,
	_smg_bullet,
	_smg_casing,

	
	/* These are NOT done. */
	_mike_button_unpressed,
	_mike_button_pressed
};


enum {
	_uses_none,
	_uses_energy,
	_uses_bullets,
};


// mostly used in lua_hud_objects
struct weapon_interface_ammo_data
{
	short type;
	short screen_left;
	short screen_top;
	short ammo_across; /* max energy for beam weapons */
	short ammo_down; /* Unused for energy weapons */
	short delta_x; /* Or width, if uses energy */
	short delta_y; /* Or height if uses energy */
	shape_descriptor bullet;	 /* or fill color index */
	shape_descriptor empty_bullet; /* or empty color index */
	bool right_to_left; /* Which way do the bullets go as they are used? */
};


struct weapon_interface_data 
{
	short item_id;
	shape_descriptor weapon_panel_shape;
	short weapon_name_start_y;
	short weapon_name_end_y;
	short weapon_name_start_x;	/* NONE means center in the weapon rectangle */
	short weapon_name_end_x;	/* NONE means center in the weapon rectangle */
	short standard_weapon_panel_top;
	short standard_weapon_panel_left;
	bool multi_weapon;
	std::array<weapon_interface_ammo_data, 2> ammo_data; // primary and secondary ammo types
	shape_descriptor multiple_shape;
	shape_descriptor multiple_unusable_shape;
	short multiple_delta_x;
	short multiple_delta_y;
};


struct hud_has_changed_data_t //
{
	bool ammo_is_dirty;
	bool weapon_is_dirty;
	bool shield_is_dirty;
	bool oxygen_is_dirty;
};

#define MAXIMUM_WEAPON_INTERFACE_DEFINITIONS (sizeof(weapon_interface_definitions)/sizeof(struct weapon_interface_data))

extern hud_has_changed_data_t interface_state; // currently unused; TODO: where should this go?

extern std::array<weapon_interface_data, 10> weapon_interface_definitions;


struct point2d;

class HUD_Class;
extern HUD_Class* activeHUD; // TODO: this


enum {
    _mask_disabled,
    _mask_enabled,
    _mask_drawing,
    _mask_erasing,
    NUMBER_OF_LUA_MASKING_MODES
};


// MML

class InfoTree;
void reset_mml_hud_definitions();

void parse_mml_hud_definitions(const InfoTree& tree);


#endif
