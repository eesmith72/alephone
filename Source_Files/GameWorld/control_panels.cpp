/*
DEVICES.C

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

#include "map.h"
#include "monsters.h"
#include "interface.h"
#include "player.h"
#include "platforms.h"
#include "SoundManager.h"
#include "computer_interface.h"
//#include "music.h"
#include "lightsource.h"
#include "game_window.h"
#include "items.h"
#include "shell.h"	// screen_print_f()
//MH: Lua scripting
#include "lua_script.h"
#include "InfoTree.h"

#include "QuickSave.h"


/* ---------- constants */

#define OXYGEN_RECHARGE_FREQUENCY 0
#define ENERGY_RECHARGE_FREQUENCY 0

#define MAXIMUM_PLATFORM_ACTIVATION_RANGE (3*WORLD_ONE)
#define MAXIMUM_CONTROL_ACTIVATION_RANGE (WORLD_ONE+WORLD_ONE_HALF)
#define OBJECT_RADIUS 50

#define MINIMUM_RESAVE_TICKS (2*TICKS_PER_SECOND)

// For detecting double-save (overwrite) in a netgame
enum {
        kDoubleClickTicks = 10
};

/* ---------- structures */

enum // control panel sounds
{
	_activating_sound,
	_deactivating_sound,
	_unusuable_sound,
	
	NUMBER_OF_CONTROL_PANEL_SOUNDS
};


struct control_panel_definition
{
	int16 _class;
	uint16 flags;
	
	int16 collection;
	int16 active_shape, inactive_shape;

	int16 sounds[NUMBER_OF_CONTROL_PANEL_SOUNDS];
	_fixed sound_frequency;
	
	int16 item;
};

/* ---------- globals */

#define NUMBER_OF_CONTROL_PANEL_DEFINITIONS (sizeof(control_panel_definitions)/sizeof(struct control_panel_definition))

static struct control_panel_definition control_panel_definitions[]=
{
	// _collection_walls1 -- LP: water
	{_panel_is_oxygen_refuel, 0, _collection_walls1, 2, 3, {_snd_oxygen_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_shield_refuel, 0, _collection_walls1, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_double_shield_refuel, 0, _collection_walls1, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/8, NONE},
	{_panel_is_tag_switch, 0, _collection_walls1, 0, 1, {_snd_chip_insertion, NONE, NONE}, FIXED_ONE, _i_uplink_chip},
	{_panel_is_light_switch, 0, _collection_walls1, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_platform_switch, 0, _collection_walls1, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls1, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_pattern_buffer, 0, _collection_walls1, 4, 4, {_snd_pattern_buffer, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_computer_terminal, 0, _collection_walls1, 4, 4, {NONE, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls1, 1, 0, {_snd_destroy_control_panel, NONE, NONE}, FIXED_ONE, NONE},

	// _collection_walls2 -- LP: lava
	{_panel_is_shield_refuel, 0, _collection_walls2, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_double_shield_refuel, 0, _collection_walls2, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/8, NONE},
	{_panel_is_triple_shield_refuel, 0, _collection_walls2, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/4, NONE},
	{_panel_is_light_switch, 0, _collection_walls2, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_platform_switch, 0, _collection_walls2, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls2, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_pattern_buffer, 0, _collection_walls2, 4, 4, {_snd_pattern_buffer, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_computer_terminal, 0, _collection_walls2, 4, 4, {NONE, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_oxygen_refuel, 0, _collection_walls2, 2, 3, {_snd_oxygen_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls2, 0, 1, {_snd_chip_insertion, NONE, NONE}, FIXED_ONE, _i_uplink_chip},
	{_panel_is_tag_switch, 0, _collection_walls2, 1, 0, {_snd_destroy_control_panel, NONE, NONE}, FIXED_ONE, NONE},

	// _collection_walls3 -- LP: sewage
	{_panel_is_shield_refuel, 0, _collection_walls3, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_double_shield_refuel, 0, _collection_walls3, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/8, NONE},
	{_panel_is_triple_shield_refuel, 0, _collection_walls3, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/4, NONE},
	{_panel_is_light_switch, 0, _collection_walls3, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_platform_switch, 0, _collection_walls3, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls3, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_pattern_buffer, 0, _collection_walls3, 4, 4, {_snd_pattern_buffer, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_computer_terminal, 0, _collection_walls3, 4, 4, {NONE, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_oxygen_refuel, 0, _collection_walls3, 2, 3, {_snd_oxygen_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls3, 0, 1, {_snd_chip_insertion, NONE, NONE}, FIXED_ONE, _i_uplink_chip},
	{_panel_is_tag_switch, 0, _collection_walls3, 1, 0, {_snd_destroy_control_panel, NONE, NONE}, FIXED_ONE, NONE},

	// _collection_walls4 -- LP: really _collection_walls5 -- pfhor
	{_panel_is_shield_refuel, 0, _collection_walls5, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_double_shield_refuel, 0, _collection_walls5, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/8, NONE},
	{_panel_is_triple_shield_refuel, 0, _collection_walls5, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/4, NONE},
	{_panel_is_light_switch, 0, _collection_walls5, 0, 1, {_snd_pfhor_switch_on, _snd_pfhor_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_platform_switch, 0, _collection_walls5, 0, 1, {_snd_pfhor_switch_on, _snd_pfhor_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls5, 0, 1, {_snd_pfhor_switch_on, _snd_pfhor_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_pattern_buffer, 0, _collection_walls5, 4, 4, {_snd_pattern_buffer, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_computer_terminal, 0, _collection_walls5, 4, 4, {NONE, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_oxygen_refuel, 0, _collection_walls5, 2, 3, {_snd_oxygen_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls5, 0, 1, {_snd_chip_insertion, NONE, NONE}, FIXED_ONE, _i_uplink_chip},
	{_panel_is_tag_switch, 0, _collection_walls5, 1, 0, {_snd_destroy_control_panel, NONE, NONE}, FIXED_ONE, NONE},

	// LP addition: _collection_walls4 -- jjaro
	{_panel_is_shield_refuel, 0, _collection_walls4, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_double_shield_refuel, 0, _collection_walls4, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/8, NONE},
	{_panel_is_triple_shield_refuel, 0, _collection_walls4, 2, 3, {_snd_energy_refuel, NONE, NONE}, FIXED_ONE+FIXED_ONE/4, NONE},
	{_panel_is_light_switch, 0, _collection_walls4, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_platform_switch, 0, _collection_walls4, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls4, 0, 1, {_snd_switch_on, _snd_switch_off, _snd_cant_toggle_switch}, FIXED_ONE, NONE},
	{_panel_is_pattern_buffer, 0, _collection_walls4, 4, 4, {_snd_pattern_buffer, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_computer_terminal, 0, _collection_walls4, 4, 4, {NONE, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_oxygen_refuel, 0, _collection_walls4, 2, 3, {_snd_oxygen_refuel, NONE, NONE}, FIXED_ONE, NONE},
	{_panel_is_tag_switch, 0, _collection_walls4, 0, 1, {_snd_chip_insertion, NONE, NONE}, FIXED_ONE, _i_uplink_chip},
	{_panel_is_tag_switch, 0, _collection_walls4, 1, 0, {_snd_destroy_control_panel, NONE, NONE}, FIXED_ONE, NONE},
};

struct control_panel_settings_definition {
	// How far can one reach to activate the controls?
	short ReachDistance;
	short ReachHorizontal;
	// For recharging
	short SingleEnergy;
	short SingleEnergyRate;
	short DoubleEnergy;
	short DoubleEnergyRate;
	short TripleEnergy;
	short TripleEnergyRate;
};

struct control_panel_settings_definition control_panel_settings = {
	MAXIMUM_CONTROL_ACTIVATION_RANGE, // ReachDistance
	2, // ReachHorizontal
	PLAYER_MAXIMUM_SUIT_ENERGY, // SingleEnergy
	1, // SingleEnergyRate
	2*PLAYER_MAXIMUM_SUIT_ENERGY, // DoubleEnergy
	2, // DoubleEnergyRate
	3*PLAYER_MAXIMUM_SUIT_ENERGY, // TripleEnergy
	3 // TripleEnergyRate
};

/* ------------ private prototypes */

control_panel_definition *get_control_panel_definition(
	const short control_panel_type);

//static bool line_side_has_control_panel(short line_index, short polygon_index, short *side_index_with_panel);
static void	somebody_save_full_auto(Player* inWhoSaved, bool inOverwrite);
static void	change_panel_state(short player_index, short panel_side_index);
void set_control_panel_texture(struct side_data *side);

static bool line_is_within_range(short monster_index, short line_index, world_distance range);

static bool switch_can_be_toggled(short line_index, bool player_hit, bool *should_destroy_switch);

static void play_control_panel_sound(short side_index, short sound_index, bool soft_rewind = false);

static bool get_recharge_status(short side_index);

/* ---------- code */

/* set the initial states of all switches based on the objects they control */
void initialize_control_panels()
{
    for (short side_index = 0; side_index < SideList.size(); side_index++)
	{
        side_data *side = &SideList[side_index];
		if (SIDE_IS_CONTROL_PANEL(side))
		{
			// LP change: modified previous fix so that it edits the side definition
			struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
			if (!definition)
			{
				SET_SIDE_CONTROL_PANEL(side,false);
				continue;
			}
			bool status= false;
			
			switch (definition->_class)
			{
				case _panel_is_tag_switch:
					status= GET_CONTROL_PANEL_STATUS(side);
					// use default position
					break;
				
				case _panel_is_light_switch:
					status= get_light_status(side->control_panel_permutation);
					break;
				
				case _panel_is_platform_switch:
					if (side->control_panel_permutation >= 0)
					{
						if (platform_is_on(get_polygon_data(side->control_panel_permutation)->permutation)) status= true;
					}
					else
					{
						SET_SIDE_CONTROL_PANEL(side, false);
						continue;
					}
					break;
			}
			
			SET_CONTROL_PANEL_STATUS(side, status);
			set_control_panel_texture(side);
		}
	}
}


void update_control_panels()
{
	for (short player_index = 0; player_index < get_number_of_players(); player_index++)
	{
        Player* player = &players[player_index];
		short side_index = player->control_panel_side_index;

		if (side_index != NONE)
		{
			struct side_data *side= get_side_data(player->control_panel_side_index);
			// LP change: idiot-proofing
			struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
			if (!definition) continue;

			if(definition->_class == _panel_is_pattern_buffer)
			{	// Player was working on a full-auto save
				if(dynamic_world.tick_count - player->ticks_at_last_successful_save > kDoubleClickTicks)
				{	// no double-click - need to safe save
					somebody_save_full_auto(player, false);
				}
			}
			else
			{
				bool still_in_use= false;

				if (player->variables.direction == player->variables.last_direction &&
					player->variables.last_position.x == player->variables.position.x &&
					player->variables.last_position.y == player->variables.position.y &&
					player->variables.last_position.z == player->variables.position.z)
				{
					switch (definition->_class)
					{
						case _panel_is_oxygen_refuel:
							if (!(dynamic_world.tick_count&OXYGEN_RECHARGE_FREQUENCY))
							{
								if (player->suit_oxygen<PLAYER_MAXIMUM_SUIT_OXYGEN)
								{
									player->suit_oxygen+= TICKS_PER_SECOND;
									mark_oxygen_display_as_dirty();
									still_in_use= true;
								}
							}
							break;
                                                
						case _panel_is_shield_refuel:
						case _panel_is_double_shield_refuel:
						case _panel_is_triple_shield_refuel:
							if (!(dynamic_world.tick_count&ENERGY_RECHARGE_FREQUENCY))
							{
								short maximum, rate;

								switch (definition->_class)
								{
									case _panel_is_shield_refuel:
										maximum= control_panel_settings.SingleEnergy;
										rate= control_panel_settings.SingleEnergyRate;
										break;
									case _panel_is_double_shield_refuel:
										maximum= control_panel_settings.DoubleEnergy;
										rate= control_panel_settings.DoubleEnergyRate;
										break;
									case _panel_is_triple_shield_refuel:
										maximum= control_panel_settings.TripleEnergy;
										rate= control_panel_settings.TripleEnergyRate;
										break;
									default:
										assert_fail(false, "");
								}
								if (player->suit_energy<maximum)
								{
									player->suit_energy= CEILING(player->suit_energy+rate, maximum);
									mark_shield_display_as_dirty();
									still_in_use= true;
								}
							}
							break;

						default:
							assert_fail(false, "");
					}
				}
			
				if (still_in_use)
				{
					set_control_panel_texture(side);
					play_control_panel_sound(side_index, _activating_sound, true);
				}
				else
				{
					change_panel_state(player_index, side_index);
					SoundManager::instance()->StopSound(NONE, definition->sounds[_activating_sound]);
				}
			}
		}
	}
}

void update_action_key(
	short player_index,
	bool triggered)
{
	short               object_index;
	short               target_type;
	
	if(triggered) 
	{
		object_index= find_action_key_target(player_index, MAXIMUM_ACTIVATION_RANGE, &target_type, true);
		
		if(object_index != NONE)
		{
			switch(target_type) 
			{
				case _target_is_platform:
					player_touch_platform_state(player_index, object_index);
					break;
				case _target_is_control_panel:
					change_panel_state(player_index, object_index);
					break;

				case _target_is_unrecognized:
					break;
					
				default:
                    throw_bug_report_f("invalid target type: %d", target_type);
					break;
			}
		}
	}
}

bool untoggled_repair_switches_on_level(bool only_last_switch) // true if replicating original M1 engine bug, otherwise false
{
	bool untoggled_switch = false;
	
    for (short side_index = 0; side_index < SideList.size() && (!untoggled_switch || only_last_switch); side_index++)
	{
        side_data* side = &SideList[side_index];
		if (SIDE_IS_CONTROL_PANEL(side) && SIDE_IS_REPAIR_SWITCH(side))
		{
			// LP change: idiot-proofing
			struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
			if (!definition) continue;
			
			switch (definition->_class)
			{
				case _panel_is_platform_switch:
					untoggled_switch= platform_is_at_initial_state(get_polygon_data(side->control_panel_permutation)->permutation) ? true : false;
					break;
				
				default:
					untoggled_switch= !GET_CONTROL_PANEL_STATUS(side);
					break;
			}
		}
	}
	
	return untoggled_switch;
}


void assume_correct_switch_position(short switch_type, /* platform or light */
                                    short permutation, /* platform or light index */ /* ghs: appears to be polygon, not platform */
                                    bool new_state)
{
    for (short side_index = 0; side_index < SideList.size(); side_index++)
    {
        side_data* side = &SideList[side_index];
		if (SIDE_IS_CONTROL_PANEL(side) && side->control_panel_permutation==permutation)
		{
			struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
			// LP change: idiot-proofing
			if (!definition) continue;
			
			if (switch_type==definition->_class)
			{
				play_control_panel_sound(side_index, new_state ? _activating_sound : _deactivating_sound);
				SET_CONTROL_PANEL_STATUS(side, new_state);
				set_control_panel_texture(side);
			}
		}
	}
}

void try_and_toggle_control_panel(
	short polygon_index,
	short line_index, 
	short projectile_index)
{
	short side_index= find_adjacent_side(polygon_index, line_index);
	
	if (side_index!=NONE)
	{
		struct side_data *side= get_side_data(side_index);

		if (SIDE_IS_CONTROL_PANEL(side))
		{
			bool should_destroy_switch;
			if (switch_can_be_toggled(side_index, false, &should_destroy_switch))
			{
				if (should_destroy_switch) SET_SIDE_CONTROL_PANEL(side, false);
				bool make_sound = false, state= GET_CONTROL_PANEL_STATUS(side);
				struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
				// LP change: idiot-proofing
				if (!definition) return;
				
				switch (definition->_class)
				{
					case _panel_is_tag_switch:
						state= !state;
						make_sound= set_tagged_light_statuses(side->control_panel_permutation, state);
						if (try_and_change_tagged_platform_states(side->control_panel_permutation, state)) make_sound= true;
						if (!side->control_panel_permutation) make_sound= true;
						if (film_profile.chip_insertion_ignores_tag_state &&
							definition->item != NONE) make_sound = true;
						if (make_sound)
						{
							SET_CONTROL_PANEL_STATUS(side, state);
							set_control_panel_texture(side);
						}
						L_Call_Projectile_Switch(side_index, projectile_index);				
						break;
					case _panel_is_light_switch:
						state= !state;
						make_sound= set_light_status(side->control_panel_permutation, state);
						
						L_Call_Projectile_Switch(side_index, projectile_index);				
						break;
					case _panel_is_platform_switch:
						state= !state;
						make_sound= try_and_change_platform_state(get_polygon_data(side->control_panel_permutation)->permutation, state);
						
						L_Call_Projectile_Switch(side_index, projectile_index);				
						break;
				}
				
				if (make_sound)
				{
					play_control_panel_sound(side_index, state ? _activating_sound : _deactivating_sound);
				}
			}
		}
	}
}

short get_panel_class(
	short panel_type)
{
	struct control_panel_definition *definition= get_control_panel_definition(panel_type);
	
	return definition->_class;
}

/* ---------- private code */

control_panel_definition *get_control_panel_definition(
	const short control_panel_type)
{
	return GetMemberWithBounds(control_panel_definitions,control_panel_type,NUMBER_OF_CONTROL_PANEL_DEFINITIONS);
}

short find_action_key_target(
	short player_index,
	world_distance range,
	short *target_type,
	bool perform_panel_actions)
{
	Player* player= get_player_data(player_index);
	short current_polygon= player->camera_polygon_index;
	world_point2d destination;
	bool done= false;
	short itemhit, line_index;
	struct polygon_data *polygon;
	
	// In case we don't hit anything
	*target_type = _target_is_unrecognized;

	/* Should we use this one, the physics one, or the object one? */
	ray_to_line_segment((world_point2d *) &player->location, &destination, player->facing, range);

//	ao__dprintf__("#%d(#%d,#%d) --> (#%d,#%d) (#%d along #%d)", current_polygon, player->location.x, player->location.y, destination.x, destination.y, range, player->facing);

	itemhit= NONE;
	while (!done)
	{
		line_index= find_line_crossed_leaving_polygon(current_polygon, (world_point2d *) &player->location, &destination);
		
		if (line_index==NONE)
		{
			done= true;
		} 
		else 
		{
			short original_polygon;

			(void)get_line_data(line_index); // verify line is valid
			original_polygon= current_polygon;
			current_polygon= find_adjacent_polygon(current_polygon, line_index);

//			ao__dprintf__("leaving polygon #%d through line #%d to polygon #%d", original_polygon, line_index, current_polygon);

			if (current_polygon!=NONE)
			{
				polygon= get_polygon_data(current_polygon);

				/* We hit a platform */				
				if (polygon->type==_polygon_is_platform && line_is_within_range(player->monster_index, line_index, MAXIMUM_PLATFORM_ACTIVATION_RANGE) &&
					platform_is_legal_player_target(polygon->permutation))
				{
//					ao__dprintf__("found platform #%d in %p", polygon->permutation, polygon);
					itemhit= polygon->permutation;
					*target_type= _target_is_platform;
					done= true;
				} 
			} 
			else 
			{
				done= true;
			}

			/* Slammed a wall */
			if (line_is_within_range(player->monster_index, line_index, control_panel_settings.ReachDistance))
			{
				if (line_side_has_control_panel(line_index, original_polygon, &itemhit))
				{
					bool should_destroy_panel;
					if (switch_can_be_toggled(itemhit, true, &should_destroy_panel))
					{
						if (should_destroy_panel && perform_panel_actions)
						{
							SET_SIDE_CONTROL_PANEL(get_side_data(itemhit), false);
						}
						*target_type= _target_is_control_panel;
						done= true;
					}
					else
					{
						if (perform_panel_actions)
						{
							play_control_panel_sound(itemhit, _unusuable_sound);
						}
						itemhit= NONE;
					}
				}
			}
		}
	}
	
	return itemhit;
}

static bool line_is_within_range(
	short monster_index,
	short line_index,
	world_distance range)
{
	world_point3d monster_origin= get_object_data(get_monster_data(monster_index)->object_index)->location;
	world_point3d line_origin;
	world_distance radius, height;
	world_distance dx, dy, dz;
	
	calculate_line_midpoint(line_index, &line_origin);
	get_monster_dimensions(monster_index, &radius, &height);
	monster_origin.z+= height>>1;
	
	dx= monster_origin.x-line_origin.x;
	dy= monster_origin.y-line_origin.y;
	dz= control_panel_settings.ReachHorizontal*(monster_origin.z-line_origin.z); /* dz is weighted */
	
	return isqrt(dx*dx + dy*dy + dz*dz)<range ? true : false;
}

//tiennou: removed static for lua access
bool line_side_has_control_panel(
	short line_index, 
	short polygon_index,
	short *side_index_with_panel)
{
	short             side_index = NONE;
	bool           has_panel = false;
	struct line_data  *line = get_line_data(line_index);
	struct side_data  *side = NULL;
	
	if (line->clockwise_polygon_owner==polygon_index)
	{
		side_index = line->clockwise_polygon_side_index;
		if (side_index != NONE)
		{
			side = get_side_data(side_index);
		}
	} 
	else
	{
		assert_fail(line->counterclockwise_polygon_owner==polygon_index, "");
		side_index = line->counterclockwise_polygon_side_index;
		if (side_index != NONE)
		{
			side= get_side_data(side_index);
		}
	}

	if (side != NULL && SIDE_IS_CONTROL_PANEL(side))
	{
		*side_index_with_panel = side_index;
		has_panel = true;
	}
	
	return has_panel;
}

static void
somebody_save_full_auto(Player* inWhoSaved, bool inOverwrite)
{
        play_control_panel_sound(inWhoSaved->control_panel_side_index, _activating_sound);

        // These need to happen before the save so that a freshly restored player
        // doesn't end up automatically saving the game :)
        inWhoSaved->control_panel_side_index = NONE;
        inWhoSaved->ticks_at_last_successful_save = dynamic_world.tick_count;

        if(inWhoSaved == local_player)
        {
                quicksave_game();
        }
        else
        {
                screen_print_f("%s has saved the game", inWhoSaved->name.c_str());
        }
}

static void	change_panel_state(
	short player_index,
	short panel_side_index)
{
	bool state, make_sound= false;
	struct side_data *side= get_side_data(panel_side_index);
	Player* player= get_player_data(player_index);
	struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
	
	// LP change: idiot-proofing
	if (!definition) return;
	
	state= GET_CONTROL_PANEL_STATUS(side);
	
	/* Do the right thing, based on the panel type.. */
	switch (definition->_class)
	{
		case _panel_is_oxygen_refuel:
		case _panel_is_shield_refuel:
		case _panel_is_double_shield_refuel:
		case _panel_is_triple_shield_refuel:
			player->control_panel_side_index= player->control_panel_side_index==panel_side_index ? NONE : panel_side_index;
			state= get_recharge_status(panel_side_index);
			SET_CONTROL_PANEL_STATUS(side, state);
			if (!state)
			{
				set_control_panel_texture(side);
				SoundManager::instance()->StopSound(NONE, definition->sounds[_activating_sound]);
			}
                                // Lua script hook
                                if (player -> control_panel_side_index == panel_side_index)
                                    L_Call_Start_Refuel (definition->_class, player_index, panel_side_index);
                                else
                                    L_Call_End_Refuel (definition->_class, player_index, panel_side_index);
			break;
		case _panel_is_computer_terminal:
			if (get_app_state() == app_state_t::game_in_progress)
			{
				bool is_overhead_map_active = PLAYER_HAS_MAP_OPEN(player);
				if (is_overhead_map_active && !film_profile.overhead_map_terminal)
					break;

				SET_PLAYER_MAP_STATUS(player, false);
				SET_PLAYER_HAD_OVERHEAD_MAP_STATUS(player, is_overhead_map_active);

                                //MH: Lua script hook
                                L_Call_Terminal_Enter(side->control_panel_permutation,player_index);
				
				/* this will handle changing levels, if necessary (i.e., if we’re finished) */
				enter_computer_interface(player_index, side->control_panel_permutation, calculate_level_completion_state());
			}
			break;
		case _panel_is_tag_switch:
			if (definition->item==NONE || (!state && try_and_subtract_player_item(player_index, definition->item)) || (!state && (side->flags & _side_item_is_optional)))
			{
				state= !state;
				
				make_sound= set_tagged_light_statuses(side->control_panel_permutation, state);
				if (try_and_change_tagged_platform_states(side->control_panel_permutation, state)) make_sound= true;
				if (!side->control_panel_permutation) make_sound= true;
				if (make_sound)
				{
					SET_CONTROL_PANEL_STATUS(side, state);
					set_control_panel_texture(side);
				}
                                //MH: Lua script hook
                                L_Call_Tag_Switch(side->control_panel_permutation,player_index, panel_side_index);
			
			}
			break;
		case _panel_is_light_switch:
			state= !state;
			make_sound= set_light_status(side->control_panel_permutation, state);
			
                        //MH: Lua script hook
                        L_Call_Light_Switch(side->control_panel_permutation,player_index, panel_side_index);

			break;
		case _panel_is_platform_switch:
			state= !state;
			make_sound= try_and_change_platform_state(get_polygon_data(side->control_panel_permutation)->permutation, state);
			
                        //MH: Lua script hook
                        L_Call_Platform_Switch(side->control_panel_permutation,player_index, panel_side_index);
			
			break;
		case _panel_is_pattern_buffer:
            switch (get_user_type())
            {
                case user_type_t::solo:
                    if (dynamic_world.tick_count - player->ticks_at_last_successful_save > MINIMUM_RESAVE_TICKS)
                    {
                        play_control_panel_sound(panel_side_index, _activating_sound);
                        
                        //MH: Lua script hook
                        L_Call_Pattern_Buffer(/*side->control_panel_permutation*/panel_side_index, player_index);
                        
                        // Assume a successful save- prevents vidding of the save game key.
                        player->ticks_at_last_successful_save = dynamic_world.tick_count;
                        // fade_out_background_music(30);
                        if (!quicksave_game())
                        {
                            player->ticks_at_last_successful_save = 0;
                        }
                        // fade_in_background_music(30);
                    }
                    break;
                    
                case user_type_t::pvp: // co-op game, presumably (no idea what happens if a multiplayer game contains a pattern buffer; probably nothing good)
                    if (player->control_panel_side_index != panel_side_index)
                    {
                        if (dynamic_world.tick_count - player->ticks_at_last_successful_save > MINIMUM_RESAVE_TICKS)
                        {
                            // User pressed "action" - we'll see if they're going to do it again.
                            player->ticks_at_last_successful_save = dynamic_world.tick_count;
                            player->control_panel_side_index = panel_side_index;
                        }
                    }
                    else // Double-press - overwrite recent saved game
                    {
                        somebody_save_full_auto(player, true);
                    }
                    break;
                    
                case user_type_t::replay:
                {} // do nothing (obviously)
            }
            break;
	}
	
	if (make_sound) { play_control_panel_sound(panel_side_index, state ? _activating_sound : _deactivating_sound); }
}


void set_control_panel_texture(side_data *side)
{
	struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
	// LP change: idiot-proofing
	if (!definition) return;
	
	side->primary_texture.texture= BUILD_DESCRIPTOR(definition->collection,
		GET_CONTROL_PANEL_STATUS(side) ? definition->active_shape : definition->inactive_shape);
}


static bool switch_can_be_toggled(
	short side_index,
	bool player_hit,
	bool *should_destroy_switch)
{
	bool valid_toggle= true;
	struct side_data *side= get_side_data(side_index);
	struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
	// LP change: idiot-proofing
	if (!definition) return false;
	if (should_destroy_switch) *should_destroy_switch = false;
	
	if (side->flags&_side_is_lighted_switch)
	{
		valid_toggle= get_light_intensity(side->primary_lightsource_index)>(3*FIXED_ONE/4) ? true : false;
	}
    else if (side->flags & _side_is_m1_lighted_switch)
    {
        valid_toggle = get_light_intensity(side->primary_lightsource_index)>(FIXED_ONE/2) ? true : false;
    }

	if (definition->item!=NONE && !player_hit) valid_toggle= false;
	if (player_hit && (side->flags&_side_switch_can_only_be_hit_by_projectiles)) valid_toggle= false;
	
	if (valid_toggle && (side->flags&_side_switch_can_be_destroyed) && should_destroy_switch)
	{
		*should_destroy_switch= true;
	}
	
	return valid_toggle;
}

static void play_control_panel_sound(
	short side_index,
	short sound_index,
	bool soft_rewind)
{
	struct side_data *side= get_side_data(side_index);
	struct control_panel_definition *definition= get_control_panel_definition(side->control_panel_type);
	
	// LP change: idiot-proofing
	if (!definition) return;

	if (!(sound_index>=0 && sound_index<NUMBER_OF_CONTROL_PANEL_SOUNDS)) return;
	
	play_side_sound(side_index, definition->sounds[sound_index], definition->sound_frequency, soft_rewind);
}

static bool get_recharge_status(
	short side_index)
{
	short player_index;
	bool status= false;
	
	for (player_index= 0; player_index<get_number_of_players(); ++player_index)
	{
		Player* player= get_player_data(player_index);
		
		if (player->control_panel_side_index==side_index) status= true;
	}
	
	return status;
}

struct control_panel_definition *original_control_panel_definitions = NULL;
struct control_panel_settings_definition *original_control_panel_settings = NULL;

void reset_mml_control_panels()
{
	if (original_control_panel_settings) {
		control_panel_settings = *original_control_panel_settings;
		free(original_control_panel_settings);
		original_control_panel_settings = NULL;
	}
	
	if (original_control_panel_definitions) {
		for (unsigned i = 0; i < NUMBER_OF_CONTROL_PANEL_DEFINITIONS; i++)
			control_panel_definitions[i] = original_control_panel_definitions[i];
		free(original_control_panel_definitions);
		original_control_panel_definitions = NULL;
	}
}

void parse_mml_control_panels(const InfoTree& root)
{
	// back up old values first
	if (!original_control_panel_settings) {
		original_control_panel_settings = (struct control_panel_settings_definition *) malloc(sizeof(struct control_panel_settings_definition));
        assert_fail(original_control_panel_settings, "");
		*original_control_panel_settings = control_panel_settings;
	}

	if (!original_control_panel_definitions) {
		original_control_panel_definitions = (struct control_panel_definition *) malloc(sizeof(struct control_panel_definition) * NUMBER_OF_CONTROL_PANEL_DEFINITIONS);
		assert_fail(original_control_panel_definitions, "");
		for (unsigned i = 0; i < NUMBER_OF_CONTROL_PANEL_DEFINITIONS; i++)
			original_control_panel_definitions[i] = control_panel_definitions[i];
	}

	root.read_wu("reach", control_panel_settings.ReachDistance);
	root.read_attr("horiz", control_panel_settings.ReachHorizontal);
	root.read_attr("single_energy", control_panel_settings.SingleEnergy);
	root.read_attr("single_energy_rate", control_panel_settings.SingleEnergyRate);
	root.read_attr("double_energy", control_panel_settings.DoubleEnergy);
	root.read_attr("double_energy_rate", control_panel_settings.DoubleEnergyRate);
	root.read_attr("triple_energy", control_panel_settings.TripleEnergy);
	root.read_attr("triple_energy_rate", control_panel_settings.TripleEnergyRate);
	
	for (const InfoTree &panel : root.children_named("panel"))
	{
		int16 index;
		if (!panel.read_indexed("index", index, NUMBER_OF_CONTROL_PANEL_DEFINITIONS))
			continue;
		
		control_panel_definition& def = control_panel_definitions[index];
		panel.read_indexed("type", def._class, NUMBER_OF_CONTROL_PANELS);
		panel.read_indexed("coll", def.collection, NUMBER_OF_COLLECTIONS);
		panel.read_indexed("active_frame", def.active_shape, MAXIMUM_SHAPES_PER_COLLECTION);
		panel.read_indexed("inactive_frame", def.inactive_shape, MAXIMUM_SHAPES_PER_COLLECTION);
		panel.read_indexed("item", def.item, NUMBER_OF_ITEM_TYPES, true);
		panel.read_fixed("pitch", def.sound_frequency, 0, SHRT_MAX+1);
		
		for (const InfoTree &sound : panel.children_named("sound"))
		{
			int16 type, which;
			if (!sound.read_indexed("type", type, NUMBER_OF_CONTROL_PANEL_SOUNDS) ||
				!sound.read_indexed("which", which, SHRT_MAX+1, true))
				continue;
			def.sounds[type] = which;
		}
	}
}
