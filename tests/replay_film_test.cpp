#include "shell.h"
#include "world.h"
#include "DataFile.hpp"
#include "shell_options.h"
#include "interface.h"
#include "preferences.h"
#include <catch2/catch_test_macros.hpp>

extern ShellOptions shell_options;


typedef std::pair<std::string, uint16_t> Replay; //replay file path and seed

#ifndef REPLAY_SET_SEED_FILENAME //enable and run this to set the correct file name with seed on new replay files


static uint16_t get_seed_from_filename(const std::string& file_name)
{
	auto position = file_name.find_last_of('.');
	auto name_without_ext = file_name.substr(0, position);
	auto seed_position = name_without_ext.find_last_of('.');
	if (seed_position == std::string::npos) throw std::exception();
	return stoi(name_without_ext.substr(seed_position + 1));
}


static std::vector<Replay> get_replays(const ao_path& dir_path)
{
    std::vector<Replay> results;
    
    if (!std::filesystem::is_directory(dir_path))
    {
        log_warning_f("No directory found at: '%s'", dir_path.c_str());
        return results;
    }

    for (const auto& item : std::filesystem::directory_iterator(dir_path))
    {

        if (std::filesystem::is_directory(item))
        {
			auto sub_replays = get_replays(item);
			results.insert(results.end(), sub_replays.begin(), sub_replays.end());
		}
		else if (get_type_of_file(item) == _typecode_film)
        {
			auto seed = get_seed_from_filename(item.path());
            results.push_back({item.path(), seed});
		}
	}

	return results;
}


static void set_replay_preferences()
{
	graphics_preferences->fps_target = 60;
}


TEST_CASE("Film replay", "[Replay]") {

	REQUIRE(!shell_options.directory.empty());
	REQUIRE(!shell_options.replay_directory.empty());

	const auto replays = get_replays(shell_options.replay_directory);

	initialize_application();
	set_replay_preferences();

	for (const auto& replay : replays) {
		INFO(replay.first);
        handle_dropped_file(replay.first);
		set_replay_speed(INT16_MAX);
		main_event_loop();
		auto seed = get_random_seed();
		CHECK(seed == replay.second);
	}

	shutdown_application();
}

#else

static std::vector<std::string> get_replays(const ao_path& dir_path)
{
    // TODO: how does this differ from the get_replays function above?
	std::vector<std::string> results;
    if (!std::filesystem::is_directory(dir_path))
    {
        log_warning_f("No directory found at: '%s'", dir_path.c_str());
        return results;
    }

    for (const auto& item : std::filesystem::directory_iterator(dir_path))

        if (std::filesystem::is_directory(item))
        {
			auto sub_replays = get_replays(item);
			results.insert(results.end(), sub_replays.begin(), sub_replays.end());
		}
		else if (entry.GetType() == _typecode_film)
        {
			try
            {
				get_seed_from_filename(item.path());
			}
			catch (...)
            {
				results.push_back(item.path());
			}
		}
	}

	return results;
}


TEST_CASE("Film replay set seed", "[Replay]") {

	REQUIRE(!shell_options.directory.empty());
	REQUIRE(!shell_options.replay_directory.empty());

	const auto replays = get_replays(shell_options.replay_directory);

	initialize_application();

	for (const auto& replay : replays)
    {
		INFO(replay);
		handle_dropped_file(replay);
		set_replay_speed(INT16_MAX);
		main_event_loop();
		auto seed = get_random_seed();
        ao_path directory = replay;
        auto name_with_seed = file.stem() + "." + std::to_string(seed) + ".filA";
        directory.remove_filename();
        ao_path new_file = directory / name_with_seed;
        std::error_code code;
		std::rename(file, new_file, code);
        assert_warn(code.value() == 0, "renaming file failed");
	}

	shutdown_application();
}

#endif
