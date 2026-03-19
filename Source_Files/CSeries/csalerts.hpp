/*
 csalerts.hpp
 
 Copyright (C) 1991-2001 and beyond by Bo Lindbergh
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

#ifndef __csalerts_hpp__
#define __csalerts_hpp__


#include "cstypes.h"
#include "cserr.hpp"
#include "string_resources.hpp" // `notify_user` supports "$NAME$" string vars expansion and will attempt to convert ao_err codes to error strings defined in string_resources_std


// -----------------------------------------------------------------------------------------
// user notifications


// Any function that wishes to hook itself in as an `notify_user` callback must have this type:
typedef void (*notify_user_proc_t)(ao_err code, const std::string& extra_message, const string_vars_t vars);


// Install/remove a custom callback which `notify_user` will call.
void set_notify_user_proc(notify_user_proc_t proc);
void reset_notify_user_proc();


// the new alert (use code = 0 to display a message string only)
void notify_user(ao_err code, const std::string& extra_message = "", string_vars_t vars = {});


// These are the default callbacks for `notify_user` (GUI apps all use dialogs; the server Hub uses stderr):
//
//void show_simple_dialog_notification(ao_err code, const std::string& extra_message, const string_vars_t vars);
//void write_to_stderr_notification(ao_err code, const std::string& extra_message, const string_vars_t vars);

// -----------------------------------------------------------------------------------------
// miscellaneous


// basic on-screen reporting; a scrolling recent-messages list

// TODO: this requires the high-level UI is initialized before it will work, which is why its implementation is in screen_shared.h(!), not here; if it isn't needed until the app is fully initialized then move it to screen_share, otherwise implement it here with a notify_user-style callback hook which initially uses simple message dialog/stderr and is upgraded to use high-level screen drawing APIs when those are ready for use. On-screen messaging is super useful, both for troubleshooting and for scrolling in-game status notifications (game saved, player killed player/player died), so it'd be worth cleaning up its API and integrate fully with csalerts system so ALL calls go to `notify_user` (we can define one or more ao_err codes that send messages to screen when that is available). Depending on usage patterns, it might even be worth pushing out to HUD plugins to render.

void screen_print(const std::string& s); // this writes a string onto screen without any special processing (it does not perform string var expansion)


// TODO: this is temporary; replace with a higher-level function that uses string vars and string resources as these are user-facing messages so need to support l10n
#define screen_print_f(format, ...) \
{ \
    char ao__tmp__[DEBUG_MESSAGE_MAX_SIZE]; \
    snprintf(ao__tmp__, sizeof(ao__tmp__), (format), __VA_ARGS__); \
    screen_print(ao__tmp__); \
}


// displayed by Mac/Win Aleph One app on first run when it doesn't have a scenario selected


// TODO: redo this as a general-purpose file/directory chooser which is used everywhere; see show_read_directory_dialog_os in choose_file_dialogs_os.cpp (caveat we need dialogs that can show a title/prompt, which those don't)
std::string show_choose_scenario_dialog();


// open website (AO homepage; Metaserver registration, leaderboard; Steam community page)
void open_url_in_browser(const std::string& url);



void display_loading_map_error(ao_err err); // TODO: generalize error reporting



#endif /* __csalerts_hpp__ */
