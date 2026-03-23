/*
 csalerts.mm -- system dialogs for macOS
 
 Copyright (C) 2010 and beyond by Gregory Smith
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


#import <Cocoa/Cocoa.h>
#include "cstypes.h"
#include "csalerts.hpp"


/*
 // TODO: confirm SDL MessageBox is satisfactory and delete this Mac-specific stuff
void notify_user_os_default(const std::string& message, alert_level_t level)
{
	NSAlert *alert = [NSAlert new];
	switch (level)
    {
        case alert_level_t::info:
           // [alert setMessageText: @"."];
            [alert setAlertStyle: NSAlertStyleWarning]; // TODO: not sure when these new names were introduced (need to see if osx 10.13 has them and availability macro if not)
            break;
            
        case alert_level_t::error:
            [alert setMessageText: @"A problem occurred."];
            [alert setAlertStyle: NSAlertStyleWarning]; // TODO: not sure when these new names were introduced (need to see if osx 10.13 has them and availability macro if not)
            break;

        case alert_level_t::fatal:
        {
            [alert setMessageText: @"Frog blast the vent core!!!"];
            [alert setAlertStyle: NSAlertStyleCritical];
            [alert addButtonWithTitle: @"Quit"];
            break;
        }
	}
    [alert setInformativeText: [NSString stringWithUTF8String: message.c_str()]];
	[alert runModal];
}
 */


// TODO: make this a general file/directory chooser function while on SDL2 (SDL3 introduces a portable function for this)
std::string display_load_scenario_dialog()
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:NO];
	[panel setCanChooseDirectories:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:@"Choose Scenario"];
	[panel setMessage:@"Select a scenario to play:"];
	[panel setPrompt:@"Choose"];
    return ([panel runModal] == NSModalResponseOK) ? [[[panel URL] path] UTF8String] : "";
}


void open_url_in_browser(const std::string& url)
{
    log_note_f("open_url_in_browser: %s\n", url.c_str());

    NSURL *urlref = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
	[[NSWorkspace sharedWorkspace] openURL:urlref];
}
