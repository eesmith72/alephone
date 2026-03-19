/*
	network_games.c

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

	Wednesday, July 19, 1995 10:16:51 AM- rdm created.

Jul 1, 2000 (Loren Petrich):
	Added Benad's changes
*/

#if !defined(DISABLE_NETWORKING)

#include "cseries.h"
#include "map.h"
#include "items.h"
#include "lua_script.h"
#include "player.h"
#include "monsters.h"
#include "network.h"
#include "network_games.h"
#include "game_window.h" // for mark_player_network_stats_as_dirty
#include "SoundManager.h"

int32 team_netgame_parameters[NUMBER_OF_TEAM_COLORS][2];

// Benad
void destroy_players_ball(
	short player_index);

#include <stdio.h>
#include <limits.h>

/* ----------- #defines */
#define SINGLE_BALL_COLOR (1)

/* ----------- enums */

/* Net game parameters */
enum { // for king of the hill
	_king_of_hill_time= 0 
};

enum { // for kill the man with the ball
	_ball_carrier_time= 0
};

enum { // for offense/defense
	_offender_time_in_base= 0, // for player->netgame_parameters
	_defending_team= 0, // for game_information->paramters
	_maximum_offender_time_in_base= 1 // for game_information->parameters
};

enum { // for rugby
	_points_scored= 0
};

enum { // for tag
	_time_spent_it= 0
};

enum { // for capture the flag.
	_flag_pulls= 0, // for player->netgame_parameters
	_winning_team= 0 // for game_information->parameters[]
};


/* ----------------- private prototypes */
static bool player_has_ball(short player_index, short color);
extern void destroy_players_ball(short player_index);

// for script controlled compass
extern bool use_lua_compass[MAXIMUM_NUMBER_OF_NETWORK_PLAYERS];
extern world_point2d lua_compass_beacons[MAXIMUM_NUMBER_OF_NETWORK_PLAYERS];
extern short lua_compass_states[MAXIMUM_NUMBER_OF_NETWORK_PLAYERS];

/* ------------------ code */

void get_player_net_ranking(int16_t player_index, bool game_is_over, int32_t& ranking, int16_t& kills, int16_t& deaths)
{
    const player_data *player = get_player_data(player_index);
    
    ranking                      = 0;
    kills                        = 0;
    deaths                       = player->monster_damage_taken.kills;
    int32_t monster_damage       = player->monster_damage_given.damage;
    int32_t total_monster_damage = monster_damage;
    
    for (int32_t i = 0; i < dynamic_world->player_count; i++)
    {
        if (i != player_index)
        {
            const player_data* other_player= get_player_data(i);
            
            kills += other_player->damage_taken[player_index].kills;
            total_monster_damage += other_player->monster_damage_given.damage;
        }
        
        deaths += player->damage_taken[i].kills;
    }
    
    switch (GET_GAME_TYPE())
    {
        case _game_of_kill_monsters:
            ranking = kills - deaths;
            break;
            
        case _game_of_cooperative_play:
            ranking = total_monster_damage ? (100 * monster_damage) / total_monster_damage : 0;
            break;
            
        case _game_of_custom:
            switch (GetLuaScoringMode())
            {
                case _game_of_most_points:
                case _game_of_most_time:
                    ranking = player->netgame_parameters[_points_scored];
                    break;
                case _game_of_least_points:
                case _game_of_least_time:
                    ranking = -player->netgame_parameters[_points_scored];
                    break;
            }
            break;
            
        case _game_of_capture_the_flag:
            ranking = player->netgame_parameters[_flag_pulls];
            break;
            
        case _game_of_king_of_the_hill:
            ranking = player->netgame_parameters[_king_of_hill_time];
            break;
            
        case _game_of_kill_man_with_ball:
            ranking = player->netgame_parameters[_ball_carrier_time];
            break;
            
        case _game_of_tag:
            ranking = -player->netgame_parameters[_time_spent_it];
            break;
            
            // START Benad
        case _game_of_defense:
        {
            /*
            *ranking = (*kills) - (*deaths);
            // Bogus for now.
            if (game_is_over && GET_GAME_PARAMETER(_winning_team) == player->team) { *ranking += 50; }
             break;
             */
            //short defending_team = GET_GAME_PARAMETER(_defending_team);
            int16_t defending_team = 0;
            if (player->team != defending_team)
            {
                ranking = player->netgame_parameters[_offender_time_in_base];
            }
            else
            {
                int32_t biggest = 0;
                for (int32_t i = 0; i < dynamic_world->player_count; i++)
                {
                    struct player_data* player = get_player_data(i);
                    if ((player->team != defending_team) && (player->netgame_parameters[_offender_time_in_base] > biggest))
                    {
                        biggest = player->netgame_parameters[_offender_time_in_base];
                    }
                }
                ranking = (dynamic_world->game_information.kill_limit * TICKS_PER_SECOND) - biggest; // in ticks
            }
            break;
            // END Benad
        }
        case _game_of_rugby:
            // Benad
            ranking = player->netgame_parameters[_points_scored];
            break;
            
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
}


void get_team_net_ranking(int16_t team, bool game_is_over, int32_t&ranking, int16_t& kills, int16_t& deaths)
{
    ranking = NONE;
    
    kills = team_damage_given[team].kills;
    deaths = team_damage_taken[team].kills + team_monster_damage_taken[team].kills;
    
    int32_t total_monster_damage = 0;
    for (int i = 0; i < NUMBER_OF_TEAM_COLORS; i++)
    {
        total_monster_damage += team_monster_damage_given[i].damage;
    }
    int32_t monster_damage = team_monster_damage_given[team].damage;
    
    switch(GET_GAME_TYPE()) 
    {
        case _game_of_kill_monsters:
            ranking = kills - deaths;
            break;
        case _game_of_custom:
            switch(GetLuaScoringMode())
            {
                case _game_of_most_points:
                case _game_of_most_time:
                    ranking = team_netgame_parameters[team][_points_scored];
                    break;
                case _game_of_least_points:
                case _game_of_least_time:
                    ranking = -team_netgame_parameters[team][_points_scored];
                    break;
            }
            break;
        case _game_of_cooperative_play:
            ranking = total_monster_damage ? (100*monster_damage)/total_monster_damage : 0;
            break;
        case _game_of_capture_the_flag:
            ranking = team_netgame_parameters[team][_flag_pulls];
            break;
        case _game_of_king_of_the_hill:
            ranking = team_netgame_parameters[team][_king_of_hill_time];
            break;
        case _game_of_kill_man_with_ball:
            ranking = team_netgame_parameters[team][_ball_carrier_time];
            break;
        case _game_of_tag:
            ranking = -team_netgame_parameters[team][_time_spent_it];
            break;
        case _game_of_defense:
        {
            short defending_team = 0;
            if (team != defending_team)
            {
                ranking = team_netgame_parameters[team][_offender_time_in_base];
            } else {
                int32_t biggest = 0;
                for (int32_t i = 0; i < NUMBER_OF_TEAM_COLORS; i++) {
                    if ((i != defending_team) && (team_netgame_parameters[i][_offender_time_in_base] > biggest))
                    {
                        biggest = team_netgame_parameters[i][_offender_time_in_base];
                    }
                }
                ranking = (dynamic_world->game_information.kill_limit * TICKS_PER_SECOND) - biggest;
            }
            break;
        }
        case _game_of_rugby:
            ranking = team_netgame_parameters[team][_points_scored];
            break;
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
}

	



void initialize_net_game(
	void)
{
  for (int i = 0; i < NUMBER_OF_TEAM_COLORS; i++) {
    team_netgame_parameters[i][0] = 0;
    team_netgame_parameters[i][1] = 0;
  }
	switch (GET_GAME_TYPE())
	{
		case _game_of_king_of_the_hill:
		// Benad
		case _game_of_defense:
			// calculate the center of the hill
			{
				int32 x = 0, y = 0;
				int16 count = 0;
                polygon_data* polygon = map_polygons;
				
				for (int32_t i =  0; i < dynamic_world->polygon_count; ++i, ++polygon)
				{
					if (polygon->type == _polygon_is_hill)
					{
						count += 1;
                        x += polygon->center.x;
                        y += polygon->center.y;
					}
				}
				
				if (count > 0) {
				  dynamic_world->game_beacon.x= static_cast<world_distance>(x/count);
				  dynamic_world->game_beacon.y= static_cast<world_distance>(y/count);
				} else {
				  // KOTH map with no hill, nice
				  dynamic_world->game_beacon.x = 0;
				  dynamic_world->game_beacon.y = 0;
				}
			}
			break;
		// START Benad
		case _game_of_rugby:
			dynamic_world->game_player_index= NONE;
			break;
		// END Benad
		case _game_of_kill_man_with_ball:
			dynamic_world->game_player_index= NONE;
//			play_local_sound(_snd_got_ball);
			break;
			
		case _game_of_tag:
			dynamic_world->game_player_index= NONE; // nobody is it, yet
			break;
	}
}

#define NETWORK_COMPASS_SLOP SIXTEENTH_CIRCLE

short get_network_compass_state(
	short player_index)
{
	short state= _network_compass_all_off;
	world_point2d *beacon= (world_point2d *) NULL;

        if (use_lua_compass [player_index])
        {
                if (lua_compass_states [player_index] & _network_compass_use_beacon)
                    beacon = lua_compass_beacons + player_index;
                else
                    state = lua_compass_states [player_index];
        }
	else
	{
                switch (GET_GAME_TYPE())
                {
                        case _game_of_king_of_the_hill: // where’s the hill
                        // Benad
                        case _game_of_defense:
                                if (get_polygon_data(get_player_data(player_index)->supporting_polygon_index)->type==_polygon_is_hill)
                                {
                                        state= _network_compass_all_on;
                                }
                                else
                                {
                                        beacon= &dynamic_world->game_beacon;
                                }
                                break;
			
                        case _game_of_tag: // where’s it
                                if (dynamic_world->game_player_index==player_index)
                                {
                                        state= _network_compass_all_on;
                                }
                                else
                                {
                                        if (dynamic_world->game_player_index!=NONE)
                                        {
                                                beacon= (world_point2d *) &get_player_data(dynamic_world->game_player_index)->location;
                                        }
                                }
                                break;
                        // START Benad
                        case _game_of_rugby:
                                if (player_has_ball(player_index, SINGLE_BALL_COLOR))
                                {
                                        state= _network_compass_all_on;
                                }
                                else
                                {
                                        if (dynamic_world->game_player_index!=NONE)
                                        {
                                                beacon= (world_point2d *) &get_player_data(dynamic_world->game_player_index)->location;
                                        }
                                }
                                break;
                        //END Benad
                        case _game_of_kill_man_with_ball: // where’s the ball
                                if (player_has_ball(player_index, SINGLE_BALL_COLOR))
                                {
                                        state= _network_compass_all_on;
                                }
                                else
                                {
                                        if (dynamic_world->game_player_index!=NONE)
                                        {
                                                beacon= (world_point2d *) &get_player_data(dynamic_world->game_player_index)->location;
                                        }
                                }
                                break;
                }
        }

	if (beacon)
	{        
		struct player_data *player= get_player_data(player_index);
		struct world_point2d *origin= (world_point2d *) &player->location;
		angle theta= NORMALIZE_ANGLE(get_object_data(player->object_index)->facing-arctangent(origin->x-beacon->x, origin->y-beacon->y));
		
		if (theta>FULL_CIRCLE-NETWORK_COMPASS_SLOP || theta<QUARTER_CIRCLE+NETWORK_COMPASS_SLOP) state|= _network_compass_se;
		if (theta>QUARTER_CIRCLE-NETWORK_COMPASS_SLOP && theta<HALF_CIRCLE+NETWORK_COMPASS_SLOP) state|= _network_compass_ne;
		if (theta>HALF_CIRCLE-NETWORK_COMPASS_SLOP && theta<HALF_CIRCLE+QUARTER_CIRCLE+NETWORK_COMPASS_SLOP) state|= _network_compass_nw;
		if (theta>HALF_CIRCLE+QUARTER_CIRCLE-NETWORK_COMPASS_SLOP || theta<NETWORK_COMPASS_SLOP) state|= _network_compass_sw;
	}

	return state;
}

// if false is returned, don’t attribute kill
bool player_killed_player(
	short dead_player_index,
	short aggressor_player_index)
{
	bool attribute_kill= true;
	
	if (dynamic_world->player_count>1)
	{
		switch (GET_GAME_TYPE())
		{
			case _game_of_tag:
				if (aggressor_player_index==dynamic_world->game_player_index || // killed by it
					dead_player_index==aggressor_player_index || // killed themselves
					dynamic_world->game_player_index==NONE) // died without an it
				{
					if (dynamic_world->game_player_index!=dead_player_index)
					{
						// change of ‘it’
						player_data* player = get_player_data(dead_player_index);
						play_object_sound(player->object_index, _snd_you_are_it);
						dynamic_world->game_player_index= dead_player_index;
					}
				}
				break;
		}
		
		if (attribute_kill)
		{
			switch (GET_GAME_TYPE())
			{
				case _game_of_kill_monsters:
					mark_player_network_stats_as_dirty(current_player_index);
					break;
			}
		}
	}

	return attribute_kill;
}

bool update_net_game(
	void)
{
	bool net_game_over= false;
	short player_index;

	if (dynamic_world->player_count>1)
	{
		switch(GET_GAME_TYPE())
		{
			case _game_of_kill_monsters:
			case _game_of_cooperative_play:
		case _game_of_custom:
				/* These games have no housekeeping associated with them. */
				break;
				
			case _game_of_capture_the_flag:
			// START Benad
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					struct polygon_data *polygon= get_polygon_data(player->supporting_polygon_index);
					
					if ( (polygon->type==_polygon_is_base && polygon->permutation==player->team) ||
						((dynamic_world->game_information.kill_limit == 819) && (polygon->type==_polygon_is_hill)) )
					{
						short ball_color= find_player_ball_color(player_index);
						
						if ((ball_color != NONE && ball_color != player->team) ||
							(ball_color != NONE && dynamic_world->game_information.kill_limit == 819))
						{
							player->netgame_parameters[_flag_pulls]++;
							team_netgame_parameters[player->team][_flag_pulls]++;
							destroy_players_ball(player_index);
						}
					}
				}
				break;
			// END Benad
				
			case _game_of_king_of_the_hill:
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					
					if(!PLAYER_IS_DEAD(player))
					{
						struct polygon_data *polygon= get_polygon_data(player->supporting_polygon_index);
					
						if(polygon->type==_polygon_is_hill)
						{
							player->netgame_parameters[_king_of_hill_time]++;
							team_netgame_parameters[player->team][_king_of_hill_time]++;
						}
					}
				}
				break;
				
			case _game_of_kill_man_with_ball:
				dynamic_world->game_player_index= NONE;
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					if (player_has_ball(player_index, SINGLE_BALL_COLOR))
					{
						struct player_data *player= get_player_data(player_index);
						
						player->netgame_parameters[_ball_carrier_time]++;
						team_netgame_parameters[player->team][_ball_carrier_time]++;
						dynamic_world->game_player_index= player_index;
						
						break;
					}
				}
				break;
				
			case _game_of_tag:
				if (dynamic_world->game_player_index!=NONE)
				{
					struct player_data *player= get_player_data(dynamic_world->game_player_index);
					
					if (!PLAYER_IS_DEAD(player) || PLAYER_IS_TOTALLY_DEAD(player))
					{
						player->netgame_parameters[_time_spent_it]+= 1;
						team_netgame_parameters[player->team][_time_spent_it]+= 1;
					}
				}
				break;

			// START Benad
			case _game_of_defense:
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					//short defending_team= GET_GAME_PARAMETER(_defending_team);
					short defending_team= 0;
					
					if((player->team != defending_team) && (!PLAYER_IS_DEAD(player)))
					{
						struct polygon_data *polygon= get_polygon_data(player->supporting_polygon_index);

						/* They are in our base! */
						if(polygon->type==_polygon_is_hill/* && polygon->permutation==defending_team*/)
						{
							player->netgame_parameters[_offender_time_in_base]++;
							team_netgame_parameters[player->team][_offender_time_in_base]++;
							/*if(player->netgame_parameters[_offender_time_in_base]>GET_GAME_PARAMETER(_maximum_offender_time_in_base))
							{
								ao__dprintf__("Game is over. Offender won.");
								//••
								dynamic_world->game_information.parameters[_winning_team]= player->team;
								net_game_over= true;
							}*/
						}
						break;
					}
				}
				break;
			
			case _game_of_rugby:
				dynamic_world->game_player_index= NONE;
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					struct polygon_data *polygon= get_polygon_data(player->supporting_polygon_index);
					
					if (player_has_ball(player_index, SINGLE_BALL_COLOR))
					{
						// START Benad changed oct. 1st
						dynamic_world->game_player_index= player_index;
						//if(polygon->type==_polygon_is_base && polygon->permutation != player->team)
						if( ( (dynamic_world->game_information.kill_limit == 819) && (polygon->type==_polygon_is_hill)
							&& (!PLAYER_IS_DEAD(player)) ) ||
							( polygon->type==_polygon_is_base && polygon->permutation != player->team && (!PLAYER_IS_DEAD(player)) ) )
						{
							/* Goal! */
							player->netgame_parameters[_points_scored]++;
							team_netgame_parameters[player->team][_points_scored]++;
							
							/* Ditch the ball.. (it will be recreated by the timer..) */
							destroy_players_ball(player_index);
							dynamic_world->game_player_index= NONE;
							break; // Break out of loop; assuming there's only one ball.
						}
						// Can't take the ball back to you own goal, otherwise weird bugs could happen
						// if an opponent touches the ball in your goal. Like real Rugby.
						else if ( polygon->type==_polygon_is_base && polygon->permutation == player->team && (!PLAYER_IS_DEAD(player)))
						{
							/* Ditch the ball.. (it will be recreated by the timer..) */
							destroy_players_ball(player_index);
							dynamic_world->game_player_index= NONE;
							break; // Break out of loop; assuming there's only one ball.
						}
						// END Benad changed oct. 1st
					}
				}
				break;
			// END Benad
			default:
                throw_bug_report("invalid game type: %d?", GET_GAME_TYPE());
				break;
		}

		if (--current_player->hud_decay < 0 && GET_GAME_TYPE() != _game_of_kill_monsters)
		{
			mark_player_network_stats_as_dirty(current_player_index);
		}
	}
	// Benad: Warning! This is actually ignored! Instead, change game_is_over().
	return net_game_over;
}

void calculate_player_rankings(player_rankings_t& rankings)
{
	// First get the stats.
	for (int16_t i = 0; i < dynamic_world->player_count; i++)
	{
		rankings[i].player_index = i;
        get_player_net_ranking(i, false, rankings[i].ranking);
	}
    
	// Now sort them. // EES: TODO: check this is sorting in right order (I believe it's meant to be highest to lowest, but the previous AO sort code was clear as mud)
    std::sort(rankings.begin(), rankings.end(), [](player_ranking_data& p1, player_ranking_data& p2){ return p1.ranking > p2.ranking; });
}

// These aren't in resources for speed.
const std::string calculate_ranking_text(int64_t ranking)
{
    char buffer[32];
    switch(GET_GAME_TYPE())
    {
        case _game_of_kill_monsters:
        case _game_of_capture_the_flag:
        case _game_of_rugby:
            snprintf(buffer, sizeof(buffer), "%lld", ranking);
            break;
            
        case _game_of_custom:
            switch(GetLuaScoringMode()) {
                case _game_of_most_points:
                    snprintf(buffer, sizeof(buffer), "%lld", ranking);
                    break;
                case _game_of_least_points:
                    snprintf(buffer, sizeof(buffer), "%lld", -ranking);
                    break;
                case _game_of_most_time:
                case _game_of_least_time:
                    int64_t seconds = std::abs(ranking) / TICKS_PER_SECOND;
                    snprintf(buffer, sizeof(buffer), "%lld:%02lld", seconds / 60, seconds % 60);
                    break;
            }
            break;
            
        case _game_of_cooperative_play:
            snprintf(buffer, sizeof(buffer), "%lld%%", ranking);
            break;
            // START Benad
        case _game_of_king_of_the_hill:
        case _game_of_kill_man_with_ball:
        case _game_of_tag:
        case _game_of_defense:
        {
            int64_t seconds = std::abs(ranking) / TICKS_PER_SECOND;
            snprintf(buffer, sizeof(buffer), "%lld:%02lld", seconds / 60, seconds % 60);
            break;
        } // END Benad
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
    return buffer;
}


const std::string calculate_ranking_text_for_post_game(int32_t ranking)
{
    std::string result;
    
    switch(GET_GAME_TYPE())
    {
        case _game_of_kill_monsters:
        case _game_of_cooperative_play:
            break;
            
        case _game_of_custom:
            switch(GetLuaScoringMode())
            {
                case _game_of_most_points:
                {
                    result = get_string(STRID(strNETWORK_GAME_STRINGS, pointsFormatString), {
                        {"$count$", [ranking]{ return std::to_string(ranking); }},
                    });
                    break;
                }
                case _game_of_least_points:
                {
                    result = get_string(STRID(strNETWORK_GAME_STRINGS, pointsFormatString), {
                        {"$count$", [ranking]{ return std::to_string(-ranking); }},
                    });
                    break;
                }
                case _game_of_most_time:
                case _game_of_least_time:
                {
                    int64_t seconds = std::abs(ranking) / TICKS_PER_SECOND;
                    result = get_string(STRID(strNETWORK_GAME_STRINGS, minutesPossessedFormatString), {
                        {"$minute$", [seconds]{ return std::to_string(seconds / 60); }},
                        {"$second$", [seconds]{ return std::to_string(seconds % 60); }},
                    });
                    break;
                }
            }
            break;
            
        case _game_of_capture_the_flag:
        {
            result = get_string(STRID(strNETWORK_GAME_STRINGS, flagPullsFormatString), {
                {"$count$", [ranking]{ return std::to_string(ranking); }},
            });
            break;
        }
        case _game_of_rugby:
        {
            result = get_string(STRID(strNETWORK_GAME_STRINGS, pointsFormatString), {
                {"$count$", [ranking]{ return std::to_string(ranking); }},
            });
            break;
        } // START Benad
        case _game_of_king_of_the_hill:
        case _game_of_kill_man_with_ball:
        case _game_of_tag:
        case _game_of_defense:
        {
            int64_t seconds = std::abs(ranking) / TICKS_PER_SECOND;
            result = get_string(STRID(strNETWORK_GAME_STRINGS, minutesPossessedFormatString), {
                {"$minute$", [seconds]{ return std::to_string(seconds / 60); }},
                {"$second$", [seconds]{ return std::to_string(seconds % 60); }},
            });
            break;
        } // END Benad
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
    return result;
}


std::string get_network_score_text_for_postgame(bool is_team_mode)
{
    int16_t string_id = NONE;
    
    switch (GET_GAME_TYPE())
    {
        case _game_of_kill_monsters:
        case _game_of_cooperative_play:
            string_id = NONE;
            break;
            
        case _game_of_custom:
            switch (GetLuaScoringMode())
            {
                case _game_of_most_points:
                case _game_of_least_points:
                    string_id = pointsScoringString;
                    break;
                    
                case _game_of_most_time:
                case _game_of_least_time:
                    string_id = timeString;
                    break;
            }
            break;
            
        case _game_of_capture_the_flag:
            string_id = flagsCapturedString;
            break;
            
        case _game_of_rugby:
            string_id = goalsString;
            break;
            
        case _game_of_king_of_the_hill:
            string_id = reignString;
            break;
            
        case _game_of_kill_man_with_ball:
            string_id = timeWithBallString;
            break;
            //START Benad
        case _game_of_defense:
            string_id = timeOnBaseString;
            break;
            // END Benad
        case _game_of_tag:
            string_id = timeItString;
            break;
            
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
    
    std::string result = "";
    
    if (string_id != NONE)
    {
        if (is_team_mode)
        {
            result += get_string(STRID(strNETWORK_GAME_STRINGS, teamString)) + " "; // "Team "
        }
        result += get_string(STRID(strNETWORK_GAME_STRINGS, string_id));
    }
    
    return result;
}

bool current_net_game_has_scores(
	void)
{
	bool has_scores;
	
	switch(GET_GAME_TYPE())
	{
		case _game_of_kill_monsters:
		case _game_of_cooperative_play:
			has_scores= false;
			break;
			
	case _game_of_custom:
		case _game_of_capture_the_flag:
		case _game_of_rugby:
		case _game_of_king_of_the_hill:
		case _game_of_kill_man_with_ball:
		case _game_of_defense:
		case _game_of_tag:
			has_scores= true;
			break;
			
		default:
			has_scores= false;
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
			break;
	}

	return has_scores;
}

bool current_game_has_balls(
	void)
{
	bool has_ball;
	
	switch(GET_GAME_TYPE())
	{
		case _game_of_kill_monsters:
		case _game_of_cooperative_play:
		case _game_of_king_of_the_hill:
		case _game_of_defense:
		case _game_of_tag:
	case _game_of_custom:
			has_ball= false;
			break;
			
		case _game_of_capture_the_flag:
		case _game_of_rugby:
		case _game_of_kill_man_with_ball:
			has_ball= true;
			break;
			
		default:
			has_ball= false;
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
			break;
	}

	return has_ball;
}

/* Note that kill limit means different things.. */
/* if capture the flag- the number of flag pulls */
bool game_is_over(
	void)
{
	bool game_over= false;

	if (dynamic_world->game_information.game_time_remaining<=0)
	{
		game_over= true;
	}
	else if(GetLuaGameEndCondition() == _game_end_now_condition)
	  {
	    game_over = true;
	  }
	else if(GetLuaGameEndCondition() == _game_no_end_condition)
	  {
	    game_over = false;
	  }
	else if(GET_GAME_OPTIONS() & _game_has_kill_limit) 
	{
		short player_index;

		switch(GET_GAME_TYPE())
		{
			case _game_of_kill_monsters:
			case _game_of_cooperative_play:
			case _game_of_king_of_the_hill:
			case _game_of_kill_man_with_ball:
			case _game_of_tag:
				/* Find out if the kill limit has been reached */
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					
					// make sure we subtract our suicides.
					if (player->total_damage_given.kills-player->damage_taken[player_index].kills >= dynamic_world->game_information.kill_limit)
					{
						// we don't actually want the game to end right away, but give a second or
						// two to see the player die.
						dynamic_world->game_information.game_options &= ~_game_has_kill_limit;
						dynamic_world->game_information.game_time_remaining= 2*TICKS_PER_SECOND;
						break;
					}
				}
				break;
				
		case _game_of_custom: // default scoring behavior is like CTF
			case _game_of_capture_the_flag:
				/* Kill limit is the number of flag pulls */
				for (int i = 0; i < NUMBER_OF_TEAM_COLORS; i++) {
					if (team_netgame_parameters[i][_flag_pulls] >= dynamic_world->game_information.kill_limit) {
						game_over = true;
						break;
					}
				}
				break;
				
			case _game_of_rugby:
				/* Kill limit is the number of flag pulls */
				for (int i = 0; i < NUMBER_OF_TEAM_COLORS; ++i)
				{
					if (team_netgame_parameters[i][_points_scored] >= dynamic_world->game_information.kill_limit)
					{
						game_over = true;
						break;
					}
				}
				break;
			// START Benad
			case _game_of_defense:
				for(player_index= 0; player_index<dynamic_world->player_count; ++player_index)
				{
					struct player_data *player= get_player_data(player_index);
					if(player->netgame_parameters[_offender_time_in_base] >
						(dynamic_world->game_information.kill_limit * TICKS_PER_SECOND)) // kill_limit is in seconds
					{
						//ao__dprintf__("Game is over. Offender won.");
						//dynamic_world->game_information.parameters[_winning_team]= player->team;
						game_over= true;
					}
				}
				break;
			// END Benad
			default:
                throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
				break;
		}
	}
	
	return game_over;
}


const std::string get_network_joined_message(int16_t game_type)
{
    int16_t game_type_string_id;
    
    switch (game_type)
    {
        case _game_of_kill_monsters: // single player and EMFH
            game_type_string_id = _carnage_word;
            break;
        case _game_of_cooperative_play:
            game_type_string_id = _cooperative_string;
            break;
        case _game_of_capture_the_flag:
            game_type_string_id = _capture_the_flag;
            break;
        case _game_of_rugby:
            game_type_string_id = _rugby;
            break;
        case _game_of_king_of_the_hill:
            game_type_string_id = _king_of_the_hill;
            break;
        case _game_of_kill_man_with_ball:
            game_type_string_id = _kill_the_man_with_the_ball;
            break;
        case _game_of_defense:
            game_type_string_id = _defender_offender;
            break;
        case _game_of_tag:
            game_type_string_id = _tag;
            break;
        case _game_of_custom:
            game_type_string_id = _custom_string;
            break;
        default:
            throw_bug_report("invalid game type: %d", GET_GAME_TYPE());
            break;
    }
    
    return get_string(STRID(strJOIN_NETWORK_STRINGS, _standard_format), {
        { "$type$", [game_type_string_id]{ return get_string(STRID(strJOIN_NETWORK_STRINGS, game_type_string_id)); }}
    });
}


/* This function is used only at network.. */
uint32_t get_entry_point_flags_for_game_type(int32_t game_type)
{
	long entry_flags = 0;
	
	switch(game_type)
	{
		case _game_of_kill_man_with_ball:
			entry_flags= _kill_the_man_with_the_ball_entry_point;
			break;
			
		case _game_of_kill_monsters:
		case _game_of_tag:
	case _game_of_custom:
			entry_flags= _multiplayer_carnage_entry_point;
			break;
			
		case _game_of_cooperative_play:
			entry_flags= _multiplayer_cooperative_entry_point;
			break;

		case _game_of_king_of_the_hill:
			entry_flags= _king_of_hill_entry_point;
			break;
		// START Benad
		case _game_of_rugby:
			entry_flags= _rugby_entry_point;
			break;
		case _game_of_capture_the_flag:
			entry_flags= _capture_the_flag_entry_point;
			break;
		case _game_of_defense:
			entry_flags= _defense_entry_point;
			break;
		// END Benad
		default:
            throw_bug_report("invalid game type: %d", game_type);
			break;
	}
		
	return entry_flags;
}

/* ------------------ local code */
static bool player_has_ball(
	short player_index,
	short color)
{
	struct player_data *player= get_player_data(player_index);
	bool has_ball= false;
	
	if(player->items[BALL_ITEM_BASE+color]>0)
	{
		has_ball= true;
	}
	
	return has_ball;
}

// START Benad
// This function moved to weapons.c
/*
static void destroy_players_ball(
	short player_index)
{
	short color, item_type;
	struct player_data *player= get_player_data(player_index);
	
	color= find_player_ball_color(player_index);
	assert_fail(color != NONE, "");

	*//* Get rid of it. *//*
	item_type= BALL_ITEM_BASE+color;
	player->items[item_type]= NONE;
	
	*//* Destroy the object (placement will recreate it..) *//*
	object_was_just_destroyed(_object_is_item, item_type);
	
	*//* Mark the player inventory as dirty.. *//*
	mark_player_inventory_as_dirty(player_index, _i_knife);
}
*/
// END Benad

#endif // !defined(DISABLE_NETWORKING)

