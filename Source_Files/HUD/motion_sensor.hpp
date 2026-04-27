/*
 motion_sensor.h
 
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

#ifndef __MOTION_SENSOR_H
#define __MOTION_SENSOR_H

#include "cseries.h"

#include "scottish_textures.h" // point2d, world_distance, angle
#include "shapes.h"


enum class blip_type_t : int16_t
{
	friendly,     // What you, friendly players, and the Bobs are
	alien,        // What the other critters are
	enemy_player, // What hostile players are
};
#define NUMBER_OF_BLIP_TYPES  (3)


struct blip_info_t {
    blip_type_t mtype;
    int16_t intensity;
    world_distance distance;
    angle direction;
};


// max number of monsters that can appear on radar
#define MAXIMUM_MOTION_SENSOR_ENTITIES  (12)

// length of monster's trail
#define NUMBER_OF_PREVIOUS_LOCATIONS    (6)


struct motion_sensor_definition
{
    uint16 update_frequency;
    uint16 rescan_frequency;
    uint16 range;
    int16 scale;
};

//extern motion_sensor_definition motion_sensor_settings;




void initialize_motion_sensor();

void reset_motion_sensor(short monster_index);


void set_motion_sensor_active(bool is_active);
bool get_motion_sensor_active();


void update_motion_sensor_blips(short time_elapsed);
    

size_t get_motion_sensor_blip_count();

blip_info_t get_motion_sensor_blip(size_t index);




void motion_sensor_scan(); // called in update_world_elements_one_tick of marathon2.cpp


bool motion_sensor_is_dirty(); // TODO: currently unused and may be deleted, unless Lua HUD script wants it






class InfoTree;

void parse_mml_motion_sensor(const InfoTree& root);

void reset_mml_motion_sensor();

#endif
