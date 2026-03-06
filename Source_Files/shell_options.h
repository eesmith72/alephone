#ifndef SHELL_OPTIONS_H
#define SHELL_OPTIONS_H

#include "cseries.h"


struct ShellOptions {
	std::unordered_map<int, bool> parse(int argc, char** argv, bool ignore_unknown_args = false);

	std::string program_name;
	
	bool nogl;
	bool nosound;
	bool nogamma;
	bool debug;
	bool nojoystick;
	bool insecure_lua;

	bool force_fullscreen;
	bool force_windowed;

	bool skip_intro;
	bool editor;

	bool no_chooser;

    // TODO: make the ao_path (ensuring native POSIX/Windows paths are converted correctly)
	std::string replay_directory;
    
	std::string directory;
	std::vector<std::string> files;

	std::string output_path;
    
    
    void sync_dropped_files()
    {
        if (directory.empty())
        {
            // See if we had a scenario folder dropped on us
            SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_DROPFILE:
                        // TODO: why 2 different members? can/should we just chuck everything in the vector?
                        ao_path path(event.drop.file);
                        if (std::filesystem::is_directory(path))
                        {
                            directory = event.drop.file;
                        }
                        else
                        {
                            files.push_back(event.drop.file);
                        }
                        SDL_free(event.drop.file);
                        break;
                }
            }
            SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
        }
    }
};

extern ShellOptions shell_options;

#endif
