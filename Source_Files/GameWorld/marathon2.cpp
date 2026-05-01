/*
MARATHON.C

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
#include "render.h"
#include "interface.h"
#include "compatibility_profiles.h"
#include "flood_map.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "network.h"
#include "scenery.h"
#include "platforms.h"
#include "lightsource.h"
#include "media.h"
#include "Music.h"
#include "fades.h"
#include "items.h"
#include "weapons.h"
#include "game_window.h"
#include "SoundManager.h"
#include "network_games.h"
#include "vbl.h" // sync_heartbeat_count
#include "tags.h"
#include "AnimatedTextures.h"
#include "ChaseCam.h"
#include "OGL_Setup.h"
#include "OGL_Render.h" // start_modern_renderer, stop_modern_renderer

#include "lua_script.h"
#include "lua_hud_script.h"

#include "Rasterizer_SW.h" // allocate_sw_texture_tables

#include "Screen.hpp"
#include "ActionQueues.h"

//#include "Screen.hpp"
#include "screen_overlay.h" // reset_messages
//#include "shell.h"

#include "Console.h"
#include "FilmExporter.h"
#include "Statistics.h"

#include "motion_sensor.hpp"

#include "preferences.h" // player_preferences (may go away again, depending where crosshairs are enabled)

#include "ephemera.h"
#include "interpolated_world.h"

#include "Plugins.h"
#include "SoundsPatch.h"

#include "setup_game.hpp"



// This is an intermediate action-flags queue for transferring action flags
// from whichever source to the engine's event handling
// ghs: making this externally available for Lua's trigger modifications
static ModifiableActionQueues* GameQueue = NULL;

ModifiableActionQueues* GetGameQueue() { return GameQueue; }

// ZZZ: We keep this around for use in prediction (we assume a player keeps on doin' what he's been doin')
static uint32 sMostRecentFlagsForPlayer[MAXIMUM_NUMBER_OF_PLAYERS];



static void load_all_game_sounds(short environment_code);



void initialize_marathon()
{
	build_trig_tables();
	// Rendering and flood-map done when starting a level, since they require map-geometry sizes
	allocate_pathfinding_memory();
	// allocate_flood_map_memory(); // now called in initialize_level_from_wad_data
    // allocate_render_memory();
	allocate_sw_texture_tables();
	initialize_weapon_manager();
	initialize_game_window();
	initialize_scenery();
	initialize_items();
	OGL_Initialize();
	GameQueue = new ModifiableActionQueues(MAXIMUM_NUMBER_OF_PLAYERS, ACTION_QUEUE_BUFFER_DIAMETER, true);
}


static size_t sPredictedTicks = 0;

void reset_intermediate_action_queues()
{
	GameQueue->reset();

	// ZZZ: I don't know that this is strictly the best place (or the best function name)
	// to do this stuff, but it works, anyway.
	for(size_t i = 0; i < MAXIMUM_NUMBER_OF_PLAYERS; i++)
		sMostRecentFlagsForPlayer[i] = 0;

	sPredictedTicks = 0;
}


// ZZZ: For prediction...
static bool sPredictionWanted= false;

void set_prediction_wanted(bool inPrediction)
{
	sPredictionWanted= inPrediction;
}

static Player sSavedPlayerData[MAXIMUM_NUMBER_OF_PLAYERS];
static monster_data sSavedPlayerMonsterData[MAXIMUM_NUMBER_OF_PLAYERS];
static object_data sSavedPlayerObjectData[MAXIMUM_NUMBER_OF_PLAYERS];
static object_data sSavedPlayerParasiticObjectData[MAXIMUM_NUMBER_OF_PLAYERS];
static short sSavedPlayerObjectNextObject[MAXIMUM_NUMBER_OF_PLAYERS];

// For sanity-checking...
static int32 sSavedTickCount;
static uint16 sSavedRandomSeed;


// ZZZ: If not already in predictive mode, save off partial game-state for later restoration.
static void enter_predictive_mode()
{
	if(sPredictedTicks == 0)
	{
		for(short i = 0; i < get_number_of_players(); i++)
		{
			sSavedPlayerData[i] = *get_player_data(i);
			if(sSavedPlayerData[i].monster_index != NONE)
			{
				sSavedPlayerMonsterData[i] = *get_monster_data(sSavedPlayerData[i].monster_index);
				if(sSavedPlayerMonsterData[i].object_index != NONE)
				{
					sSavedPlayerObjectData[i] = *get_object_data(sSavedPlayerMonsterData[i].object_index);
					sSavedPlayerObjectNextObject[i] = sSavedPlayerObjectData[i].next_object;
					if(sSavedPlayerObjectData[i].parasitic_object != NONE)
						sSavedPlayerParasiticObjectData[i] = *get_object_data(sSavedPlayerObjectData[i].parasitic_object);
				}
			}
		}
		
		// Sanity checking
		sSavedTickCount = dynamic_world.tick_count;
		sSavedRandomSeed = get_random_seed();
	}
}


#if COMPARE_MEMORY
// ZZZ: I wrote this function to help catch incomplete state save/restore operations on entering and exiting predictive mode
// It's not currently in use anywhere, but may prove useful sometime?  so I'm including it in my submission.
static void compare_memory(const char* inChunk1, const char* inChunk2, size_t inSize, size_t inIgnoreStart, size_t inIgnoreEnd, const char* inDescription, int inDescriptionNumber)
{
	bool trackingDifferences = false;
	size_t theDifferenceStart;
	
	for(size_t i = 0; i < inSize; i++)
	{
		if(inChunk1[i] != inChunk2[i])
		{
			if(!trackingDifferences)
			{
				theDifferenceStart = i;
				trackingDifferences = true;
			}
		}
		else
		{
			if(trackingDifferences)
			{
				if(theDifferenceStart < inIgnoreStart || i >= inIgnoreEnd)
                    log_warning_f("%s %d: differences in bytes [%d,%d)", inDescription, inDescriptionNumber, theDifferenceStart, i);
				trackingDifferences = false;
			}
		}
	}

	if(trackingDifferences)
	{
		if(theDifferenceStart < inIgnoreStart || inSize >= inIgnoreEnd)
            log_warning_f("%s %d: differences in bytes [%d,%d)", inDescription, inDescriptionNumber, theDifferenceStart, inSize);
	}
}
#endif

// ZZZ: if in predictive mode, restore the saved partial game-state (it'd better take us back
// to _exactly_ the same full game-state we saved earlier, else problems.)
static void exit_predictive_mode()
{
	if(sPredictedTicks > 0)
	{
		for(short i = 0; i < get_number_of_players(); i++)
		{
			Player* player = get_player_data(i);
			
			assert_fail(player->monster_index == sSavedPlayerData[i].monster_index, "");

			{
				// We *don't* restore this tiny part of the game-state back because
				// otherwise the player can't use [] to scroll the inventory panel.
				// [] scrolling happens outside the normal input/update system, so that's
				// enough to persuade me that not restoring this won't OOS any more often
				// than []-scrolling did before prediction.  :)
				int16 saved_interface_flags = player->hud_flags;
				int16 saved_interface_decay = player->hud_decay;
				
				*player = sSavedPlayerData[i];

				player->hud_flags = saved_interface_flags;
				player->hud_decay = saved_interface_decay;
			}

			if(sSavedPlayerData[i].monster_index != NONE)
			{
				assert_fail(get_monster_data(sSavedPlayerData[i].monster_index)->object_index == sSavedPlayerMonsterData[i].object_index, "");

				*get_monster_data(sSavedPlayerData[i].monster_index) = sSavedPlayerMonsterData[i];
				
				if(sSavedPlayerMonsterData[i].object_index != NONE)
				{
					assert_fail(get_object_data(sSavedPlayerMonsterData[i].object_index)->parasitic_object == sSavedPlayerObjectData[i].parasitic_object, "");

					remove_object_from_polygon_object_list(sSavedPlayerMonsterData[i].object_index);
					
					*get_object_data(sSavedPlayerMonsterData[i].object_index) = sSavedPlayerObjectData[i];

					// We have to defer this insertion since the object lists could still have other players
					// in their predictive locations etc. - we need to reconstruct everything exactly as it
					// was when we entered predictive mode.
					deferred_add_object_to_polygon_object_list(sSavedPlayerMonsterData[i].object_index, sSavedPlayerObjectNextObject[i]);
					
					if(sSavedPlayerObjectData[i].parasitic_object != NONE)
						*get_object_data(sSavedPlayerObjectData[i].parasitic_object) = sSavedPlayerParasiticObjectData[i];
				}
			}
		}

		perform_deferred_polygon_object_list_manipulations();
		
		sPredictedTicks = 0;

		// Sanity checking
		if(sSavedTickCount != dynamic_world.tick_count)
            log_warning_f("saved tick count %d != dynamic_world.tick_count %d", sSavedTickCount, dynamic_world.tick_count);

		if(sSavedRandomSeed != get_random_seed())
            log_warning_f("saved random seed %d != get_random_seed() %d", sSavedRandomSeed, get_random_seed());
	}
}


// ZZZ: move a single tick's flags (if there's one present for each player in the Base Queues)
// from the Base Queues into the Output Queues, overriding each with the corresponding player's
// flags from the Overlay Queues, if non-empty.
static bool overlay_queue_with_queue_into_queue(ActionQueues* inBaseQueues, ActionQueues* inOverlayQueues, ActionQueues* inOutputQueues)
{
        bool haveFlagsForAllPlayers = true;
        for(int p = 0; p < get_number_of_players(); p++)
        {
                if(inBaseQueues->countActionFlags(p) <= 0)
                {
                        haveFlagsForAllPlayers = false;
                        break;
                }
        }
        
        if(!haveFlagsForAllPlayers)
        {
                return false;
        }
        
        for(int p = 0; p < get_number_of_players(); p++)
        {
                // Trust me, this is right - we dequeue from the Base Queues whether or not they get overridden.
                uint32 action_flags = inBaseQueues->dequeueActionFlags(p);
                
                if(inOverlayQueues != NULL && inOverlayQueues->countActionFlags(p) > 0)
                {
                        action_flags = inOverlayQueues->dequeueActionFlags(p);
                }
                
                inOutputQueues->enqueueActionFlags(p, &action_flags, 1);
        }
        
        return true;
}


// Return values for update_world_elements_one_tick()
enum {
        kUpdateNormalCompletion,
        kUpdateGameOver,
        kUpdateChangeLevel
};


// ZZZ: split out from update_world()'s loop.
static int update_world_elements_one_tick(bool& call_postidle)
{
	if (m1_solo_player_in_terminal()) // M1 terminals stop the world
	{
		update_m1_solo_player_in_terminal(GameQueue);
		call_postidle = false;
	} 
	else
	{
		decode_hotkeys(*GameQueue);
		L_Call_Idle();
		call_postidle = true;
		
		update_lights();
		update_medias();
		update_platforms();
		
		update_control_panels(); // don't put after update_players
		update_players(GameQueue, false);
		move_projectiles();
		move_monsters();
		update_effects();
		recreate_objects();
		
		handle_random_sound_image();
		animate_scenery();

		update_ephemera();
		
		if (film_profile.animate_items) { animate_items(); }
		
		AnimTxtr_Update();
		ChaseCam_Update();
		motion_sensor_scan();
		check_m1_exploration();
		
#if !defined(DISABLE_NETWORKING)
		update_net_game();
#endif // !defined(DISABLE_NETWORKING)
	}
    /*
    if (get_app_state() == app_state_t::change_level) // TODO: this is going to move out of here, probably to end of game_event_loop
    {
        ao_err err = transfer_to_new_level(get_next_level_number()); // nope, obviously
        if (err)
        {
            display_loading_map_error(err); // move this up
            return kUpdateGameOver; //
        }
        else
        {
            return kUpdateChangeLevel;
        }
    }
    */
    
    // TODO: pretty sure moving this up to caller is safe
#if !defined(DISABLE_NETWORKING)
    if (network_game_is_over())
    {
        return kUpdateGameOver;
    }
#endif // !defined(DISABLE_NETWORKING)

    dynamic_world.tick_count+= 1;
    dynamic_world.game_information.game_time_remaining-= 1;

    return kUpdateNormalCompletion;
}



// ZZZ: new formulation of update_world(), should be simpler and clearer I hope.
// Now returns (whether something changed, number of real ticks elapsed) since, with
// prediction, something can change even if no real ticks have elapsed.

bool update_world(int32_t& elapsed_time)
{
    // we return separately 1. "whether to redraw" and 2. "how many game-ticks elapsed"
    elapsed_time = 0;
    bool needs_redraw = false;
    
    bool did_predict = false;
    bool canUpdate = true;
    int theUpdateResult = kUpdateNormalCompletion;

#ifndef DISABLE_NETWORKING
	if (game_is_networked())
	{
		NetProcessMessagesInGame();

		if (!NetCheckWorldUpdate()) { return false; }
	}
#endif

        while (canUpdate)
        {
                // If we have flags in the GameQueue, or can put a tick's-worth there, we're ok.
                // Note that GameQueue should be stocked evenly (i.e. every player has the same # of flags)
                if(GameQueue->countActionFlags(0) == 0)
                {
                        canUpdate = overlay_queue_with_queue_into_queue(GetRealActionQueues(), GetLuaActionQueues(), GameQueue);
                }

		if(!sPredictionWanted)
		{
			// See if the speed-limiter (net time or heartbeat count) will let us advance a tick
#if !defined(DISABLE_NETWORKING)
			int theMostRecentAllowedTick = game_is_networked() ? NetGetNetTime() : get_heartbeat_count();
#else
			int theMostRecentAllowedTick = get_heartbeat_count();
#endif
			
			if(dynamic_world.tick_count >= theMostRecentAllowedTick)
			{
				canUpdate = false;
			}
		}
		
		// If we can't update, we can't update.  We're done for now.
		if(!canUpdate)
		{
			break;
		}

		exit_interpolated_world();

		// Transition from predictive -> real update mode, if necessary.
		exit_predictive_mode();
		
		// Capture the flags for each player for use in prediction
		for(short i = 0; i < get_number_of_players(); i++)
			sMostRecentFlagsForPlayer[i] = GameQueue->peekActionFlags(i, 0);

		bool call_postidle = true;
		theUpdateResult = update_world_elements_one_tick(call_postidle);

        elapsed_time++;
		
		if (call_postidle)
			L_Call_PostIdle();
		if(theUpdateResult != kUpdateNormalCompletion || FilmExporter::instance()->IsExporting())
		{
			canUpdate = false;
		}

		}


        // This and the following voodoo comes, effectively, from Bungie's code.
        if(theUpdateResult == kUpdateChangeLevel)
        {
            elapsed_time = 0;
        }

	// Game over, man. Game over.
	if (theUpdateResult == kUpdateGameOver)
	{
        set_next_app_state(game_is_live() ? app_state_t::exit_game : app_state_t::load_and_play_demo_film); // TODO: this needs checked: how it behaves with user's film replays versus auto-running demos may be different
        elapsed_time = 0;
	}
	else if (elapsed_time)
	{
		//main_screen.swap_if_needed(); // TODO: ?
		update_fades(true);
	}

	check_recording_replaying();
	
	if(theUpdateResult == kUpdateNormalCompletion && sPredictionWanted)
	{
		NetUpdateUnconfirmedActionFlags();

		// We use "2" to make sure there's always room for our one set of elements.
		// (thePredictiveQueues should always hold only 0 or 1 element for each player.)
		ModifiableActionQueues	thePredictiveQueues(get_number_of_players(), 2, true);

		// Observe, since we don't use a speed-limiter in predictive mode, that there cannot be flags
		// stranded in the GameQueue.  Unfortunately this approach will mispredict if a script is
		// controlling the local player.  We could be smarter about it if that eventually becomes an issue.
		for ( ; sPredictedTicks < NetGetUnconfirmedActionFlagsCount(); sPredictedTicks++)
		{
			exit_interpolated_world();
			
			// Real -> predictive transition, if necessary
			enter_predictive_mode();

			// Enqueue stuff into thePredictiveQueues
			for(short thePlayerIndex = 0; thePlayerIndex < get_number_of_players(); thePlayerIndex++)
			{
				uint32 theFlags = (thePlayerIndex == local_player_index) ? NetGetUnconfirmedActionFlag((int32_t)sPredictedTicks) : sMostRecentFlagsForPlayer[thePlayerIndex];
				thePredictiveQueues.enqueueActionFlags(thePlayerIndex, &theFlags, 1);
			}
			
			// update_players() will dequeue the elements we just put in there
			decode_hotkeys(thePredictiveQueues);
			update_players(&thePredictiveQueues, true);

            did_predict = true;

		} // loop while local player has flags we haven't used for prediction
	} // if we should predict

	
	if (did_predict || elapsed_time > 0)
	{
		enter_interpolated_world();
        needs_redraw = true;
	}
    
    return needs_redraw;
}



// TODO: could do with pulling enter_gameworld and exit_gameworld into their own file so we can clean them up in isolation from this marathon2.cpp monster; right now they're pretty much a dumping ground for anything that looks related which needs a home but should probably be thinned out a bit (e.g. Stats don't belong here)

// icky; they're here because enter_gameworld initializes them
extern bool first_frame_rendered;
extern float last_heartbeat_fraction;


/* call this function after the new level has been completely read into memory, after
	player->location and player->facing have been updated, and as close to the end of
	the loading process in general as possible. */
void enter_gameworld(bool is_restoring_saved_game) // this arg is awkward
{
    
    // EES: dumping this here to be straightened out; ghs: hack to get new MML-specified sounds loaded // TODO: FIX: lazy, dumb, and annoying; going to disable it so we can extract scenario loading code from gameworld code; fixing the loading of MML-defined sounds is TODO (ideally they'd load under new IDs, but that'd break existing scenarios that use this [rather stupid] feature); the bigger problem will be unloading the custom sounds and reloading the defaults when going to a different level
    //sound_manager.UnloadAllSounds();
    
    set_lua_rects();
    
    main_screen.start_gameworld_renderer();
    
    
    // TODO: all of this scenario loading moves out of here: everything loads into memory when scenario is first loaded/changed; the only stuff that should load here are level-specific patches
	/* mark our shape collections for loading and load them */
	mark_environment_collections(static_world.environment_code, true);
	mark_all_monster_collections(true);
	mark_player_collections(true);
	mark_map_collections(true);
	MarkLuaCollections(true);
	MarkLuaHUDCollections(true);
	load_collections(true, modern_renderer_is_active());
	sounds_patches.clear();
	Plugins::instance()->load_sounds_patches();
	load_sounds_patch_data();
	load_all_monster_sounds();
	load_all_game_sounds(static_world.environment_code);
    
    initialize_monsters_for_new_level();
    
    
#if !defined(DISABLE_NETWORKING)
    // EES: seems a bit odd to have this next line mid-way in map setup, but not going to attempt reordering it
    
    // consolidated these network-related lines; hopefully reordering them relative the calls below isn't breaking anything
	// tell the keyboard controller to start recording keyboard flags
    if (game_is_networked()) // coop or PvP
    {
        NetSync(); // make sure everybody is ready // TODO: NetSync was the only line that returned `success` value, but NetSync (as it is implemented) never fails (which is sus.) so there's nothing to cause enter_gameworld to return an error; therefore we change its return type to void, which simplifies straightening out calling code; in future, once net code reports errors sensibly, this may return an error code, in which case enter_gameworld and its callers will need revised again
        
        NetSetChatCallbacks(InGameChatCallbacks::instance());
    }
#endif
    
    // make sure nobody’s holding a weapon illegal in the new environment
    check_player_weapons_for_environment_change();
    
    randomize_scenery_shapes();
    
	L_Call_Init(is_restoring_saved_game); // this function sets up Lua's RNGindirectly, then calls each loaded script's `init` handler
    
	init_interpolated_world();
    
	first_frame_rendered = false; // smelly; see also TODO in game_event_loop.cpp
	last_heartbeat_fraction = -1.f;
    
    // EES: sticking as much setup crap here as possible
    reset_action_queues();
    reset_motion_sensor(current_player_index);
    ChaseCam_Initialize();
    reset_fov();
    set_crosshairs_is_visible(player_preferences.crosshairs_active);
    reset_messages(); // probably unnecessary here, but need to confirm (overlay messages may end up tying in with notify_user)
    

    SDL_SetModState(KMOD_NONE); // Reset modifier key status
    set_keyboard_controller_status(game_is_live());
    set_prediction_wanted(game_is_networked());
    
    
    sound_manager.UpdateListener();

 //   LoadLuaHUDScript(); // TODO: where to load 0+ LuaHUDScript instances into vector? (also bear in mind the vector needs to be sorted by stacking order so that any overlapping content appears before/after according to what's sensible, e.g. scrolling message lists should probably go behind floating radar and inventory panels)
    
    // moved here from setup_game.cpp and consolidated
    switch (get_user_type())
    {
        case user_type_t::solo:
            LoadSoloLua(); // TODO: what about coop and solo game replay?
            // fall-thru
        case user_type_t::coop:
            // fall-thru
        case user_type_t::pvp:
            LoadAchievementsLua();
            LoadStatsLua();
            break;
        case user_type_t::replay:
            LoadReplayNetLua(); // TODO: again, AO not making a lick of sense
    }

    // LP: this is in case we are starting underneath a liquid // TODO: we've put
    //if (!modern_renderer_is_active() || !(TEST_FLAG(ogl_preferences.Flags, OGL_Flag_Fader)))
    //{
    //    set_fade_effect(NONE);
    //    SetFadeEffectDelay(TICKS_PER_SECOND / 2);
    //}
    //validate_world_window(); // TODO: this just called RequestDrawingTerm; confirm that's no longer needed

    
    // TODO: where to put the UI fades?
    // Zero out fades *AND* any inadvertant fades from script start... // EES: why here, though? presumably it's a UI fade, so probably best to move these lines into main_event_loop
    stop_fade();
    set_fade_effect(NONE);
    
    if (get_user_type() != user_type_t::replay) { start_recording(); }
}




// call this function when exiting the current level (quit/revert/interlevel teleport)
void exit_gameworld()
{
    /*
    if (game_is_live()) // from finish_game
    {
        stop_recording();
    }
    else
    {
        stop_replay();
        //FilmExporter::instance()->StopExporting(); // moved here from finish_game; who knows where it should eventually end up (e.g. if we want it to record chapter screens, probably not here)
    }
    */

    remove_all_projectiles();
    remove_all_nonpersistent_effects();
    
    // TODO: get rid of this; only unload when changing scenarios
    /* mark our shape collections for unloading */
    mark_environment_collections(static_world.environment_code, false);
    mark_all_monster_collections(false);
    mark_player_collections(false);
    mark_map_collections(false);
    MarkLuaCollections(false);
    MarkLuaHUDCollections(false);
    

    //Close and unload the Lua state
    UnloadLuaHUDScript();
    UnloadLuaScripts();
    
#if !defined(DISABLE_NETWORKING)
    NetSetChatCallbacks(NULL);
    
    // Only can transfer if NetUnSync returns true // TODO: which it always does, no?
    if (game_is_networked()) { NetUnSync(); } // TODO: wondering if this should return ao_err, e.g. STRID(gameError, errUnsyncOnLevelChange), but right now it doesn't (and there's a comment elsewhere it should never fail) so figure it out later (it might be an async call, in which case it can't fail now but might fail later - in which case how is rest of app notified of its shame?)
#endif
    
    Console::instance()->deactivate_input();
    set_keyboard_controller_status(false);
    
    // TODO: FIX: M1 level music keeps playing after returning to main menu; why? (I mean, the Music class is a bag of shit; needs stripped back and simplified)
    
    // Hackish. Should probably be in stop_all_sounds(), but that just doesn't work out.
    
    Music::instance()->QuickFade(); // moved here from finish_game
    
    Music::instance()->StopLevelMusic();
    Music::instance()->Pause();
    sound_manager.StopAllSounds();
    
    sound_manager.UnloadAllSounds(); // TODO: FIX: put this here - won't someone shut the bloody level music off
    
    // don't send stats on film replay, obviously
   // if (game_is_live()) { StatsManager::instance()->Process(); } where should this be called?
    
    stop_fade(); // stop any existing [effect] fades
    set_fade_effect(NONE);
    reset_messages(); // flush the message overlays
    
    main_screen.stop_gameworld_renderer();
}








/* This is called when an object of some mass enters a polygon from another */
/* polygon.  It handles triggering lightsources, platforms, and whatever */
/* else it is that we can think of. */
void changed_polygon(
	short original_polygon_index,
	short new_polygon_index,
	short player_index)
{
	struct polygon_data *new_polygon= get_polygon_data(new_polygon_index);
	Player* player= player_index!=NONE ? get_player_data(player_index) : (Player* ) NULL;
	
	(void) (original_polygon_index);
	
	/* Entering this polygon.. */
	switch (new_polygon->type)
	{
		case _polygon_is_visible_monster_trigger:
			if (player)
			{
				activate_nearby_monsters(player->monster_index, player->monster_index,
					_pass_solid_lines|_activate_deaf_monsters|_use_activation_biases|_activation_cannot_be_avoided);
			}
			break;
		case _polygon_is_invisible_monster_trigger:
		case _polygon_is_dual_monster_trigger:
			if (player)
			{
				activate_nearby_monsters(player->monster_index, player->monster_index,
					_pass_solid_lines|_activate_deaf_monsters|_activate_invisible_monsters|_use_activation_biases|_activation_cannot_be_avoided);
			}
			break;
		
		case _polygon_is_item_trigger:
			if (player)
			{
				trigger_nearby_items(new_polygon_index);
			}
			break;

		case _polygon_is_light_on_trigger:
		case _polygon_is_light_off_trigger:
			set_light_status(new_polygon->permutation,
				new_polygon->type==_polygon_is_light_off_trigger ? false : true);
			break;
			
		case _polygon_is_platform:
			platform_was_entered(new_polygon->permutation, player ? true : false);
			break;
		case _polygon_is_platform_on_trigger:
		case _polygon_is_platform_off_trigger:
			if (player)
			{
				try_and_change_platform_state(get_polygon_data(new_polygon->permutation)->permutation,
					new_polygon->type==_polygon_is_platform_off_trigger ? false : true);
			}
			break;
			
		case _polygon_must_be_explored:
			/* When a player enters a must be explored, it now becomes a normal polygon, to allow */
			/*  for must be explored flags to work across cooperative net games */
			if(player)
			{
				new_polygon->type= _polygon_is_normal;
			}
			break;
			
		default:
			break;
	}
}

// Lua scripts can now override completion state
short calculate_level_completion_state()
{
	short completion_state;
	if (L_Calculate_Completion_State(completion_state))
	{
		return completion_state;
	}
	else
	{
		return calculate_classic_level_completion_state();
	}
}

/* _level_failed is the same as _level_finished but indicates a non-fatal failure condition (e.g.,
	too many civilians died during _mission_rescue) */
short calculate_classic_level_completion_state(
	void)
{
	short completion_state= _level_finished;
	
	/* if there are any monsters left on an extermination map, we haven’t finished yet */
	if (static_world.mission_flags&_mission_extermination)
	{
		if (live_aliens_on_map()) completion_state= _level_unfinished;
	}
	
	/* if there are any polygons which must be explored and have not been entered, we’re not done */
	if ((static_world.mission_flags & _mission_exploration) || (static_world.mission_flags & _mission_exploration_m1))
	{
        for (const auto& polygon : PolygonList)
		{
            if (polygon.type == _polygon_must_be_explored)
			{
				completion_state = _level_unfinished;
				break;
			}
		}
	}
	
	/* if there are any items left on this map, we’re not done */
	if (static_world.mission_flags&_mission_retrieval)
	{
		if (unretrieved_items_on_map()) completion_state= _level_unfinished;
	}
	
	/* if there are any untoggled repair switches on this level then we’re not there */
	if ((static_world.mission_flags&_mission_repair) ||
	    (static_world.mission_flags&_mission_repair_m1))
	{
		/* M1 only required last repair switch to be toggled */
		bool only_last_switch = (film_profile.m1_buggy_repair_goal && (static_world.mission_flags&_mission_repair_m1));
		if (untoggled_repair_switches_on_level(only_last_switch)) completion_state= _level_unfinished;
	}

	/* if we’ve finished the level, check failure conditions */
	if (completion_state==_level_finished)
	{
		/* if this is a rescue mission and more than half of the civilians died, the mission failed */
		if (static_world.mission_flags&(_mission_rescue|_mission_rescue_m1) &&
			dynamic_world.current_civilian_causalties>dynamic_world.current_civilian_count/2)
		{
			completion_state= _level_failed;
		}
	}
	
	return completion_state;
}

short calculate_damage(
	struct damage_definition *damage)
{
	short total_damage= damage->base + (damage->random ? global_random()%damage->random : 0);
	
	total_damage= FIXED_INTEGERAL_PART(total_damage*damage->scale);
	
	/* if this damage was caused by an alien modify it for the current difficulty level */
	if (damage->flags&_alien_damage)
	{
		switch (dynamic_world.game_information.difficulty_level)
		{
			case _wuss_level: total_damage-= total_damage>>1; break;
			case _easy_level: total_damage-= total_damage>>2; break;
			/* harder levels do not cause more damage */
		}
	}
	
	return total_damage;
}

#define MINOR_OUCH_FREQUENCY 0xf
#define MAJOR_OUCH_FREQUENCY 0x7
#define MINOR_OUCH_DAMAGE 15
#define MAJOR_OUCH_DAMAGE 7

// LP temp fix: assignments are intended to approximate Marathon 1 (minor = lava, major = PfhorSlime)
#define _damage_polygon _damage_lava
#define _damage_major_polygon _damage_goo

void cause_polygon_damage(
	short polygon_index,
	short monster_index)
{
	struct polygon_data *polygon= get_polygon_data(polygon_index);
	struct monster_data *monster= get_monster_data(monster_index);
	struct object_data *object= get_object_data(monster->object_index);
    
    short polygon_type = polygon->type;
    // apply damage from flooded platforms
    if (polygon->type == _polygon_is_platform)
	{
		struct platform_data *platform= get_platform_data(polygon->permutation);
		if (platform && PLATFORM_IS_FLOODED(platform))
		{
			short adj_index = find_flooding_polygon(polygon_index);
			if (adj_index != NONE)
			{
				struct polygon_data *adj_polygon = get_polygon_data(adj_index);
				polygon_type = adj_polygon->type;
			}
		}
	}


	if ((polygon_type==_polygon_is_minor_ouch && !(dynamic_world.tick_count&MINOR_OUCH_FREQUENCY) && object->location.z==polygon->floor_height) ||
		(polygon_type==_polygon_is_major_ouch && !(dynamic_world.tick_count&MAJOR_OUCH_FREQUENCY)))
	{
		struct damage_definition damage;
		
		damage.flags= _alien_damage;
		damage.type= polygon_type==_polygon_is_minor_ouch ? _damage_polygon : _damage_major_polygon;
		damage.base= polygon_type==_polygon_is_minor_ouch ? MINOR_OUCH_DAMAGE : MAJOR_OUCH_DAMAGE;
		damage.random= 0;
		damage.scale= FIXED_ONE;
		
		damage_monster(monster_index, NONE, NONE, (world_point3d *) NULL, &damage, NONE);
	}
}

/* ---------- private code */
	


// LP: suppressed this as superfluous; won't try to reassign these sounds for M1 compatibility
static void load_all_game_sounds(
	short environment_code)
{
}

/*
#define NUMBER_OF_PRELOAD_SOUNDS (sizeof(preload_sounds)/sizeof(short))
static short preload_sounds[]=
{
	_snd_teleport_in,
	_snd_teleport_out,
	_snd_bullet_ricochet,
	_snd_magnum_firing,
	_snd_assault_rifle_firing,
	_snd_body_falling,
	_snd_body_exploding,
	_snd_bullet_hitting_flesh
};

#define NUMBER_OF_PRELOAD_SOUNDS0 (sizeof(preload_sounds0)/sizeof(short))
static short preload_sounds0[]= {_snd_water, _snd_wind};

#define NUMBER_OF_PRELOAD_SOUNDS1 (sizeof(preload_sounds1)/sizeof(short))
static short preload_sounds1[]= {_snd_lava, _snd_wind};

static void load_all_game_sounds(
	short environment_code)
{
	load_sounds(preload_sounds, NUMBER_OF_PRELOAD_SOUNDS);

	switch (environment_code)
	{
		case 0: load_sounds(preload_sounds0, NUMBER_OF_PRELOAD_SOUNDS0); break;
		case 1: load_sounds(preload_sounds1, NUMBER_OF_PRELOAD_SOUNDS1); break;
		case 2: break;
		case 3: break;
		case 4: break;
	}
}
*/
