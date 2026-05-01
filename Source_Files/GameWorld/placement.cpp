/*
	PLACEMENT.C

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

#include "player.h" // get_number_of_players
#include "map.h"
#include "monsters.h"
#include "items.h"
#include "compatibility_profiles.h"

#define NUMBER_OF_TICKS_BETWEEN_RECREATION  (15 * TICKS_PER_SECOND)
#define INVISIBLE_RANDOM_POINT_RETRIES      (10)


// Placement frequencies for each type of object in map. (This is packed in WAD as 64-item array of 32 items followed by 32 monsters.) // TODO: double-check this
// Caution: it should be possible to increase the limit on monster types, but changing number of item types will break existing
// Lua scripts, as the Lua API treats both items and monsters as a single array of 'items'; see get_placement_info in lua_objects.cpp
std::array<object_frequency_definition, MAXIMUM_OBJECT_TYPES> item_placement_info;
std::array<object_frequency_definition, MAXIMUM_OBJECT_TYPES> monster_placement_info;



static void _recreate_objects(short object_type, short max_object_types, struct object_frequency_definition *placement_info, short *object_counts, short *random_counts);
static void add_objects(short object_class, short object_type, short count, bool is_initial_drop);
static bool pick_random_initial_location_of_type(short saved_type, short type, struct object_location *location);
static short pick_random_facing(short polygon_index, world_point2d *location);
static bool choose_invisible_random_point(short *polygon_index, world_point2d *p, short object_type, bool initial_drop);
static bool polygon_is_valid_for_object_drop(world_point2d *location, short polygon_index, short object_type, bool initial_drop, bool is_random_location);


void unpack_placement_data(uint8_t* Stream, size_t count)
{
    // if count is 0 (buggy map data, or possibly a level with no placed objects), these must still be cleared (the resulting level won't have any objects [except player?])
    item_placement_info.fill({});
    monster_placement_info.fill({});
    
    log_note_f("Expected <=%zu monster types, found %d", monster_placement_info.size(), NUMBER_OF_MONSTER_TYPES);
    log_note_f("Expected <=%zu item types, found %d", item_placement_info.size(), NUMBER_OF_ITEM_TYPES);
    
    Stream = unpack_object_frequency_definition(Stream, item_placement_info.data(), item_placement_info.size());
    Stream = unpack_object_frequency_definition(Stream, monster_placement_info.data(), monster_placement_info.size());
    
    monster_placement_info[0] = {}; // Clears the data for monster #0, the Marine
    
#ifdef DEBUG
	{
		if (monster_placement_info[_monster_marine].initial_count > 0 || monster_placement_info[_monster_marine].minimum_count > 0
            || ((monster_placement_info[_monster_marine].random_count > 0 || monster_placement_info[_monster_marine].random_count == NONE)
                && monster_placement_info[_monster_marine].random_chance > 1))
		{
            throw_bug_report_f("placement data would drop marine: %d", 0);
		}
        for (short i = 1; i < monster_placement_info.size(); i++)
		{
			if (monster_placement_info[i].initial_count < 0) throw_bug_report_f("bad monster initial count: %d", i);
			if (monster_placement_info[i].minimum_count < 0) throw_bug_report_f("bad monster minimum count: %d", i);
			if (monster_placement_info[i].maximum_count < 0) throw_bug_report_f("bad monster maximum count: %d", i);
		}
        for (short i = 0; i < item_placement_info.size(); i++)
		{
			if (item_placement_info[i].initial_count < 0) throw_bug_report_f("bad item initial count: %d", i);
			if (item_placement_info[i].minimum_count < 0) throw_bug_report_f("bad item minimum count: %d", i);
			if (item_placement_info[i].maximum_count < 0) throw_bug_report_f("bad item maximum count: %d", i);
		}
	}
#endif
}


// This places items and monsters on the map
void initialize_items_and_monsters()
{
    dynamic_world.current_civilian_count      = 0; // presumably adding Bob monsters increments this
    dynamic_world.current_civilian_causalties = 0;
    
    for (int16_t i = 0; i< item_placement_info.size(); i++)
    {
        if (item_placement_info[i].initial_count)
        {
            add_objects(_object_is_item, i, item_placement_info[i].initial_count, true);
        }
        dynamic_world.random_items_left[i] = item_placement_info[i].random_count;
    }
    
    for (int16_t i = 1; i < monster_placement_info.size(); i++)
    {
        if (monster_placement_info[i].initial_count && (!film_profile.initial_monster_fix || GET_GAME_OPTIONS() & _monsters_replenish))
        {
            add_objects(_object_is_monster, i, monster_placement_info[i].initial_count, true);
        }
        dynamic_world.random_monsters_left[i] = monster_placement_info[i].random_count;
    }
}


/*************************************************************************************************
 *
 * Function: mark_all_monster_collections
 * Purpose:  this needs to be called when a map is loaded to make sure the necessary collections
 *           are loaded.
 *
 *************************************************************************************************/
void mark_all_monster_collections(bool loading)
{
	
    for (short index= 1; index < monster_placement_info.size(); index++)
	{
        object_frequency_definition* placement_info = &monster_placement_info[index];
        
		if (placement_info->initial_count > 0 || placement_info->minimum_count > 0
            || ((placement_info->random_count > 0 || placement_info->random_count == NONE)
                && placement_info->random_chance > 1))
		{
			mark_monster_collections(index, loading);
		}
		placement_info++;
	}
}


void load_all_monster_sounds()
{
	for (short index= 1; index < monster_placement_info.size(); index++)
	{
        object_frequency_definition* placement_info = &monster_placement_info[index];
        
		if (placement_info->initial_count > 0 || placement_info->minimum_count > 0
            || ((placement_info->random_count > 0 || placement_info->random_count == NONE)
                && placement_info->random_chance > 1))
		{
			load_monster_sounds(index);
		}
		placement_info++;
	}
}


/*************************************************************************************************
 *
 * Function: recreate_objects
 * Purpose:  this needs to be called periodically (probably from update_world() in marathon.c)
 *           it will periodically create new objects (monsters (not players) and items) if they
 *           need to be recreated.
 *
 *************************************************************************************************/
void recreate_objects()
{
	static int32 delay = 0;

	/* If time goes backwards, it means that they started a new game.  Therefore we must */
	/*  reset our delay. */
	if (dynamic_world.tick_count < delay) delay = 0;
	
	if (dynamic_world.tick_count - delay > NUMBER_OF_TICKS_BETWEEN_RECREATION)
	{
		delay= dynamic_world.tick_count;

		if (GET_GAME_OPTIONS()&_monsters_replenish)
		{
            _recreate_objects(_object_is_monster, monster_placement_info.size() - 1, monster_placement_info.data() + 1,
                              dynamic_world.current_monster_count, dynamic_world.random_monsters_left);
		}
		
        _recreate_objects(_object_is_item, item_placement_info.size(), item_placement_info.data(),
                          dynamic_world.current_item_count, dynamic_world.random_items_left);
	}
}

/*************************************************************************************************
 *
 * Function: object_was_just_added
 * Purpose:  when an object (monster or item) is created, (probably by new_monster() or new_item()
 *           this function is called. it will update my structures keep track of how many items
 *           there are.
 *
 *************************************************************************************************/
void object_was_just_added(
	short object_class, 
	short object_type)
{
	assert_fail(object_type >= 0 && object_type < MAXIMUM_OBJECT_TYPES, "");
	switch(object_class)
	{
		case _object_is_monster:
			dynamic_world.current_monster_count[object_type]++;
			break;
			
		case _object_is_item:
			dynamic_world.current_item_count[object_type]++;
			break;
			
		default:
            throw_ao_exception_f("invalid object_class: %x", 1, object_class);
			break;
	}
}

/*************************************************************************************************
 *
 * Function: object_was_just_destroyed
 * Purpose:  when an object (monster or item) is destroyed, this function is called. it will 
 *           update my structures keep track of how many items there are. it will also add
 *           a new item if that is necessary.
 *
 *************************************************************************************************/
void object_was_just_destroyed(short object_class, short object_type)
{
	short diff = 0;
	
	assert_fail(object_type >= 0 && object_type < MAXIMUM_OBJECT_TYPES, "");
	
	switch (object_class)
	{
		case _object_is_monster:
			dynamic_world.current_monster_count[object_type]--;
            if (GET_GAME_OPTIONS() & _monsters_replenish)
            {
                diff = monster_placement_info[object_type].minimum_count - dynamic_world.current_monster_count[object_type];
            }
			break;
			
		case _object_is_item:
			// we need to make this check because we might have destroyed an item
			// that the user was holding, but that item has a current count of 0 because
			// we never placed any on the map.
			if (dynamic_world.current_item_count[object_type]) dynamic_world.current_item_count[object_type]--;
			diff = item_placement_info[object_type].minimum_count - dynamic_world.current_item_count[object_type];
			break;
			
		default:
            throw_ao_exception_f("bad object class: %x", 1, object_class);
			break;
	}
	
	if (diff > 0)
	{
		add_objects(object_class, object_type, 1, false);
	}
}

/*************************************************************************************************
 *
 * Function: get_random_player_starting_location_and_facing
 * Purpose:  returns a good place for the player to start.
 *
 *************************************************************************************************/
short get_random_player_starting_location_and_facing(short max_player_index, short team, object_location *location)
{
	int32 monster_distance, player_distance;
	uint32 best_distance;
	short starting_location_index, maximum_starting_locations, offset, index = NONE, best_index = NONE;
	struct object_location current_location;
	
	maximum_starting_locations= get_number_of_players_starting_location_and_facing(team, 0);
	
	// if it's a team game, and there are no starts, just pick one at random
	if (maximum_starting_locations == 0)
    {
		team = NONE;
		maximum_starting_locations = get_number_of_players_starting_location_and_facing(team, 0);
	}

	offset= global_random() % maximum_starting_locations;
	best_distance= 0;
	
	for (starting_location_index= 0; starting_location_index<maximum_starting_locations; starting_location_index++)
	{
		index = get_player_starting_location_and_facing(team, (starting_location_index + offset) % maximum_starting_locations, current_location);

		/* Determine the distances to the nearest monster and player */
		point_is_player_visible(max_player_index, current_location.polygon_index, (world_point2d *)&current_location.p, &player_distance);
		point_is_monster_visible(current_location.polygon_index, (world_point2d *)&current_location.p, &monster_distance);
		
		if (monster_distance != 0 && player_distance != 0)
		{
			uint32 combined_distance = 1LL*player_distance + (monster_distance>>1); // [0, ~2^31.6]; weight player distance more heavily.

			if (combined_distance > best_distance)
			{
				best_index = index;
				best_distance = combined_distance;
				*location= current_location;
			}
		}
	}
	
	/* in the extremely unlikely event that there is a player or monster exactly on every location, punt */
	if (best_index == NONE)
	{
		best_index = index;
		*location= current_location;
	}

	return best_index;
}

/*------------------------------------------------------------------------------------------------
 *
 *                                   Private Functions
 *
 *-----------------------------------------------------------------------------------------------*/

/*************************************************************************************************
 *
 * Function: _recreate_objects
 * Purpose:  called by recreate objects, to do the actual recreation (if necessary). 
 *           This is a separate function so that it can handle both items and monsters in one loop
 *
 *************************************************************************************************/
static void _recreate_objects(
	short object_type,    // _object_is_monster or _object_is_item
	short max_object_types,  // how many monsters or objects are defined
	struct object_frequency_definition *placement_info, // from global array, probably.
	short *object_counts, // from dynamic_world
	short *random_counts) // from dynamic_world
{
	short index;
	short objects_to_add;
	bool add_random;
	struct object_frequency_definition *indexed_placement_info= placement_info;
	
	assert_fail(max_object_types<=MAXIMUM_OBJECT_TYPES, "");
	
	// it's time to check if we want to add new things.
	for (index= object_type==_object_is_monster ? 1 : 0; index < max_object_types; index++)
	{
		/* Make sure that we are at the minimum */
		objects_to_add = indexed_placement_info->minimum_count - object_counts[index];
		if (objects_to_add < 0) objects_to_add = 0;

		/* Should we add a random one? */
		if ((indexed_placement_info->random_count == NONE || random_counts[index] > 0)
			&& object_counts[index] + objects_to_add < indexed_placement_info->maximum_count
			&& global_random() < indexed_placement_info->random_chance)
		{
			add_random = true;
			objects_to_add++;
		} else {
			add_random= false;
		}

		/* If we need to add any.. */		
		if (objects_to_add)
		{
			add_objects(object_type, index, objects_to_add, false);
			
			/* If we added a random, and the random_count is not NONE (which means infinite.) */
			if (add_random && indexed_placement_info->random_count != NONE) 
			{
				random_counts[index]--;
			}
		}

		indexed_placement_info++;
	}
}

/*************************************************************************************************
 *
 * Function: add_objects
 * Purpose:  This adds an object (monster or items) as many times as specified.
 *
 *************************************************************************************************/
static void add_objects(short object_class, short object_type, short count, bool is_initial_drop)
{
	assert_fail(object_class == _object_is_item || object_class == _object_is_monster, "");
	
    short saved_type = (object_class == _object_is_item) ? _saved_item : _saved_monster;
    
	short flags = (object_class == _object_is_item) ? item_placement_info[object_type].flags : monster_placement_info[object_type].flags;
    
	for (short i = 0; i < count; i++)
    {
        object_location location = {};
		location.polygon_index = NONE; //This is unnecessary, but for psychological benefits
		
		bool need_random_location= false;
		if (is_initial_drop || !(flags & _reappears_in_random_location))
		{
			if (!pick_random_initial_location_of_type(saved_type, object_type, &location))
			{
				if (is_initial_drop && (flags & _reappears_in_random_location)) need_random_location = true;
				else continue;
			}
		}
		else 
		{
			need_random_location= true;
		}
		
		if (need_random_location)
		{
			if (choose_invisible_random_point(&location.polygon_index, (world_point2d *)&location.p, object_class, is_initial_drop))
			{
				if (object_class == _object_is_monster) 
				{
					location.yaw= pick_random_facing(location.polygon_index, (world_point2d *)&location.p);
				}
			}
			else
			{
				continue;
			}
		}
		
		if (object_class == _object_is_item)
		{
			new_item(&location, object_type);
		}
		else
		{
			short monster_index= new_monster(&location, object_type);
			
			if (monster_index!=NONE && !is_initial_drop) 
			{
				activate_monster(monster_index);
				find_closest_appropriate_target(monster_index, true);
			}
		}
	}
}

/*************************************************************************************************
 *
 * Function: pick_random_initial_location_of_type
 * Purpose:  this picks a place to place an object (monster or item). It picks a pre-defined
 *           starting location, but it will pick a random one.
 * Note:     this unfortunately needs _saved_item or _saved_monster instead of 
 *           _object_is_item or _object_is_monster
 *
 *************************************************************************************************/
static bool pick_random_initial_location_of_type(short saved_type, short type, object_location *location)
{
	bool found_location = false;
	
	int16_t actual_type = (saved_type == _saved_item) ? _object_is_item : _object_is_monster;
    int16_t max = SavedObjectList.size();
	int16_t index = global_random() % max;
	
	for (int16_t i = 0; i < max; i++)
	{
        map_object* saved_object = &SavedObjectList[index];
		
		if (saved_object->type == saved_type && saved_object->index == type)
		{
			location->p             = saved_object->location;
			location->polygon_index = saved_object->polygon_index;
			location->yaw           = saved_object->facing;
			location->flags         = saved_object->flags;

			if (polygon_is_valid_for_object_drop((world_point2d *)&location->p, location->polygon_index, actual_type, true, false))
			{
				found_location = true;
				break;
			}
		}
		index = (index + 1) % max;
	} 
	
	return found_location;
}

/*************************************************************************************************
 *
 * Function: pick_random_facing
 * Purpose:  attempt to pick a good facing for an object. this is used so that monsters don't
 *           end up pointing at a wall, when pointing out into the open is a better idea.
 *
 *************************************************************************************************/
static short pick_random_facing(
	short polygon_index, 
	world_point2d *location)
{
	short          i;
	short          facing;
	short          new_polygon_index;
	world_point2d  end_point;
	
	facing= global_random() % NUMBER_OF_ANGLES;
	for (i= 0; i<(FULL_CIRCLE/QUARTER_CIRCLE); i++)
	{
		end_point = *location;
		translate_point2d(&end_point, WORLD_ONE, facing);
		new_polygon_index = find_new_object_polygon(location, &end_point, polygon_index);
		if (new_polygon_index != NONE)
			break;
		facing = NORMALIZE_ANGLE(facing + QUARTER_CIRCLE);	
	}
	
	return facing;
}

/*************************************************************************************************
 *
 * Function: choose_invisible_random_point
 * Purpose:  Find a place that's a good place to drop an object.
 *
 *************************************************************************************************/
static bool choose_invisible_random_point(
	short *polygon_index, 
	world_point2d *p, 
	short object_type, 
	bool initial_drop)
{
	short retries;
	bool found_legal_point= false;
	
	for (retries = 0; retries < INVISIBLE_RANDOM_POINT_RETRIES && !found_legal_point; ++retries)
	{
		short random_polygon_index = global_random() % PolygonList.size();

		find_center_of_polygon(random_polygon_index, p);
		if(polygon_is_valid_for_object_drop(p, random_polygon_index, object_type, initial_drop, true))
		{
			*polygon_index = random_polygon_index;
			found_legal_point = true;
		}
	}

	return found_legal_point;
}

/*************************************************************************************************
 *
 * Function: polygon_is_valid_for_object_drop
 * Purpose:  decide if we can drop an object in this polygon.
 *
 *************************************************************************************************/
static bool polygon_is_valid_for_object_drop(
	world_point2d *location,
	short polygon_index,
	short object_type,
	bool initial_drop,
	bool is_random_location)
{
	struct polygon_data *polygon = get_polygon_data(polygon_index);
	bool valid = false;
	int32 distance; // only used to call point_is_player_visible()

	(void) (initial_drop);
	
	switch (polygon->type)
	{
		case _polygon_is_item_impassable:
			if (object_type==_object_is_item && !is_random_location)
			{
				valid= false;
				break;
			}
		case _polygon_is_monster_impassable:
		case _polygon_is_platform:
		case _polygon_is_teleporter:
			if (is_random_location)
			{
				valid= false;
				break;
			}
			
		default:
			if (!POLYGON_IS_DETACHED(polygon))
			{
				if (!point_is_player_visible(get_number_of_players(), polygon_index, location, &distance) || initial_drop)
				{
					short object_index= polygon->first_object;
					
					valid= true;
					while (object_index!=NONE && valid)
					{
						struct object_data *object = get_object_data(object_index);
						
						if (is_random_location)
						{
							switch (object_type)
							{
								case _object_is_item:
									switch (GET_OBJECT_OWNER(object))
									{
										case _object_is_item:
											valid= false;
									}
									break;
									
								case _object_is_monster:
									switch (GET_OBJECT_OWNER(object))
									{
										case _object_is_projectile:
										case _object_is_monster:
										case _object_is_effect:
											valid= false;
									}
									break;
									
								default:
                                    throw_ao_exception_f("bad object type: %x", 1, object_type);
									break;
							}
						}
						else
						{
							if (object->location.x==location->x && object->location.y==location->y)
							{
								valid= false;
							}
						}
						
						object_index= object->next_object;
					}
				}
			}
	}

	return valid;
}
