
#include "setup_game.hpp"

#include "gameworld_entrance.hpp"


// TODO: #includes are pretty entangled

//#include "mouse.h" // hide_cursor
//#include "joystick.h"

#include "shell_options.h" // shell_options.replay_directory

#include "preferences.hpp" // player_preferences
#include "player.h" // Player


#include "QuickSave.h" // get_last_saved_game_path
#include "choose_file_dialogs_os.hpp" // display_read_saved_film_dialog

#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialogs.h"

#include "app_state.hpp"
#include "network.h" // game_info

#include "vbl.h" // set_recording_header_data

#include "map.h" // player_start_data, dynamic_world
#include "map_wad.h"

#include "OpenALManager.h"
#include "FilmExporter.h"
#include "Music.h"

#include "lua_script.h" // run_lua_scripts

#include "Plugins.h"
#include "XML_LevelScript.h" // load_base_and_default_scripts
#include "motion_sensor.hpp" // reset_motion_sensor



player_identities_t player_identities; // was player_starts // TODO: get rid of this: player_identities should be stored in Player instances; when we need to pack them into saved game(?)/film, iterate players; see also replay.header.starts


//bool record_game = false;
short record_game_version = default_recording_version;
uint32_t original_map_file_checksum = 0;


// -----------------------------------------------------------------------------------------
// set player identities

// In this scheme, a "start" corresponds to a player available at the moment, who will
// participate in the game we're starting up.  In general when this code talks about a
// 'player', it is referring to an already-existing player in the game world (which should
// already be restored or initialized fairly well before these routines are used).
// Generalized game startup will match starts to existing players, set leftover players
// as "zombies" so they exist but just stand there, and create new players to correspond
// with any remaining starts.  As such it can handle both resume-game (some players already
// exist) and new-game (get_number_of_players() == 0) operations.
/*
 
void create_network_player_identities()
{
    
    player_identities.clear();
    // manky mess: there should be a single call which returns a vector
    int32_t count = NetGetNumberOfPlayers();
    for (int32_t i = 0; i < count; i++)
    {
        player_info* player = NetGetPlayerData(i);
        player_identities.push_back({NetGetPlayerIdentifier(i), player->team, player->color, player->name});
    }
}


// for film replay, the film header contains the original game's player identities, which are restored when film is loaded


// -----------------------------------------------------------------------------------------
// make restored game relevant
// called by join_resumed_coop_game, load_and_start_game, create_new_game


// so confused; if we [always] create players from player identities, they are already matched!

// matching players to marine monsters is a separate job


// EES: so much incoherent stupid... JJ's caffeine+deadline-fueled partial-chaos is fine and that's not the problem; it's the proud spaghettification subsequently inflicted on his messy but working codebase by LP &co absolutely which is. Best way forward is to figure out what this code is supposed to do, then do it right.
/*
// TODO: this is from network.cpp; there's a lot of similarity to synchronize_player_identities so figure out how to merge them, ideally merging identities fully into players
// This should be safe to use whether starting or resuming and whether single-player or multiplayer.
void match_starts_with_existing_players()
{
    int32_t startAssignedToPlayer[MAXIMUM_NUMBER_OF_PLAYERS];
    for (int32_t i = 0; i < MAXIMUM_NUMBER_OF_PLAYERS; i++)
    {
        startAssignedToPlayer[i] = NONE;
    }
    
    // First, match identities to players by name.
    for (int32_t identity_index = 0; identity_index < player_identities.size(); identity_index++)
    {
        for (int32_t player_index = 0; player_index < get_number_of_players(); player_index++) // TODO: player_count is only on dynamic_world for serialization; can we get rid of that and always use player_identities.size() or whatever's storing Player instances (TBH, identities should probably attach to those - their state is already duplicated there); so much crap should go away just with single source of truth
        {
            if (startAssignedToPlayer[player_index] == NONE)
            {
                if (player_identities[identity_index].name == get_player_data(player_index)->name) // just so weird
                {
                    startAssignedToPlayer[player_index] = identity_index;
                    break;
                }
            }
        }
    }

    // Match remaining identities to remaining players arbitrarily.
    for (int32_t identity_index = 0; identity_index < player_identities.size(); identity_index++)
    {
        if (startAssignedToPlayer[identity_index] == NONE)
        {
            for (int32_t player_index = 0; player_index < get_number_of_players(); player_index++)
            {
                if (startAssignedToPlayer[player_index] == NONE)
                {
                    startAssignedToPlayer[player_index] = identity_index;
                    break;
                }
            }
        }
    }
    
    // Create new identities for any players not covered.
    int32_t player_index = 0;
    while (player_identities.size() < get_number_of_players())
    {
        if (startAssignedToPlayer[player_index] == NONE)
        {
            // probably wrong
            startAssignedToPlayer[player_index] = (int32_t)player_identities.size();
            Player* thePlayer = get_player_data(player_index);
            player_identities.push_back({NONE, thePlayer->team, thePlayer->color, thePlayer->name}); // TODO: not a fan of using NONE
        }
        player_index++;
    }

    // Assign remaining starts to players that don't exist yet
    player_index = get_number_of_players();
    for (int32_t identity_index = 0; identity_index < player_identities.size(); identity_index++)
    {
        if (startAssignedToPlayer[identity_index] == NONE)
        {
            startAssignedToPlayer[player_index] = identity_index;
            player_index++;
        }
    }

    // Reorder starts to match players - this is particularly unclever
    std::vector<player_identity_t> identities = player_identities;
    for (int32_t player_index = 0; player_index < player_identities.size(); player_index++)
    {
        player_identities[player_index] = identities[startAssignedToPlayer[player_index]];
    }
}



// This should be safe to use whether starting or resuming, and whether single- or multiplayer.
static void synchronize_player_identities(int16_t local_player_index) // why is local_player_index passed here instead of being on dynamic_data or whatever
{
    assert_fail(local_player_index >= 0 && local_player_index < player_identities.size(), "");
    assert_fail(get_number_of_players() <= player_identities.size(), ""); // TODO: smelly; what's not clear is
    
    int32_t player_index = 0;
    
    // First we process existing players
    for (; player_index < get_number_of_players(); player_index++)
    {
        player_identity_t& identity = player_identities[player_index];
        Player* thePlayer = get_player_data(player_index);
        
        if (player_identities[player_index].identifier == NONE)
        {
            // No start (live player) to go with this player (stored player)
            SET_PLAYER_ZOMBIE_STATUS(thePlayer, true);
        }
        else
        {
            // Update player's appearance to match the start
            SET_PLAYER_DOESNT_AUTO_SWITCH_WEAPONS_STATUS(thePlayer, false); // identity.get_doesnt_auto_switch_weapons()
            thePlayer->team = identity.team;
            thePlayer->color = identity.color;
            thePlayer->identifier = identity.identifier;
            thePlayer->name = identity.name;

            // Make sure if player was saved as zombie, they're not now.
            SET_PLAYER_ZOMBIE_STATUS(thePlayer, false);
        }
    }
    
    // If there are any identities left, we need new players for them // why? why is get_number_of_players() not the same? this muddled mess has no concept of single source of truth
    for (; player_index < player_identities.size(); player_index++)
    {
        create_player(player_identities[player_index]);
    }
    
    set_local_player_index(local_player_index);
    set_current_player_index(local_player_index);
}
*/

// -----------------------------------------------------------------------------------------
// next bit of mess; it is shrinking

// The single-player machine, gatherer, and joiners all will use this routine.  It should take most of its
// cues from the "extras" that load_level_from_saved_game_file() does.
static void make_restored_game_relevant(bool inNetgame)
{
    run_lua_scripts(); // called all over the fucking place
    
}


void synchronize_restored_solo_player()
{
    dynamic_world.game_information.difficulty_level = player_preferences.difficulty_level;
    set_custom_behaviors_enabled(true);
    assert_fail(player_identities.size() == 1, "");
    //synchronize_player_identities(0);
}


void synchronize_restored_coop_players()
{
    game_info *network_game_info = NetGetGameData();
    
    // only value which isn't being directly set is level_number (which IIRC I added to game_configuration_t struct)
    dynamic_world.game_information.game_time_remaining = network_game_info->time_limit;
    dynamic_world.game_information.kill_limit          = network_game_info->kill_limit;
    dynamic_world.game_information.game_type           = network_game_info->net_game_type;
    dynamic_world.game_information.game_options        = network_game_info->game_options;
    dynamic_world.game_information.initial_random_seed = network_game_info->initial_random_seed;
    dynamic_world.game_information.difficulty_level    = network_game_info->difficulty_level;
    dynamic_world.game_information.cheat_flags         = network_game_info->cheat_flags;
    
    // ZZZ: until players specify their behavior modifiers over the network, to avoid out-of-sync we must force them all the same.
    set_custom_behaviors_enabled(false);
    
    int16_t theLocalPlayerIndex = NetGetLocalPlayerIndex(); // idiocy
    assert_fail(theLocalPlayerIndex != NONE, "");
    //synchronize_player_identities(theLocalPlayerIndex); // I mean, this assigns local_player_index and current_player_index
}


// -----------------------------------------------------------------------------------------
// TODO: consolidate these and move back to map_wad.cpp



// a saved co-op game distributed over network or a film file's embedded level data
ao_err load_saved_game_from_flat_data(uint8_t* saved_flat_data)
{
    assert_fail(saved_flat_data, "");

    wad_header_t header;
    wad_data* wad = inflate_flat_data(saved_flat_data, &header); // this takes ownership of the flat data
    assert_fail(wad, "inflate_flat_data should never fail");
    
    dynamic_world_t dynamic_data;
    ao_err err = get_dynamic_data_from_wad(wad, dynamic_data);
    if (err)
    {
        free_wad(wad);
        return err;
    }
    
    // set_random_seed() needs to happen before synchronize_player_identities()
    // since the latter calls create_player() which almost certainly uses global_random().
    // Note we always take the random seed directly from the dynamic_world, no need to screw around
    // with copying it from game_information or the like.
    set_random_seed(dynamic_data.random_seed);

    // Find the original Map file from which this game was saved so we can run its level scripts.
    err = set_current_map_path_to_file_with_checksum(header.original_map_file_checksum);
    if (!err)
    {
        // TODO: seems odd to run scripts before the level is loaded (there may be some reason for it; the initialize_level_from_wad_data function has its own entanglements midway through); if we could push all script-related calls after initialize_level_from_wad_data below, we could extract them to their own function
        load_base_and_default_scripts(dynamic_data.current_level_number);
        //
        initialize_level_from_wad_data(wad, true, header.data_version);
    }
    free_wad(wad); // (the wad takes ownership of the flat data, so this disposes both)
    
    return err;
}



// -----------------------------------------------------------------------------------------

// temporary; currently isn't called anywhere cos we're still disentangling startup
void setup_film_recording_for_new_game(int16_t level_number, bool is_saved_game)
{
    // only if we're recording
    set_recording_header_data(dynamic_world.current_level_number, get_current_map_checksum(),
                              default_recording_version, player_identities, dynamic_world.game_information);
    
    // the map data is added to film file (this should only be necessary when starting from a saved game, otherwise we only need the Map file's UID, which currently is calculated checksum)
    uint8_t* flat_data = nullptr;
    ao_err err = get_flat_data_from_wad_file(get_current_map_path(), level_number, flat_data);
    if (err) return; // TODO: error reporting
    set_recording_saved_wad_data(flat_data); // horrible; we should be opening the current film recording file and writing header and map to it first; however, since the goal is to overhaul file formats it may be best to leave as-is until fully ready to tug on that thread
}


// -----------------------------------------------------------------------------------------

// This is called when the game level is changed somehow
// The only thing that has to be valid in the entry point is the level_index
ao_err load_level(int16_t level_number, bool is_new_game)
{
    ao_err err = no_err;
            
    load_base_and_default_scripts(level_number); // eep; I mean, it's not really safe to share scripts over network as levels are shared, so running the scripts from local scenario makes sense, but it's lousy architecture and needs to change if netgame customizations are ever to work properly
    
    
    if (get_user_type() == user_type_t::pvp)
    {
        err = NetChangeMap(level_number); // TODO: pretty sure we can sack this off in favor of everyone reading their local Map file - we'll need that anyway if scripts are ever to be fully supported in netgames; the only time a level wad needs to be sent over network is when it's a saved game file in a coop game; when entering a new level, only thing that needs sent is map uuid and level number, which will ensure everyone's Map file is the same; anyway, can't do that till file formats are upgraded so leave this for now. It would be good if this code and code for loading saved game could be consolidated though.
    }
    else
    {
        err = load_level_from_map_wad_file(get_current_map_path(), level_number, false);
    }
    if (err) return err;
    
    
    if (is_new_game)
    {
        initialize_network_players();
    }
    else
    {
        bind_current_players_to_level();
    }
    
    
    // ghs: this runs very early now: we want to be before initialize_items_and_monsters // whyyyyy? why can't it load de dam map, then run de dam scripts?
    run_lua_scripts();
    
    initialize_object_placements();
    initialize_control_panels();
    
    return err;
}





ao_err load_level_from_saved_game_file(const ao_path& saved_game_path) // TODO: should consolidate load_saved_game_from_flat_data
{
    // Find the original scenario this saved game was a part of
    ao_err err = set_current_map_path_to_file_with_checksum(read_wad_file_parent_checksum(saved_game_path));
    if (err) { return err; } // The original Map file wasn't found. The original M2 behavior was to continue playing the saved game file, then fail when exiting the level, but this is the right time to bail.
    
    // get the original Map file's level number from the saved game file, then look up and run the Map file's level script for that level
    
    // this is annoying but level scripts can interact with map loading so, for now, we're stuck with it here; at least it should clean up a bit more with WADFile + WAD classes
    // TODO: how do the scripts know their state at the point the game was saved? see SavedLuaState; iirc Lua script state is loaded/saved as part of the saved game file's map wad
    dynamic_world_t dynamic_data;
    err = get_dynamic_data_from_saved_game_file(saved_game_path, dynamic_data);
    if (err) return err;
    
    // Load the level from the saved game file
    load_base_and_default_scripts(dynamic_data.current_level_number);
    err = load_level_from_map_wad_file(saved_game_path, 0, true);
    
    return err;
}



ao_err revert_game()
{
    assert_fail(get_number_of_players() == 1, ""); // so what about co-op games?
    
    ao_err err = no_err;
    
    reset_recording();
    
    const ao_path saved_game_path = get_current_saved_game_path();
    
    if (saved_game_path.empty()) // Reload their last saved game
    {
        //  game_information = revert_game_data.game_information; // TODO: FIX
          //err = new_game(get_initial_level_number(), 1); // number of players is obviously 1
          if (err) return err;
    }
    else // user started a new game and died before their first pattern buffer!
    {
        err = load_level_from_saved_game_file(saved_game_path);
        if (err) return err;
        
        // there are other scripts; see enter_game state in main_event_loop
        run_lua_scripts();
        
        enter_gameworld(true);
    }
    
    return err;
}





// -----------------------------------------------------------------------------------------



static void configure_new_network_game(game_info* info)
{
    /*
    //record_game  = true;
    
    // ZZZ: until players specify their behavior modifiers over the network, to avoid out-of-sync we must force them all the same.
    //set_custom_behaviors_enabled(false);
    
    //set_user_type(user_type_t::pvp);
    create_network_player_identities();
    
    // despite the name, `game_information` is actually a `game_configuration_t` struct
    // TODO: game_configuration_t and game_info structs are virtually identical, so why are they not one?
    initial_game_information.level_number_gameinfo           = info->level_number;
    initial_game_information.game_time_remaining    = info->time_limit;
    initial_game_information.kill_limit             = info->kill_limit;
    initial_game_information.game_type              = info->net_game_type;
    initial_game_information.game_options           = info->game_options;
    initial_game_information.initial_random_seed    = info->initial_random_seed;
    initial_game_information.difficulty_level       = info->difficulty_level;
    initial_game_information.cheat_flags            = info->cheat_flags;
    
    //std::fill_n(game_information.parameters, 2, 0);
    
    original_map_file_checksum = info->original_map_file_checksum; // weird
    
    load_film_profile(FILM_PROFILE_DEFAULT);
     */
}


void configure_new_coop_game()
{
    
    game_info* info = NetGetGameData(); // this gets level number
    configure_new_network_game(info);
}


void configure_new_pvp_game()
{
    game_info* info = NetGetGameData(); // this gets level number
    configure_new_network_game(info);

}


ao_err configure_replay_game()
{
    //record_game = false;
    
    int16_t level_number; // where to put this?
    uint32_t map_checksum; // TODO: unused here, which seems wrong; it's probably original_map_file_checksum
    int16_t recording_version; // used in following compatibility check
    get_recording_header_data(level_number, map_checksum, recording_version, player_identities, get_game_configuration());
    
    if (recording_version > max_handled_recording)
    {
        //stop_replay(); // why would it be started?
        return STRID(strERRORS, replayVersionTooNew);
    }
    
    load_film_profile_for_recording_version(recording_version);
    
    get_game_configuration().game_options |= _overhead_map_is_omniscient; // not sure why, seems wrong (automap visibility is stored in saved game wad)
    // ZZZ: until films store behavior modifiers, we must require that they record and playback only with standard modifiers.
    set_custom_behaviors_enabled(false);
    return no_err;
}




ao_err OLD_create_new_game(int16_t level_number)
{
    ao_err err = no_err;
    /*
    if (game_is_live())
    {
        // TODO: seems wrong; if there's no Map file we shouldn't get this far (things get a little tricky with film files, but best solution is to include their original scenario configuration - Map, Shapes, Sounds, etc; scripts and other customizations; i.e. embedding the level doesn't really cut it for solo/coop films as there may be level jumps)
        if(!std::filesystem::is_regular_file(get_scenario_map_path()))
        {
            return STRID(strERRORS, missingFile); // error codes should be more specific, e.g. mapFileNotFound
        }
        
        //
        uint32_t map_checksum = (get_user_type() == user_type_t::pvp) ? original_map_file_checksum : get_current_map_checksum();
        set_recording_header_data(number_of_players, level_number, map_checksum, record_game_version, player_identities, &game_information);
        start_recording(); // should be called last, immediately before entering gameworld
    }
    
    //hide_cursor();
    // This has already been done to get to gather/join
    //if (can_interface_fade_out()) { animate_interface_fade_out(true); } // TODO: move to main_event_loop
    
    
    // this smells
    if (is_saved_game_replay())
    {
        //make_restored_game_relevant(false);
        //run_lua_scripts();
    }
    else
    {
        //err = new_game(level_number, player_identities.size());
    }
    
    
    
    if (err) // ick; cleaning up game state after normal/failed game should likely be the next app state transition
    {
        //if (record_game) { stop_recording(); }
        if (game_is_networked()) { NetExit(); }
    }
     */
    return err;
}



// this looks like a superset of exit_gameworld functionality

void finish_game()
{
    // TODO: how does this differ from exit_gameworld?
    
    
    //assert_fail(get_app_state() == app_state_t::game_in_progress        ||
    //            get_app_state() == app_state_t::load_and_play_demo_film ||
    //            get_app_state() == app_state_t::revert_to_saved_game    ||
    //            get_app_state() == app_state_t::change_level            ||
    //            get_app_state() == app_state_t::epilogue_screen, "");

    /*
    if (shell_options.editor && shell_options.should_output_to_file())
    {
        //L_Call_Cleanup();
        ao_path file = shell_options.output_path;
        ao_err err = export_level(file);
        exit(err);
    }
     */
    
     // TODO: should some/all/none of this crap move to exit_gameworld?
    // Fade out! (Pray)
    //Music::instance()->ClearLevelPlaylist();
    //Music::instance()->QuickFade();
    /*
    animate_interface_fade_out();
     */
    
    // Get as much memory back as we can. // TODO: NO, it's not 1995! Scenario gets fully loaded when selected, stays fully loaded until a different scenario is selected/process exits.
    //unload_all_collections();
    //sound_manager.UnloadAllSounds();
    
    /*
    switch (get_user_type())
    {
        case user_type_t::solo:
            break;
            
        case user_type_t::coop:
        case user_type_t::pvp:
            NetUnSync(); // gracefully exit from the game
            
            set_app_state(app_state_t::gather_network_game); // TODO: smells

            change_screen_mode(_screentype_menu);
             animate_interface_fade_out(false);
            display_net_game_stats();
            NetExit();
            break;
            
        case user_type_t::replay:
            
            if (!shell_options.replay_directory.empty())
            {
                set_app_state(app_state_t::exit_game); // TODO: FIX: smelly
                //return_to_main_menu = false; // presumably exit this game and load the next film
            }
            else if (!(dynamic_world.game_information.game_type == _game_of_kill_monsters && get_number_of_players() == 1))
            {
                set_app_state(app_state_t::gather_network_game); // TODO: smells

                 animate_interface_fade_out(false);
                display_net_game_stats();
            }
             
            break;
    }
    */
    //set_local_player_index(NONE);
    //set_current_player_index(NONE);
    
    load_scenario_from_environment_preferences();
}


