/*

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

/*
 *  shell.cpp - Main game loop and input handling
 */

#include "cseries.h"

#include "map.h"
#include "monsters.h"
#include "player.h"
#include "render.h"
#include "shell.h"
#include "interface.h"
#include "SoundManager.h"
#include "fades.h"
#include "screen.h"
#include "Music.h"
#include "images.h"
#include "vbl.h"
#include "preferences.h"
#include "tags.h" /* for scenario file type.. */
#include "mouse.h"
#include "joystick.h"
#include "screen_drawing.h"
#include "computer_interface.h"
#include "map_wad.h" /* yuck... */
#include "game_window.h" /* for draw_interface() */
#include "physics_wad.h"
#include "items.h"
#include "interface_menus.h"
#include "weapons.h"
#include "lua_script.h"

#include "Crosshairs.h"
#include "OGL_Render.h"
#include "OGL_Blitter.h"
#include "XML_ParseTreeRoot.h"
#include "DataFile.hpp"
#include "Plugins.h"
#include "FilmProfile.h"
#include "ScenarioChooser.h"

#include "mytm.h"	// mytm_initialize(), for platform-specific shell_*.h


#include "resource_manager.h"
#include "sdl_dialogs.h"
#include "FontRenderer_SDL.hpp"
#include "sdl_widgets.h"



#include "OGL_Headers.h"

#include "alephversion.h"

#include "network.h"
#include "Console.h"
#include "Movie.h"
#include "HTTP.h"
#include "WadImageCache.h"


#include "shell_options.h"

#ifdef HAVE_STEAM
#include "steamshim_child.h"
#endif




//ao_path default_data_dir;  // Default scenario directory


#ifdef HAVE_STEAM
std::vector<item_subscribed_query_result::item> subscribed_workshop_items;
steam_game_information steam_game_info;
#endif


void PlayInterfaceButtonSound(short SoundID);


// From vbl_sdl.cpp
void execute_timer_tasks(uint64_t time);


// Prototypes
static void initialize_marathon_music_handler(void);
static void process_event(const SDL_Event &event);


// cross-platform static variables
short vidmasterLevelOffset = 1; // can be set with MML


static std::string a1_getenv(const char* name)
{
#ifdef __WIN32__
	wchar_t* wstr = _wgetenv(utf8_to_wide(name).c_str());
	return wstr ? wide_to_utf8(wstr) : std::string{};
#else
	char* str = getenv(name);
	return str ? str : std::string{};
#endif
}





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
    


bool handle_open_document(const ao_path& path) // TODO: relative paths/filenames should be expanded to absolute paths upstream
{
	bool done = false;
    
	switch (get_type_of_file(path))
    {
        case _typecode_scenario:
            set_current_map_path(path);
            done = shell_options.editor && handle_edit_map(); // TODO: map editing should eventually be available as an optional button on main screen
            break;
        case _typecode_savegame:
            done = load_and_start_game(path);
            break;
        case _typecode_film:
            done = handle_open_replay(path);
            break;
        case _typecode_physics:
            set_external_physics_file(path);
            break;
        case _typecode_shapes:
            set_current_shapes_file(path);
            break;
        case _typecode_sounds:
            SoundManager::instance()->OpenSoundFile(path);
            break;
        default:
            break;
    }
	
	return done;
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
        if (message)
            fprintf(stderr, "Couldn't initialize SDL (%d): %s\n", err, message);
        else
            fprintf(stderr, "Couldn't initialize SDL (%d)\n", err);
        exit(1);
    }
    
    // We only want text input events at specific times
    SDL_StopTextInput();
    
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







void initialize_application(void)
{
    load_standard_strings();
    
#if defined(__WIN32__)
	if (LoadLibraryW(L"exchndl.dll")) shell_options.debug = true;
	SDL_setenv("SDL_AUDIODRIVER", "directsound", 0);
#endif
    
    initialize_sdl();
    
    // see if there are scenarios to choose from
    
    shell_options.sync_dropped_files();
    
	const std::string default_data_env = a1_getenv("ALEPHONE_DEFAULT_DATA");
    
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

	const std::string data_env = a1_getenv("ALEPHONE_DATA");
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
    
    initialize_physics(); // EES: not sure where this should be in load order until scenario/environment prefs/MML loading order is clarified, so leaving here for now
    
    // font loading uses environment_preferences, and MMLs can load fonts, so get
    initialize_preferences();

	load_film_profile(FILM_PROFILE_DEFAULT);
    
    initialize_fonts(false);
    
    // note: MML can change default filenames
	LoadBaseMMLScripts(true);
    
	// Check for presence of files (one last chance to change scenario_data_search_paths)
	if (!has_default_files())
    {
        std::string chosen_dir = show_choose_scenario_dialog();
        if (!chosen_dir.empty())
        {
			// remove original argument (or fallback) from search path
			if (dsp_delete_pos < scenario_data_search_paths.size())
				scenario_data_search_paths.erase(scenario_data_search_paths.begin() + dsp_delete_pos);
			// add selected directory where command-line argument would go
			scenario_data_search_paths.insert(scenario_data_search_paths.begin() + dsp_insert_pos, chosen_dir);
			
            scenario_dir = chosen_dir;
			
			// Parse MML files again, now that we have a new dir to search
			initialize_fonts(false); // TODO: any reason for calling initialize_fonts here? it will be called again below
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

	initialize_fonts(true);
    
	Plugins::instance()->enumerate();
    
//    initialize_local_storage_directories();
//    initialize_preferences();
    initialize_quicksaves_dir();
	WadImageCache::instance()->initialize_cache();

#ifndef HAVE_OPENGL
	graphics_preferences->screen_mode.acceleration = _no_acceleration;
#endif
	if (shell_options.nogl)
		graphics_preferences->screen_mode.acceleration = _no_acceleration;
	if (shell_options.force_fullscreen)
		graphics_preferences->screen_mode.fullscreen = true;
	if (shell_options.force_windowed)		// takes precedence over fullscreen because windowed is safer
		graphics_preferences->screen_mode.fullscreen = false;
	write_preferences();

	Plugins::instance()->load_mml(true);

//	SDL_WM_SetCaption(application_name, application_name);

// #if defined(HAVE_SDL_IMAGE) && !(defined(__APPLE__) && defined(__MACH__))
// 	SDL_WM_SetIcon(IMG_ReadXPMFromArray(const_cast<char**>(alephone_xpm)), 0);
// #endif
	
	HTTPClient::Init();

	// Initialize everything
	mytm_initialize();
//	initialize_fonts();
	SoundManager::instance()->Initialize(*sound_preferences);
	initialize_marathon_music_handler();
	initialize_keyboard_controller();
	initialize_gamma();
	alephone::Screen::instance()->Initialize(&graphics_preferences->screen_mode);
	initialize_marathon();
	initialize_screen_drawing();
	initialize_dialogs();
	initialize_terminal_manager();
	initialize_shape_handler();
	initialize_fades();
	initialize_images_manager();
	load_environment_from_preferences();
	initialize_game_state();
}


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


static void initialize_marathon_music_handler(void)
{
	ao_path path = get_default_music_path();
    if (!path.empty()) Music::instance()->SetupIntroMusic(path);
}


bool quit_without_saving(void)
{
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add (new w_static_text("Are you sure you wish to"), d);
	placer->dual_add (new w_static_text("cancel the game in progress?"), d);
	placer->add (new w_spacer(), true);
	
	horizontal_placer *button_placer = new horizontal_placer;
	w_button *default_button = new w_button("YES", dialog_ok, &d);
	button_placer->dual_add (default_button, d);
	button_placer->dual_add (new w_button("NO", dialog_cancel, &d), d);
	d.activate_widget(default_button);
	placer->add(button_placer, true);
	d.set_widget_placer(placer);
	return d.run() == 0;
}

// ZZZ: moved level-numbers widget into sdl_widgets for a wider audience.

const int32 AllPlayableLevels = _single_player_entry_point | _multiplayer_carnage_entry_point | _multiplayer_cooperative_entry_point | _kill_the_man_with_the_ball_entry_point | _king_of_hill_entry_point | _rugby_entry_point | _capture_the_flag_entry_point;

short get_level_number_from_user(void) // TODO: this function has absolutely no business being in the top-level(!) `shell.cpp`, but cleaning up and relocating it is a job for another day
{
	// Get levels
    std::vector<entry_point> levels;
	if (!get_entry_points(levels, AllPlayableLevels))
    {
		entry_point dummy;
		dummy.level_number = 0;
		dummy.utf8_level_name = "Untitled Level";
		levels.push_back(dummy);
	}

	// Create dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
    
    std::stringstream introduction(get_string(STRID(vidmasterStringSetID, strVidmasterIntroduction)));
    std::string line;
    while (std::getline(introduction, line, '\n')) // we will ignore the potential for naughtily-crafted MML strings
    {
        placer->dual_add(new w_static_text(line.c_str()), d);
    }
    placer->add(new w_spacer(), true);
    std::stringstream oath(get_string(STRID(vidmasterStringSetID, strVidmasterOath)));
    while (std::getline(oath, line, '\n')) // we will ignore the potential for naughtily-crafted MML strings
    {
        placer->dual_add(new w_static_text(line.c_str()), d);
    }
    
    std::string start_at_text = get_string(STRID(vidmasterStringSetID, strVidmasterIntroduction));
	placer->add(new w_spacer(), true);
    placer->dual_add(new w_static_text(start_at_text.c_str()), d);

	w_levels *level_w = new w_levels(levels, &d);
	level_w->set_offset(vidmasterLevelOffset);
	placer->dual_add(level_w, d);
	placer->add(new w_spacer(), true);
	placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

	d.activate_widget(level_w);
	d.set_widget_placer(placer);

	// Run dialog
	short level;
	if (d.run() == 0)		// OK
		// Should do noncontiguous map files OK
		level = levels[level_w->get_selection()].level_number;
	else
		level = NONE;

	// Redraw main menu
	update_game_window();
	return level;
}

const uint32 TICKS_BETWEEN_EVENT_POLL = 16; // 60 Hz
void main_event_loop(void)
{
	uint32 last_event_poll = 0;
	short game_state;

	while ((game_state = get_game_state()) != _quit_game) {
		uint64_t cur_time = machine_tick_count();
		bool yield_time = false;
		bool poll_event = false;

		switch (game_state) {
			case _game_in_progress:
			case _change_level:
				if ((get_fps_target() == 0 && get_keyboard_controller_status()) || Console::instance()->input_active() || cur_time - last_event_poll >= TICKS_BETWEEN_EVENT_POLL) {
					poll_event = true;
					last_event_poll = cur_time;
			  } else {				  
					SDL_PumpEvents ();	// This ensures a responsive keyboard control
			  }
				break;

			case _display_intro_screens:
			case _display_main_menu:
			case _display_chapter_heading:
			case _display_prologue:
			case _display_epilogue:
			case _begin_display_of_epilogue:
			case _display_credits:
			case _display_intro_screens_for_demo:
			case _display_quit_screens:
			case _displaying_network_game_dialogs:
				yield_time = interface_fade_finished();
				poll_event = true;
				break;

			case _close_game:
			case _switch_demo:
			case _revert_game:
				yield_time = poll_event = true;
				break;
		}

		if (poll_event) {
			global_idle_proc();

			SDL_Event event;
			if (yield_time)
			{
				// The game is not in a "hot" state, yield time to other
				// processes but only try for a maximum of 30ms
				if (SDL_WaitEventTimeout(&event, 30))
				{
					process_event(event);
				}
			}

			while (SDL_PollEvent(&event))
			{
				process_event(event);
			}

#ifdef HAVE_STEAM
			while (auto steam_event = STEAMSHIM_pump()) {
				switch (steam_event->type) {
					case SHIMEVENT_IS_OVERLAY_ACTIVATED:
						if (steam_event->okay && get_game_state() == _game_in_progress && !game_is_networked)
						{
							pause_game();
						}
						break;

					default:
						break;
				}
			}
#endif
		}

		execute_timer_tasks(machine_tick_count());
		idle_game_state(machine_tick_count());

		auto fps_target = get_fps_target();
		if (!get_keyboard_controller_status())
		{
			fps_target = 30;
		}
	
		if (game_state == _game_in_progress && fps_target != 0)
		{
			int elapsed_machine_ticks = machine_tick_count() - cur_time;
			int desired_elapsed_machine_ticks = MACHINE_TICKS_PER_SECOND / fps_target;

			if (desired_elapsed_machine_ticks - elapsed_machine_ticks > desired_elapsed_machine_ticks / 3)
			{
				sleep_for_machine_ticks(1);
			}
		}
		else if (game_state != _game_in_progress)
		{
			static uint64_t last_redraw = 0U;
			if (machine_tick_count() > last_redraw + TICKS_PER_SECOND / 30)
			{
				update_game_window();
				last_redraw = machine_tick_count();
			}
		}
	}
}

static bool has_cheat_modifiers(void)
{
	SDL_Keymod m = SDL_GetModState();
#if (defined(__APPLE__) && defined(__MACH__))
	return ((m & KMOD_SHIFT) && (m & KMOD_CTRL)) || ((m & KMOD_ALT) && (m & KMOD_GUI));
#else
	return (m & KMOD_SHIFT) && (m & KMOD_CTRL) && !(m & KMOD_ALT) && !(m & KMOD_GUI);
#endif
}

static bool event_has_cheat_modifiers(const SDL_Event &event)
{
	Uint16 m = event.key.keysym.mod;
#if (defined(__APPLE__) && defined(__MACH__))
	return ((m & KMOD_SHIFT) && (m & KMOD_CTRL)) || ((m & KMOD_ALT) && (m & KMOD_GUI));
#else
	return (m & KMOD_SHIFT) && (m & KMOD_CTRL) && !(m & KMOD_ALT) && !(m & KMOD_GUI);
#endif
}

static void process_screen_click(const SDL_Event &event)
{
	int x = event.button.x, y = event.button.y;
	alephone::Screen::instance()->window_to_screen(x, y);
	portable_process_screen_click(x, y, has_cheat_modifiers());
}

static void handle_game_key(const SDL_Event &event)
{
	SDL_Keycode key = event.key.keysym.sym;
	SDL_Scancode sc = event.key.keysym.scancode;
	bool changed_screen_mode = false;
	bool changed_prefs = false;
	bool changed_resolution = false;

	if (Console::instance()->input_active()) {
		switch(key) {
			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				Console::instance()->enter();
				break;
			case SDLK_ESCAPE:
				Console::instance()->abort();
				break;
			case SDLK_BACKSPACE:
				Console::instance()->backspace();
				break;
			case SDLK_DELETE:
				Console::instance()->del();
				break;
			case SDLK_UP:
				Console::instance()->up_arrow();
				break;
			case SDLK_DOWN:
				Console::instance()->down_arrow();
				break;
			case SDLK_LEFT:
				Console::instance()->left_arrow();
				break;
			case SDLK_RIGHT:
				Console::instance()->right_arrow();
				break;
			case SDLK_HOME:
				Console::instance()->line_home();
				break;
			case SDLK_END:
				Console::instance()->line_end();
				break;
			case SDLK_a:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->line_home();
				break;
			case SDLK_b:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->left_arrow();
				break;
			case SDLK_d:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->del();
				break;
			case SDLK_e:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->line_end();
				break;
			case SDLK_f:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->right_arrow();
				break;
			case SDLK_h:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->backspace();
				break;
			case SDLK_k:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->forward_clear();
				break;
			case SDLK_n:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->down_arrow();
				break;
			case SDLK_p:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->up_arrow();
				break;
			case SDLK_t:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->transpose();
				break;
			case SDLK_u:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->clear();
				break;
			case SDLK_w:
				if (event.key.keysym.mod & KMOD_CTRL)
					Console::instance()->delete_word();
				break;
		}
	}
	else
	{
		if (sc == SDL_SCANCODE_ESCAPE || sc == AO_SCANCODE_JOYSTICK_ESCAPE) // (ZZZ) Quit gesture (now safer)
		{
			if(!player_controlling_game())
				do_menu_item_command(mGame, iQuitGame, false);
			else {
				if(get_ticks_since_local_player_in_terminal() > 1 * TICKS_PER_SECOND) {
					if(!game_is_networked) {
						do_menu_item_command(mGame, iQuitGame, false);
					}
					else {
#if defined(__APPLE__) && defined(__MACH__)
						screen_print("If you wish to quit, press Command-Q");
#else
						screen_print("If you wish to quit, press Alt+Q.");
#endif
					}
				}
			}
		}
		else if (input_preferences->shell_key_bindings[_key_volume_up].count(sc))
		{
			changed_prefs = SoundManager::instance()->AdjustVolumeUp(Sound_AdjustVolume());
		}
		else if (input_preferences->shell_key_bindings[_key_volume_down].count(sc))
		{
			changed_prefs = SoundManager::instance()->AdjustVolumeDown(Sound_AdjustVolume());
		}
		else if (input_preferences->shell_key_bindings[_key_switch_view].count(sc))
		{
			walk_player_list();
			render_screen(NONE);
		}
		else if (input_preferences->shell_key_bindings[_key_zoom_in].count(sc))
		{
			if (zoom_overhead_map_in())
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
			else
				PlayInterfaceButtonSound(Sound_ButtonFailure());
		}
		else if (input_preferences->shell_key_bindings[_key_zoom_out].count(sc))
		{
			if (zoom_overhead_map_out())
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
			else
				PlayInterfaceButtonSound(Sound_ButtonFailure());
		}
		else if (input_preferences->shell_key_bindings[_key_inventory_left].count(sc))
		{
			if (player_controlling_game()) {
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				scroll_inventory(-1);
			} else
				decrement_replay_speed();
		}
		else if (input_preferences->shell_key_bindings[_key_inventory_right].count(sc))
		{
			if (player_controlling_game()) {
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				scroll_inventory(1);
			} else
				increment_replay_speed();
		}
		else if (input_preferences->shell_key_bindings[_key_toggle_fps].count(sc))
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			extern bool displaying_fps;
			displaying_fps = !displaying_fps;
		}
		else if (input_preferences->shell_key_bindings[_key_activate_console].count(sc))
		{
			if (game_is_networked) {
#if !defined(DISABLE_NETWORKING)
				Console::instance()->activate_input(InGameChatCallbacks::SendChatMessage, InGameChatCallbacks::prompt());
#endif
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
			} 
			else if (Console::instance()->use_lua_console())
			{
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				Console::instance()->activate_input(ExecuteLuaString, ">");
			}
			else
			{
				PlayInterfaceButtonSound(Sound_ButtonFailure());
			}
		} 
		else if (input_preferences->shell_key_bindings[_key_show_scores].count(sc))
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			{
				extern bool ShowScores;
				ShowScores = !ShowScores;
			}
		}	
		else if (sc == SDL_SCANCODE_F1) // Decrease screen size
		{
			if (!graphics_preferences->screen_mode.hud)
			{
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				graphics_preferences->screen_mode.hud = true;
				changed_screen_mode = changed_prefs = true;
			}
			else
			{
				int mode = alephone::Screen::instance()->FindMode(get_screen_mode()->width, get_screen_mode()->height);
				if (mode < alephone::Screen::instance()->GetModes().size() - 1)
				{
					PlayInterfaceButtonSound(Sound_ButtonSuccess());
					graphics_preferences->screen_mode.width = alephone::Screen::instance()->ModeWidth(mode + 1);
					graphics_preferences->screen_mode.height = alephone::Screen::instance()->ModeHeight(mode + 1);
					graphics_preferences->screen_mode.auto_resolution = false;
					graphics_preferences->screen_mode.hud = false;
					changed_screen_mode = changed_prefs = changed_resolution = true;
				} else
					PlayInterfaceButtonSound(Sound_ButtonFailure());
			}
		}
		else if (sc == SDL_SCANCODE_F2) // Increase screen size
		{
			if (graphics_preferences->screen_mode.hud)
			{
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				graphics_preferences->screen_mode.hud = false;
				changed_screen_mode = changed_prefs = true;
			}
			else
			{
				int mode = alephone::Screen::instance()->FindMode(get_screen_mode()->width, get_screen_mode()->height);
				int automode = get_screen_mode()->fullscreen ? 0 : 1;
				if (mode > automode)
				{
					PlayInterfaceButtonSound(Sound_ButtonSuccess());
					graphics_preferences->screen_mode.width = alephone::Screen::instance()->ModeWidth(mode - 1);
					graphics_preferences->screen_mode.height = alephone::Screen::instance()->ModeHeight(mode - 1);
					if ((mode - 1) == automode)
						graphics_preferences->screen_mode.auto_resolution = true;
					graphics_preferences->screen_mode.hud = true;
					changed_screen_mode = changed_prefs = changed_resolution = true;
				} else
					PlayInterfaceButtonSound(Sound_ButtonFailure());
			}
		}
		else if (sc == SDL_SCANCODE_F3) // Resolution toggle
		{
			if (!OGL_IsActive()) {
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				if (graphics_preferences->screen_mode.high_resolution) {
					graphics_preferences->screen_mode.high_resolution = false;
					graphics_preferences->screen_mode.draw_every_other_line = false;
				} else if (!graphics_preferences->screen_mode.draw_every_other_line) {
					graphics_preferences->screen_mode.draw_every_other_line = true;
				} else {
					graphics_preferences->screen_mode.high_resolution = true;
					graphics_preferences->screen_mode.draw_every_other_line = false;
				}
				changed_screen_mode = changed_prefs = true;
			} else
				PlayInterfaceButtonSound(Sound_ButtonFailure());
		}
		else if (sc == SDL_SCANCODE_F4)		// Reset OpenGL textures
		{
#ifdef HAVE_OPENGL
			if (OGL_IsActive()) {
				// Play the button sound in advance to get the full effect of the sound
				PlayInterfaceButtonSound(Sound_OGL_Reset());
				OGL_ResetTextures();
			} else
#endif
				PlayInterfaceButtonSound(Sound_ButtonInoperative());
		}
		else if (sc == SDL_SCANCODE_F5) // Make the chase cam switch sides
		{
			if (ChaseCam_IsActive())
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
			else
				PlayInterfaceButtonSound(Sound_ButtonInoperative());
			ChaseCam_SwitchSides();
		}
		else if (sc == SDL_SCANCODE_F6) // Toggle the chase cam
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			ChaseCam_SetActive(!ChaseCam_IsActive());
		}
		else if (sc == SDL_SCANCODE_F7) // Toggle tunnel vision
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			SetTunnelVision(!GetTunnelVision());
		}
		else if (sc == SDL_SCANCODE_F8) // Toggle the crosshairs
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			player_preferences->crosshairs_active = !player_preferences->crosshairs_active;
			Crosshairs_SetActive(player_preferences->crosshairs_active);
			changed_prefs = true;
		}
		else if (sc == SDL_SCANCODE_F9) // Screen dump
		{
			dump_screen();
		}
		else if (sc == SDL_SCANCODE_F10) // Toggle the position display
		{
			PlayInterfaceButtonSound(Sound_ButtonSuccess());
			{
				extern bool ShowPosition;
				ShowPosition = !ShowPosition;
			}
		}
		else if (sc == SDL_SCANCODE_F11
#ifdef HAVE_STEAM
				 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
				 ) // Decrease gamma level
		{
			if (graphics_preferences->screen_mode.gamma_level) {
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				graphics_preferences->screen_mode.gamma_level--;
				change_gamma_level(graphics_preferences->screen_mode.gamma_level);
				changed_prefs = true;
			} else
				PlayInterfaceButtonSound(Sound_ButtonFailure());
		}
		else if (sc == SDL_SCANCODE_F12
#ifdef HAVE_STEAM
				 && (event.key.keysym.mod & KMOD_SHIFT)
#endif
				 ) // Increase gamma level
		{
			if (graphics_preferences->screen_mode.gamma_level < NUMBER_OF_GAMMA_LEVELS - 1) {
				PlayInterfaceButtonSound(Sound_ButtonSuccess());
				graphics_preferences->screen_mode.gamma_level++;
				change_gamma_level(graphics_preferences->screen_mode.gamma_level);
				changed_prefs = true;
			} else
				PlayInterfaceButtonSound(Sound_ButtonFailure());
		}
		else
		{
			if (get_game_controller() == _demo)
				set_game_state(_close_game);
		}
	}
	
	if (changed_screen_mode) {
		screen_mode_data temp_screen_mode = graphics_preferences->screen_mode;
		temp_screen_mode.fullscreen = get_screen_mode()->fullscreen;
		change_screen_mode(&temp_screen_mode, true, changed_resolution);
		render_screen(0);
	}

	if (changed_prefs)
		write_preferences();
}

static void process_game_key(const SDL_Event &event)
{
	switch (get_game_state()) {
	case _game_in_progress:
#if defined(__APPLE__) && defined(__MACH__)
		if ((event.key.keysym.mod & KMOD_GUI))
#else
		if ((event.key.keysym.mod & KMOD_ALT) || (event.key.keysym.mod & KMOD_GUI))
#endif
		{
			int item = -1;
			switch (event.key.keysym.sym) {
			case SDLK_p:
				item = iPause;
				break;
			case SDLK_s:
				item = iSave;
				break;
			case SDLK_r:
				item = iRevert;
				break;
			case SDLK_q:
// On Mac, this key will trigger the application menu so we ignore it here
#if !defined(__APPLE__) && !defined(__MACH__)
				item = iQuitGame;
#endif
				break;
			case SDLK_RETURN:
				item = 0;
				toggle_fullscreen();
				break;
			default:
				break;
			}
			if (item > 0)
				do_menu_item_command(mGame, item, event_has_cheat_modifiers(event));
			else if (item != 0)
				handle_game_key(event);
		} else
			handle_game_key(event);
		break;
	case _display_intro_screens:
	case _display_chapter_heading:
	case _display_prologue:
	case _display_epilogue:
	case _display_credits:
	case _display_quit_screens:
		if (interface_fade_finished())
			force_game_state_change();
		else
			stop_interface_fade();
		break;

	case _display_intro_screens_for_demo:
		stop_interface_fade();
		display_main_menu();
		break;

	case _quit_game:
	case _close_game:
	case _revert_game:
	case _switch_demo:
	case _change_level:
	case _begin_display_of_epilogue:
	case _displaying_network_game_dialogs:
		break;

	case _display_main_menu: 
	{
		if (!interface_fade_finished())
			stop_interface_fade();
		int item = -1;
		switch (event.key.keysym.sym) {
		case SDLK_n:
			item = iNewGame;
			break;
		case SDLK_o:
			item = iLoadGame;
			break;
		case SDLK_g:
			item = iGatherGame;
			break;
		case SDLK_j:
			item = iJoinGame;
			break;
		case SDLK_p:
			item = iPreferences;
			break;
		case SDLK_r:
			item = iReplaySavedFilm;
			break;
		case SDLK_c:
			item = iCredits;
			break;
		case SDLK_q:
			item = iQuit;
			break;
		case SDLK_F9:
			dump_screen();
			break;
		case SDLK_RETURN:
#if defined(__APPLE__) && defined(__MACH__)
			if ((event.key.keysym.mod & KMOD_GUI))
#else
			if ((event.key.keysym.mod & KMOD_GUI) || (event.key.keysym.mod & KMOD_ALT))
#endif
			{
				toggle_fullscreen();
			} else {
				process_main_menu_highlight_select(event_has_cheat_modifiers(event));
			}
			break;
		case SDLK_a:
			item = iAbout;
			break;
		case SDLK_UP:
		case SDLK_LEFT:
			process_main_menu_highlight_advance(true);
			break;
		case SDLK_DOWN:
		case SDLK_RIGHT:
			process_main_menu_highlight_advance(false);
			break;
		case SDLK_TAB:
			process_main_menu_highlight_advance(event.key.keysym.mod & KMOD_SHIFT);
			break;
		case SDLK_UNKNOWN:
			switch (static_cast<int>(event.key.keysym.scancode)) {
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_UP:
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_LEFT:
					process_main_menu_highlight_advance(true);
					break;
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
					process_main_menu_highlight_advance(false);
					break;
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_A:
					process_main_menu_highlight_select(false);
					break;
				case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_GUIDE:
					process_main_menu_highlight_select(true);
					break;
				default:
					break;
			}
			break;
		default:
			break;
		}
		if (item > 0) {
			draw_menu_button_for_command(item);
			do_menu_item_command(mInterface, item, event_has_cheat_modifiers(event));
		}
		break;
	}
	}
}

static void process_event(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_MOUSEMOTION:
		if (get_game_state() == _game_in_progress)
		{
			mouse_moved(event.motion.xrel, event.motion.yrel);
		}
		break;
	case SDL_MOUSEWHEEL:
		if (get_game_state() == _game_in_progress)
		{
			bool up = (event.wheel.y > 0);
#if SDL_VERSION_ATLEAST(2,0,4)
			if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
				up = !up;
#endif
			mouse_scroll(up);
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (get_game_state() == _game_in_progress) 
		{
			if (!get_keyboard_controller_status())
			{
				resume_game();
			}
			else
			{
				SDL_Event e2;
				memset(&e2, 0, sizeof(SDL_Event));
				e2.type = SDL_KEYDOWN;
				e2.key.keysym.sym = SDLK_UNKNOWN;
				e2.key.keysym.scancode = (SDL_Scancode)(AO_SCANCODE_BASE_MOUSE_BUTTON + event.button.button - 1);
				process_game_key(e2);
			}
		}
		else
			process_screen_click(event);
		break;
	
	case SDL_CONTROLLERBUTTONDOWN:
		if (get_game_state() == _game_in_progress && !get_keyboard_controller_status())
		{
			resume_game();
		}
		else
		{
			joystick_button_pressed(event.cbutton.which, event.cbutton.button, true);
			SDL_Event e2;
			memset(&e2, 0, sizeof(SDL_Event));
			e2.type = SDL_KEYDOWN;
			e2.key.keysym.sym = SDLK_UNKNOWN;
			e2.key.keysym.scancode = (SDL_Scancode)(AO_SCANCODE_BASE_JOYSTICK_BUTTON + event.cbutton.button);
			process_game_key(e2);
		}
		break;
		
	case SDL_CONTROLLERBUTTONUP:
		joystick_button_pressed(event.cbutton.which, event.cbutton.button, false);
		break;
		
	case SDL_CONTROLLERAXISMOTION:
		joystick_axis_moved(event.caxis.which, event.caxis.axis, event.caxis.value);
		break;
	
	case SDL_JOYDEVICEADDED:
		joystick_added(event.jdevice.which);
		break;
			
	case SDL_JOYDEVICEREMOVED:
		if (joystick_removed(event.jdevice.which) && get_game_state() == _game_in_progress)
			pause_game();
		break;
			
	case SDL_KEYDOWN:
		process_game_key(event);
		break;

	case SDL_TEXTINPUT:
		if (Console::instance()->input_active()) {
		    Console::instance()->textEvent(event);
		}
		break;
		
	case SDL_QUIT:
		if (get_game_state() == _game_in_progress)
			do_menu_item_command(mGame, iQuitGame, false);
		else
			set_game_state(_quit_game);
		break;

	case SDL_WINDOWEVENT:
		switch (event.window.event) {
			case SDL_WINDOWEVENT_FOCUS_LOST:
				if (get_game_state() == _game_in_progress && get_keyboard_controller_status() && !Movie::instance()->IsRecording() && shell_options.replay_directory.empty()) {
					pause_game();
				}

				set_game_focus_lost();
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
#if (defined(__APPLE__) && defined(__MACH__))
    			// work around Mojave issue
				static bool gFirstWindow = true;
				if (gFirstWindow) {
					gFirstWindow = false;
					SDL_Window *win = SDL_GetWindowFromID(event.window.windowID);
					if (!MainScreenIsOpenGL() && (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
						SDL_SetWindowFullscreen(win, 0);
						SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
					} else {
						SDL_Window *w2 = SDL_CreateWindow("Loading", 0, 0, 100, 100, 0);
						SDL_RaiseWindow(w2);
						SDL_RaiseWindow(win);
						SDL_DestroyWindow(w2);
					}
				}
#endif
				set_game_focus_gained();
				break;
		}
		break;
	}
	
}



static bool load_mml_files_from_directory(ao_path dir, bool load_menu_mml_only)
{
	// Get sorted list of files in directory
    std::set<ao_path> paths; // case-sensitive order
    find_mml_files_in_directory(paths, dir);
    if (paths.empty()) return false;
	
	// Parse each file
	for (const ao_path& path : paths)
    {
		ParseMMLFromFile(path, load_menu_mml_only);
	}
	
	return true;
}


void LoadBaseMMLScripts(bool load_menu_mml_only)
{
	for (const ao_path& path : scenario_data_search_paths)
    {
        log_note_f("searching for MML in: %s", path.c_str());
        load_mml_files_from_directory(path / "MML", load_menu_mml_only);
        load_mml_files_from_directory(path / "Scripts", load_menu_mml_only);
	}
}




void PlayInterfaceButtonSound(short SoundID)
{
	if (TEST_FLAG(input_preferences->modifiers,_inputmod_use_button_sounds))
		SoundManager::instance()->PlaySound(SoundID, (world_location3d *) NULL, NONE);
}
