/*
 motion_sensor.cpp
 
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

/*
??monsters which translate only on frame changes periodically drop off the motion sensor making it appear jumpy

//when entities first appear on the sensor they are initially visible
//entities are drawn one at a time, so the dark spot from one entity can cover the light spot from another
//sometimes the motion sensor shapes are not masked correctly (fixed from SHAPES.C)
//must save old points in real world coordinates because entities get redrawn when the player turns
*/



#include "motion_sensor.hpp"

#include "map.h"
#include "monsters.h" // MonsterList
#include "render.h"
#include "interface.h"
#include "player.h"
#include "network_games.h"
#include "InfoTree.h"

//#include "HUDRenderer.h"


// -----------------------------------------------------------------------------------------
// M2 default settings


static const blip_type_t monster_blip_types_std[NUMBER_OF_MONSTER_TYPES] = // this is the M2 default which can be overridden by MML
{
	// Marine
	blip_type_t::friendly, // pay no heed to this entry as a Marine can be Friend or Enemy; get_blip_type_for_monster will decide
	// Ticks
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// S'pht
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// Pfhor
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// Bob
	blip_type_t::friendly,
	blip_type_t::friendly,
	blip_type_t::friendly,
	blip_type_t::friendly,
	// Drone
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// Cyborg
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// Enforcer
	blip_type_t::alien,
	blip_type_t::alien,
	// Hunter
	blip_type_t::alien,
	blip_type_t::alien,
	// Trooper
	blip_type_t::alien,
	blip_type_t::alien,
	// Big Cyborg, Hunter
	blip_type_t::alien,
	blip_type_t::alien,
	// F'lickta
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// S'pht'Kr
	blip_type_t::alien,
	blip_type_t::alien,
	// Juggernauts
	blip_type_t::alien,
	blip_type_t::alien,
	// Tiny ones
	blip_type_t::alien,
	blip_type_t::alien,
	blip_type_t::alien,
	// VacBobs
	blip_type_t::friendly,
	blip_type_t::friendly,
	blip_type_t::friendly,
	blip_type_t::friendly,
};


static const motion_sensor_definition motion_sensor_settings_std = {
	5,  // update_frequency
	15, // rescan_frequency
	(8 * WORLD_ONE), // range
	64 // scale
};


struct tracked_monster_t
{
    uint16 flags; /* [slot_used.1] [slot_being_removed.1] [unused.14] */
    
    short monster_index;
    shape_descriptor shape;
    blip_type_t display_type;
    
    short remove_delay; /* only valid if this entity is being removed [0,NUMBER_OF_PREVIOUS_LOCATIONS) */
    
    point2d previous_points[NUMBER_OF_PREVIOUS_LOCATIONS];
    bool visible_flags[NUMBER_OF_PREVIOUS_LOCATIONS];
    
    world_point3d last_location;
    angle last_facing;
};


// an entity can’t just be jerked from the array, because his signal should fade out, so we
// mark him as ‘being removed’ and wait until his last signal fades away to actually remove him
#define SLOT_IS_BEING_REMOVED_BIT       (0x4000)
#define SLOT_IS_BEING_REMOVED(e)        ((e)->flags &  (uint16_t)SLOT_IS_BEING_REMOVED_BIT)
#define MARK_SLOT_AS_BEING_REMOVED(e)   ((e)->flags |= (uint16_t)SLOT_IS_BEING_REMOVED_BIT)



#define OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(o) true

#define FLICKER_FREQUENCY 0xe

#define MOTION_SENSOR_SIDE_LENGTH (123)


// -----------------------------------------------------------------------------------------
// active settings (defaults and/or MML-defined)

short motion_sensor_player_index; // the player whose screen we are viewing (normally local player, except in replays)


tracked_monster_t tracked_monsters[MAXIMUM_MOTION_SENSOR_ENTITIES]; // tracks monsters in range

static bool motion_sensor_is_dirty;

static int32 ticks_since_last_update, ticks_since_last_rescan;




std::vector<blip_info_t> m_blips; // set by update_motion_sensor_blips;


static blip_type_t monster_blip_types[NUMBER_OF_MONSTER_TYPES];

motion_sensor_definition motion_sensor_settings; // TODO: scale is used in HUDRenderer::draw_entity_blip



bool is_motion_sensor_active = true;

void set_motion_sensor_active(bool is_active)
{
    if (is_active != is_motion_sensor_active)
    {
        is_motion_sensor_active = is_active;
        motion_sensor_is_dirty = true;
    }
}

bool get_motion_sensor_active()
{
    return is_motion_sensor_active;
}


// -----------------------------------------------------------------------------------------
// private


void erase_all_entity_blips()
{
    object_data* owner_object = get_object_data(get_player_data(motion_sensor_player_index)->object_index);
    
	// first erase all locations where the entity changed locations and then did not change locations, and erase its last location
	for (int32_t entity_index = 0; entity_index < MAXIMUM_MOTION_SENSOR_ENTITIES; entity_index++)
	{
        tracked_monster_t* entity = &tracked_monsters[entity_index];
        
		if (SLOT_IS_USED(entity))
		{
            motion_sensor_is_dirty = true;
			// see if our monster slot is free; if it is mark this entity as being removed; of course this isn’t wholly accurate
            // and we might start tracking a new monster which has been placed in our old monster’s slot, but we eat that chance
			if (SLOT_IS_USED(&MonsterList.at(entity->monster_index)))
			{
                object_data* object = get_object_data(get_monster_data(entity->monster_index)->object_index);
				world_distance distance = guess_distance2d((world_point2d*)&object->location, (world_point2d*)&owner_object->location);
				
				// verify that we’re still in range (and mark us as being removed if we’re not
				if (distance>motion_sensor_settings.range || !OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(object))
				{
					MARK_SLOT_AS_BEING_REMOVED(entity);
				}
			}
			else
			{
				MARK_SLOT_AS_BEING_REMOVED(entity);
			}

			// adjust the arrays to make room for new entries
			memmove(entity->visible_flags+1, entity->visible_flags, (NUMBER_OF_PREVIOUS_LOCATIONS-1)*sizeof(bool));
			memmove(entity->previous_points+1, entity->previous_points, (NUMBER_OF_PREVIOUS_LOCATIONS-1)*sizeof(point2d));
			entity->visible_flags[0]= false;
				
			// if we’re not being removed, make room for a new location and calculate it
			if (!SLOT_IS_BEING_REMOVED(entity))
			{
				struct monster_data *monster= get_monster_data(entity->monster_index);
				struct object_data *object= get_object_data(monster->object_index);
				
				// remember if this entity is visible or not
				if (object->transfer_mode!=_xfer_invisibility && object->transfer_mode!=_xfer_subtle_invisibility &&
					(!(static_world->environment_flags&_environment_magnetic) || !((dynamic_world->tick_count+4*monster->object_index)&FLICKER_FREQUENCY)))
				{
					if (object->location.x!=entity->last_location.x || object->location.y!=entity->last_location.y ||
						object->location.z!=entity->last_location.z || object->facing!=entity->last_facing)
					{
						entity->visible_flags[0]= true;
	
						entity->last_location= object->location;
						entity->last_facing= object->facing;
					}
				}
				
				// calculate the 2d position on the motion sensor
				entity->previous_points[0].x= object->location.x;
				entity->previous_points[0].y= object->location.y;
				transform_point2d((world_point2d *)&entity->previous_points[0], (world_point2d *)&owner_object->location, NORMALIZE_ANGLE(owner_object->facing+QUARTER_CIRCLE));
				//entity->previous_points[0].x>>= motion_sensor_settings.scale;
				entity->previous_points[0].x /= motion_sensor_settings.scale;
			//	entity->previous_points[0].y>>= motion_sensor_settings.scale;
				entity->previous_points[0].y /= motion_sensor_settings.scale;
			}
			else
			{
				// if this is the last point of an entity which was being removed; mark it as unused
				if ((entity->remove_delay+= 1)>=NUMBER_OF_PREVIOUS_LOCATIONS)
				{
					MARK_SLOT_AS_FREE(entity);
				}
			}
		}
	}
}


static blip_type_t get_blip_type_for_monster(short monster_index)
{
    monster_data* monster = get_monster_data(monster_index);
    
    if (MONSTER_IS_PLAYER(monster)) // a player, who may be self/friend/enemy
    {
        player_data* marine = get_player_data(monster_index_to_player_index(monster_index));
        player_data* owner  = get_player_data(motion_sensor_player_index);
        
        return GET_GAME_TYPE() == _game_of_cooperative_play ||
               (marine->team == owner->team && !(GET_GAME_OPTIONS() & _force_unique_teams)) ? blip_type_t::friendly : blip_type_t::enemy_player;
    }
    else
    {
        return monster_blip_types[monster->type];
    }
}


/* if we find an entity that is being removed, we continue with the removal process and ignore
	the new signal; the new entity will probably not be added to the sensor again for a full
	second or so (the range should be set so that this is reasonably hard to do) */
static short add_motion_sensor_blip(short monster_index)
{
    tracked_monster_t* entity = tracked_monsters;
    int32_t entity_index = 0;
	int16_t best_unused_index = NONE;
    
	for (;entity_index < MAXIMUM_MOTION_SENSOR_ENTITIES; entity_index++, entity++)
	{
		if (SLOT_IS_USED(entity))
		{
			if (entity->monster_index == monster_index) break;
		}
		else if (best_unused_index == NONE)
        {
            best_unused_index = entity_index;
		}
	}

	if (entity_index == MAXIMUM_MOTION_SENSOR_ENTITIES) // not found; add new entity if we can
	{
		if (best_unused_index != NONE)
		{
            monster_data *monster = get_monster_data(monster_index);
            object_data *object = get_object_data(monster->object_index);

			entity= tracked_monsters + best_unused_index;
			
			entity->flags         = 0;
			entity->monster_index = monster_index;
            entity->display_type  = get_blip_type_for_monster(monster_index);
			            
			for (int32_t i = 0; i < NUMBER_OF_PREVIOUS_LOCATIONS; i++)
            {
                entity->visible_flags[i] = false;
            }
            
			entity->last_location = object->location;
			entity->last_facing   = object->facing;
			entity->remove_delay  = 0;
			MARK_SLOT_AS_USED(entity);
		}
		
		entity_index = best_unused_index;
	}
	
	return entity_index;
}


// -----------------------------------------------------------------------------------------
// public


void initialize_motion_sensor() // called once when starting app/hub
{
    memset(tracked_monsters, 0, sizeof(tracked_monsters));
    
    reset_mml_motion_sensor();
}


// should be called before the motion sensor is used, but after its shapes are loaded (because it will do bitmap copying)
void reset_motion_sensor(short player_index)
{
    motion_sensor_player_index = player_index;
    ticks_since_last_update = ticks_since_last_rescan = 0;
    
    for (int32_t i = 0; i < MAXIMUM_MOTION_SENSOR_ENTITIES; i++) { MARK_SLOT_AS_FREE(tracked_monsters + i); }
}


// the interface code will call this function and only draw the motion sensor if we return true
bool motion_sensor_has_changed()
{
    bool changed = motion_sensor_is_dirty;
    motion_sensor_is_dirty = false;
    return changed;
}



// regardless of frame rate, this is called from update_world_elements_one_tick to update the positions of the objects we are tracking

void motion_sensor_scan()
{
    if (!is_motion_sensor_active || (GET_GAME_OPTIONS() & _motion_sensor_does_not_work) || m1_solo_player_in_terminal()) { return; }

    object_data* owner_object = get_object_data(get_player_data(motion_sensor_player_index)->object_index);

    // if we need to scan for new objects, flood around the owner monster looking for other, visible monsters within our range
    if ((--ticks_since_last_rescan) < 0)
    {
        for (monster_data& monster : MonsterList)
        {
            if (SLOT_IS_USED(&monster) && (MONSTER_IS_PLAYER(&monster) || MONSTER_IS_ACTIVE(&monster)))
            {
                object_data* object = get_object_data(monster.object_index);
                world_distance distance = guess_distance2d((world_point2d*)&object->location, (world_point2d*)&owner_object->location);
                
                if (distance < motion_sensor_settings.range && OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(object))
                {
                    add_motion_sensor_blip(object->permutation);
                    motion_sensor_is_dirty = true;
                }
            }
        }
        ticks_since_last_rescan = motion_sensor_settings.rescan_frequency;
    }

    if ((--ticks_since_last_update) < 0)
    {
        erase_all_entity_blips();
        ticks_since_last_update = motion_sensor_settings.update_frequency;
    }
}


// EES: this is called from Lua HUD, though I'm unclear the distinction between this and motion_sensor_scan; Q. would it be better to give Lua access to

void update_motion_sensor_blips(short time_elapsed)
{
    // If we need to update the motion sensor, draw all active entities
    m_blips.clear();
    
    for (int32_t intensity = NUMBER_OF_PREVIOUS_LOCATIONS - 1; intensity >= 0; intensity--)
    {
        for (int32_t entity_index = 0; entity_index < MAXIMUM_MOTION_SENSOR_ENTITIES; entity_index++)
        {
            tracked_monster_t* entity = &tracked_monsters[entity_index];
            if (SLOT_IS_USED(entity) && entity->visible_flags[intensity])
            {
                blip_type_t mtype = entity->display_type;
                
                world_point2d origin, target;
                origin.x = origin.y = 0;
                target.x = entity->previous_points[intensity].x * motion_sensor_settings.scale;;
                target.y = entity->previous_points[intensity].y * motion_sensor_settings.scale;
                
                blip_info_t info;
                info.mtype = mtype;
                info.intensity = intensity;
                info.distance = distance2d(&origin, &target);
                info.direction = arctangent(target.x, target.y);
                
                m_blips.push_back(info);
            }
        }
    }
}


// called from Lua HUD

size_t get_motion_sensor_blip_count()
{
    return m_blips.size();
}


blip_info_t get_motion_sensor_blip(size_t index)
{
    return m_blips[index];
}


// -----------------------------------------------------------------------------------------
// MML


void reset_mml_motion_sensor()
{
    motion_sensor_settings = motion_sensor_settings_std;
    memcpy(monster_blip_types, monster_blip_types_std, sizeof(monster_blip_types));
}


void parse_mml_motion_sensor(const InfoTree& root)
{
    reset_mml_motion_sensor();
    
    root.read_attr("scale", motion_sensor_settings.scale);
    short range;
    if (root.read_wu("range", range))
        motion_sensor_settings.range = range;
    root.read_attr("update_frequency", motion_sensor_settings.update_frequency);
    root.read_attr("rescan_frequency", motion_sensor_settings.rescan_frequency);
    
    for (const InfoTree &assign : root.children_named("assign"))
    {
        int16_t index, mtype;
        if (assign.read_indexed("monster", index, NUMBER_OF_MONSTER_TYPES))
        {
            assign.read_indexed("type", mtype, NUMBER_OF_BLIP_TYPES);
            monster_blip_types[index] = (blip_type_t)mtype;
        }
    }
}
