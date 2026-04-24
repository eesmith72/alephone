

#include "cseries.h"

#include "shell.h"
#include "shell_options.h"
#include "alephversion.h"

#include "main_event_loop.hpp"

//#include <SDL2/SDL_main.h>



int main(int argc, char** argv)
{
    time_t t = time(NULL);
    printf("%s %s (%s)\n\n", A1_DISPLAY_NAME, A1_DISPLAY_VERSION, A1_DISPLAY_DATE_VERSION);
    printf("Copyright (C) 1991-%d by Bungie, Inc. and the \"Aleph One\" developers.\n", gmtime(&t)->tm_year + 1900);
    printf("This is Free Software with ABSOLUTELY NO WARRANTY. You are welcome to\n"
		   "redistribute it under certain conditions. See COPYING.md for details.\n"
		   "<https://www.bungie.net/> <%s>\n\n", A1_HOMEPAGE_URL);
    
	shell_options.parse(argc, argv);

	auto code = 0;
/*
	try {
*/
		initialize_application();
        
        for (auto& it : shell_options.dropped_files)
		{
            app_state_t next_state = handle_dropped_file(it);
            if (next_state != app_state_t::undefined)
            {
                // TODO: if user passes conflicting files, e.g. film files with editor enabled, should display error
                set_next_app_state(next_state);
            }
		}

		main_event_loop();
    
    // TODO: if we catch exceptions in order to call shutdown_application, we should re-throw it when done and let the OS generate a detailed crash report which user can submit in bug ticket
/*
	}
	catch (std::exception& e) {
        reset_notify_user_callback();
		try
		{
			log_fatal_f("Unhandled exception: %s", e.what());
		}
		catch (...)
		{
		}
		code = 1;
	}
	catch (...) {
        reset_notify_user_callback();
		try
		{
			log_fatal("Unknown exception");
		}
		catch (...)
		{
		}
		code = 1;
	}

	try
	{
        reset_notify_user_callback();
		shutdown_application();
	}
	catch (...)
	{

	}
*/
	return code;
}
