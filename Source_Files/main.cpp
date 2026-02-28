

#include "cseries.h"

#include "shell_options.h"
#include "shell.h"
#include "alephversion.h"
#include <SDL2/SDL_main.h>


int main(int argc, char** argv)
{
    time_t t = time(NULL);
    printf("\x1b[1m%s\x1b[m %s (released %s)\n", A1_DISPLAY_NAME, A1_DISPLAY_VERSION, A1_DISPLAY_DATE_VERSION);
    printf("Copyright (C) 1991-%i by Bungie, Inc. and the \"Aleph One\" developers.\n", gmtime(&t)->tm_year);
    printf("This is Free Software with ABSOLUTELY NO WARRANTY. You are welcome to\n"
		   "redistribute it under certain conditions. See COPYING.md for details.\n"
		   "<https://www.bungie.net/> <%s>\n", A1_HOMEPAGE_URL);
    
	shell_options.parse(argc, argv);

	auto code = 0;
/*
	try {
*/
		// Initialize everything
		initialize_application();

		for (std::vector<std::string>::iterator it = shell_options.files.begin(); it != shell_options.files.end(); ++it)
		{
			if (handle_open_document(*it))
			{
				break;
			}
		}

		// Run the main loop
		main_event_loop();
/*
	}
	catch (std::exception& e) {
        reset_alert_user_callback();
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
        reset_alert_user_callback();
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
        reset_alert_user_callback();
		shutdown_application();
	}
	catch (...)
	{

	}
*/
	return code;
}
