/*
PLAYER.C

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

#define DONT_REPEAT_DEFINITIONS

#include "player.h"

#include "map.h"
#include "monster_definitions.h"
#include "monsters.h"
#include "interface.hpp"
#include "SoundManager.h"
#include "visual_effects.hpp"
#include "compatibility_profiles.h"
#include "media.h"
#include "items.h"
#include "weapons.h"
#include "vbl.h" // sync_heartbeat_count
#include "hud_manager.h"
#include "computer_interface.h"
#include "projectiles.h"
#include "network_games.h"
#include "network.h"
#include "Screen.hpp"
#include "shell.h" // for screen_print_f()
#include "Console.h"
#include "camera.hpp" // main_camera_settings
#include "InfoTree.h"
#include "screen_overlay.h"
#include "motion_sensor.hpp" // reset_motion_sensor
#include "preferences.hpp" // player_preferences

#include "ChaseCam.h"
#include "Packing.h"
#include "network.h"

#include "ActionQueues.h"

#include "lua_script.h"


void exit_game_event_loop(app_state_t next_state); // defined in "game_event_loop.cpp" till we decide where best to put it


/*
//anybody on the receiving pad of a teleport should explode (what happens to invincible guys?)
// Really should create a function that initializes the player state.
??new players should teleport in
*/


/* ---------- constants */

struct player_powerup_durations_definition {
	short InvisibilityDuration;
	short InvincibilityDuration;
	short ExtravisionDuration;
	short InfravisionDuration;
};

// These are variables, because they can be set with an XML parser
struct player_powerup_durations_definition player_powerup_durations = {
	70*TICKS_PER_SECOND,
	50*TICKS_PER_SECOND,
	3*TICKS_PER_MINUTE,
	3*TICKS_PER_MINUTE
};

#define kINVISIBILITY_DURATION  player_powerup_durations.InvisibilityDuration
#define kINVINCIBILITY_DURATION player_powerup_durations.InvincibilityDuration
#define kEXTRAVISION_DURATION   player_powerup_durations.ExtravisionDuration
#define kINFRAVISION_DURATION   player_powerup_durations.InfravisionDuration

#define MINIMUM_REINCARNATION_DELAY (TICKS_PER_SECOND)
#define NORMAL_REINCARNATION_DELAY (10*TICKS_PER_SECOND)
#define SUICIDE_REINCARNATION_DELAY (15*TICKS_PER_SECOND)

#define DEAD_PLAYER_HEIGHT WORLD_ONE_FOURTH

#define OXYGEN_WARNING_LEVEL TICKS_PER_MINUTE
#define OXYGEN_WARNING_FREQUENCY (TICKS_PER_MINUTE/4)
#define OXYGEN_WARNING_OFFSET (10*TICKS_PER_SECOND)

#define LAST_LEVEL 100

/* ---------- structures */


struct damage_response_definition
{
	short type;
	short damage_threshhold; /* NONE is none, otherwise bumps fade by one if over threshhold */
	
	short fade;
	short sound, death_sound, death_action;
};

struct player_powerup_definition
{
	short Powerup_Invincibility;
	short Powerup_Invisibility;
	short Powerup_Infravision;
	short Powerup_Extravision;
	short Powerup_TripleEnergy;
	short Powerup_DoubleEnergy;
	short Powerup_Energy;
	short Powerup_Oxygen;
};



std::vector<Player> players;


struct damage_record team_damage_given[NUMBER_OF_TEAM_COLORS];
struct damage_record team_damage_taken[NUMBER_OF_TEAM_COLORS];
struct damage_record team_monster_damage_taken[NUMBER_OF_TEAM_COLORS];
struct damage_record team_monster_damage_given[NUMBER_OF_TEAM_COLORS];
struct damage_record team_friendly_fire[NUMBER_OF_TEAM_COLORS];

// pointers into players vector; these should never be null while game in progress so we should be able to get rid of the redundant indexes and store only Player instance (or vice-versa)
Player* local_player = nullptr;
Player* current_player = nullptr;
short local_player_index = NONE;
short current_player_index = NONE;


static ActionQueues sRealActionQueues(MAXIMUM_NUMBER_OF_PLAYERS, ACTION_QUEUE_BUFFER_DIAMETER, false);

ActionQueues* GetRealActionQueues() { return &sRealActionQueues; }

static struct player_shape_definitions player_shapes=
{
	6, /* collection */
	
	9, 8, /* dying hard, dying soft */
	11, 10, /* dead hard, dead soft */
	{7, 0, 0, 24, 23}, /* legs: stationary, walking, running, sliding, airborne */
	// LP additions: SMG-wielding/firing shapes (just before last two)
	{1, 3, 20, 26, 14, 12, 31, 16, 28, 33, 5, 18}, /* idle torsos: fists, magnum, fusion, assault, rocket, flamethrower, alien, shotgun, double pistol, double shotgun, da ball */
	{1, 3, 21, 26, 14, 12, 31, 16, 28, 33, 5, 18}, /* charging torsos: fists, magnum, fusion, assault, rocket, flamethrower, alien, shotgun, double pistol, double shotgun, ball */
	{2, 4, 22, 27, 15, 13, 32, 17, 28, 34, 6, 19}, /* firing torsos: fists, magnum, fusion, assault, rocket, flamethrower, alien, shotgun, double pistol, double shotgun, ball */
};

#define NUMBER_OF_PLAYER_INITIAL_ITEMS (sizeof(player_initial_items)/sizeof(short))
static short player_initial_items[]= 
{ 
	_i_magnum,  // First weapon is the weapon he will use...
	_i_knife,
	_i_knife,
	_i_magnum_magazine, 
	_i_magnum_magazine,
	_i_magnum_magazine,
	
	// LP additions, in case one wants to start very loaded
     // AS: if we want to start loaded, shouldn't it be '_i_bong'?
	_i_knife,
	_i_knife,
	_i_knife,
	_i_knife,
	_i_knife,
	
	_i_knife,
	_i_knife,
	_i_knife,
	_i_knife,
	_i_knife
};
	
#define NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS (sizeof(damage_response_definitions)/sizeof(struct damage_response_definition))

static struct damage_response_definition damage_response_definitions[]=
{
	{_damage_explosion, 100, _fade_yellow, NONE, _snd_human_scream, _monster_is_dying_hard},
	{_damage_crushing, NONE, _fade_red, NONE, _snd_human_wail, _monster_is_dying_hard},
	{_damage_projectile, NONE, _fade_red, NONE, _snd_human_scream, NONE},
	{_damage_shotgun_projectile, NONE, _fade_red, NONE, _snd_human_scream, NONE},
	{_damage_electrical_staff, NONE, _fade_cyan, NONE, _snd_human_scream, NONE},
	{_damage_hulk_slap, NONE, _fade_cyan, NONE, _snd_human_scream, NONE},
	{_damage_absorbed, 100, _fade_white, _snd_absorbed, NONE, NONE},
	{_damage_teleporter, 100, _fade_white, _snd_absorbed, NONE, NONE},
	{_damage_flame, NONE, _fade_orange, NONE, _snd_human_wail, _monster_is_dying_flaming},
	{_damage_hound_claws, NONE, _fade_red, NONE, _snd_human_scream, NONE},
	{_damage_compiler_bolt, NONE, _fade_static, NONE, _snd_human_scream, NONE},
	{_damage_alien_projectile, NONE, _fade_dodge_purple, NONE, _snd_human_wail, _monster_is_dying_flaming},
	{_damage_hunter_bolt, NONE, _fade_burn_green, NONE, _snd_human_scream, NONE},
	{_damage_fusion_bolt, 60, _fade_negative, NONE, _snd_human_scream, NONE},
	{_damage_fist, 40, _fade_red, NONE, _snd_human_scream, NONE},
	{_damage_yeti_claws, NONE, _fade_burn_cyan, NONE, _snd_human_scream, NONE},
	{_damage_yeti_projectile, NONE, _fade_dodge_yellow, NONE, _snd_human_scream, NONE},
	{_damage_defender, NONE, _fade_purple, NONE, _snd_human_scream, NONE},
	{_damage_lava, NONE, _fade_long_orange, NONE, _snd_human_wail, _monster_is_dying_flaming},
	{_damage_goo, NONE, _fade_long_green, NONE, _snd_human_wail, _monster_is_dying_flaming},
	{_damage_suffocation, NONE, NONE, NONE, _snd_suffocation, _monster_is_dying_soft},
	{_damage_energy_drain, NONE, NONE, NONE, NONE, NONE},
	{_damage_oxygen_drain, NONE, NONE, NONE, NONE, NONE},
	{_damage_hummer_bolt, NONE, _fade_flicker_negative, NONE, _snd_human_scream, NONE},
};

// LP change: made this much bigger than the number of M2/Moo polygons
#define NO_TELEPORTATION_DESTINATION INT16_MAX

// These are all configureable with MML.
struct player_settings_definition player_settings = {
	PLAYER_MAXIMUM_SUIT_ENERGY,    // InitialEnergy
	PLAYER_MAXIMUM_SUIT_OXYGEN,    // InitialOxygen
	PLAYER_MAXIMUM_SUIT_ENERGY/4,  // StrippedEnergy
	PLAYER_MAXIMUM_SUIT_ENERGY,    // SingleEnergy
	2*PLAYER_MAXIMUM_SUIT_ENERGY,  // DoubleEnergy
	3*PLAYER_MAXIMUM_SUIT_ENERGY,  // TripleEnergy
	FIXED_ONE_HALF,                // PlayerSelfLuminosity
	true,                          // CanSwim
	false,                         // PlayerShotsGuided
	QUARTER_CIRCLE/3,              // PlayerHalfVisualArc
	QUARTER_CIRCLE/3,              // PlayerHalfVertVisualArc
	31,                            // PlayerVisualRange
	31,                            // PlayerDarkVisualRange
	1,                             // OxygenDepletion
	0,                             // OxygenReplenishment
	0,                             // OxygenChange
	_damage_fusion_bolt            // Vulnerability
};

// LP: the various powerup item ID's are changeable, but set to appropriate defaults here
struct player_powerup_definition player_powerups = {
	_i_invincibility_powerup,
	_i_invisibility_powerup,
	_i_infravision_powerup,
	_i_extravision_powerup,
	_i_triple_energy_powerup,
	_i_double_energy_powerup,
	_i_energy_powerup,
	_i_oxygen_powerup
};

/* ---------- private prototypes */

static void set_player_shapes(short player_index, bool animate);
void revive_player(short player_index);
static void bind_player_to_level(Player& player);
static void kill_player(short player_index, short aggressor_player_index, short action);
static void give_player_initial_items(short player_index);
static void get_player_transfer_mode(short player_index, short *transfer_mode, short *transfer_period);
static void set_player_dead_shape(short player_index, bool dying);
static void remove_dead_player_items(short player_index);
static void update_player_teleport(short player_index);
static void handle_player_in_vacuum(short player_index, uint32 action_flags);
static void update_player_media(short player_index);
static short calculate_player_team(short base_team);

static void try_and_strip_player_items(short player_index);

static void ReplenishPlayerOxygen(short player_index, uint32 action_flags);

// From AlexJLS patch; monster data necessary so that player as monster can be activated to make guided missiles work
static void adjust_player_physics(monster_data *me);


/* ---------- code */

Player *get_player_data(const size_t player_index)
{
    return &players.at(player_index);
}


int16_t get_number_of_players()
{
    return players.size();
}




// called in new_game and load_level, which is dumb

//extern std::vector<player_identity_t> player_identities; // temporary

void initialize_network_players()
{
    TODO("multiplayer");
    // TODO: can create_network_player_identities (in setup_game.cpp) be merged in here? (order of operations is significant)
    /*
     // manky mess: there should be a single call which returns a vector
     int32_t count = NetGetNumberOfPlayers();
     for (int32_t i = 0; i < count; i++)
     {
         player_info* player = NetGetPlayerData(i);
         player_identities.push_back({NetGetPlayerIdentifier(i), player->team, player->color, player->name});
     }
     */
    
    /*
    clear_players();
    for (auto& identity : player_identities) { create_player(identity); }
    
    short local_player_index_ = game_is_networked() ? NetGetLocalPlayerIndex() : 0;
    assert_fail_f(local_player_index_ < players.size(), "local_player_index=%d but only %zu players", local_player_index_, players.size());
    set_local_player_index(local_player_index_);
    set_current_player_index(local_player_index_);
     */
}


void initialize_player_for_solo_game()
{
    clear_players();
    
    create_player({0, player_preferences.color, player_preferences.color, player_preferences.name});
    //players[0].identity.set_doesnt_auto_switch_weapons(dont_switch_to_new_weapon()); // TODO: customizations need stored on Player, presumably with versioning
    
    set_local_player_index(0);
    set_current_player_index(0);
    
    dynamic_world.game_information.difficulty_level = player_preferences.difficulty_level;
    set_custom_behaviors_enabled(true);
}


void bind_current_players_to_level()
{
    for (auto& player : players) { bind_player_to_level(player); }
}


// returns player index
int16_t create_player(player_identity_t identity)
{
    int16_t player_index = players.size();
    assert_fail(player_index < MAXIMUM_NUMBER_OF_PLAYERS, "");
    Player& player = players.emplace_back();
    player.player_index = player_index;

    player.teleporting_destination = NO_TELEPORTATION_DESTINATION;
    player.hud_flags               = 0; // Doesn't matter-> give_player_initial_items will take care of it.
    player.suit_energy             = player_settings.InitialEnergy;
    player.suit_oxygen             = player_settings.InitialOxygen;
	
    player.invincibility_duration  = 0;
    player.invisibility_duration   = 0;
    player.infravision_duration    = 0;
    player.extravision_duration    = 0;
    
    //SET_PLAYER_DOESNT_AUTO_SWITCH_WEAPONS_STATUS(player, player_identifier_doesnt_auto_switch_weapons(identifier));
    // TODO: obvious refactor is obvious
  //  player.flags      = identity.flags; TODO: FIX
    player.identifier = identity.identifier;
    player.color      = identity.color;
    player.team       = identity.team;
    player.name       = identity.name;
	
	// initialize inventory
    for (int16_t i = 0; i < NUMBER_OF_ITEMS; i++) { player.items[i] = NONE; }

	// attach Player instance to a Marine monster in map, and set up physics, shapes, and other miscellenia
	bind_player_to_level(player); // annoying indexes are annoying; TODO: pass player instance and code that needs index gets it from that

	// Mark the player's inventory as dirty
	mark_player_inventory_as_dirty(player_index, NONE);
	initialize_player_weapons(player_index);
	
	// give the player his initial items
	give_player_initial_items(player_index);
	try_and_strip_player_items(player_index);
	
	return player_index;
}


void change_view_to_next_player()
{
	Player* player;
	short player_index= current_player_index;
	
	/* find the next player in the list we can look at and switch to them */
	do
	{
		if ((player_index+= 1)>=get_number_of_players()) player_index= 0;
		player= get_player_data(player_index);
	}
	while (!(GET_GAME_OPTIONS()&_overhead_map_is_omniscient) && local_player->team!=player->team);
	
	if (current_player_index!=player_index)
	{
		set_current_player_index(player_index);
        reset_motion_sensor(current_player_index);
		dirty_terminal_view(player_index); /* In case they are in terminal mode.. */
	}
}


void clear_players()
{
    set_local_player_index(NONE);
    set_current_player_index(NONE);
    
    players.clear();
    
    memset(team_damage_given, 0, sizeof(team_damage_given));
    memset(team_damage_taken, 0, sizeof(team_damage_taken));
    memset(team_monster_damage_taken, 0, sizeof(team_monster_damage_taken));
    memset(team_monster_damage_given, 0, sizeof(team_monster_damage_given));
    memset(team_friendly_fire, 0, sizeof(team_friendly_fire));
}



/* This will be called by entering map for two reasons:
 * 1) get rid of crap typed between levels, by accident.
 * 2) loading a game doesn't currently reset the player queues, so garbage will cause lags.
 */
/* The above comment is stale.  Now loading map calls this and so does new_game. Calling this from entering map would bone us. */
//static void reset_player_queues()

// ZZZ addition: need to reset (potentially) multiple sets of ActionQueues, not just the RealActionQueues.
// This function doesn't necessarily belong in this file, but I wasn't sure where else to put it.
void reset_action_queues()
{
    sRealActionQueues.reset();
    reset_intermediate_action_queues();
    reset_recording_and_playback_queues();
    sync_heartbeat_count();
}

// ZZZ: queue_action_flags() replaced by ActionQueues::enqueueActionFlags()
// ZZZ: dequeue_action_flags() replaced by ActionQueues::dequeueActionFlags()
// ZZZ: get_action_queue_size() replaced by ActionQueues::countActionFlags()


// ZZZ addition: keep track of the number of ticks since the local player was in terminal mode
// Note this mechanism is not very careful; should not be used for _important_ decisions.
static int  sLocalPlayerTicksSinceTerminal = 1 * TICKS_PER_MINUTE;

int get_ticks_since_local_player_in_terminal()
{
    return sLocalPlayerTicksSinceTerminal;
}

bool m1_solo_player_in_terminal()
{
	return (static_world.environment_flags & _environment_terminals_stop_time)
		&& (get_number_of_players() == 1) 
		&& player_in_terminal_mode(local_player_index);
}

void update_m1_solo_player_in_terminal(ActionQueues* inActionQueuesToUse)
{
	update_terminal_state_with_action_flags(local_player_index, inActionQueuesToUse->dequeueActionFlags(local_player_index));
	update_terminal_state_for_player(local_player_index);
	sLocalPlayerTicksSinceTerminal = 0;
}

static const auto hotkey_mask = _cycle_weapons_forward | _cycle_weapons_backward;

void decode_hotkeys(ModifiableActionQueues& action_queues)
{
	for (auto player_index = 0; player_index < get_number_of_players(); ++player_index)
	{
		bool suppress_action_flags = false;
		
		auto action_flags = action_queues.peekActionFlags(player_index, 0);
		auto player = get_player_data(player_index);
		player->hotkey = 0;
		
		if (player->hotkey_sequence == 0x03)
		{
			// the next set of flags must contain one cycle flag, or this isn't
			// a legitimate hot key and we need to pass both cycle flags back
			// through (one tick late) because the player somehow managed to
			// activate them at exactly the same time!
			if (action_flags & hotkey_mask)
			{
				suppress_action_flags = true;
				player->hotkey_sequence <<= 2;
				player->hotkey_sequence |= (action_flags >> _cycle_weapons_forward_bit) & 0x03;
			}
			else
			{
				action_queues.modifyActionFlags(player_index, _cycle_weapons_forward, _cycle_weapons_forward);
				action_queues.modifyActionFlags(player_index, _cycle_weapons_forward, _cycle_weapons_backward);
				player->hotkey_sequence = 0;
			}
		}
		else if (player->hotkey_sequence)
		{
			assert_fail((player->hotkey_sequence & 0x0c) == 0x0c, "");
			suppress_action_flags = true;
			player->hotkey_sequence <<= 2;
			player->hotkey_sequence |= (action_flags >> _cycle_weapons_forward_bit) & 0x03;

			if (!film_profile.hotkey_fix || !PLAYER_IS_DEAD(player))
			{
				player->hotkey = 1 + (player->hotkey_sequence & 0x03) + 4 * (((player->hotkey_sequence >> 2) & 0x03) - 1);
			}
			player->hotkey_sequence = 0;
		}
		else if ((action_flags & hotkey_mask) == hotkey_mask)
		{
			suppress_action_flags = true;
			player->hotkey_sequence = 0x03;
		}

		if (suppress_action_flags)
		{
			action_queues.modifyActionFlags(player_index, 0, _cycle_weapons_forward);
			action_queues.modifyActionFlags(player_index, 0, _cycle_weapons_backward);
		}
	}
}

/* assumes ∂t==1 tick */
void update_players(ActionQueues* inActionQueuesToUse, bool inPredictive)
{
	if (!inPredictive)
	{
		// ZZZ: update ticks-since-terminal stuff
		sLocalPlayerTicksSinceTerminal++;
		if (player_in_terminal_mode(local_player_index)) sLocalPlayerTicksSinceTerminal = 0;
	}
	
    for (int16_t player_index = 0; player_index < players.size(); player_index++)
	{
        Player* player = &players[player_index];
		uint32 action_flags = inActionQueuesToUse->dequeueActionFlags(player_index);

		if (action_flags == 0xffffffff)
		{
			// net dead
			if(!player->netdead && !inPredictive)
			{
				if (!PLAYER_IS_DEAD(player))
				{
					// kills invincible players, too  (ZZZ annotation: unless physics model says otherwise)
					detonate_projectile(&player->location, player->camera_polygon_index, _projectile_minor_fusion_dispersal,
			 NONE, NONE, 10*FIXED_ONE);
				}

                screen_print_f("%s has become disconnected", player->name.c_str());
				player->netdead = true;
			}

			action_flags= 0;
		}

		if (PLAYER_IS_TELEPORTING(player)) action_flags= 0;
		
		/* Deal with the terminal mode crap. */
		if (player_in_terminal_mode(player_index))
		{
			if(!inPredictive)
			{
				update_terminal_state_with_action_flags(player_index, action_flags);
				update_terminal_state_for_player(player_index);
			}
			action_flags= 0;
		}
		
		player->run_key = action_flags & _run_dont_walk;
		
		bool IsSwimming = TEST_FLAG(player->variables.flags,_HEAD_BELOW_MEDIA_BIT) && player_settings.CanSwim;

		// if we’ve got the ball we can’t run (that sucks) Benad: also works with _game_of_rugby and _game_of_capture_the_flag
		// LP change: made it possible to swim under a liquid if one has the ball
        if (IsSwimming)
        {
            // if our head is under media, we can’t run (that sucks, too)
            if (action_flags & _run_dont_walk)
            {
                action_flags &= ~_run_dont_walk;
                action_flags |= _swim;
            }
        }
        else
        {
            if (GET_GAME_TYPE() == _game_of_kill_man_with_ball && dynamic_world.ball_player_index == player_index)
            {
                action_flags&= ~_run_dont_walk;
            }
            if ((GET_GAME_TYPE() == _game_of_rugby || GET_GAME_TYPE() == _game_of_capture_the_flag) && find_player_ball_color(player_index) != NONE)
            {
                action_flags&= ~_run_dont_walk;
            }
        }

		update_player_physics_variables(player_index, action_flags, inPredictive);

		if (!inPredictive)
		{
			player->invisibility_duration= FLOOR(player->invisibility_duration-1, 0);
			player->invincibility_duration= FLOOR(player->invincibility_duration-1, 0);
			player->infravision_duration= FLOOR(player->infravision_duration-1, 0);
			// ZZZ: alert the player when he can respawn, if he was being penalized
			if(player->reincarnation_delay > 0)
			{
				--player->reincarnation_delay;
				short message_player_index = local_player_index;
				if(game_is_replay())
				{
					message_player_index = current_player_index;
				}
				if(((GET_GAME_OPTIONS()&_suicide_is_penalized) || (GET_GAME_OPTIONS()&_dying_is_penalized)) && (player_index == message_player_index))
				{
					if(player->reincarnation_delay == 0)
                    {
                        screen_print("You may rise to fight again");
                    }
					else if(player->reincarnation_delay < 4 * TICKS_PER_SECOND && (player->reincarnation_delay % TICKS_PER_SECOND) == 0)
                    {
                        screen_print_f("%d...", player->reincarnation_delay / TICKS_PER_SECOND);
                    }
				}
			}
			if (player->extravision_duration)
			{
				if ((player->extravision_duration -= 1) == 0)
				{
					if (player_index==current_player_index) deactivate_wide_vision();
				}
			}
			// LP change: made this code more general;
			// find the oxygen-change rate appropriate to each environment,
			// then handle the rate appropriately.
			if ((static_world.environment_flags&_environment_vacuum) || (player->variables.flags&_HEAD_BELOW_MEDIA_BIT))
				player_settings.OxygenChange = - player_settings.OxygenDepletion;
			else
				player_settings.OxygenChange = player_settings.OxygenReplenishment;

			if (player_settings.OxygenChange < 0)
				handle_player_in_vacuum(player_index, action_flags);
			else if (player_settings.OxygenChange > 0)
				ReplenishPlayerOxygen(player_index, action_flags);

			// if ((static_world.environment_flags&_environment_vacuum) || (player->variables.flags&_HEAD_BELOW_MEDIA_BIT)) handle_player_in_vacuum(player_index, action_flags);

			if (PLAYER_IS_DEAD(player))
			{
				/* do things dead players do (sit around and check for self-reincarnation) */
				if (PLAYER_HAS_MAP_OPEN(player))
					SET_PLAYER_MAP_STATUS(player, false);

				auto player_is_stationary =
					(player->variables.action == _player_stationary) ||
					(get_number_of_players() == 1) ||
					(film_profile.finally_respawn &&
					 dynamic_world.tick_count >
					 player->ticks_at_death + 15 * TICKS_PER_SECOND);

				if (PLAYER_IS_TOTALLY_DEAD(player) &&
					(action_flags&_action_trigger_state) &&
					player_is_stationary)
				{
					// ZZZ: let the player know why he's not respawning
					if (player->reincarnation_delay)
					{
						short message_player_index = game_is_replay() ? current_player_index : local_player_index;
						if (player_index == message_player_index)
						{
							int theSeconds = player->reincarnation_delay / TICKS_PER_SECOND;
							// If 3 or less, he'll be getting a countdown anyway, and may start spamming the action key.
                            if (theSeconds > 3) { screen_print_f("%d penalty seconds remain", theSeconds); }
						}
					}
					else
					{
						if (get_number_of_players() == 1)
						{
                            exit_game_event_loop(app_state_t::revert_to_saved_game); // TODO: smelly; check this
						}
						else revive_player(player_index);
					}
				}
				update_player_weapons(player_index, 0);
				update_action_key(player_index, false);
			}
			else
			{
				/* do things live players do (get items, update weapons, check action key, breathe) */
				swipe_nearby_items(player_index);
				update_player_weapons(player_index, action_flags);
				update_action_key(player_index, (action_flags&_action_trigger_state) ? true : false);
				if (action_flags&_toggle_map)
					SET_PLAYER_MAP_STATUS(player, !PLAYER_HAS_MAP_OPEN(player));

				// ZZZ: moved this here out of "player becoming netdead" area above; that looked wrong
				// AlexJLS patch: effect of dangerous polygons
				cause_polygon_damage(player->supporting_polygon_index,player->monster_index);
			}

			update_player_teleport(player_index);
			update_player_media(player_index);
			set_player_shapes(player_index, true);
			
		} // !inPredictive
	}
}



void damage_player(
	short monster_index,
	short aggressor_index,
	short aggressor_type,
	struct damage_definition *damage,
	short projectile_index)
{
	short player_index= monster_index_to_player_index(monster_index);
	short aggressor_player_index= NONE; /* will be valid if the aggressor is a player */
	Player* player= get_player_data(player_index);
	short damage_amount= calculate_damage(damage);
	short damage_type= damage->type;
	struct damage_response_definition *definition;

	(void) (aggressor_type);
	
	// LP change: made this more general
	if (player->invincibility_duration && damage->type!=player_settings.Vulnerability)
	{
		damage_type= _damage_absorbed;
	}

	{
		unsigned i;
		
		for (i=0,definition=damage_response_definitions;
				definition->type!=damage_type && i<NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS;
				++i,++definition)
			;
		assert_warn_f(i!=NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS, "can't react to damage type #%d", damage_type);
		// assert_fail_f(i!=NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS, "can't react to damage type #%d", damage_type);
	}
	
	if (damage_type!=_damage_absorbed)
	{
		/* record damage taken */
		if (aggressor_index!=NONE)
		{
			struct monster_data *aggressor= get_monster_data(aggressor_index);
			
			if (!PLAYER_IS_DEAD(player))
			{
				if (MONSTER_IS_PLAYER(aggressor))
				{
					Player* aggressor_player;
					
					aggressor_player_index= monster_index_to_player_index(aggressor_index);
					aggressor_player= get_player_data(aggressor_player_index);
					player->damage_taken[aggressor_player_index].damage+= damage_amount;
					aggressor_player->total_damage_given.damage+= damage_amount;
					team_damage_taken[player->team].damage += damage_amount;
					team_damage_given[aggressor_player->team].damage += damage_amount;
					if (player->team == aggressor_player->team) {
					  team_friendly_fire[player->team].damage += damage_amount;
					}
				}
				else
				{
					player->monster_damage_taken.damage+= damage_amount;
					team_monster_damage_taken[player->team].damage+= damage_amount;
				}
			}
		}

		switch (damage->type)
		{
			case _damage_oxygen_drain:
			{
				// LP change: pegging to maximum value
				player->suit_oxygen= int16(MIN(int32(player->suit_oxygen)-int32(damage_amount),int32(INT16_MAX)));
				L_Call_Player_Damaged(player_index, aggressor_player_index, aggressor_index, damage->type, damage_amount, projectile_index);
				if (player->suit_oxygen < 0) player->suit_oxygen= 0;
				if (player_index==current_player_index) mark_oxygen_display_as_dirty();
				break;
			}
			default:
			{
				// LP change: pegging to maximum value
				player->suit_energy= int16(MIN(int32(player->suit_energy)-int32(damage_amount),int32(INT16_MAX)));
				L_Call_Player_Damaged(player_index, aggressor_player_index, aggressor_index, damage->type, damage_amount, projectile_index);			
				/* damage the player, recording the kill if the aggressor was another player and we died */
				if (player->suit_energy<0)
				{
					if (damage->type!=_damage_energy_drain)
					{
						if (!PLAYER_IS_DEAD(player))
						{
							short action= definition->death_action;
							
							play_object_sound(player->object_index, definition->death_sound, player_index == current_player_index);

							if (action==NONE)
							{
								action= (damage_amount>PLAYER_MAXIMUM_SUIT_ENERGY/2) ? _monster_is_dying_hard : _monster_is_dying_soft;
							}
							
							kill_player(player_index, aggressor_player_index, action);
#if !defined(DISABLE_NETWORKING)
							if (aggressor_player_index!=NONE)
							{
								Player* aggressor_player= get_player_data(aggressor_player_index);
								
								if (player_killed_player(player_index, aggressor_player_index))
								{
									player->damage_taken[aggressor_player_index].kills+= 1;
									team_damage_taken[player->team].kills += 1;
									if (aggressor_player_index != player_index)
									{
										aggressor_player->total_damage_given.kills+= 1;
										team_damage_given[aggressor_player->team].kills += 1;
									}
									if (player->team == aggressor_player->team) {
									  team_friendly_fire[aggressor_player->team].kills += 1;
									}
								}
								Console::instance()->report_kill(player_index, aggressor_player_index, projectile_index);
							}
							else
#endif // !defined(DISABLE_NETWORKING)
							{
								player->monster_damage_taken.kills+= 1;
								team_monster_damage_taken[player->team].kills += 1;
							}
							L_Call_Player_Killed (player_index, aggressor_player_index, action, projectile_index);
						}
						
						player->suit_oxygen= 0;
						if (player_index==current_player_index) mark_oxygen_display_as_dirty();
					}
					
					player->suit_energy= 0;
				}
				break;
			}
		}
	}
	
	{
		if (!PLAYER_IS_DEAD(player))
        {
            play_object_sound(player->object_index, definition->sound, player_index == current_player_index);
        }
		if (player_index == current_player_index)
		{
            short fade = definition->fade;
			if (fade != NONE)
            {
                if (definition->damage_threshhold != NONE && damage_amount > definition->damage_threshhold) fade += 1;
                start_gameworld_damage_effect(fade);
            }
			if (damage_amount) mark_shield_display_as_dirty();
		}
	}

	if (player_in_terminal_mode(player_index)) { abort_terminal_mode(player_index); }
}


short player_identifier_to_player_index(short player_identifier)
{
	Player* player;
	short player_index;
	
	for (player_index=0;player_index<get_number_of_players();++player_index)
	{
		player= get_player_data(player_index);
		
		if (player->identifier==player_identifier) break;
	}
	assert_fail(player_index!=get_number_of_players(), "");
	
	return player_index;
}


void mark_player_collections(bool loading)
{
	mark_collection(player_shapes.collection, loading);
	mark_weapon_collections(loading);
	mark_item_collections(loading);
	mark_interface_collections(loading);
}


player_shape_definitions* get_player_shape_definitions()
{
    return &player_shapes;
}


void set_local_player_index(short player_index) // the player being controlled by the user (unclear if this has any meaning in film replays)
{
	local_player_index = player_index;
	local_player = player_index == NONE ? nullptr : get_player_data(player_index);
}


void set_current_player_index(short player_index) // the player currently viewing the gameworld (film replays allow switching between players)
{
	current_player_index= player_index;
	current_player = player_index == NONE ? nullptr : get_player_data(player_index);
}


void team_damage_from_player_data()
{
  for (short player_index = 0; player_index < get_number_of_players(); player_index++) {
    Player* player = get_player_data(player_index);
    team_damage_given[player->team].damage += player->total_damage_given.damage;
    team_damage_given[player->team].kills += player->total_damage_given.kills;
    team_monster_damage_given[player->team].damage += player->monster_damage_given.damage;
    team_monster_damage_given[player->team].kills += player->monster_damage_given.kills;
    team_monster_damage_taken[player->team].damage += player->monster_damage_taken.damage;
    team_monster_damage_taken[player->team].kills += player->monster_damage_taken.kills; 
    for (short opponent_index = 0; opponent_index < get_number_of_players(); opponent_index++) {
      Player* opponent = get_player_data(player_index);
      team_damage_taken[player->team].damage += player->damage_taken[opponent_index].damage;
      team_damage_taken[player->team].kills += player->damage_taken[opponent_index].kills;
      if (player->team == opponent->team) {
	team_friendly_fire[player->team].damage += player->damage_taken[opponent_index].damage;
	team_friendly_fire[player->team].kills += player->damage_taken[opponent_index].kills;
      }
    }
  }
}   


Player& get_player_with_monster_index(short monster_index)
{
    for (auto& player : players)
    {
        if (player.monster_index == monster_index) return player;
    }
    throw_bug_report_f("Didn't find a player with monster index: %d", monster_index);
}


short monster_index_to_player_index(short monster_index)
{
    return get_player_with_monster_index(monster_index).player_index;
}


short get_polygon_index_supporting_player(short monster_index)
{
	return get_player_with_monster_index(monster_index).supporting_polygon_index;
}


bool legal_player_powerup(
	short player_index,
	short item_index)
{
	Player* player= get_player_data(player_index);
	bool legal= true;

	if ((static_world.environment_flags & _environment_m1_weapons)
		&& film_profile.m1_bce_pickup)
	{
		return true;
	}

	if (item_index == player_powerups.Powerup_Invincibility)
	{
		if (player->invincibility_duration) legal= false;
	}
	else if (item_index == player_powerups.Powerup_Invisibility)
	{
		if (player->invisibility_duration>kINVISIBILITY_DURATION) legal= false;
	}
	else if (item_index == player_powerups.Powerup_Infravision)
	{
		if (player->infravision_duration) legal= false;
	}
	else if (item_index == player_powerups.Powerup_Extravision)
	{
		if (player->extravision_duration) legal= false;
	}
	else if (item_index == player_powerups.Powerup_TripleEnergy)
	{
		if (player->suit_energy>=player_settings.TripleEnergy) legal= false;
	}
	else if (item_index == player_powerups.Powerup_DoubleEnergy)
	{
		if (player->suit_energy>=player_settings.DoubleEnergy) legal= false;
	}
	else if (item_index == player_powerups.Powerup_Energy)
	{
		if (player->suit_energy>=player_settings.SingleEnergy) legal= false;
	}
	else if (item_index == player_powerups.Powerup_Oxygen)
	{
		if (player->suit_oxygen>=5*PLAYER_MAXIMUM_SUIT_OXYGEN/6) legal= false;
	}

	return legal;
}

void process_player_powerup(
	short player_index,
	short item_index)
{
	Player* player= get_player_data(player_index);
	
	if (item_index == player_powerups.Powerup_Invincibility)
	{
		player->invincibility_duration+= kINVINCIBILITY_DURATION;
	}
	else if (item_index == player_powerups.Powerup_Invisibility)
	{
		player->invisibility_duration+= kINVISIBILITY_DURATION;
	}
	else if (item_index == player_powerups.Powerup_Infravision)
	{
		player->infravision_duration+= kINFRAVISION_DURATION;
	}
	else if (item_index == player_powerups.Powerup_Extravision)
	{
		if (player_index==current_player_index) activate_wide_vision();
		player->extravision_duration+= kEXTRAVISION_DURATION;
	}
	else if (item_index == player_powerups.Powerup_TripleEnergy)
	{
		if (player->suit_energy<player_settings.TripleEnergy)
		{
			player->suit_energy= player_settings.TripleEnergy;
			if (player_index==current_player_index) mark_shield_display_as_dirty();
		}
	}
	else if (item_index == player_powerups.Powerup_DoubleEnergy)
	{
		if (player->suit_energy<player_settings.DoubleEnergy)
		{
			player->suit_energy= player_settings.DoubleEnergy;
			if (player_index==current_player_index) mark_shield_display_as_dirty();
		}
	}
	else if (item_index == player_powerups.Powerup_Energy)
	{
		if (player->suit_energy<player_settings.SingleEnergy)
		{
			player->suit_energy= player_settings.SingleEnergy;
			if (player_index==current_player_index) mark_shield_display_as_dirty();
		}
	}
	else if (item_index == player_powerups.Powerup_Oxygen)
	{
		player->suit_oxygen= CEILING(player->suit_oxygen+PLAYER_MAXIMUM_SUIT_OXYGEN/2, PLAYER_MAXIMUM_SUIT_OXYGEN);
		if (player_index==current_player_index) mark_oxygen_display_as_dirty();
	}
}


world_distance dead_player_minimum_polygon_height(short polygon_index)
{
	for (auto& player : players)
	{
        if (PLAYER_IS_DEAD(&player) && player.camera_polygon_index == polygon_index) { return DEAD_PLAYER_HEIGHT; }
	}
    return 0;
}


bool try_and_subtract_player_item(short player_index, short item_type)
{
	Player* player= get_player_data(player_index);
	bool found_one= false;

	assert_fail(item_type>=0 && item_type<NUMBER_OF_ITEMS, "");
	if (player->items[item_type]>=0)
	{
		if (!(player->items[item_type]-= 1)) player->items[item_type]= NONE;
		mark_player_inventory_as_dirty(player_index, item_type);
		found_one= true;
	}
	
	return found_one;
}

/* ---------- private prototypes */

// LP change: assumes nonpositive change rate
static void handle_player_in_vacuum(
	short player_index,
	uint32 action_flags)
{
	Player* player= get_player_data(player_index);

	if (player->suit_oxygen>0)	
	{
		short breathing_frequency;
		
		// lolbungie
		breathing_frequency = TICKS_PER_MINUTE/2;
		/*
		switch (player->suit_oxygen/TICKS_PER_MINUTE)
		{
			case 0: breathing_frequency= TICKS_PER_MINUTE/6;
			case 1: breathing_frequency= TICKS_PER_MINUTE/5;
			case 2: breathing_frequency= TICKS_PER_MINUTE/4;
			case 3: breathing_frequency= TICKS_PER_MINUTE/3;
			default: breathing_frequency= TICKS_PER_MINUTE/2;
		}
		 */
		
		assert_fail(player_settings.OxygenChange <= 0, "");
		short oxygenChange = player_settings.OxygenChange;
		switch (dynamic_world.game_information.difficulty_level)
		{
			case _total_carnage_level:
				if (action_flags&_run_dont_walk) oxygenChange+= player_settings.OxygenChange;
			case _major_damage_level:
				if (action_flags&(_left_trigger_state|_right_trigger_state)) oxygenChange+= player_settings.OxygenChange;
				break;
		}
		
		while (oxygenChange < 0)
		{
			player->suit_oxygen -= 1;
			oxygenChange += 1;
			if (player->suit_oxygen % breathing_frequency == 0 &&
				player_index == current_player_index)
			{
				sound_manager.PlaySound(Sound_Breathing(), nullptr, NONE);
			}

			const auto offset_o2 = player->suit_oxygen + OXYGEN_WARNING_OFFSET;
			if (offset_o2 < OXYGEN_WARNING_LEVEL &&
				offset_o2 % OXYGEN_WARNING_FREQUENCY == 0 &&
				player_index == current_player_index)
			{
				sound_manager.PlaySound(Sound_OxygenWarning(), nullptr, NONE);
			}
		}
				
		if (player->suit_oxygen<=0)
		{
			struct damage_definition damage;
			
			damage.flags= 0;
			damage.type= _damage_suffocation;
			damage.base= player->suit_energy+1;
			damage.random= 0;
			damage.scale= FIXED_ONE;
			
			damage_player(player->monster_index, NONE, NONE, &damage, NONE);
		}
	}
}

// LP: assumes nonnegative change rate
static void ReplenishPlayerOxygen(short player_index, uint32 action_flags)
{
	(void)(action_flags);
	
	Player* player= get_player_data(player_index);
	
	// Be careful to avoid short-integer wraparound
	assert_fail(player_settings.OxygenChange >= 0, "");
	if (player->suit_oxygen < PLAYER_MAXIMUM_SUIT_OXYGEN)
	{
		if (player->suit_oxygen < PLAYER_MAXIMUM_SUIT_OXYGEN - player_settings.OxygenChange)
			player->suit_oxygen += player_settings.OxygenChange;
		else
			player->suit_oxygen = PLAYER_MAXIMUM_SUIT_OXYGEN;
	}
}

extern bool shapes_file_is_m1();


// TODO: how and where is this used (it's not just for level jumps)
static void update_player_teleport(short player_index)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct object_data *object= get_object_data(monster->object_index);
	struct polygon_data *polygon= get_polygon_data(object->polygon);
	bool player_was_interlevel_teleporting= false;
	
	/* This is the players that are carried across the teleport unwillingly.. */
	if(PLAYER_IS_INTERLEVEL_TELEPORTING(player))
	{
		player_was_interlevel_teleporting= true;
		
		player->interlevel_teleport_phase+= 1;
		switch(player->interlevel_teleport_phase)
		{
			case PLAYER_TELEPORTING_DURATION:
				monster->action= _monster_is_moving;
				SET_PLAYER_TELEPORTING_STATUS(player, false);
				SET_PLAYER_INTERLEVEL_TELEPORTING_STATUS(player, false);
				break;

			/* +1 because they are teleporting to a new level, and we want the squeeze in to happen */
			/*  after the level transition */
			case PLAYER_TELEPORTING_MIDPOINT+1:
				/* Either the player is teleporting, or everyone is. (level change) */
				if (entering_level_uses_teleport_effect()) 
				{
					if (player_index == current_player_index) 
					{
						start_teleport_in_effect();
						if (shapes_file_is_m1()) start_gameworld_damage_effect(_fade_bright);
					}
                    
					play_object_sound(player->object_index, Sound_TeleportIn(), player_index == current_player_index);
				}
				player->teleporting_destination= NO_TELEPORTATION_DESTINATION;
				break;
				
			default:
				break;
		}
	}

	if (PLAYER_IS_TELEPORTING(player))
	{
		switch (player->teleporting_phase+= 1)
		{
			case PLAYER_TELEPORTING_MIDPOINT:
				if (player->teleporting_destination >= 0) // if
				{
					short destination_polygon_index= player->teleporting_destination;
					struct polygon_data *destination_polygon= get_polygon_data(destination_polygon_index);
					struct damage_definition damage;
					world_point3d destination;
					
					/* Determine where we are going. */
					destination.x= destination_polygon->center.x;
					destination.y= destination_polygon->center.y;
					destination.z= destination_polygon->floor_height;

					damage.type= _damage_teleporter;
					damage.base= damage.random= damage.flags= damage.scale= 0;
					damage_monsters_in_radius(NONE, NONE, NONE, &destination, destination_polygon_index, WORLD_ONE, &damage, NONE);

					translate_map_object(player->object_index, &destination, destination_polygon_index);
					initialize_player_physics_variables(player_index);
	
					// LP addition: handles the current player's chase cam; in screen.c, we find that it's the current player whose view gets rendered
					if (player_index == current_player_index) ChaseCam_Reset();
				}
                else // -ve level number = interlevel (why?)
                {
                    set_next_level_number(-player->teleporting_destination - 1);
                    
					// change to the next level (if this is the last level, it will be handled further on)
					exit_game_event_loop(app_state_t::change_level); // TODO: FIX: this isn't right; we need to break out of game event loop and ensure state transitions from game event loop to the new state as soon as main event loop resumes
					
				}
				break;

			case PLAYER_TELEPORTING_MIDPOINT+1:
				 /* Interlevel or my intralevel.. */
				if (player->teleporting_destination >= 0 || entering_level_uses_teleport_effect())
				{
					if (player_index == current_player_index)
					{
						start_teleport_in_effect();
						if (shapes_file_is_m1()) start_gameworld_damage_effect(_fade_bright);
					}

					play_object_sound(player->object_index, Sound_TeleportIn(), player_index == current_player_index);
				}
				else {
					player->teleporting_phase = PLAYER_TELEPORTING_DURATION;
				}

				player->teleporting_destination = NO_TELEPORTATION_DESTINATION;

				if (player->teleporting_phase != PLAYER_TELEPORTING_DURATION) break;
			
				[[fallthrough]];
			case PLAYER_TELEPORTING_DURATION:
				monster->action= _monster_is_moving;
				SET_PLAYER_TELEPORTING_STATUS(player, false);
				break;
		}
	}
	else if(!player_was_interlevel_teleporting)
	{
		/* Note that control panels can set the teleporting destination. */
		if (player->teleporting_destination!=NO_TELEPORTATION_DESTINATION ||
			(((polygon->type==_polygon_is_automatic_exit && calculate_level_completion_state()!=_level_unfinished) || polygon->type==_polygon_is_teleporter) &&
				player->variables.position.x==player->variables.last_position.x &&
				player->variables.position.y==player->variables.last_position.y &&
				player->variables.position.z==player->variables.last_position.z &&
				player->variables.last_direction==player->variables.direction &&
				object->location.z==polygon->floor_height))
		{
			if(--player->delay_before_teleport<0)
			{
				SET_PLAYER_TELEPORTING_STATUS(player, true);
				monster->action= _monster_is_teleporting;
				player->teleporting_phase= 0;
				player->delay_before_teleport= 0; /* The only function that changes this are */
													/* computer terminals. */

				/* They are in an automatic exit. */
				if (player->teleporting_destination==NO_TELEPORTATION_DESTINATION)
				{
					if (polygon->type==_polygon_is_automatic_exit && calculate_level_completion_state()!=_level_unfinished)
					{
						/* This is an auto exit, and they are successful */
						// LP change: moved down by 1 so that level 0 will be valid
						player->teleporting_destination= -polygon->permutation - 1;
					}
					else
					{
						/* This is a simple teleporter */
						player->teleporting_destination= polygon->permutation;
					}
				}
	
				if (player->teleporting_destination>=0) /* simple teleport */
				{
					if (player_index==current_player_index) 
					{
						start_teleport_out_effect();
					}
					play_object_sound(player->object_index, Sound_TeleportOut(), player_index == current_player_index);
				}
				else /* Level change */
				{
					short other_player_index;
				
					/* Everyone plays the teleporting effect out. */
					if (exiting_level_uses_teleport_effect()) {
						start_teleport_out_effect();
						play_object_sound(current_player->object_index, Sound_TeleportOut(), player_index == current_player_index);
					}
					
					/* Every players object plays the sound, and everyones monster responds. */
					for (other_player_index= 0; other_player_index<get_number_of_players(); ++other_player_index)
					{
						player= get_player_data(other_player_index);

						/* Set them to be teleporting if the already aren’t, or if they are but it */
						/*  is a simple teleport (intralevel) */
						if (player_index!=other_player_index)
						{
							monster= get_monster_data(player->monster_index);
					
							/* Tell everyone else to use the teleporting junk... */
							SET_PLAYER_INTERLEVEL_TELEPORTING_STATUS(player, true);
							player->interlevel_teleport_phase= other_player_index < player_index ? 0 : -1;
						
							monster->action= _monster_is_teleporting;
						}
					}
				}
			}
		}
	}
}

static void update_player_media(short player_index)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct object_data *object= get_object_data(monster->object_index);
	struct polygon_data *polygon= get_polygon_data(object->polygon);

	{
		short sound_type= NONE;
			
		if (player_index==current_player_index)
		{
			bool under_media= (player->variables.flags&_HEAD_BELOW_MEDIA_BIT);
			short media_index= polygon->media_index;
						
			world_point3d cam_pos;
			short cam_poly;
			angle cam_yaw;
			angle cam_pitch;
			if (ChaseCam_GetPosition(cam_pos, cam_poly, cam_yaw, cam_pitch))
			{
				struct polygon_data *cam_polygon= get_polygon_data(cam_poly);
				media_index= cam_polygon->media_index;
				media_data *media = get_media_data(media_index);
				world_distance media_height= (media_index==NONE || !media) ? INT16_MIN : media->height;
				under_media = (cam_pos.z < media_height);
			}
            
            // TODO: in future we should be able to define tints in air/vacuum as well, e.g. in a corridor with colored/emergency lighting (IIRC MML can already do this in OGL but it'd be neater to do it via fades, which will work in Classic too)
            
            // EES: bringing logic to fades.cpp, we update its active tint only when player's head changes medium
            static int16_t previous_media_index = NONE; // air/vacuum=NONE
            
            if (media_index != previous_media_index)
            {
                if (under_media)
                {
                    start_gameworld_tint_effect(get_media_submerged_fade_effect(media_index));
                    previous_media_index = media_index;
                }
                else
                {
                    stop_gameworld_tint_effect();
                    previous_media_index = NONE;
                }
            }
		}
	
		if (player->variables.flags&_FEET_BELOW_MEDIA_BIT)
		{
			// LP change: idiot-proofing
			struct media_data *media= get_media_data(polygon->media_index); // should be valid
			{
			world_distance current_magnitude= (player->variables.old_flags&_HEAD_BELOW_MEDIA_BIT) ? media->current_magnitude : (media->current_magnitude>>1);
			world_distance external_magnitude= FIXED_TO_WORLD(GUESS_HYPOTENUSE(std::abs(player->variables.external_velocity.i), std::abs(player->variables.external_velocity.j)));
			struct damage_definition *damage= get_media_damage(polygon->media_index, (player->variables.flags&_HEAD_BELOW_MEDIA_BIT) ? FIXED_ONE : FIXED_ONE/4);
			
			// apply current if possible
			if (!PLAYER_IS_DEAD(player) && external_magnitude<current_magnitude) accelerate_player(player->monster_index, 0, NORMALIZE_ANGLE(media->current_direction-HALF_CIRCLE), media->current_magnitude>>2);
			
			// cause damage if possible
			if (damage) damage_player(player->monster_index, NONE, NONE, damage, NONE);
			
			// feet entering media sound
			if (!(player->variables.old_flags&_FEET_BELOW_MEDIA_BIT)) sound_type= _media_snd_feet_entering;
			// head entering media sound
			if (!(player->variables.old_flags&_HEAD_BELOW_MEDIA_BIT) && (player->variables.flags&_HEAD_BELOW_MEDIA_BIT)) sound_type= _media_snd_head_entering;
			// head leaving media sound
			if (!(player->variables.flags&_HEAD_BELOW_MEDIA_BIT) && (player->variables.old_flags&_HEAD_BELOW_MEDIA_BIT)) sound_type= _media_snd_head_leaving;
			}
		}
		else
		{
			// feet leaving media sound
			if (polygon->media_index!=NONE && (player->variables.old_flags&_FEET_BELOW_MEDIA_BIT)) sound_type= _media_snd_feet_leaving;
		}
		
		if (sound_type!=NONE)
		{
			play_object_sound(monster->object_index, get_media_sound(polygon->media_index, sound_type), player_index == current_player_index);
		}
	}

	if (player->variables.flags&_STEP_PERIOD_BIT)
	{
		short sound_index= NONE;
		
		if ((player->variables.flags&_FEET_BELOW_MEDIA_BIT) && !(player->variables.flags&_HEAD_BELOW_MEDIA_BIT))
		{
			sound_index= get_media_sound(polygon->media_index, _media_snd_splashing);
		}
		else
		{
			/* make ordinary walking sound */
		}
		
		if (sound_index!=NONE)
		{
			play_object_sound(monster->object_index, sound_index, player_index == current_player_index);
		}
	}
}

static void set_player_shapes(
	short player_index,
	bool animate)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct physics_variables *variables= &player->variables;
	struct object_data *legs= get_object_data(monster->object_index);
	struct object_data *torso= get_object_data(legs->parasitic_object);
	shape_descriptor new_torso_shape, new_legs_shape;
	short transfer_mode, transfer_period;
	
	get_player_transfer_mode(player_index, &transfer_mode, &transfer_period);
	
	/* if we’re not dead, handle changing shapes (if we are dead, the correct dying shape has
		already been set and we just have to wait for the animation to finish) */
	if (!PLAYER_IS_DEAD(player))
    {
        short torso_shape;
        short mode, pseudo_weapon_type;
        
        get_player_weapon_mode_and_type(player_index, &pseudo_weapon_type, &mode);
        assert_fail_f(pseudo_weapon_type>=0 && pseudo_weapon_type<PLAYER_TORSO_SHAPE_COUNT, "Pseudo Weapon Type out of range: %d", pseudo_weapon_type);
        switch(mode)
        {
            case _shape_weapon_firing: torso_shape= player_shapes.firing_torsos[pseudo_weapon_type]; break;
            case _shape_weapon_idle: torso_shape= player_shapes.torsos[pseudo_weapon_type]; break;
            case _shape_weapon_charging: torso_shape= player_shapes.charging_torsos[pseudo_weapon_type]; break;
            default:
                assert_fail(false, "");
                break;
        }
        assert_fail(player->variables.action>=0 && player->variables.action<NUMBER_OF_PLAYER_ACTIONS, "");
        
        new_legs_shape= BUILD_DESCRIPTOR(BUILD_COLLECTION(player_shapes.collection, player->team), player_shapes.legs[player->variables.action]);
        new_torso_shape= BUILD_DESCRIPTOR(BUILD_COLLECTION(player_shapes.collection, player->color), torso_shape);
        
        /* stuff in the transfer modes */
        if (legs->transfer_mode!=transfer_mode) { legs->transfer_mode= transfer_mode; legs->transfer_period= transfer_period; legs->transfer_phase= 0; }
        if (torso->transfer_mode!=transfer_mode) { torso->transfer_mode= transfer_mode; torso->transfer_period= transfer_period; torso->transfer_phase= 0; }
        
        /* stuff in new shapes only if they have changed (and reset phases if they have) */
        if (new_legs_shape!= legs->shape) { legs->shape= new_legs_shape; legs->sequence= 0; }
        if (new_torso_shape!=torso->shape) { torso->shape= new_torso_shape; torso->sequence= 0; }
	}
	
	if (animate)
	{
		/* animate the player only if we’re not airborne and not totally dead */
		if ((variables->action!=_player_airborne || (PLAYER_IS_TELEPORTING(player) || PLAYER_IS_INTERLEVEL_TELEPORTING(player)))&&!PLAYER_IS_TOTALLY_DEAD(player)) animate_object(monster->object_index);
		if (PLAYER_IS_DEAD(player) && !PLAYER_IS_TELEPORTING(player) && (GET_OBJECT_ANIMATION_FLAGS(legs)&_obj_last_frame_animated) && !PLAYER_IS_TOTALLY_DEAD(player))
		{
			/* we’ve finished the animation; let the player reincarnate if he wants to */
			SET_PLAYER_TOTALLY_DEAD_STATUS(player, true);
			set_player_dead_shape(player_index, false);

			/* If you had something cool, you don't anymore.. */
			remove_dead_player_items(player_index);
		}
	}
}

/* We can rebuild him!! */
void revive_player(short player_index)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct object_location location;
	struct object_data *object;
	short team;

	/* Figure out where the player starts */
	team= calculate_player_team(player->team);
	get_random_player_starting_location_and_facing(get_number_of_players(), team, &location);

	monster->action= _monster_is_moving; /* was probably _dying or something */

	/* remove only the player’s torso, which should be invisible anyway, and turn his legs
		into garbage */
	remove_parasitic_object(monster->object_index);
	register_dead_monster(monster->object_index);

	/* create a new pair of legs, and (completely behind MONSTERS.C’s back) reattach it to
		it’s monster (shape will be set by set_player_shapes, below) */
	player->object_index= monster->object_index= new_map_object(&location, 0);
	object= get_object_data(monster->object_index);
	SET_OBJECT_SOLIDITY(object, true);
	SET_OBJECT_OWNER(object, _object_is_monster);
	object->permutation= player->monster_index;
	
	/* create a new torso (shape will be set by set_player_shapes, below) */
	attach_parasitic_object(monster->object_index, 0, location.yaw);

	initialize_player_physics_variables(player_index);

	player->weapon_intensity_decay= 0;
	player->suit_energy= PLAYER_MAXIMUM_SUIT_ENERGY;
	player->suit_oxygen= PLAYER_MAXIMUM_SUIT_OXYGEN;
	SET_PLAYER_DEAD_STATUS(player, false);
	SET_PLAYER_TOTALLY_DEAD_STATUS(player, false);
	SET_PLAYER_TELEPORTING_STATUS(player, false);
	SET_PLAYER_INTERLEVEL_TELEPORTING_STATUS(player, false);
	player->invincibility_duration= 0;
	player->invisibility_duration= 0;
	player->infravision_duration= 0;
	player->extravision_duration= 0;
	player->control_panel_side_index= NONE; // not using a control panel.

	give_player_initial_items(player_index);

	/* set the correct shapes and transfer mode */
	set_player_shapes(player_index, false);

	try_and_strip_player_items(player_index);

    // TODO: straighten this bodgery
	/* Update the interface to reflect your player's changed status */
    if (player_index==current_player_index) reset_motion_sensor(current_player_index);
	
	// LP addition: handles the current player's chase cam;
	// in screen.c, we find that it's the current player whose view gets rendered
	if (player_index == current_player_index) ChaseCam_Reset();
    
	// LP addition: set field-of-view approrpriately
    if (player_index == current_player_index) main_camera_settings.clear_effect(); // EES: yeesh
        
	L_Call_Player_Revived (player_index);
}

// The player just changed map levels, recreate him, and all of the objects associated with him. // this is also called by create_player (e.g. in synchronize_player_identities)
static void bind_player_to_level(Player& player)
{
	short monster_index;
	monster_data* monster;
	short placement_team;
	object_location location;
	bool  player_teleported_dead= false;
	
	/* Determine the location */
	placement_team= calculate_player_team(player.team);
	get_random_player_starting_location_and_facing(player.player_index, placement_team, &location);

	/* create an object and a monster for this player */
	monster_index= new_monster(&location, _monster_marine);
	monster= get_monster_data(monster_index);

	/* add our parasitic torso */
	attach_parasitic_object(monster->object_index, 0, location.yaw);
	
	/* and initialize it */
	if(PLAYER_IS_TOTALLY_DEAD(&player) || PLAYER_IS_DEAD(&player))
	{
		player_teleported_dead= true;
	}

	/* Clear the transient flags, leave the persistant flags */
    player.flags &= (_player_is_teleporting_flag | _player_is_interlevel_teleporting_flag | PLAYER_PERSISTANT_FLAGS );
    player.monster_index= monster_index;
    player.object_index= monster->object_index;

	/* initialize_player_physics_variables sets all of these */
    player.facing= player.elevation= 0;
    player.location.x= player.location.y= player.location.z= 0;
    player.camera_location.x= player.camera_location.y= player.camera_location.z= 0;

	/* We don't change... */
	/* physics_model, suit_energy, suit_oxygen, current_weapon, desired_weapon */
	/* None of the weapons array data... */
	/* None of the items array data.. */
	/* The inventory offset/dirty flags.. */
	// ZZZ: netdead...
	mark_player_inventory_screen_as_dirty(player.player_index, _weapon);

	/* Nuke the physics */
	obj_clear(player.variables);

	/* Reset the player weapon data and the physics variable.. (after updating player_count) */
	initialize_player_physics_variables(player.player_index);
	set_player_shapes(player.player_index, false);

    player.control_panel_side_index = NONE; // not using a control panel.
	initialize_player_terminal_info(player.player_index);

	try_and_strip_player_items(player.player_index);

	if(player_teleported_dead)
	{
		kill_player(player.player_index, NONE, _monster_is_dying_soft);
	}
	
	// LP addition: handles the current player's chase cam;
	// in screen.c, we find that it's the current player whose view gets rendered
	if (player.player_index == current_player_index) ChaseCam_Reset();
	
	// Done here so that players' missiles will always be guided
	// if they are intended to be guided
	adjust_player_physics(get_monster_data(player.monster_index));
	
	// Marathon 1 won't activate monsters immediately at level start
	if (static_world.environment_flags & _environment_activation_ranges)
		monster->ticks_since_last_activation = dynamic_world.tick_count;
}


static void kill_player(short player_index, short aggressor_player_index, short action)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct object_data *legs= get_object_data(monster->object_index);
	struct object_data *torso= get_object_data(legs->parasitic_object);

	/* discharge any of our weapons which were holding charges */
	discharge_charged_weapons(player_index);
	initialize_player_weapons(player_index);

	/* make our legs ownerless scenery, mark our monster as dying, stuff in the right dying shape */
	SET_OBJECT_OWNER(legs, _object_is_normal);
	monster->action= action;
	monster_died(player->monster_index);
	set_player_dead_shape(player_index, true);
	
	/* make our torso invisible */
	SET_OBJECT_INVISIBILITY(torso, true);

	/* make our player dead */
	SET_PLAYER_DEAD_STATUS(player, true);

	player->reincarnation_delay= MINIMUM_REINCARNATION_DELAY;
	if (GET_GAME_OPTIONS()&_dying_is_penalized) player->reincarnation_delay+= NORMAL_REINCARNATION_DELAY;
	if (aggressor_player_index==player_index && (GET_GAME_OPTIONS()&_suicide_is_penalized)) player->reincarnation_delay+= SUICIDE_REINCARNATION_DELAY;

	kill_player_physics_variables(player_index);

	player->ticks_at_death = dynamic_world.tick_count;
}

static void give_player_initial_items(
	short player_index)
{
	Player* player= get_player_data(player_index);

	for(unsigned loop= 0; loop<NUMBER_OF_PLAYER_INITIAL_ITEMS; ++loop)
	{
		/* Get the item.. */
		assert_fail(player_initial_items[loop]>=0 && player_initial_items[loop]<NUMBER_OF_ITEMS, "");

		if(player->items[player_initial_items[loop]]==NONE)
		{
			player->items[player_initial_items[loop]]= 1;
		} else {
			player->items[player_initial_items[loop]]+= 1;
		}
		
		process_new_item_for_reloading(player_index, player_initial_items[loop]);
	}
}

static void remove_dead_player_items(
	short player_index)
{
	Player* player= get_player_data(player_index);
	short item_type;

	// subtract all initial items	
	for (unsigned i= 0; i<NUMBER_OF_PLAYER_INITIAL_ITEMS; ++i)
	{
		if (player->items[player_initial_items[i]]>0)
		{
			player->items[player_initial_items[i]]-= 1;
		}
	}

	// drop any balls
	// START Benad: dec. 3
	{
		short ball_color= find_player_ball_color(player_index);
		
		if (ball_color!=NONE)
		{
			struct polygon_data *polygon= get_polygon_data(player->supporting_polygon_index);
			
			if ( ((GET_GAME_TYPE()==_game_of_rugby) || (GET_GAME_TYPE()==_game_of_capture_the_flag))
				&& (( (dynamic_world.game_information.kill_limit == 819) && (polygon->type==_polygon_is_hill) ) ||
					( polygon->type==_polygon_is_base && polygon->permutation != player->team ) ) )
			{
				player->items[BALL_ITEM_BASE + ball_color]= NONE;
				dynamic_world.current_item_count[BALL_ITEM_BASE + ball_color]--;
			}
			else
			{
				short item_type= BALL_ITEM_BASE + ball_color;
				
				// Benad: using location and supporting_polygon_index instead of
				// camera_location and camera_polygon_index... D'uh...
				drop_the_ball(&player->location, player->supporting_polygon_index, 
					player->monster_index, _monster_marine, item_type);
				player->items[item_type]= NONE;
			}
		}
	}
	// END Benad: dec. 3
	
	for (item_type= 0; item_type<NUMBER_OF_ITEMS; ++item_type)
	{
		short item_count= player->items[item_type];
		// START Benad
		if ((item_type >= BALL_ITEM_BASE) && (item_type < BALL_ITEM_BASE+MAXIMUM_NUMBER_OF_PLAYERS))
			continue;
		// END Benad
		while ((item_count-= 1)>=0)
		{
			short item_kind= get_item_kind(item_type);
			bool dropped= false;
			
			// if we’re not set to burn items or this is an important item (i.e., repair chip) drop it
			if (!(GET_GAME_OPTIONS()&_burn_items_on_death) ||
				(item_kind==_item && get_number_of_players()>1))
			{
				if (item_kind!=_ammunition || !(global_random()&1))
				{
					world_point3d random_point;
					short random_polygon_index;
					short retries= 5;
					
					do
					{
						random_point_on_circle(&player->location, player->supporting_polygon_index, WORLD_ONE_FOURTH, &random_point, &random_polygon_index);
					}
					while (random_polygon_index==NONE && --retries);
					
					if (random_polygon_index!=NONE)
					{
						struct object_location location;

						location.polygon_index= random_polygon_index;
                        location.p.x= random_point.x;
                        location.p.y= random_point.y;
                        location.p.z= 0;
						location.yaw= 0;
						location.flags= 0;
						new_item(&location, item_type);
						
						dropped= true;
					}
				}
			}
			
			if (film_profile.count_dead_dropped_items_correctly || !dropped) object_was_just_destroyed(_object_is_item, item_type);
		}

		player->items[item_type]= NONE;
	}

	mark_player_inventory_as_dirty(player_index, NONE);
}

static void get_player_transfer_mode(
	short player_index,
	short *transfer_mode,
	short *transfer_period)
{
	Player* player= get_player_data(player_index);
	short duration= 0;

	*transfer_period= 1;
	*transfer_mode= NONE;
	if (PLAYER_IS_TELEPORTING(player))
	{
		if (player->teleporting_destination >= 0 || (player->teleporting_phase < PLAYER_TELEPORTING_MIDPOINT && exiting_level_uses_teleport_effect()) || (player->teleporting_phase >= PLAYER_TELEPORTING_MIDPOINT && entering_level_uses_teleport_effect()))
		{
			*transfer_mode= player->teleporting_phase<PLAYER_TELEPORTING_MIDPOINT ? _xfer_fold_out : _xfer_fold_in;
			*transfer_period= PLAYER_TELEPORTING_MIDPOINT+1;
		}
        // TODO: if not using teleporting effect, use fade in?
	}
	else if (PLAYER_IS_INTERLEVEL_TELEPORTING(player))
	{
		if (player->teleporting_destination >= 0 || (player->interlevel_teleport_phase < PLAYER_TELEPORTING_MIDPOINT && exiting_level_uses_teleport_effect()) || (player->interlevel_teleport_phase >= PLAYER_TELEPORTING_MIDPOINT && entering_level_uses_teleport_effect()))
		{
			*transfer_mode= player->interlevel_teleport_phase <PLAYER_TELEPORTING_MIDPOINT ? _xfer_fold_out : _xfer_fold_in;
			*transfer_period= PLAYER_TELEPORTING_MIDPOINT+1;
		}
        // TODO: if not using teleporting effect, use fade out?
	}
	else
	{
		if (player->invincibility_duration) 
		{
			*transfer_mode= _xfer_static;
			duration= player->invincibility_duration;
		}
		else
		{
			if (player->invisibility_duration) 
			{
				*transfer_mode= player->invisibility_duration>kINVISIBILITY_DURATION ? _xfer_subtle_invisibility : _xfer_invisibility;
				duration= player->invisibility_duration;
			}
		}
		
		if (duration && duration<10*TICKS_PER_SECOND)
		{
			switch (duration/(TICKS_PER_SECOND/6))
			{
				case 46: case 37: case 29: case 22: case 16: case 11: case 7: case 4: case 2:
					if(player->invincibility_duration && player->invisibility_duration)
						*transfer_mode= player->invisibility_duration>kINVISIBILITY_DURATION ? _xfer_subtle_invisibility : _xfer_invisibility;
					else
						*transfer_mode= NONE;
					break;
			}
		}
	}
}

static void set_player_dead_shape(
	short player_index,
	bool dying)
{
	Player* player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	short shape;
	
	if (monster->action==_monster_is_dying_flaming)
	{
		shape= dying ? FLAMING_DYING_SHAPE : FLAMING_DEAD_SHAPE;
	}
	else
	{
		if (dying)
		{
			shape= (monster->action==_monster_is_dying_hard) ? player_shapes.dying_hard : player_shapes.dying_soft;
		}
		else
		{
			shape= (monster->action==_monster_is_dying_hard) ? player_shapes.dead_hard : player_shapes.dead_soft;
		}
	
		shape= BUILD_DESCRIPTOR(BUILD_COLLECTION(player_shapes.collection, player->team), shape);
	}

	if (dying)
	{
		set_object_shape_and_transfer_mode(monster->object_index, shape, NONE);
	}
	else
	{
		randomize_object_sequence(monster->object_index, shape);
	}
}

static short calculate_player_team(
	short base_team)
{
	short team = NONE;

	/* Starting locations are based on the team type. */
	switch(GET_GAME_TYPE())
	{
		case _game_of_kill_monsters:
		case _game_of_cooperative_play:
		case _game_of_tag:
		case _game_of_king_of_the_hill:
		case _game_of_kill_man_with_ball:
			team= NONE;
			break;

	case _game_of_custom:
		case _game_of_defense:
		case _game_of_rugby:						
		case _game_of_capture_the_flag:
			// START Benad
			if ((dynamic_world.game_information.kill_limit == 819) ||
				((GET_GAME_TYPE() == _game_of_defense) &&
				(dynamic_world.game_information.kill_limit == 1080))) // 1080 seconds, or 18:00...
				team= NONE;
			else
				team= base_team;
				// assert_fail_f(false, "Kill limit: %d", dynamic_world.game_information.kill_limit);
			// END Benad
			break;
	}
	
	return team;
}	

// #define STRIPPED_ENERGY (PLAYER_MAXIMUM_SUIT_ENERGY/4)
static void try_and_strip_player_items(
	short player_index)
{
	Player* player= get_player_data(player_index);
	short item_type;
	
	if (static_world.environment_flags&_environment_rebellion)
	{
		for (item_type= 0; item_type<NUMBER_OF_ITEMS; ++item_type)
		{
			switch (item_type)
			{
				case _i_knife:
					break;
				
				default:
					player->items[item_type]= NONE;
					break;
			}
		}
	
		mark_player_inventory_as_dirty(player_index, NONE);
		initialize_player_weapons(player_index);
		
		// LP change: using variable for this
		if (player->suit_energy > player_settings.StrippedEnergy)
			player->suit_energy = player_settings.StrippedEnergy;
	}
}


static void adjust_player_physics(monster_data *me)
{	
	// LP: Fix the player physics so that guided missiles will work correctly
	if (player_settings.PlayerShotsGuided)
	{
		struct monster_definition *vacbob = get_monster_definition_external(_civilian_fusion_security);
		// AlexJLS patch: make this player active, so guided weapons can work
		SET_MONSTER_ACTIVE_STATUS(me,true);
		
		// Gets called once for every player character created or re-created;
		// that seems to be OK
		SetPlayerViewAttribs(vacbob->half_visual_arc,vacbob->half_vertical_visual_arc / 3,
			vacbob->visual_range * 2,
			vacbob->dark_visual_range * 2);
	}
}


static void StreamToPhysVars(uint8* &S, physics_variables& Object)
{
		StreamToValue(S,Object.head_direction);
		StreamToValue(S,Object.last_direction);
		StreamToValue(S,Object.direction);
		StreamToValue(S,Object.elevation);
		StreamToValue(S,Object.angular_velocity);
		StreamToValue(S,Object.vertical_angular_velocity);	
		StreamToValue(S,Object.velocity);
		StreamToValue(S,Object.perpendicular_velocity);
		StreamToValue(S,Object.last_position.x);
		StreamToValue(S,Object.last_position.y);
		StreamToValue(S,Object.last_position.z);
		StreamToValue(S,Object.position.x);
		StreamToValue(S,Object.position.y);
		StreamToValue(S,Object.position.z);
		StreamToValue(S,Object.actual_height);
		
		StreamToValue(S,Object.adjusted_pitch);
		StreamToValue(S,Object.adjusted_yaw);

		StreamToValue(S,Object.external_velocity.i);
		StreamToValue(S,Object.external_velocity.j);
		StreamToValue(S,Object.external_velocity.k);
		StreamToValue(S,Object.external_angular_velocity);
		
		StreamToValue(S,Object.step_phase);
		StreamToValue(S,Object.step_amplitude);	
		
		StreamToValue(S,Object.floor_height);
		StreamToValue(S,Object.ceiling_height);	
		StreamToValue(S,Object.media_height);
		
		StreamToValue(S,Object.action);
		StreamToValue(S,Object.old_flags);	
		StreamToValue(S,Object.flags);
}

static void PhysVarsToStream(uint8* &S, physics_variables& Object)
{
		ValueToStream(S,Object.head_direction);
		ValueToStream(S,Object.last_direction);
		ValueToStream(S,Object.direction);
		ValueToStream(S,Object.elevation);
		ValueToStream(S,Object.angular_velocity);
		ValueToStream(S,Object.vertical_angular_velocity);	
		ValueToStream(S,Object.velocity);
		ValueToStream(S,Object.perpendicular_velocity);
		ValueToStream(S,Object.last_position.x);
		ValueToStream(S,Object.last_position.y);
		ValueToStream(S,Object.last_position.z);
		ValueToStream(S,Object.position.x);
		ValueToStream(S,Object.position.y);
		ValueToStream(S,Object.position.z);
		ValueToStream(S,Object.actual_height);
		
		ValueToStream(S,Object.adjusted_pitch);
		ValueToStream(S,Object.adjusted_yaw);

		ValueToStream(S,Object.external_velocity.i);
		ValueToStream(S,Object.external_velocity.j);
		ValueToStream(S,Object.external_velocity.k);
		ValueToStream(S,Object.external_angular_velocity);
		
		ValueToStream(S,Object.step_phase);
		ValueToStream(S,Object.step_amplitude);	
		
		ValueToStream(S,Object.floor_height);
		ValueToStream(S,Object.ceiling_height);	
		ValueToStream(S,Object.media_height);
		
		ValueToStream(S,Object.action);
		ValueToStream(S,Object.old_flags);	
		ValueToStream(S,Object.flags);
}


inline void StreamToDamRec(uint8* &S, damage_record& Object)
{
		StreamToValue(S,Object.damage);
		StreamToValue(S,Object.kills);
}

static void DamRecToStream(uint8* &S, damage_record& Object)
{
		ValueToStream(S,Object.damage);
		ValueToStream(S,Object.kills);
}


uint8 *unpack_player_data(uint8 *Stream, size_t Count)
{
	uint8* S = Stream;

    clear_players();

	for (size_t k = 0; k < Count; k++)
	{
        Player* ObjPtr = &players.emplace_back();
        
		StreamToValue(S,ObjPtr->identifier);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->color);
		StreamToValue(S,ObjPtr->team);
        char tmp[MAXIMUM_PLAYER_NAME_LENGTH];
		StreamToBytes(S,tmp, sizeof(tmp));
        ObjPtr->name = convert_macroman_cstr_to_utf8_string(tmp);
        S += 2;
		StreamToValue(S,ObjPtr->location.x);
		StreamToValue(S,ObjPtr->location.y);
		StreamToValue(S,ObjPtr->location.z);
		StreamToValue(S,ObjPtr->camera_location.x);
		StreamToValue(S,ObjPtr->camera_location.y);
		StreamToValue(S,ObjPtr->camera_location.z);	
		StreamToValue(S,ObjPtr->camera_polygon_index);
		StreamToValue(S,ObjPtr->facing);
		StreamToValue(S,ObjPtr->elevation);
		StreamToValue(S,ObjPtr->supporting_polygon_index);
		StreamToValue(S,ObjPtr->last_supporting_polygon_index);
		
		StreamToValue(S,ObjPtr->suit_energy);
		StreamToValue(S,ObjPtr->suit_oxygen);
		
		StreamToValue(S,ObjPtr->monster_index);
		StreamToValue(S,ObjPtr->object_index);
		
		StreamToValue(S,ObjPtr->weapon_intensity_decay);
		StreamToValue(S,ObjPtr->weapon_intensity);
		
		StreamToValue(S,ObjPtr->invisibility_duration);
		StreamToValue(S,ObjPtr->invincibility_duration);
		StreamToValue(S,ObjPtr->infravision_duration);	
		StreamToValue(S,ObjPtr->extravision_duration);
		
		StreamToValue(S,ObjPtr->delay_before_teleport);
		StreamToValue(S,ObjPtr->teleporting_phase);
		StreamToValue(S,ObjPtr->teleporting_destination);
		StreamToValue(S,ObjPtr->interlevel_teleport_phase);
		
		StreamToList(S,ObjPtr->items,NUMBER_OF_ITEMS);
		
		StreamToValue(S,ObjPtr->hud_flags);
		StreamToValue(S,ObjPtr->hud_decay);
		
		StreamToPhysVars(S,ObjPtr->variables);
		
		StreamToDamRec(S,ObjPtr->total_damage_given);
		for (int k=0; k<MAXIMUM_NUMBER_OF_PLAYERS; k++)
			StreamToDamRec(S,ObjPtr->damage_taken[k]);
		StreamToDamRec(S,ObjPtr->monster_damage_taken);
		StreamToDamRec(S,ObjPtr->monster_damage_given);
		
		StreamToValue(S,ObjPtr->reincarnation_delay);
		
		StreamToValue(S,ObjPtr->control_panel_side_index);
		
		StreamToValue(S,ObjPtr->ticks_at_last_successful_save);
		
		StreamToList(S,ObjPtr->netgame_parameters,2);
		
		S += 256*2;

		ObjPtr->hotkey_sequence = 0;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_data), "");
	return S;
}
uint8 *pack_player_data(uint8 *Stream, Player *Objects, size_t Count)
{
	uint8* S = Stream;
	Player* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->identifier);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->color);
		ValueToStream(S,ObjPtr->team);
        char tmp[MAXIMUM_PLAYER_NAME_LENGTH];
        convert_utf8_string_to_macroman_cstr(ObjPtr->name, tmp, sizeof(tmp));
		BytesToStream(S,tmp, sizeof(tmp));
        S += 2;
		ValueToStream(S,ObjPtr->location.x);
		ValueToStream(S,ObjPtr->location.y);
		ValueToStream(S,ObjPtr->location.z);
		ValueToStream(S,ObjPtr->camera_location.x);
		ValueToStream(S,ObjPtr->camera_location.y);
		ValueToStream(S,ObjPtr->camera_location.z);	
		ValueToStream(S,ObjPtr->camera_polygon_index);
		ValueToStream(S,ObjPtr->facing);
		ValueToStream(S,ObjPtr->elevation);
		ValueToStream(S,ObjPtr->supporting_polygon_index);
		ValueToStream(S,ObjPtr->last_supporting_polygon_index);
		
		ValueToStream(S,ObjPtr->suit_energy);
		ValueToStream(S,ObjPtr->suit_oxygen);
		
		ValueToStream(S,ObjPtr->monster_index);
		ValueToStream(S,ObjPtr->object_index);
		
		ValueToStream(S,ObjPtr->weapon_intensity_decay);
		ValueToStream(S,ObjPtr->weapon_intensity);
		
		ValueToStream(S,ObjPtr->invisibility_duration);
		ValueToStream(S,ObjPtr->invincibility_duration);
		ValueToStream(S,ObjPtr->infravision_duration);	
		ValueToStream(S,ObjPtr->extravision_duration);
		
		ValueToStream(S,ObjPtr->delay_before_teleport);
		ValueToStream(S,ObjPtr->teleporting_phase);
		ValueToStream(S,ObjPtr->teleporting_destination);
		ValueToStream(S,ObjPtr->interlevel_teleport_phase);
		
		ListToStream(S,ObjPtr->items,NUMBER_OF_ITEMS);
		
		ValueToStream(S,ObjPtr->hud_flags);
		ValueToStream(S,ObjPtr->hud_decay);
		
		PhysVarsToStream(S,ObjPtr->variables);
		
		DamRecToStream(S,ObjPtr->total_damage_given);
		for (int k=0; k<MAXIMUM_NUMBER_OF_PLAYERS; k++)
			DamRecToStream(S,ObjPtr->damage_taken[k]);
		DamRecToStream(S,ObjPtr->monster_damage_taken);
		DamRecToStream(S,ObjPtr->monster_damage_given);
		
		ValueToStream(S,ObjPtr->reincarnation_delay);
		
		ValueToStream(S,ObjPtr->control_panel_side_index);
		
		ValueToStream(S,ObjPtr->ticks_at_last_successful_save);
		
		ListToStream(S,ObjPtr->netgame_parameters,2);
		
		S += 256*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_player_data), "");
	return S;
}

short *original_player_initial_items = NULL;
struct damage_response_definition *original_damage_response_definitions = NULL;
struct player_powerup_durations_definition *original_player_powerup_durations = NULL;
struct player_powerup_definition *original_player_powerups = NULL;
struct player_shape_definitions *original_player_shapes = NULL;
struct player_settings_definition *original_player_settings = NULL;

void reset_mml_player()
{
	if (original_player_settings) {
		player_settings = *original_player_settings;
		free(original_player_settings);
		original_player_settings = NULL;
	}
	if (original_player_initial_items) {
		for (unsigned i = 0; i < NUMBER_OF_PLAYER_INITIAL_ITEMS; i++)
			player_initial_items[i] = original_player_initial_items[i];
		free(original_player_initial_items);
		original_player_initial_items = NULL;
	}
	if (original_damage_response_definitions) {
		for (unsigned i = 0; i < NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS; i++)
			damage_response_definitions[i] = original_damage_response_definitions[i];
		free(original_damage_response_definitions);
		original_damage_response_definitions = NULL;
	}
	if (original_player_powerup_durations) {
		player_powerup_durations = *original_player_powerup_durations;
		free(original_player_powerup_durations);
		original_player_powerup_durations = NULL;
	}
	if (original_player_powerups) {
		player_powerups = *original_player_powerups;
		free(original_player_powerups);
		original_player_powerups = NULL;
	}
	if (original_player_shapes) {
		player_shapes = *original_player_shapes;
		free(original_player_shapes);
		original_player_shapes = NULL;
	}
}

void parse_mml_player(const InfoTree& root)
{
	// back up old values first
	if (!original_player_settings) {
		original_player_settings = (struct player_settings_definition *) malloc(sizeof(struct player_settings_definition));
        assert_fail(original_player_settings, "");
		*original_player_settings = player_settings;
	}
	if (!original_player_initial_items) {
		original_player_initial_items = (short *) malloc(sizeof(short) * NUMBER_OF_PLAYER_INITIAL_ITEMS);
		assert_fail(original_player_initial_items, "");
		for (unsigned i = 0; i < NUMBER_OF_PLAYER_INITIAL_ITEMS; i++)
			original_player_initial_items[i] = player_initial_items[i];
	}
	if (!original_damage_response_definitions) {
		original_damage_response_definitions = (struct damage_response_definition *) malloc(sizeof(struct damage_response_definition) * NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS);
		assert_fail(original_damage_response_definitions, "");
		for (unsigned i = 0; i < NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS; i++)
			original_damage_response_definitions[i] = damage_response_definitions[i];
	}
	if (!original_player_powerup_durations) {
		original_player_powerup_durations = (struct player_powerup_durations_definition *) malloc(sizeof(struct player_powerup_durations_definition));
		assert_fail(original_player_powerup_durations, "");
		*original_player_powerup_durations = player_powerup_durations;
	}
	if (!original_player_powerups) {
		original_player_powerups = (struct player_powerup_definition *) malloc(sizeof(struct player_powerup_definition));
		assert_fail(original_player_powerups, "");
		*original_player_powerups = player_powerups;
	}
	if (!original_player_shapes) {
		original_player_shapes = (struct player_shape_definitions *) malloc(sizeof(struct player_shape_definitions));
        assert_fail(original_player_powerups, "");
		*original_player_shapes = player_shapes;
	}
	
	root.read_attr_bounded<int16>("energy", player_settings.InitialEnergy, 0, SHRT_MAX);
	root.read_attr_bounded<int16>("oxygen", player_settings.InitialOxygen, 0, SHRT_MAX);
	root.read_attr_bounded<int16>("stripped", player_settings.StrippedEnergy, 0, SHRT_MAX);
	root.read_fixed("light", player_settings.PlayerSelfLuminosity, 0, SHRT_MAX);
	root.read_attr("oxygen_deplete", player_settings.OxygenDepletion);
	root.read_attr("oxygen_replenish", player_settings.OxygenReplenishment);
	root.read_indexed("vulnerability", player_settings.Vulnerability, NUMBER_OF_DAMAGE_TYPES, true);
	root.read_attr("guided", player_settings.PlayerShotsGuided);
	root.read_attr("half_visual_arc", player_settings.PlayerHalfVisualArc);
	root.read_attr("half_vertical_visual_arc", player_settings.PlayerHalfVertVisualArc);
	root.read_attr("visual_range", player_settings.PlayerVisualRange);
	root.read_attr("dark_visual_range", player_settings.PlayerDarkVisualRange);
	root.read_attr("single_energy", player_settings.SingleEnergy);
	root.read_attr("double_energy", player_settings.DoubleEnergy);
	root.read_attr("triple_energy", player_settings.TripleEnergy);
	root.read_attr("can_swim", player_settings.CanSwim);
	
	for (const InfoTree &item : root.children_named("item"))
	{
		int16 index;
		if (!item.read_indexed("index", index, NUMBER_OF_PLAYER_INITIAL_ITEMS))
			continue;
		item.read_indexed("type", player_initial_items[index], NUMBER_OF_ITEM_TYPES);
	}
	
	for (const InfoTree &dmg : root.children_named("damage"))
	{
		int16 index;
		if (!dmg.read_indexed("index", index, NUMBER_OF_DAMAGE_RESPONSE_DEFINITIONS))
			continue;
		damage_response_definition& def = damage_response_definitions[index];
		
		dmg.read_attr("threshold", def.damage_threshhold);
		dmg.read_attr("fade", def.fade);
		dmg.read_attr("sound", def.sound);
		dmg.read_attr("death_sound", def.death_sound);
		dmg.read_attr("death_action", def.death_action);
	}
	
	for (const InfoTree &assign : root.children_named("powerup_assign"))
	{
		assign.read_indexed("invincibility", player_powerups.Powerup_Invincibility, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("invisibility", player_powerups.Powerup_Invisibility, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("infravision", player_powerups.Powerup_Infravision, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("extravision", player_powerups.Powerup_Extravision, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("triple_energy", player_powerups.Powerup_TripleEnergy, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("double_energy", player_powerups.Powerup_DoubleEnergy, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("energy", player_powerups.Powerup_Energy, NUMBER_OF_ITEM_TYPES, true);
		assign.read_indexed("oxygen", player_powerups.Powerup_Oxygen, NUMBER_OF_ITEM_TYPES, true);
	}
	
	for (const InfoTree &powerup : root.children_named("powerup"))
	{
		powerup.read_attr_bounded<int16>("invincibility", kINVINCIBILITY_DURATION, 0, SHRT_MAX);
		powerup.read_attr_bounded<int16>("invisibility", kINVISIBILITY_DURATION, 0, SHRT_MAX);
		powerup.read_attr_bounded<int16>("infravision", kINFRAVISION_DURATION, 0, SHRT_MAX);
		powerup.read_attr_bounded<int16>("extravision", kEXTRAVISION_DURATION, 0, SHRT_MAX);
	}
	
	for (const InfoTree &shp : root.children_named("shape"))
	{
		int16 type;
		if (!shp.read_indexed("type", type, 5))
			continue;
		
		if (type == 0)
		{
			int16 subtype;
			if (!shp.read_indexed("subtype", subtype, 5))
				continue;
			switch (subtype)
			{
				case 0:
					shp.read_indexed("value", player_shapes.collection, MAXIMUM_COLLECTIONS);
					break;
				case 1:
					shp.read_indexed("value", player_shapes.dying_hard, MAXIMUM_SHAPES_PER_COLLECTION);
					break;
				case 2:
					shp.read_indexed("value", player_shapes.dying_soft, MAXIMUM_SHAPES_PER_COLLECTION);
					break;
				case 3:
					shp.read_indexed("value", player_shapes.dead_hard, MAXIMUM_SHAPES_PER_COLLECTION);
					break;
				case 4:
					shp.read_indexed("value", player_shapes.dead_soft, MAXIMUM_SHAPES_PER_COLLECTION);
					break;
			}
		}
		else if (type == 1)
		{
			int16 subtype;
			if (!shp.read_indexed("subtype", subtype, NUMBER_OF_PLAYER_ACTIONS))
				continue;
			shp.read_indexed("value", player_shapes.legs[subtype], MAXIMUM_SHAPES_PER_COLLECTION);
		}
		else if (type == 2)
		{
			int16 subtype;
			if (!shp.read_indexed("subtype", subtype, PLAYER_TORSO_SHAPE_COUNT))
				continue;
			shp.read_indexed("value", player_shapes.torsos[subtype], MAXIMUM_SHAPES_PER_COLLECTION);
		}
		else if (type == 3)
		{
			int16 subtype;
			if (!shp.read_indexed("subtype", subtype, PLAYER_TORSO_SHAPE_COUNT))
				continue;
			shp.read_indexed("value", player_shapes.charging_torsos[subtype], MAXIMUM_SHAPES_PER_COLLECTION);
		}
		else if (type == 4)
		{
			int16 subtype;
			if (!shp.read_indexed("subtype", subtype, PLAYER_TORSO_SHAPE_COUNT))
				continue;
			shp.read_indexed("value", player_shapes.firing_torsos[subtype], MAXIMUM_SHAPES_PER_COLLECTION);
		}
	}
}

