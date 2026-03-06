/*
	Copyright (C) 2024 Benoit Hauquier and the "Aleph One" developers.

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

#include "StandaloneHub.h"

#include "preferences.h"
#include "network_star.h"
#include "vbl.h"
#include "map.h"
#include "wad.h"
#include "map_wad.h"


enum class StandaloneHubState
{
	_waiting_for_gatherer,
	_game_in_progress,
	_quit
};


ao_path log_dir;

static void initialize_hub(short port)
{
	reinitialize_default_strings();
	log_dir = get_logs_directory();
    std::filesystem::create_directories(log_dir); // TODO: any errors we should worry about?
	network_preferences = new network_preferences_data;
	network_preferences->game_port = port;
	network_preferences->game_protocol = _network_game_protocol_star;
	DefaultHubPreferences();
	mytm_initialize();
	initialize_keyboard_controller();
	initialize_marathon();
}

static bool hub_init_game(void)
{
	initialize_map_for_new_game();

	byte* wad = nullptr;
	int wad_length = StandaloneHub::Instance()->GetMapData(&wad);
	if (!wad) return false; //something is wrong

	auto wad_copy = new byte[wad_length];
	std::memcpy(wad_copy, wad, wad_length);

	wad_header header;
	auto wad_data = inflate_flat_data(wad_copy, &header);
	if (!wad_data) { delete[] wad_copy; return false; }

	bool saved_game = get_dynamic_data_from_wad(wad_data, dynamic_world) && get_player_data_from_wad(wad_data);
	free_wad(wad_data);

	StandaloneHub::Instance()->SetSavedGame(saved_game);

	return true;
}

static bool hub_game_in_progress(bool& game_is_done)
{
	game_is_done = false;

	if (hub_is_active() && !StandaloneHub::Instance()->HasGameEnded())
	{
		NetProcessMessagesInGame();
		return true;
	}

	if (!NetUnSync()) return false; //should never happen

	bool next_game = false;

	if (StandaloneHub::Instance()->GetGameDataFromGatherer())
	{
		initialize_map_for_new_level();
		next_game = NetChangeMap(nullptr) && NetSync(); //don't stop the server if it fails here
	}

	if (!next_game)
	{
		game_is_done = true;
		return StandaloneHub::Reset();
	}

	StandaloneHub::Instance()->SetGameEnded(false);
	return true;
}

static bool hub_host_game(bool& game_has_started)
{
	game_has_started = false;

	if (!StandaloneHub::Init(GAME_PORT))
	{
        log_error("Error while trying to instantiate Aleph One remote hub");
		return false;
	}

	if (!StandaloneHub::Instance()->WaitForGatherer()) return true;

	if (!StandaloneHub::Instance()->GetGameDataFromGatherer() || !hub_init_game())
	{
		return StandaloneHub::Reset();
	}

	bool gathering_done;
	bool success = StandaloneHub::Instance()->SetupGathererGame(gathering_done);

	if (!success)
	{
        log_error("Error while trying to gather game on Aleph One remote hub");
		return false;
	}

	if (!gathering_done) return true;

	if (NetStart() && NetChangeMap(nullptr) && NetSync())
	{
		game_has_started = true;
		return true;
	}

	return StandaloneHub::Reset();
}

static void main_loop_hub()
{
	auto game_state = StandaloneHubState::_waiting_for_gatherer;

	while (game_state != StandaloneHubState::_quit)
	{
		switch (game_state)
		{
			case StandaloneHubState::_waiting_for_gatherer:
				{
					bool game_has_started;

					if (!hub_host_game(game_has_started))
						game_state = StandaloneHubState::_quit;
					else if (game_has_started)
						game_state = StandaloneHubState::_game_in_progress;

					break;
				}

			case StandaloneHubState::_game_in_progress:
				{
					bool game_is_done;

					if (!hub_game_in_progress(game_is_done))
						game_state = StandaloneHubState::_quit;
					else if (game_is_done)
						game_state = StandaloneHubState::_waiting_for_gatherer;

					break;
				}
		}

		sleep_for_machine_ticks(1);
	}
}

static uint16_t parse_port(char* port_arg)
{
	std::string port_str = port_arg;
	bool parsed = true;

	if (port_str.length() > 5) return 0;

	for (char c : port_str)
	{
		if (!isdigit(c))
		{
			parsed = false;
			break;
		}
	}

	uint32_t port = parsed ? std::atoi(port_arg) : 0;
	return port > UINT16_MAX ? 0 : port;
}

int main(int argc, char** argv)
{
	auto code = 0;
	short port = 0;

	if (argc > 1)
	{
		port = parse_port(argv[1]);
	}

	if (!port)
	{
		printf("Invalid or missing argument \"port\" for network standalone hub");
		return 1;
	}

	try {

		// Initialize everything
		initialize_hub(port);

		// Run the main loop
		main_loop_hub();

	}
	catch (std::exception& e) {
		try
		{
			log_fatal_f("Unhandled exception: %s", e.what());
		}
		catch (...)
		{
		}
		code = 1;
	}
	catch (...) { // TODO: are there any exceptions in AO which don't subclass std::exception?
		try
		{
			log_fatal("Unknown exception");
		}
		catch (...)
		{
		}
		code = 1;
	}

	return code;
}
