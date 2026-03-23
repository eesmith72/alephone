
#include "setup_game.hpp"

#include "mouse.h" // hide_cursor
#include "joystick.h"

#include "shell_options.h" // shell_options.replay_directory

#include "preferences.h" // player_preferences
#include "player.h" // player_data


#include "QuickSave.h" // get_last_saved_game_path
#include "choose_file_dialogs_os.hpp" // display_read_saved_film_dialog

#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialogs.h"


#include "vbl.h" // set_recording_header_data

#include "map.h" // player_start_data, dynamic_world
#include "map_wad.h"


//#include "image_blitter.hpp"


#include "OpenALManager.h"
#include "MovieExporter.h"
#include "Music.h"


#include "Plugins.h"
#include "XML_LevelScript.h" // ResetLevelScript
#include "lua_hud_script.h" // LoadHUDLua
#include "motion_sensor.hpp" // reset_motion_sensor



// no idea if this is used
#ifdef PERFORMANCE
#include <perf.h>

extern TP2PerfGlobals perf_globals;
#endif




// TODO: consider moving player starts (std::vector<player_identity_t>) onto game info struct

// ----- ZZZ start support for generalized game startup -----
// (should this be split out (with some other game startup code) to a new file?)

// In this scheme, a "start" corresponds to a player available at the moment, who will
// participate in the game we're starting up.  In general when this code talks about a
// 'player', it is referring to an already-existing player in the game world (which should
// already be restored or initialized fairly well before these routines are used).
// Generalized game startup will match starts to existing players, set leftover players
// as "zombies" so they exist but just stand there, and create new players to correspond
// with any remaining starts.  As such it can handle both resume-game (some players already
// exist) and new-game (dynamic_world->player_count == 0) operations.

// also called in load_and_start_game
static void set_solo_player_identity(player_start_data* outStartArray, short* outStartCount) // TODO: convert to vector<player_start_data>
{
    memcpy(outStartArray, 0, sizeof(*outStartArray));
    
    *outStartCount = 1;
    outStartArray[0] = {0, player_preferences->color, player_preferences->color, player_preferences->name};
    
    set_player_start_doesnt_auto_switch_weapons_status(&outStartArray[0], dont_switch_to_new_weapon());
}


void set_network_player_identities(player_start_data* outStartArray, short* outStartCount)
{
    memcpy(outStartArray, 0, sizeof(*outStartArray));

    *outStartCount = NetGetNumberOfPlayers();

    for (int32_t i = 0; i < *outStartCount; i++)
    {
        player_info* player = NetGetPlayerData(i);
        outStartArray[i] = {NetGetPlayerIdentifier(i), player->team, player->color, player->name};
    }
}






// This should be safe to use whether starting or resuming, and whether single- or multiplayer.
static void synchronize_players_with_starts(const player_start_data* inStartArray, short inStartCount, short inLocalPlayerIndex)
{
    assert_fail(inLocalPlayerIndex >= 0 && inLocalPlayerIndex < inStartCount, "");
    
    // s will walk through all the starts
    int s = 0;
    
    // First we process existing players
    for( ; s < dynamic_world->player_count; s++)
    {
        player_data* thePlayer = get_player_data(s);
        
        if(inStartArray[s].identifier == NONE)
        {
            // No start (live player) to go with this player (stored player)
            SET_PLAYER_ZOMBIE_STATUS(thePlayer, true);
        }
        else
        {
            // Update player's appearance to match the start
            thePlayer->team = inStartArray[s].team;
            thePlayer->color = inStartArray[s].color;
            thePlayer->identifier = player_identifier_value(inStartArray[s].identifier);
            thePlayer->name = inStartArray[s].name;
            
            SET_PLAYER_DOESNT_AUTO_SWITCH_WEAPONS_STATUS(thePlayer,
                                                         player_identifier_doesnt_auto_switch_weapons(inStartArray[s].identifier));
            
            // Make sure if player was saved as zombie, they're not now.
            SET_PLAYER_ZOMBIE_STATUS(thePlayer, false);
        }
    }
    
    // Designate the local player if they already exist
    if (inLocalPlayerIndex < s)
    {
        set_local_player_index(inLocalPlayerIndex);
        set_current_player_index(inLocalPlayerIndex);
    }
    
    // If there are any starts left, we need new players for them
    for( ; s < inStartCount; s++)
    {
        new_player_flags flags = (s == inLocalPlayerIndex ? new_player_make_local_and_current : 0);
        int theIndex = new_player(inStartArray[s].team, inStartArray[s].color, inStartArray[s].identifier, flags);
        assert_fail(theIndex == s, "");
        player_data* thePlayer = get_player_data(theIndex);
        thePlayer->name = inStartArray[s].name;
    }
}


static short find_start_for_identifier(const player_start_data* inStartArray, short inStartCount, short player_identifier)
{
    player_identifier = player_identifier_value(player_identifier); // mask crap
    for (int32_t i = 0; i < inStartCount; i++)
    {
        if (player_identifier_value(inStartArray[i].identifier) == player_identifier) { return i; }
    }
    return NONE;
}







// The single-player machine, gatherer, and joiners all will use this routine.  It should take most of its
// cues from the "extras" that load_game_from_file() does.
static void make_restored_game_relevant(bool inNetgame, const player_start_data* inStartArray, short inStartCount)
{
    RunLuaScript();
    game_is_networked = inNetgame;
    
    // set_random_seed() needs to happen before synchronize_players_with_starts()
    // since the latter calls new_player() which almost certainly uses global_random().
    // Note we always take the random seed directly from the dynamic_world, no need to screw around
    // with copying it from game_information or the like.
    set_random_seed(dynamic_world->random_seed);
    
    short theLocalPlayerIndex;
    
#if !defined(DISABLE_NETWORKING)
    // Much of the code in this if()...else is very similar to code in create_new_game(), should probably try to share.
    if(inNetgame)
    {
        game_info *network_game_info = NetGetGameData();
        
        dynamic_world->game_information.game_time_remaining= network_game_info->time_limit;
        dynamic_world->game_information.kill_limit= network_game_info->kill_limit;
        dynamic_world->game_information.game_type= network_game_info->net_game_type;
        dynamic_world->game_information.game_options= network_game_info->game_options;
        dynamic_world->game_information.initial_random_seed= network_game_info->initial_random_seed;
        dynamic_world->game_information.difficulty_level= network_game_info->difficulty_level;
        dynamic_world->game_information.cheat_flags= network_game_info->cheat_flags;
        
        // ZZZ: until players specify their behavior modifiers over the network,
        // to avoid out-of-sync we must force them all the same.
        set_custom_behaviors_enabled(false);
        
        theLocalPlayerIndex = NetGetLocalPlayerIndex();
    }
    else
#endif // !defined(DISABLE_NETWORKING)
    {
        dynamic_world->game_information.difficulty_level= player_preferences->difficulty_level;
        set_custom_behaviors_enabled(true);
        theLocalPlayerIndex = find_start_for_identifier(inStartArray, inStartCount, 0);
    }
    
    assert_fail(theLocalPlayerIndex != NONE, "");
    
    synchronize_players_with_starts(inStartArray, inStartCount, theLocalPlayerIndex);
    
    entering_map(true);
    
    reset_motion_sensor(theLocalPlayerIndex);
}




ao_err load_saved_game_from_flat_data(byte* saved_flat_data)
{
    assert_fail(saved_flat_data, "");

    wad_header_t header;
    wad_data* wad = inflate_flat_data(saved_flat_data, &header);
    assert_fail(wad, "inflate_flat_data should never fail");
    
    dynamic_data dynamic_data_wad;
    ao_err err = get_dynamic_data_from_wad(wad, &dynamic_data_wad);
    if (err)
    {
        free_wad(wad);
        return err;
    }

    Plugins::instance()->set_mode(dynamic_data_wad.player_count > 1 ? Plugins::kMode_Net : Plugins::kMode_Solo);

    ResetLevelScript();
    
    // find the original Map file from which this saved game was created
    err = set_current_map_path_to_file_with_checksum(header.parent_checksum);
    if (err) // can't find it; originally M2 would let user play to end of level and then fail, but since we have no idea if this level has scripts attached it's best to fail now
    {
        free_wad(wad);
        
        // TODO: this is pretty much the original code
        hide_cursor();
        
        reset_current_map_path_to_default();

        LoadAchievementsLua();
        LoadStatsLua();
        
        return STRID(strERRORS, cantFindMap);
    }

    RunLevelScript(dynamic_data_wad.current_level_number);

    process_map_wad(wad, true /* resuming */, header.data_version);
    free_wad(wad); /* Note that the flat data points into the wad. */
    // ZZZ: maybe this is what the Bungie comment meant, but apparently
    // free_wad() somehow (voodoo) frees theSavedGameFlatData as well.
    
    // TODO: what if success is false here?
    
    // try to locate the Map file for the saved-game, so that (1) we have a crack
    // at continuing the game if the original gatherer disappears, and (2) we can
    // save the game on our own machine and continue it properly (as part of a bigger scenario) later.
    LoadAchievementsLua();
    LoadStatsLua();

    if (!game_is_networked)
    {
        if (dynamic_world->player_count == 1)
            LoadSoloLua();
        else
            LoadReplayNetLua();
    }

    return err;
}






// ZZZ end generalized game startup support -----

#if !defined(DISABLE_NETWORKING)
// ZZZ: this will get called (eventually) shortly after NetUpdateJoinState() returns netStartingResumeGame
ao_err join_networked_resume_game() // co-op game
{
    player_start_data theStarts[MAXIMUM_NUMBER_OF_PLAYERS];

    uint8_t* flat_data;
    ao_err err = NetReceiveGameData(false, flat_data);
    if (err) return err;
    
    int32_t saved_wad_length = get_flat_data_length(flat_data);
    std::vector<uint8_t> saved_wad_data(flat_data, flat_data + saved_wad_length); // TODO: as in load_and_start_game, don't think this vector is adding value
    
    err = load_saved_game_from_flat_data(flat_data);
    if (err) return err;
    
    Crosshairs_SetActive(player_preferences->crosshairs_active);
    LoadHUDLua();
                    
    // set the revert-game info to defaults (for full-auto saving on the local machine)
    reset_revert_game_file_to_default();
    
    short theStartCount;
    set_network_player_identities(theStarts, &theStartCount);
    make_restored_game_relevant(true, theStarts, theStartCount);
    
    set_recording_header_data(theStartCount, dynamic_world->current_level_number, NetGetGameData()->parent_checksum,
                              default_recording_version, theStarts, &dynamic_world->game_information);

    set_recording_saved_wad_data(saved_wad_data);
    start_recording();

    start_game(false);
    
    return err;
}
#endif // !defined(DISABLE_NETWORKING)





// ZZZ: changes to use generalized game startup support // This will be used only on the machine that picked "Continue Saved Game".
ao_err load_and_start_game(bool is_coop)
{
    ao_err err = no_err;
    
    if (get_user_type() == user_type_t::network_player)
    {
        bool use_remote_hub;
        
        show_cursor();
        err = display_network_gather_dialog(true, use_remote_hub);
        hide_cursor();
        
        if (!err && use_remote_hub) { err = join_networked_resume_game(); }
    }
    else // TODO: if the above conditional and `return` is correct, there is conditional code below that will always/never execute
    {
        // this would be solo_player or replay_player
        
        Crosshairs_SetActive(player_preferences->crosshairs_active);
        LoadHUDLua();
        
        // load the scripts we put off before
        if (get_user_type() == user_type_t::solo_player)
        {
            LoadSoloLua();
        }
        LoadAchievementsLua();
        LoadStatsLua();
        
        player_start_data theStarts[MAXIMUM_NUMBER_OF_PLAYERS];
        short theNumberOfStarts;
        
        
        // TODO: and it goes off here, so probably a misplaced brace
        
        if (get_user_type() != user_type_t::network_player)
        {
            // this seems off: what about replay?
            set_solo_player_identity(theStarts, &theNumberOfStarts);
            match_starts_with_existing_players(theStarts, &theNumberOfStarts);
        }
        
        if (get_user_type() == user_type_t::network_player)
        {
            set_network_player_identities(theStarts, &theNumberOfStarts);
            match_starts_with_existing_players(theStarts, &theNumberOfStarts);
            NetSetupTopologyFromStarts(theStarts, theNumberOfStarts);
            
            NetStart();
            
            uint8_t* wad = nullptr;
            err = get_map_for_net_transfer(is_coop ? 0 : NetGetGameData()->level_number, wad);
            if (err) return err;

            err = NetDistributeGameDataToAllPlayers(wad, get_flat_data_length(wad), false /* do_physics? */);
            if (err) return err;
        }

        
        
        make_restored_game_relevant(get_user_type() == user_type_t::network_player, theStarts, theNumberOfStarts);
        
        // only if we're recording
        set_recording_header_data(theNumberOfStarts, dynamic_world->current_level_number,
                                  get_user_type() == user_type_t::network_player ? NetGetGameData()->parent_checksum : get_current_map_checksum(),
                                  default_recording_version, theStarts, &dynamic_world->game_information);
        
        uint8_t* flat_data = nullptr;
        get_map_for_net_transfer(0, flat_data);
        
        std::vector<uint8_t> saved_wad_data(flat_data, flat_data + get_flat_data_length(flat_data));
        set_recording_saved_wad_data(saved_wad_data); // TODO: this appears to do copy of the vector; why not just pass flat_data directly and let replay_private_data struct take ownership of it?
        start_recording();
        
        start_game(false);
    }
    return err;
}






game_data game_information;

// why were they called player starts? they're player identities
player_start_data player_identities[MAXIMUM_NUMBER_OF_PLAYERS]; // TODO: use vector (though will still need to limit to max players, for now)
short number_of_players; // this goes away once identities is vector

bool is_networked = false;
bool clean_up_on_failure = true; // crap

bool record_game = false;
short record_game_version = default_recording_version;
uint32_t parent_checksum = 0;






void configure_new_solo_game(int16_t level_number)
{
    set_user_type(user_type_t::solo_player);
    set_custom_behaviors_enabled(true);
    
    // TODO: change starts array to vector and size it exactly (make sure any code that assumes it's always 8 items is updated accordingly)
    set_solo_player_identity(player_identities, &number_of_players);
    
    game_information.level_number           = level_number;
    game_information.game_time_remaining    = INT32_MAX;
    game_information.kill_limit             = 0;
    game_information.game_type              = _game_of_kill_monsters;
    game_information.game_options           = _burn_items_on_death | _ammo_replenishes | _weapons_replenish | _monsters_replenish;
    game_information.initial_random_seed    = machine_tick_count();
    game_information.difficulty_level       = player_preferences->difficulty_level;
    game_information.cheat_flags            = default_cheat_flags; // EES: added this; TODO: why wasn't this set here before? where is it/should it be set?
    
    //std::fill_n(game_information.parameters, 2, 0);
    
    // ZZZ: until film files store player behavior flags, all films recorded must use standard behavior. // TODO: this restriction to go away
    record_game = is_player_behavior_standard();
    
    // TODO: what is point of this? we should always use current compatibility profile and any fixes/behavior flags get stored in recording file; see also Solo Gameplay menu in Player Preferences (absolute fucking shambles)
    switch (player_preferences->solo_profile)
    {
        case _solo_profile_aleph_one:
            load_film_profile(FILM_PROFILE_DEFAULT);
            break;
        case _solo_profile_marathon_2:
            load_film_profile(FILM_PROFILE_MARATHON_2);
            record_game_version = RECORDING_VERSION_MARATHON_2;
            break;
        case _solo_profile_marathon_infinity:
            load_film_profile(FILM_PROFILE_MARATHON_INFINITY);
            record_game_version = RECORDING_VERSION_MARATHON_INFINITY;
            break;
    }
}


void configure_new_network_game(game_info* info)
{
    set_user_type(user_type_t::network_player);
    set_network_player_identities(player_identities, &number_of_players);
    
    // despite the name, `game_information` is actually a `game_data` struct
    // TODO: game_data and game_info structs are virtually identical, so why are they not one?
    game_information.level_number           = info->level_number;
    game_information.game_time_remaining    = info->time_limit;
    game_information.kill_limit             = info->kill_limit;
    game_information.game_type              = info->net_game_type;
    game_information.game_options           = info->game_options;
    game_information.initial_random_seed    = info->initial_random_seed;
    game_information.difficulty_level       = info->difficulty_level;
    game_information.cheat_flags            = info->cheat_flags;
    
    //std::fill_n(game_information.parameters, 2, 0);
    
    parent_checksum                         = info->parent_checksum;
    
    is_networked = true;
    record_game  = true;
    
    // ZZZ: until players specify their behavior modifiers over the network,
    // to avoid out-of-sync we must force them all the same.
    set_custom_behaviors_enabled(false);
    
    load_film_profile(FILM_PROFILE_DEFAULT);
}



ao_err configure_replay_game()
{
    int16_t level_number;
    uint32_t map_checksum; // unused
    int16_t recording_version;
    get_recording_header_data(number_of_players, level_number, map_checksum, recording_version, player_identities, &game_information);
    
    if (recording_version > max_handled_recording)
    {
        stop_replay(); // why would it be started?
        return STRID(strERRORS, replayVersionTooNew);
    }
    
    load_film_profile_for_recording_version(recording_version);
    
    game_information.game_options |= _overhead_map_is_omniscient;
    record_game = false;
    // ZZZ: until films store behavior modifiers, we must require that they record and playback only with standard modifiers.
    set_custom_behaviors_enabled(false);
    return no_err;
}







ao_err create_new_game(int16_t level_number, bool cheat)
{
    ao_err err = no_err;

    if (game_is_live())
    {
        // TODO: seems wrong
        if(!std::filesystem::is_regular_file(get_default_map_path()))
        {
            return STRID(strERRORS, missingFile); // error codes should be more specific, e.g. mapFileNotFound
        }
        uint32_t map_checksum = (get_user_type() == user_type_t::network_player) ? parent_checksum : get_current_map_checksum(); // TODO: check this
        set_recording_header_data(number_of_players, level_number, map_checksum, record_game_version, player_identities, &game_information);
        start_recording();
    }
    
    
    if (!err)
    {
        hide_cursor();
        // This has already been done to get to gather/join
        if(can_interface_fade_out()) { animate_ui_fade_out_blocking(true); }
        
        // Try to display the first chapter screen
        if (get_user_type() == user_type_t::solo_player && !is_saved_game_replay())
        {
            FindLevelMovie(level_number);
            show_movie(level_number);
            try_and_display_chapter_screen(level_number, false, false);
        }
        
        Plugins::instance()->set_mode(number_of_players > 1 ? Plugins::kMode_Net : Plugins::kMode_Solo);
        Crosshairs_SetActive(player_preferences->crosshairs_active);
        LoadHUDLua();
        
        if (is_saved_game_replay())
        {
            make_restored_game_relevant(false, player_identities, number_of_players);
        }
        else
        {
            err = new_game(level_number, number_of_players, is_networked, &game_information, player_identities);
        }
    }
    if (!err)
    {
        start_game(false);
    }
    else // ick; cleaning up game state after normal/failed game should likely be the next app state transition
    {
        if (record_game) { stop_recording(); }
        set_local_player_index(NONE);
        set_current_player_index(NONE);
        // The only time we don't clean up is on the replays
        if (get_user_type() == user_type_t::network_player && clean_up_on_failure) { NetExit(); }
        
        /*
         TODO:
         show_cursor();
         display_loading_map_error(err);
         display_main_menu();
         */
    }
    
    return err;
}



void start_game(bool changing_level)
{
    // TODO: this function is shitty and its relevant contents should probably migrate to enter_level
    
    reset_screen(); // LP change: reset screen so that extravision will not be persistent
    
    enter_screen();
    L_Call_HUDInit(); // always tear down and recreate HUD on level jumps (e.g. in case the level loads a new one; handy for Minf where restyled HUDs might reflect player's current loyalty); this also simplifies implementation; TODO: confirm this works
    
    // LP: this is in case we are starting underneath a liquid
    if (!ogl_is_active() || !(TEST_FLAG(Get_OGL_ConfigureData().Flags,OGL_Flag_Fader)))
    {
        set_fade_effect(NONE);
        SetFadeEffectDelay(TICKS_PER_SECOND/2);
    }

    // Screen should already be black!
    validate_world_window();
    
#ifdef PERFORMANCE
    PerfControl(perf_globals, true);
#endif
    
    //game_state.ticks_until_next_screen  = MACHINE_TICKS_PER_SECOND;
    //game_state.last_ticks_on_idle       = machine_tick_count();

    assert_fail (!changing_level == !is_vbl_reading_user_inputs(), "");
    if (!changing_level)
    {
        set_keyboard_controller_status(true); // TODO: this smells; it should be in main_event_loop.cpp
    }

    SoundManager::instance()->UpdateListener();
}





ao_err transfer_to_new_level(short level_number) // TODO: split this up: we want its contents [mostly] in exit_level state so gameworld has exactly one entry point and one exit point
{
    ao_err err = no_err;

    // Only can transfer if NetUnSync returns true // TODO: which it always does, no?
    if (game_is_networked) { NetUnSync(); } // TODO: wondering if this should return ao_err, e.g. STRID(gameError, errUnsyncOnLevelChange), but right now it doesn't (and there's a comment elsewhere it should never fail) so figure it out later (it might be an async call, in which case it can't fail now but might fail later - in which case how is rest of app notified of its shame?)
    
    stop_fade();
    set_fade_effect(NONE);
    exit_screen(); // Enter_screen will be called again in start_game
    
    set_keyboard_controller_status(false);
    FindLevelMovie(level_number);
    show_movie(level_number);

    // if this is the M2_EPILOGUE_LEVEL_NUMBER, then it is time to get out of here already
    // (as we've just played the epilogue movie, we can move on to the app_state_t::epilogue_screen game state)
    if (level_number == (shapes_file_is_m1() ? M1_EPILOGUE_LEVEL_NUMBER : M2_EPILOGUE_LEVEL_NUMBER))
    {
        finish_game(false);
        show_cursor(); // for some reason, cursor stays hidden otherwise

        if (shell_options.replay_directory.empty()) // TODO: bizarre test, presumably if it's batch-exporting movies?
        {
            advance_app_state_queuing_next(app_state_t::epilogue_screen);
        }
        
        conclude_app_state_timeout();
    }
    else
    {
        if (!game_is_networked) try_and_display_chapter_screen(level_number, true, false);
        
        err = goto_level(level_number, dynamic_world->player_count, nullptr);
        set_keyboard_controller_status(true);
        
        if (err) // yeah, no
        {
            finish_game(true);
        }
        else
        {
            start_game(true);
        }
    }
    return err;
}



void finish_game(bool return_to_main_menu) // TODO: stupid fucking argument; caller needs to set the appropriate state transition
{
    set_keyboard_controller_status(false);

#ifdef PERFORMANCE
    PerfControl(perf_globals, false);
#endif
    
    //assert_fail(get_app_state() == app_state_t::game_in_progress        ||
    //            get_app_state() == app_state_t::load_and_play_demo_film ||
    //            get_app_state() == app_state_t::revert_to_saved_game    ||
    //            get_app_state() == app_state_t::change_level            ||
    //            get_app_state() == app_state_t::epilogue_screen, "");

    stop_fade();
    set_fade_effect(NONE);
    L_Call_HUDCleanup();
    exit_screen();

    if (game_is_live())
    {
        stop_recording();
    }
    else
    {
        stop_replay();
    }
    MovieExporter::instance()->StopRecording();

    if (shell_options.editor && shell_options.should_output_to_file())
    {
        L_Call_Cleanup();
        ao_path file = shell_options.output_path;
        ao_err err = export_level(file);
        exit(err);
    }

    // Fade out! (Pray) // should be interface_color_table for valkyrie, but doesn't work.
    Music::instance()->ClearLevelPlaylist();
    Music::instance()->Fade(0, MACHINE_TICKS_PER_SECOND / 2, MusicPlayer::FadeType::Sinusoidal);
    animate_ui_fade_blocking(_cinematic_fade_out, interface_color_table);
    clear_screen();
    animate_ui_fade_blocking(_end_cinematic_fade_out, interface_color_table);

    show_cursor();

    leaving_map();
    CloseLuaHUDScript();
    
    // Get as much memory back as we can. // TODO: NO, it's not 1995! Scenario gets fully loaded when selected, stays fully loaded until a different scenario is selected/process exits.
    unload_all_collections();
    SoundManager::instance()->UnloadAllSounds();
    

    if (get_user_type() == user_type_t::network_player)
    {
        NetUnSync(); // gracefully exit from the game

        // Don't update the screen, etc
        set_app_state(app_state_t::gather_network_game); // TODO: smells

        change_screen_mode(_screentype_menu);
        force_system_colors(false);
        display_net_game_stats();
        NetExit();
    }
    else if (game_is_replay())
    {
        if (!shell_options.replay_directory.empty())
        {
            set_app_state(app_state_t::exit_game); // TODO: FIX: smelly
            return_to_main_menu = false; // presumably exit this game and load the next film
        }
        else if (!(dynamic_world->game_information.game_type == _game_of_kill_monsters && dynamic_world->player_count == 1))
        {
            set_app_state(app_state_t::gather_network_game); // TODO: smells

            force_system_colors(false);
            display_net_game_stats();
        }
    }
    
    set_local_player_index(NONE);
    set_current_player_index(NONE);
    
    load_scenario_from_environment_preferences();
}



void clean_up_after_failed_game(bool inNetgame, bool inRecording, bool inFullCleanup)
{
    if (inRecording) { stop_recording(); }
    
    set_local_player_index(NONE);
    set_current_player_index(NONE);
    
#if !defined(DISABLE_NETWORKING)
    // The only time we don't clean up is on the replays
    if (inFullCleanup && inNetgame) { NetExit(); }
#endif // !defined(DISABLE_NETWORKING)
}









void handle_save_film()
{
    force_system_colors(false);
    show_cursor(); // JTP: Hidden by force_system_colors
    
    // EES: drop this here for now; can organize dialogs etter later
    // Get source file specification
    ao_path src_path = get_recording_path();
    
    if (!src_path.empty())
    {
        // Ask user for destination file
        ao_path dst_path = display_write_saved_film_dialog(); // TODO: level name and timecode would be better default name (does recording header contain this info?)
        if (!dst_path.empty())
        {
            ao_err err = rename_file(src_path, dst_path);
            if (err)
            {
                notify_user(STRID(strERRORS, fileError), "Filesystem error $err$", {
                    {"$code$", [err]{ return std::to_string(err); }},
                    //{"$text$", [code]{ return code.message(); }}, // TODO: find out what error strings are and put them into strings
                });
            }
        }
    }
    
    hide_cursor(); // JTP: Will be shown by display_main_menu
    display_main_menu();
}




/*
ao_err handle_open_replay(const ao_path& File)
{
    DraggedReplayFile = File;
    
    force_system_colors(true);
    ao_err err = create_new_game(_replay_from_file, false);
    if(err) display_main_menu(); // TODO: caller should do this when it gets an error back
    return err;
}
*/


/*
void handle_replay(bool last_replay)
{
    if(!last_replay) force_system_colors(true);
    ao_err err = create_new_game(_replay, !last_replay);
    if (err) display_main_menu();
}
*/

/*
ao_err handle_edit_map()
{
    force_system_colors(true);
    ao_err err = create_new_game(user_type_t::solo_player, false);
    if (err) display_main_menu();
    return err;
}
*/
