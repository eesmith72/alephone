



#include "gameworld_entrance.hpp"

#include "map.h"
#include "render.h"
#include "interface.hpp"
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
#include "hud_manager.h"
#include "SoundManager.h"
#include "network_games.h"
#include "vbl.h" // sync_heartbeat_count
#include "tags.h"
#include "AnimatedTextures.h"
#include "ChaseCam.h"
//#include "OGL_Setup.h"
#include "OGL_Render.h" // modern_renderer_is_active
//#include "ClassicRasterizer.h" // allocate_sw_texture_tables

#include "lua_script.h"
#include "lua_hud_script.h"


#include "Screen.hpp"
#include "ActionQueues.h"

//#include "Screen.hpp"
#include "screen_overlay.h" // reset_messages
//#include "shell.h"

#include "Console.h"
#include "FilmExporter.h"
#include "Statistics.h"

#include "motion_sensor.hpp"

#include "preferences.hpp" // player_preferences (may go away again, depending where crosshairs are enabled)

#include "ephemera.h"
#include "interpolated_world.h"

#include "Plugins.h"
#include "SoundsPatch.h"

#include "setup_game.hpp"




// icky; they're here because enter_gameworld initializes them
extern bool first_frame_rendered;
extern float last_heartbeat_fraction;


/* call this function after the new level has been completely read into memory, after
	player->location and player->facing have been updated, and as close to the end of
	the loading process in general as possible. */
void enter_gameworld(bool is_restoring_saved_game) // (the level scripts' `init` handler need to know if this level is new or resumed from a saved state)
{
    
    // EES: dumping this here to be straightened out; ghs: hack to get new MML-specified sounds loaded // TODO: FIX: lazy, dumb, and annoying; going to disable it so we can extract scenario loading code from gameworld code; fixing the loading of MML-defined sounds is TODO (ideally they'd load under new IDs, but that'd break existing scenarios that use this [rather stupid] feature); the bigger problem will be unloading the custom sounds and reloading the defaults when going to a different level
    //sound_manager.UnloadAllSounds();
    
    L_Call_HUDResize(); // moved here for now (from enter_screen in screen.cpp); TODO: a general `hud_manager.start()` (probably after the level scripts have been run below)

    main_screen.start_gameworld_renderer();
    
    
    // TODO: all of this scenario loading moves out of here: everything loads into memory when scenario is first loaded/changed; the only stuff that should load here are level-specific patches
	/* mark our shape collections for loading and load them */
	mark_environment_collections(static_world.environment_code, true);
	mark_all_monster_collections(true);
	mark_player_collections(true);
	mark_map_collections(true);
	MarkLuaCollections(true);
	MarkLuaHUDCollections(true);
	load_collections(true, modern_renderer_is_active()); // shapes patches may require OGL, so pass bool indicating which renderer is in use
	sounds_patches.clear();
	Plugins::instance()->load_sounds_patches();
	load_sounds_patch_data();
	load_all_monster_sounds();
    
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
    main_camera_settings.clear_effect(); // was reset_fov
    //set_crosshairs_is_visible(player_preferences.crosshairs_active);
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
   // stop_fade();
   // set_fade_effect(NONE);
    
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




