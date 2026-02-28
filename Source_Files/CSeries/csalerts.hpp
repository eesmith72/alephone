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
#include "string_resources.hpp"


// -----------------------------------------------------------------------------------------
// user alerts


// A function that wishes to hook itself in as an alert_user handler.
typedef void (*alert_user_proc_t)(aoerr code, const std::string& extra_message, const string_vars_t vars);


// Install/remove a custom callback which alert_user will call.
void set_alert_user_callback(alert_user_proc_t proc);
void reset_alert_user_callback();


// TODO: may be useful to have reserved strDEBUG that spits the message string to stderr/console

// the new alert (use code = 0 to display a message string only)
void alert_user(aoerr code, const std::string& extra_message = "", string_vars_t vars = {});


// exposed here in case anyone has something to say they don't want going through alert_user.
// Note: this does not use string resources and will not expand string vars
void show_alert_in_simple_dialog(aoerr code, const std::string& message = "");





// displayed by Mac/Win Aleph One app on first run when it doesn't have a scenario selected
std::string show_choose_scenario_dialog(); // TODO: scenario chooser should use high-level w_widgets dialog with bells and whistles on


// -----------------------------------------------------------------------------------------
// open website (AO homepage; Metaserver registration, leaderboard; Steam community page)


void open_url_in_browser(const std::string& url);



#endif /* __csalerts_hpp__ */
