

#include "game_event_loop.hpp"
//#include "game_window.h" 

#include "map.h" // player_start_data, dynamic_world
#include "map_wad.h"
#include "preferences.h" // player_preferences
#include "player.h" // player_data
#include "motion_sensor.hpp" // reset_motion_sensor
#include "Plugins.h"
#include "XML_LevelScript.h" // ResetLevelScript
#include "mouse.h" // hide_cursor
#include "lua_hud_script.h" // LoadHUDLua
#include "vbl.h" // set_recording_header_data
#include "QuickSave.h" // get_last_saved_game_path
#include "choose_file_dialogs_os.hpp" // show_read_saved_film_dialog
#include "OpenALManager.h"
#include "Movie.h"
#include "Music.h"
#include "shell_options.h"
#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialogs.h"

#include "image_blitter.hpp"



static ao_path DraggedReplayFile;


#define CLOSE_WITHOUT_WARNING_DELAY (5 * TICKS_PER_SECOND)



void pause_game()
{
    set_keyboard_controller_status(false);
    show_cursor();
    if (!game_is_networked && OpenALManager::Get()) OpenALManager::Get()->Pause(true);
}


void resume_game()
{
    hide_cursor();
    if (ogl_is_active()) { alephone::Screen::instance()->bound_screen(true); }
    validate_world_window();
    set_keyboard_controller_status(true);
    if (OpenALManager::Get()) OpenALManager::Get()->Pause(false);
}



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

static void construct_single_player_start(player_start_data* outStartArray, short* outStartCount)
{
        if(outStartCount != NULL)
        {
                *outStartCount = 1;
        }

        outStartArray[0].team = player_preferences->color;
        outStartArray[0].color = player_preferences->color;
        outStartArray[0].identifier = 0;
        outStartArray[0].name = player_preferences->name;
                
        set_player_start_doesnt_auto_switch_weapons_status(&outStartArray[0], dont_switch_to_new_weapon());
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

static short find_start_for_identifier(const player_start_data* inStartArray, short inStartCount, short _inIdentifier)
{
        short inIdentifier= player_identifier_value(_inIdentifier);
        short s;
        for(s = 0; s < inStartCount; s++)
        {
                if(player_identifier_value(inStartArray[s].identifier) == inIdentifier)
                {
                        break;
                }
        }
        
        return (s == inStartCount) ? NONE : s;
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
    // Much of the code in this if()...else is very similar to code in begin_game(), should probably try to share.
    if(inNetgame)
    {
        game_info *network_game_info= (game_info *)NetGetGameData();
        
        dynamic_world->game_information.game_time_remaining= network_game_info->time_limit;
        dynamic_world->game_information.kill_limit= network_game_info->kill_limit;
        dynamic_world->game_information.game_type= network_game_info->net_game_type;
        dynamic_world->game_information.game_options= network_game_info->game_options;
        dynamic_world->game_information.initial_random_seed= network_game_info->initial_random_seed;
        dynamic_world->game_information.difficulty_level= network_game_info->difficulty_level;
        dynamic_world->game_information.cheat_flags= network_game_info->cheat_flags;
        
        // ZZZ: until players specify their behavior modifiers over the network,
        // to avoid out-of-sync we must force them all the same.
        standardize_player_behavior_modifiers();
        
        theLocalPlayerIndex = NetGetLocalPlayerIndex();
    }
    else
#endif // !defined(DISABLE_NETWORKING)
    {
        dynamic_world->game_information.difficulty_level= player_preferences->difficulty_level;
        restore_custom_player_behavior_modifiers();
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
    construct_multiplayer_starts(theStarts, &theStartCount);
    make_restored_game_relevant(true, theStarts, theStartCount);
    
    set_recording_header_data(theStartCount, dynamic_world->current_level_number, ((game_info*)NetGetGameData())->parent_checksum,
                              default_recording_version, theStarts, &dynamic_world->game_information);

    set_recording_saved_wad_data(saved_wad_data);
    start_recording();

    start_game(_network_player, false);
    
    return err;
}
#endif // !defined(DISABLE_NETWORKING)



// TODO: sort this mess out another time
static bool saved_game_was_networked(const ao_path& saved_game)
{
    
    return (saved_game == get_last_saved_game_path()) ? get_last_saved_game_was_multiplayer() : false;
}



// Returns false if user cancels.
// Game has been loaded from file before this is called so elements like
// dynamic_world->player_count are available.  Cursor has been hidden when called.
static bool show_restore_network_game_dialog(const ao_path& file, short& player_mode)
{
    dialog d;

    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("RESUME GAME"), d);
    placer->add(new w_spacer, true);
    
    horizontal_placer *resume_as_placer = new horizontal_placer;
    w_toggle* theRestoreAsNetgameToggle = new w_toggle(dynamic_world->player_count > 1);
    theRestoreAsNetgameToggle->load_labels(kSingleOrNetworkStringSetID);
    resume_as_placer->dual_add(theRestoreAsNetgameToggle->adding_label("Resume as"), d);
    resume_as_placer->dual_add(theRestoreAsNetgameToggle, d);

    placer->add(resume_as_placer, true);
    
    placer->add(new w_spacer(), true);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("RESUME", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

    placer->add(button_placer, true);


    d.set_widget_placer(placer);
    
    if(d.run() == 0)
    {
        player_mode = theRestoreAsNetgameToggle->get_selection() ? _network_player : _single_player;
        return true;
    }
    else
    {
        return false;
    }
}






// ZZZ: changes to use generalized game startup support
// This will be used only on the machine that picked "Continue Saved Game".
ao_err load_and_start_game(const ao_path& File)
{
    hide_cursor();
    if (can_interface_fade_out()) { animate_ui_fade_out_blocking(true); }

    auto pluginMode = saved_game_was_networked(File) == 1 ? Plugins::kMode_Net : Plugins::kMode_Solo;
    
    //std::unique_ptr<byte, decltype(&free)> theSavedGameFlatData(nullptr, free);
    uint8_t* flat_data = nullptr;
    int32_t flat_data_length = 0;

    Plugins::instance()->set_mode(pluginMode);
    
    ao_err err = load_game_from_file(File, false);

    if (err) goto error;

    game_state.user = _single_player;

    if (File == get_last_saved_game_path() && get_last_saved_game_was_multiplayer())
    {
        if (!show_restore_network_game_dialog(File, game_state.user)) goto error; // user canceled dialog
    }
    
    err = get_flat_data(File, 0, flat_data);
    if (err)
    {
        show_cursor();
        return err;
    }
    flat_data_length = get_flat_data_length(flat_data);
    assert_fail(flat_data_length > 0, "");
    
    // TODO: this flow is awful hokey, check against original
#if !defined(DISABLE_NETWORKING)
    if (game_state.user == _network_player)
    {
        set_game_state(_displaying_network_game_dialogs);

        // we don't know at this point if we are going to use a remote hub but this must be set if we do
        NetSetResumedGameWadForRemoteHub(flat_data, flat_data_length);

        bool use_remote_hub;
        show_cursor();
        err = show_network_gather_dialog(true, use_remote_hub);
        hide_cursor();
        if (!err && use_remote_hub) { err = join_networked_resume_game(); }
        
        free(flat_data);
    }
    else // TODO: if the above conditional and `return` is correct, there is conditional code below that will always/never execute
#endif // !defined(DISABLE_NETWORKING)
    {
        Crosshairs_SetActive(player_preferences->crosshairs_active);
        LoadHUDLua();
        
        // load the scripts we put off before
        if (game_state.user == _single_player)
        {
            LoadSoloLua();
        }
        LoadAchievementsLua();
        LoadStatsLua();
        
        player_start_data theStarts[MAXIMUM_NUMBER_OF_PLAYERS];
        short theNumberOfStarts;
        
#if !defined(DISABLE_NETWORKING)
        if (game_state.user == _network_player)
        {
            construct_multiplayer_starts(theStarts, &theNumberOfStarts);
        }
        else
#endif // !defined(DISABLE_NETWORKING)
        {
            construct_single_player_start(theStarts, &theNumberOfStarts);
        }
        
        match_starts_with_existing_players(theStarts, &theNumberOfStarts);
        
#if !defined(DISABLE_NETWORKING)
        if (game_state.user == _network_player)
        {
            NetSetupTopologyFromStarts(theStarts, theNumberOfStarts);
            NetStart();
            
            err = NetDistributeGameDataToAllPlayers(flat_data, flat_data_length, false /* do_physics? */);
            if (err) goto error;
        }
#endif // !defined(DISABLE_NETWORKING)
        
        make_restored_game_relevant(game_state.user == _network_player, theStarts, theNumberOfStarts);
        
        set_recording_header_data(theNumberOfStarts, dynamic_world->current_level_number,
                                  game_state.user == _network_player ? ((game_info*)NetGetGameData())->parent_checksum : get_current_map_checksum(),
                                  default_recording_version, theStarts, &dynamic_world->game_information);
        
        std::vector<uint8_t> saved_wad_data(flat_data, flat_data + flat_data_length);
        set_recording_saved_wad_data(saved_wad_data); // TODO: this appears to do copy of the vector; why not just pass flat_data directly and let replay_private_data struct take ownership of it?
        start_recording();
        
        start_game(game_state.user, false);
    }
    return err;
    
error:
    /* Reset the system colors, since the screen clut is all black.. */
    force_system_colors(false); // not sure why this is only done here
    show_cursor(); // JTP: Was hidden by force system colors
    display_loading_map_error(err); // TODO: move to caller
    return err;
}







ao_err begin_game(short user, bool cheat)
{
    struct entry_point entry;
    struct player_start_data starts[MAXIMUM_NUMBER_OF_PLAYERS];
    struct game_data game_information;
    short number_of_players;
    bool is_networked= false;
    bool clean_up_on_failure= true;
    bool record_game= false;
    short record_game_version = default_recording_version;
    uint32 parent_checksum = 0;
    
    ao_err err = no_err;
    
    objlist_clear(starts, MAXIMUM_NUMBER_OF_PLAYERS);

    game_state.user = user;
    
    switch (user)
    {
        case _network_player:
#if !defined(DISABLE_NETWORKING)
            {
                game_info *network_game_info= (game_info *)NetGetGameData();

                construct_multiplayer_starts(starts, &number_of_players);

                game_information.game_time_remaining= network_game_info->time_limit;
                game_information.kill_limit= network_game_info->kill_limit;
                game_information.game_type= network_game_info->net_game_type;
                game_information.game_options= network_game_info->game_options;
                game_information.initial_random_seed= network_game_info->initial_random_seed;
                game_information.difficulty_level= network_game_info->difficulty_level;
                parent_checksum = network_game_info->parent_checksum;
                entry.level_number = network_game_info->level_number;
                entry.utf8_level_name[0] = 0;
    
                game_information.cheat_flags = network_game_info->cheat_flags;
                std::fill_n(game_information.parameters, 2, 0);

                is_networked= true;
                record_game= true;
                // ZZZ: until players specify their behavior modifiers over the network,
                // to avoid out-of-sync we must force them all the same.
                standardize_player_behavior_modifiers();

                load_film_profile(FILM_PROFILE_DEFAULT);
            }
#endif // !defined(DISABLE_NETWORKING)
            break;

        case _replay_from_file:
        case _replay:
        case _demo:
        {
            switch (user)
            {
                case _replay_from_file:
                    err = setup_for_replay_from_file(DraggedReplayFile, get_current_map_checksum());
                    user = _replay;
                    break;
                    
                case _replay:
                {
                    show_cursor(); // JTP: Hidden one way or another :p
                    SDL_Keymod m = SDL_GetModState();
#ifdef __MACOSX__
                    bool prompt_to_export = (m & KMOD_ALT);
#else
                    bool prompt_to_export = (m & KMOD_ALT) || (m & KMOD_GUI); // TODO: any reason (other than tradition) we don't accept both keys on macOS?
#endif
                    ao_path film_file = cheat ? show_read_saved_film_dialog() : get_recording_path();
                    if (!film_file.empty() && std::filesystem::is_regular_file(film_file)) // TODO: I'm guessing the first test is redundant; check and remove
                    {
                        if (!std::filesystem::is_regular_file(get_default_map_path()))
                        {
                            return STRID(strERRORS, missingFile);
                        }
                        
                        ao_err err = setup_for_replay_from_file(film_file, get_current_map_checksum(), prompt_to_export);
                        if (err) return err;
                        
                        hide_cursor();
                    }
                    else
                    {
                        return errUserCancelled;
                    }
                }
                    break;
                    
                case _demo:
                    err = setup_replay_from_random_resource();
                    break;
                    
                default:
                    throw_bug_report("invalid user type: %d", user);
                    break;
            }
            
            if (!err)
            {
                uint32 unused1;
                short recording_version;
                
                get_recording_header_data(&number_of_players,
                                          &entry.level_number, &unused1, &recording_version,
                                          starts, &game_information);
                
                if (recording_version > max_handled_recording)
                {
                    stop_replay();
                    err = STRID(strERRORS, replayVersionTooNew);
                }
                else
                {
                    load_film_profile_for_recording_version(recording_version);
                    
                    entry.utf8_level_name[0] = 0;
                    game_information.game_options |= _overhead_map_is_omniscient;
                    record_game = false;
                    // ZZZ: until films store behavior modifiers, we must require
                    // that they record and playback only with standard modifiers.
                    standardize_player_behavior_modifiers();
                }
            }
            break;
        }
            
        case _single_player:
            if (cheat) // vidmaster level jump dialog
            {
                bool success = show_vidmaster_dialog(entry.level_number);
                if (!success) err = errUserCancelled;
                clear_screen();
                // TODO: confirm we either enter game or return to main menu
            }
            else
            {
                entry.level_number = 0;
            }
    
            // ZZZ: let the user use his behavior modifiers in single-player.
            restore_custom_player_behavior_modifiers();
            
            entry.utf8_level_name.clear(); // TODO: needed?
            starts[0].identifier = 0;

            construct_single_player_start(starts, &number_of_players);

            game_information.game_time_remaining= INT32_MAX;
            game_information.kill_limit = 0;
            game_information.game_type= _game_of_kill_monsters;
            game_information.game_options= _burn_items_on_death|_ammo_replenishes|_weapons_replenish|_monsters_replenish;
            game_information.initial_random_seed= machine_tick_count();
            game_information.difficulty_level= player_preferences->difficulty_level;
            std::fill_n(game_information.parameters, 2, 0);
                
                        // ZZZ: until film files store player behavior flags, we must require
                        // that all films recorded be made with standard behavior.
            record_game= is_player_behavior_standard();

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
            
            break;
            
        default:
            throw_bug_report("invalid user type: %d", user);
            break;
    }

    if (!err)
    {
        if (record_game)
        {
            // TODO: seems wrong
            if(!std::filesystem::is_regular_file(get_default_map_path()))
            {
                err = STRID(strERRORS, missingFile);
                display_loading_map_error(err); // TODO: move up
            }
            else
            {
                set_recording_header_data(number_of_players, entry.level_number, (user == _network_player) ? parent_checksum : get_current_map_checksum(),
                    record_game_version, starts, &game_information);
                start_recording();
            }
        }
    }
    if (!err)
    {
        hide_cursor();
        // This has already been done to get to gather/join
        if(can_interface_fade_out()) { animate_ui_fade_out_blocking(true); }
        
        // Try to display the first chapter screen
        if (user != _network_player && user != _demo && !is_saved_game_replay())
        {
            FindLevelMovie(entry.level_number);
            show_movie(entry.level_number);
            try_and_display_chapter_screen(entry.level_number, false, false);
        }
        
        Plugins::instance()->set_mode(number_of_players > 1 ? Plugins::kMode_Net : Plugins::kMode_Solo);
        Crosshairs_SetActive(player_preferences->crosshairs_active);
        LoadHUDLua();
        
        if (is_saved_game_replay())
        {
            make_restored_game_relevant(false, starts, number_of_players);
        }
        else
        {
            err = new_game(number_of_players, is_networked, &game_information, starts, &entry);
        }
    }
    if (!err)
    {
        start_game(user, false);
    }
    else
    {
        if (record_game) { stop_recording(); }
        set_local_player_index(NONE);
        set_current_player_index(NONE);
        // The only time we don't clean up is on the replays
        if (user == _network_player && clean_up_on_failure) { NetExit(); }
        
        /*
         TODO:
         show_cursor();
         display_loading_map_error(err);
         display_main_menu();
         */
    }
    
    return err;
}


void start_game(short user, bool changing_level)
{
    
    // LP change: reset screen so that extravision will not be persistent
    reset_screen();
    
    enter_screen();
    if (!changing_level)
        L_Call_HUDInit();
    
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

    // ZZZ: If it's a netgame, we want prediction; else no.
    set_prediction_wanted(user == _network_player);

    game_state.state= _game_in_progress;
    game_state.current_screen= 0;
    game_state.ticks_until_next_screen = MACHINE_TICKS_PER_SECOND;
    game_state.last_ticks_on_idle= machine_tick_count();
    game_state.user= user;
    game_state.flags= 0;

    assert_fail((!changing_level&&!get_keyboard_controller_status()) || (changing_level && get_keyboard_controller_status()), "");
    if(!changing_level)
    {
        set_keyboard_controller_status(true);
    }

    SoundManager::instance()->UpdateListener();
}





void finish_game(bool return_to_main_menu)
{
    set_keyboard_controller_status(false);

#ifdef PERFORMANCE
    PerfControl(perf_globals, false);
#endif
    /* Note that we have to deal with the switch demo state later because */
    /* Alain's code calls us at interrupt level 1. (so we defer it) */
    assert_fail(game_state.state==_game_in_progress || game_state.state==_switch_demo || game_state.state==_revert_game
                || game_state.state==_change_level || game_state.state==_begin_display_of_epilogue, "");

    stop_fade();
    set_fade_effect(NONE);
    L_Call_HUDCleanup();
    exit_screen();

    /* Stop the replay */
    switch(game_state.user)
    {
        case _single_player:
        case _network_player:
            stop_recording();
            break;
            
        case _demo:
        case _replay:
            stop_replay();
            break;

        default:
            throw_bug_report("invalid user type: %d", game_state.user);
            break;
    }
    Movie::instance()->StopRecording();

    if (shell_options.editor && shell_options.should_output_to_file())
    {
        L_Call_Cleanup();
        ao_path file = shell_options.output_path;
        ao_err err = export_level(file);
        exit(err);
    }

    /* Fade out! (Pray) */ // should be interface_color_table for valkyrie, but doesn't work.
    Music::instance()->ClearLevelPlaylist();
    Music::instance()->Fade(0, MACHINE_TICKS_PER_SECOND / 2, MusicPlayer::FadeType::Sinusoidal);
    animate_ui_fade_blocking(_cinematic_fade_out, interface_color_table);
    clear_screen();
    animate_ui_fade_blocking(_end_cinematic_fade_out, interface_color_table);

    show_cursor();

    leaving_map();
    CloseLuaHUDScript();
    
    /* Get as much memory back as we can. */
    unload_all_collections();
    SoundManager::instance()->UnloadAllSounds();
    
#if !defined(DISABLE_NETWORKING)
    if (game_state.user==_network_player)
    {
        NetUnSync(); // gracefully exit from the game

        /* Don't update the screen, etc.. */
        game_state.state= _displaying_network_game_dialogs;

        change_screen_mode(_screentype_menu);
        force_system_colors(false);
        display_net_game_stats();
        NetExit();
    }
    else
#endif // !defined(DISABLE_NETWORKING)

    if (game_state.user == _replay)
    {
        if (!shell_options.replay_directory.empty())
        {
            game_state.state = _quit_game;
            return_to_main_menu = false;
        }
        else if (!(dynamic_world->game_information.game_type == _game_of_kill_monsters && dynamic_world->player_count == 1))
        {
            game_state.state = _displaying_network_game_dialogs;

            force_system_colors(false);
            display_net_game_stats();
        }
    }
    
    set_local_player_index(NONE);
    set_current_player_index(NONE);
    
    load_scenario_from_environment_preferences();
    if ((game_state.user == _replay && shell_options.replay_directory.empty()) || game_state.user == _demo)
    {
        Plugins::instance()->set_mode(Plugins::kMode_Menu);
    }
    if (return_to_main_menu) display_main_menu();
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




// dumping handle_ functions here for now

void handle_network_game(bool gatherer) // called from main loop
{
    ao_err err = no_err;
    
#if !defined(DISABLE_NETWORKING)
    bool joined_resume_game = false;

    force_system_colors(true);

    // Don't update the screen, etc.
    game_state.state= _displaying_network_game_dialogs;
    game_state.user = _network_player;
    
    show_cursor();
    if (gatherer)
    {
        bool use_remote_hub;
        err = show_network_gather_dialog(false, use_remote_hub);
        if (!err && !use_remote_hub) NetStart();
    }
    else
    {
        err = show_network_join_dialog(joined_resume_game);
    }
    hide_cursor();
    
    if (!err)
    {
        if (joined_resume_game)
        {
            err = join_networked_resume_game();
            if (err)
            {
                NetExit();
            }
        }
        else
        {
            err = begin_game(_network_player, false);
        }
    }
    if (err)
    {
        // pulled these out of clean_up_after_failed_game
        set_local_player_index(NONE);
        set_current_player_index(NONE);
        
        // We must restore the colors on cancel.
        show_cursor();
        display_loading_map_error(err);
        display_main_menu();

    }
#else // !defined(DISABLE_NETWORKING)
    notify_user(STRID(strERRORS, networkNotSupportedForDemo));
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
        ao_path dst_path = show_write_saved_film_dialog(); // TODO: level name and timecode would be better default name (does recording header contain this info?)
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



void handle_load_game()
{
    bool success = false;
    
    force_system_colors(false);
    show_cursor(); // JTP: Was hidden by force system colors
    
    ao_path path;
    success = show_load_quicksaved_game_dialog(path); //show_read_saved_game_dialog();
    
    if (success)
    {
        success = load_and_start_game(path);
    }

    if (!success)
    {
        hide_cursor(); // JTP: Will be shown when fade stops
        display_main_menu();
    }
}


ao_err handle_open_replay(const ao_path& File)
{
    DraggedReplayFile = File;
    
    force_system_colors(true);
    ao_err err = begin_game(_replay_from_file, false);
    if(err) display_main_menu(); // TODO: caller should do this when it gets an error back
    return err;
}




void handle_replay(bool last_replay)
{
    if(!last_replay) force_system_colors(true);
    ao_err err = begin_game(_replay, !last_replay);
    if (err) display_main_menu();
}



ao_err handle_edit_map()
{
    force_system_colors(true);
    ao_err err = begin_game(_single_player, false);
    if (err) display_main_menu();
    return err;
}








extern bool first_frame_rendered;
float last_heartbeat_fraction = -1.f;
bool is_network_pregame = false;

bool idle_game_state(uint64_t time)
{
    auto machine_ticks_elapsed = time - game_state.last_ticks_on_idle;

    if(machine_ticks_elapsed || game_state.ticks_until_next_screen==0)
    {
        if(game_state.ticks_until_next_screen != INFINITE_TIME_DELAY)
        {
            game_state.ticks_until_next_screen-= machine_ticks_elapsed;
        }
        
        /* Note that we still go through this if we have an indefinate phase.. */
        if (game_state.ticks_until_next_screen <= 0)
        {
            ao_err err = no_err;
            
            switch(get_game_state())
            {
                    // these are irrelevant since idle_game_state is called in game loop, not app loop
                case _display_quit_screens:
                case _display_intro_screens:
                case _display_prologue:
                case _display_epilogue:
                case _display_credits:
                    next_game_screen();
                    break;

                case _display_intro_screens_for_demo:
                case _display_main_menu:
                    /* Start the demo.. */
                    if(!environment_preferences.auto_play_demos ||
                       !begin_game(_demo, false))
                    {
                        /* This means that there was not a valid demo to play */
                        game_state.ticks_until_next_screen= TICKS_UNTIL_DEMO_STARTS;
                    }
                    break;

                case _close_game:
                    display_main_menu();
                    break;

                case _switch_demo:
                    /* This is deferred to the idle task because it */
                    /*  occurs at interrupt time.. */
                    switch(game_state.user)
                    {
                        case _replay:
                            finish_game(true);
                            break;
                            
                        case _demo:
                            finish_game(false);
                            display_introduction_screen_for_demo();
                            break;
                            
                        default:
                            assert_fail(false, "");
                            break;
                    }
                    break;
                
                case _display_chapter_heading:
                    //ao__dprintf__("Chapter heading...");
                    break;

                case _quit_game:
                    /* About to quit, but can still hit this through order of ops.. */
                    break;

                case _revert_game:
                    /* Reverting while in the update loop sounds sketchy.. */
                    err = revert_game();
                    if (!err)
                    {
                        game_state.state= _game_in_progress;
                        game_state.ticks_until_next_screen = 15 * MACHINE_TICKS_PER_SECOND;
                        game_state.last_ticks_on_idle= machine_tick_count();
                        SoundManager::instance()->UpdateListener();
                        reset_motion_sensor(current_player_index);
                    }
                    else
                    {
                        display_loading_map_error(err);
                        finish_game(true);
                    }
                    break;
                    
                case _begin_display_of_epilogue:
                    display_epilogue();
                    break;

                case _game_in_progress:
                    game_state.ticks_until_next_screen = 15 * MACHINE_TICKS_PER_SECOND;
                    //game_state.last_ticks_on_idle= machine_tick_count();
                    break;

                case _change_level:
                case _displaying_network_game_dialogs:
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
        }
        game_state.last_ticks_on_idle= machine_tick_count();
    }

    /* if we’re not paused and there’s something to draw (i.e., anything different from
        last time), render a frame */
    if (game_state.state==_game_in_progress)
    {
        // ZZZ change: update_world() whether or not get_keyboard_controller_status() is true
        // This way we won't fill up queues and stall netgames if one player switches out for a bit.
        std::pair<bool, int16> theUpdateResult= update_world();
        short ticks_elapsed= theUpdateResult.second;
        bool redraw = false;

        if (get_keyboard_controller_status())
        {
            // ZZZ: I don't know for sure that render_game_to_screen works best with the number of _real_
            // ticks elapsed rather than the number of (potentially predictive) ticks elapsed.
            // This is a guess.
            auto heartbeat_fraction = get_heartbeat_fraction();
            if (theUpdateResult.first || (last_heartbeat_fraction != -1 && last_heartbeat_fraction != heartbeat_fraction)) {
                last_heartbeat_fraction = heartbeat_fraction;
                render_game_to_screen(ticks_elapsed);
                first_frame_rendered = ticks_elapsed > 0;
                is_network_pregame = false;
            }
            else
                redraw = game_is_networked && is_network_pregame;
        }
        else
            redraw = true;

        if (redraw)
        {
            static uint64_t last_redraw = 0;
            if (current_player && machine_tick_count() > last_redraw + MACHINE_TICKS_PER_SECOND / 30)
            {
                last_redraw = machine_tick_count();
                render_game_to_screen(ticks_elapsed);
                if (ticks_elapsed) is_network_pregame = false;
            }
        }
        
        return theUpdateResult.first;
    }
    else
    {
        swap_screen_if_requested();
        update_interface_fades();
        return false;
    }
}





static bool should_quit_without_warning_dialog()
{
    return dynamic_world->tick_count - local_player->ticks_at_last_successful_save < CLOSE_WITHOUT_WARNING_DELAY;
}


void do_gameworld_command(short menu_item)
{
    switch(menu_item)
    {
        case iPause:
            switch(game_state.user)
            {
                case _single_player:
                case _replay:
                    if (get_keyboard_controller_status())
                    {
                        pause_game();
                    }
                    else
                    {
                        resume_game();
                    }
                    break;
                    
                case _demo:
                    finish_game(true);
                    break;
                    
                case _network_player:
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
            break;
            
        case iSave:
            switch(game_state.user)
            {
                case _single_player:
#if 0
                    quicksave_game();
                    validate_world_window();
#endif
                    break;
                    
                case _demo:
                case _replay:
                    finish_game(true);
                    break;
                    
                case _network_player:
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
            break;
            
        case iRevert:
            /* Not implemented.. */
            break;
            
        case iCloseGame:
        case iQuitGame:
        {
            bool really_wants_to_quit = false;
            
            switch(game_state.user)
            {
                case _single_player:
                    if (PLAYER_IS_DEAD(local_player) || should_quit_without_warning_dialog() || shell_options.should_output_to_file())
                    {
                        really_wants_to_quit = true;
                    }
                    else
                    {
                        pause_game();
                        show_cursor();
                        really_wants_to_quit = show_quit_without_saving_dialog();
                        hide_cursor();
                        resume_game();
                    }
                    break;
                    
                case _demo:
                case _replay:
                case _network_player:
                    really_wants_to_quit = true;
                    break;
                    
                default:
                    assert_fail(false, "");
                    break;
            }
            
            if (really_wants_to_quit)
            {
                set_game_state(_close_game);
            }
        }
            break;
            
        default:
            assert_fail(false, "");
            break;
    }
}
