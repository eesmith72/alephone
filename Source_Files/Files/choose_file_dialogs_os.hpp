/*
 choose_file_dialogs_os.hpp -- OS-native file and directory chooser dialogs
 
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

#ifndef choose_file_dialogs_os_hpp
#define choose_file_dialogs_os_hpp

#include "cseries.hpp"
#include "find_files.hpp"

//#include "DataFile.hpp"

// TODO: for now, this implements Native File Dialogs only (the themed dialogs have terrible UX so need redone anyway)

// TODO: dialogs should probably return errUserCanceled/no_err for consistency with other APIs


// -----------------------------------------------------------------------------------------
// choose file/directory dialogs

// TODO: nfd APIs don't take a prompt string, which is not great


ao_path display_read_directory_dialog_os(const ao_path& start_dir = ""); // was FileSpecifier.ReadDirectoryDialog

#define display_open_directory_dialog     display_read_directory_dialog_os


ao_err display_read_file_dialog_os(ao_path& result, filetype_t type, const std::string& prompt = "", const ao_path& start_path = ""); // was FileSpecifier.Read[File]Dialog

#define display_read_file_dialog          display_read_file_dialog_os

// called by display_load_saved_game_dialog when user clicks LOAD OTHER
#define display_read_saved_game_dialog(path)  (display_read_file_dialog((path), _typecode_savegame, "CONTINUE SAVED GAME", get_saved_games_dir()))

#define display_read_saved_film_dialog(path)  (display_read_file_dialog((path), _typecode_film,     "REPLAY SAVED FILM",   get_saved_films_dir()))


ao_path display_write_file_dialog_os(filetype_t file_type, const std::string& prompt = "",
                                    const ao_path& start_path = "", const std::string& default_filename = ""); // was FileSpecifier.WriteDialog[Async]

// TODO: what about default filename?
#define display_write_saved_game_dialog()     (display_write_file_dialog_os(_typecode_savegame, "SAVE GAME",   get_saved_games_dir()))

//display_write_file_dialog(_typecode_savegame, get_string(STRID(strPROMPTS, _save_replay_prompt)), start, name);
#define display_export_saved_game_dialog()    (display_write_file_dialog_os(_typecode_savegame, "EXPORT GAME", get_saved_games_dir()))


#define display_write_saved_film_dialog()     (display_write_file_dialog_os(_typecode_film,     "SAVE FILM",   get_saved_films_dir()))

#define display_write_exported_film_dialog(default_file_name)  (display_write_file_dialog_os(_typecode_movie,  "EXPORT FILM", get_saved_films_dir(), (default_file_name)))



// -----------------------------------------------------------------------------------------

// dumping these here for now:

bool display_confirm_overwrite_file_dialog(const std::string& filename);




#endif /* choose_file_dialogs_os_hpp */
