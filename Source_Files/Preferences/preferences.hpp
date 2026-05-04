/*
 preferences.hpp -- umbrella header for user preferences and dialogs
 
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

// TODO: migrate prefs file to RapidJSON (good practice before attempting to modernize scenario file formats)

// TODO: all hardcoded strings (titles, labels, menu options) need moved into string_resources_std.cpp so they can be localized later

#ifndef preferences_hpp
#define preferences_hpp

#include "cseries.hpp"

#include "player_preferences.hpp"
#include "network_preferences.hpp"
#include "graphics_preferences.hpp"
#include "sound_preferences.hpp"
#include "input_preferences.hpp"
#include "environment_preferences.hpp"


//-----------------------------------------------------------------------------


void read_preferences();

void write_preferences();


//-----------------------------------------------------------------------------


void display_main_preferences_dialog();


#endif /* preferences_hpp */
