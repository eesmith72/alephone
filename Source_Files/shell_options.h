#ifndef SHELL_OPTIONS_H
#define SHELL_OPTIONS_H

#include "cseries.h"


struct ShellOptions
{
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

    // TODO: check native Windows path strings are always converted correctly
    
	std::string directory;
	std::vector<std::string> dropped_files;
    
    std::string replay_directory; // TODO: get rid of this and add film files directly to `film_files`; Q. if directory contains Map and other files (presumably used by the films), also add the directory to search paths?
    
    std::vector<ao_path> film_files; // dropped film files will replay automatically // TODO: FIX: finish implementing
    

	std::string output_path;
    
    
    ao_path pull_film_path()
    {
        if (film_files.empty()) return "";
        ao_path path = film_files.front();
        film_files.erase(film_files.begin());
        return path;
    }
    
    
    bool should_output_to_file() { return !output_path.empty(); }

    void read_dropped_files()
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
                            directory = event.drop.file; // why only one? should dropping multiple directories be allowed?
                        }
                        else
                        {
                            dropped_files.push_back(event.drop.file);
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
