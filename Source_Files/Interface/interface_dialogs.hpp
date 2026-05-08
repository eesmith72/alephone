/*
 interface_dialogs.hpp
 
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

#ifndef interface_dialogs_hpp
#define interface_dialogs_hpp

#include "cseries.hpp"


ao_err display_restore_saved_game_as_coop_dialog(const ao_path& file, bool& restore_coop);


// returns false if cancelled
bool display_confirm_exit_game_dialog();

ao_err display_vidmaster_dialog(int16_t& level_number);



// MML

struct InfoTree;
void reset_mml_vidmaster_dialog_strings();
void parse_mml_vidmaster_dialog_strings(const InfoTree& root);



#endif /* interface_dialogs_hpp */
