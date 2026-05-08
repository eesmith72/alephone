/*
 shell.cpp - initialize and shutdown application
 
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


#include "cseries.hpp"

#include "map.h"
#include "monsters.h"
#include "player.h"
#include "render.h"
#include "shell.h"
#include "interface.hpp"
#include "SoundManager.h"
#include "visual_effects.hpp"
#include "Screen.hpp"
#include "Music.h"
#include "images.h"
#include "vbl.h"
#include "preferences.hpp"
#include "tags.h" /* for scenario file type.. */
#include "mouse.h"
#include "joystick.h"
#include "screen_drawing.h"
#include "computer_interface.h"
#include "map_wad.h"
//#include "hud_manager.h"
#include "physics_wad.h"
#include "items.h"
#include "weapons.h"
#include "lua_script.h"

#include "OGL_Render.h"
#include "ImageBlitter.hpp"
#include "XML_ParseTreeRoot.h"
#include "DataFile.hpp"
#include "Plugins.h"
#include "compatibility_profiles.h"
#include "ScenarioChooser.h"


#include "resource_manager.h"
#include "sdl_dialogs.h"
#include "fonts.hpp"
#include "sdl_widgets.h"

#include "XML_LevelScript.h"

#include "alephversion.h"

#include "network.h"
#include "Console.h"
#include "FilmExporter.h"
#include "HTTP.h"
#include "WadImageCache.h"

#include "main_event_loop.hpp"

#include "shell_options.h"

#ifdef HAVE_STEAM
#include "steamshim_child.h"
#endif


#ifdef HAVE_STEAM
std::vector<item_subscribed_query_result::item> subscribed_workshop_items;
steam_game_information steam_game_info;
#endif


static std::string ao_getenv(const char* name)
{
#ifdef __WIN32__
	wchar_t* wstr = _wgetenv(utf8_to_wide(name).c_str());
	return wstr ? wide_to_utf8(wstr) : std::string{};
#else
	char* str = getenv(name);
	return str ? str : std::string{};
#endif
}


// -----------------------------------------------------------------------------------------
// initialize sub-systems; TODO: these should eventually relocate to the appropriate subdir

void initialize_local_storage_directories()
{
    
    ao_create_directories(get_local_storage_dir()); // logs, screenshots, last game recording, etc; e.g. "$HOME/.alephone" on Linux
    ao_create_directories(get_preferences_dir());
    
    // TODO: recordings dir? // Directory for recordings (except film buffer, which is stored in local_storage_dir)
    ao_create_directories(get_saved_games_dir());
    ao_create_directories(get_quicksaves_dir()); // parent directory for per-scenario subdirs
    ao_create_directories(get_image_cache_dir());
    ao_create_directories(get_saved_films_dir());
    ao_create_directories(get_screenshots_dir());
}


static void initialize_sdl()
{
    int32_t err = SDL_Init(SDL_INIT_VIDEO
                              | (shell_options.nosound ? 0 : SDL_INIT_AUDIO)
                              | (shell_options.nojoystick ? 0 : SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)
                              | (shell_options.debug ? SDL_INIT_NOPARACHUTE : 0));
    
    if (!err)
    {
        // TODO: any reason SDL_Image isn't a required dependency by now?
#if defined(HAVE_SDL_IMAGE)
        IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
#endif
        err = TTF_Init();
    }
    
    if (err)
    {
        const char* message = SDL_GetError();
        fprintf(stderr, "Couldn't initialize SDL (%d). %s\n", err, message ? message : "");
        exit(1);
    }
    
    initialize_fonts(); // make sure the base fonts are loaded; TODO: fonts will be reset anyway upon loading MML so this call might be redundant - but leave it here so that, at minimum, the builtin fonts are always available to dialogs
    
    SDL_StopTextInput(); // We only want text input events at specific times
    initialize_joystick();
}


ao_path initialize_quicksaves_dir()
{
    ao_path quicksaves_dir;

#ifdef HAVE_STEAM
    if (is_workshop_scenario)
    {
        quicksaves_dir = get_quicksaves_dir() / "Marathon Infinity" / "Workshop";
    }
    else
#endif
    {
        quicksaves_dir = get_quicksaves_dir() / Scenario::instance()->get_filesystem_safe_name();
    }
    ao_create_directories(quicksaves_dir);

    return quicksaves_dir;
}


static void initialize_marathon_music_handler(void)
{
    ao_path path = get_scenario_music_path();
    if (!path.empty()) Music::instance()->SetupIntroMusic(path);
}


// -----------------------------------------------------------------------------------------
// STARTUP; TODO: this should simplify further as calls relocate into initialize_SUBSYSTEM functions in the appropriate subdirs


void initialize_application()
{
    load_standard_strings();
    
#if defined(__WIN32__)
	if (LoadLibraryW(L"exchndl.dll")) shell_options.debug = true;
	SDL_setenv("SDL_AUDIODRIVER", "directsound", 0);
#endif
    
    initialize_sdl();
    
    // see if there are scenarios to choose from
    
    shell_options.read_dropped_files();
    
	const std::string default_data_env = ao_getenv("ALEPHONE_DEFAULT_DATA");
    
    // EES: TODO: trying to disentangle the default_data_dir/scenario_dir
    
    ao_path scenario_dir;
	if (!shell_options.directory.empty())
	{
		scenario_dir = shell_options.directory;
	}
	else if (!default_data_env.empty())
	{
		scenario_dir = default_data_env;
	}
    else
    {
        scenario_dir = get_default_game_data_dir();
    }

	ScenarioChooser chooser;
	chooser.add_primary_scenario(scenario_dir);

#ifdef HAVE_STEAM
	if (!STEAMSHIM_init())
	{
        // Since GPL forbids linking libsteam_api.dylib directly to AO, the Launcher is a non-GPL executable that is permitted to link it, giving AO access to libsteam's services via parent-child process pipes. (It is not clear why the shim runs AO instead of AO running the shim as a subprocess, but the only limitation seems to be that it makes the Steam builds untestable when run on their own.)
        notify_user(0, "You must launch the Steam version of Classic Marathon using the Classic Marathon Launcher."); // TODO: define strSTEAM[_ERRORS] strings
		exit(1);
	}

	bool got_info = false, got_items = false;
	STEAMSHIM_getGameInfo();
	if (shell_options.editor || shell_options.no_chooser)
	{
		got_items = true;
	}
	else
	{
		STEAMSHIM_queryWorkshopItemScenario();
	}

	while (STEAMSHIM_alive() && (!got_info || !got_items))
	{
		auto result = STEAMSHIM_pump();

		if (!result)
		{
			sleep_for_machine_ticks(30);
			continue;
		}

		switch (result->type)
		{
			case SHIMEVENT_GET_GAME_INFO:
				steam_game_info = result->game_info;
				got_info = true;
				break;
			case SHIMEVENT_WORKSHOP_QUERY_ITEM_SUBSCRIBED_RESULT:
				if (result->items_subscribed.result_code == 1)
				{
					for (const auto& steam_scenario : result->items_subscribed.items)
					{
						chooser.add_workshop_scenario(steam_scenario.install_folder_path);
					}
				}
				got_items = true;
				break;
			default:
				break;
		}
	}
#endif // HAVE_STEAM
    
    
	auto is_workshop_scenario = false;
	if (!shell_options.editor && !shell_options.no_chooser)
	{
        chooser.add_directory(chooser.num_scenarios() > 0 ? scenario_dir / "Scenarios" : scenario_dir);
        
		if (chooser.num_scenarios() > 1)
		{
			std::string chosen_path;
			std::tie(chosen_path, is_workshop_scenario) = chooser.run();
			
			shell_options.directory = chosen_path; // ugh
		}
	}
    
    // in case we need to redo search path later:
    size_t dsp_insert_pos = scenario_data_search_paths.size();
    size_t dsp_delete_pos = (size_t)-1;
    

	// Find data directories, construct search path
#ifndef SCENARIO_IS_BUNDLED
    scenario_dir = get_default_game_data_dir();
#endif
    
#if defined(__MACOSX__)
    ao_path embedded_scenario_dir = get_macos_app_bundle_game_data_dir();
    if (!embedded_scenario_dir.empty()) { scenario_data_search_paths.push_back(embedded_scenario_dir); }
#endif
    
	if (shell_options.directory != "")
	{
        scenario_dir = shell_options.directory;
		dsp_delete_pos = scenario_data_search_paths.size();
		scenario_data_search_paths.push_back(shell_options.directory);
	}
	else if (!default_data_env.empty())
	{
        scenario_dir = default_data_env;
		dsp_delete_pos = scenario_data_search_paths.size();
		scenario_data_search_paths.push_back(default_data_env);
	}

	const std::string data_env = ao_getenv("ALEPHONE_DATA");
	if (!data_env.empty())
    {
		// Read colon-separated list of directories
		string path = data_env;
		string::size_type pos;
#ifdef __WIN32__
#define PATHS_SEPARATOR (';')
#else
#define PATHS_SEPARATOR (':')
#endif
		while ((pos = path.find(PATHS_SEPARATOR)) != std::string::npos)
        {
			if (pos)
            {
				string element = path.substr(0, pos);
				scenario_data_search_paths.push_back(element);
			}
			path.erase(0, pos + 1);
		}
		if (!path.empty())
			scenario_data_search_paths.push_back(path);
	}
    else
    {
		if (shell_options.directory == "" && default_data_env == "")
		{
			dsp_delete_pos = scenario_data_search_paths.size();
			scenario_data_search_paths.push_back(scenario_dir);
		}
		scenario_data_search_paths.push_back(get_local_storage_dir());
	}
    
    // moved these up here
    initialize_local_storage_directories();
	initialize_resources();
    
    load_default_physics(); // EES: not sure where this should be in load order until scenario/environment prefs/MML loading order is clarified, so leaving here for now
    
    // initialize environment_preferences before initializing fonts (scenarios can load their own fonts)
    read_preferences();
    
	load_film_profile(FILM_PROFILE_DEFAULT);
    
    
    // note: MML can change default filenames
	LoadBaseMMLScripts(true);
    
	// Check for presence of files (one last chance to change scenario_data_search_paths)
	if (!default_scenario_files_exist()) // TODO: this just smells weird
    {
        std::string chosen_dir = display_load_scenario_dialog();
        if (!chosen_dir.empty())
        {
			// remove original argument (or fallback) from search path
			if (dsp_delete_pos < scenario_data_search_paths.size())
				scenario_data_search_paths.erase(scenario_data_search_paths.begin() + dsp_delete_pos);
			// add selected directory where command-line argument would go
			scenario_data_search_paths.insert(scenario_data_search_paths.begin() + dsp_insert_pos, chosen_dir);
			
            scenario_dir = chosen_dir;
			
			// Parse MML files again, now that we have a new dir to search
			LoadBaseMMLScripts(true);
		}
	}

#ifdef HAVE_STEAM
	STEAMSHIM_queryWorkshopItemMod(Scenario::instance()->GetName());

	while (STEAMSHIM_alive())
	{
		auto result = STEAMSHIM_pump();

		if (!result)
		{
			sleep_for_machine_ticks(30);
			continue;
		}

		if (result->type == SHIMEVENT_WORKSHOP_QUERY_ITEM_SUBSCRIBED_RESULT)
		{
			if (result->items_subscribed.result_code == 1)
			{
				for (const auto& item : result->items_subscribed.items)
					{
						subscribed_workshop_items.push_back(item);
					}
			}
			break;
		}
	}
#endif
    
	Plugins::instance()->enumerate();
    
    initialize_quicksaves_dir();
	WadImageCache::instance()->initialize_cache();

	if (shell_options.force_fullscreen)
		graphics_preferences.fullscreen = true;
	if (shell_options.force_windowed)		// takes precedence over fullscreen because windowed is safer
		graphics_preferences.fullscreen = false;
	
    write_preferences(); // TODO: this is saving the fullscreen change set by shell, which is a bit odd as shell options should probably only apply to this session (mind, editing any prefs would also save the shell's change)
    
    
	Plugins::instance()->load_mml(true);
	
	HTTPClient::Init();

	// Initialize everything
	initialize_timing();
	sound_manager.initialize();
	initialize_marathon_music_handler();
	initialize_keyboard_controller();
	main_screen.initialize();
	initialize_marathon();
	initialize_dialogs();
	initialize_computer_terminals();
	initialize_shapes();
	initialize_images_manager();
	load_scenario_from_environment_preferences();
	initialize_app_state();
    
    if (shell_options.insecure_lua) { notify_user(STRID(strDEBUG, db_insecure_lua)); }
    
    if (shell_options.editor)
    {
        set_next_app_state(app_state_t::map_editor);
    }
    else if (!shell_options.film_files.empty())
    {
        set_next_app_state(app_state_t::load_and_play_dropped_film);
    }
    else if (shell_options.skip_intro)
    {
        set_next_app_state(app_state_t::main_menu);
    }
    else
    {
        set_next_app_state(app_state_t::startup_screen);
    }
}


// -----------------------------------------------------------------------------------------
// SHUTDOWN


void shutdown_application(void)
{
	WadImageCache::instance()->save_cache();

	shutdown_dialogs();
    
#if defined(HAVE_SDL_IMAGE)
	IMG_Quit();
#endif

	TTF_Quit();
	SDL_Quit();

#ifdef HAVE_STEAM
	STEAMSHIM_deinit();
#endif
}


// -----------------------------------------------------------------------------------------
// process files passed via drag-n-drop/CLI
// (this is called by main.cpp, not initialize_application, so tests can call it directly)

app_state_t handle_dropped_file(const ao_path& path) // TODO: relative paths/filenames should be expanded to absolute paths upstream
{
    // TODO: if Map/Shapes/Sounds/etc files are dropped, should that permanently change the scenario? TBH, it's bad design: would make more sense if dropping files added them to the app's scenarios/ directory (if not already installed), caveat not sure where to put a Map if it's being edited
    
    app_state_t next_state = app_state_t::undefined;
    
    switch (get_type_of_file(path))
    {
        case _typecode_map:
            set_current_map_path(path);
            if (shell_options.editor) { next_state = app_state_t::map_editor; }
            break;
            
        case _typecode_savegame:
            configure_game_for_resumed_campaign(path);
            next_state = app_state_t::load_saved_game; // TODO: this will skip startup screens and jump straight into game; is that UX ok or should it go through startup screens first?
            break;
            
        case _typecode_film:
            shell_options.film_files.push_back(path);
            next_state = app_state_t::load_and_play_dropped_film;
            break;
            
        case _typecode_physics:
            set_external_physics_file(path);
            break;
            
        case _typecode_shapes:
            open_shapes_file(path);
            break;
            
        case _typecode_sounds:
            sound_manager.OpenSoundFile(path);
            break;
            
        default:
            break;
    }
    
    return next_state;
}

