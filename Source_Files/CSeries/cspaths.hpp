/*
 cspaths.hpp -- directory paths and application info
 
 Copyright (C) 2017 and beyond by Jeremiah Morris
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

#ifndef __cspaths_hpp__
#define __cspaths_hpp__

#include "cserr.hpp"
#include "cstypes.hpp"


// -----------------------------------------------------------------------------------------
// app's display name for use in string vars and dialogs


std::string get_application_name();


std::string get_username_os(); // dumping this here from preferences.cpp


// -----------------------------------------------------------------------------------------
// standard AO directories


#if defined(__MACOSX__)

// (macOS only) the DataFiles dir inside the .app bundle; this is searched before the defalt data dir
ao_path get_macos_app_bundle_game_data_dir();

#endif

ao_path get_default_game_data_dir(); // on macOS and Windows, the directory containing the app; on Linux...?

ao_path get_local_storage_dir(); // local (per-user) data file directory which is parent path for the following:

#define get_screenshots_dir()  (get_local_storage_dir() / "Screenshots")
#define get_saved_games_dir()  (get_local_storage_dir() / "Saved Games")
#define get_quicksaves_dir()   (get_local_storage_dir() / "Quick Saves")
#define get_image_cache_dir()  (get_local_storage_dir() / "Image Cache")
#define get_saved_films_dir()  (get_local_storage_dir() / "Recordings") /* (except film buffer which is stored in local_storage_dir) */

ao_path get_preferences_dir();

ao_path get_logs_dir();

void ao_create_directories(const ao_path& dir);

// sanitize a string (e.g. level name) for use in a file name
// TODO: FIX: this strips all non alnum chars which is excessive // TODO: make sure this is used on all generated file names (e.g. exported saved games, films, movies)
void make_string_filesystem_safe(std::string& path_component);


// TODO: where best to put these?


inline ao_err make_temp_file(ao_path& path) // caution: modifies the path in-place
{
    std::string tmp_path = path.generic_u8string() + ".XXXXXX"; // this assumes mkstemp will error if resulting string exceeds MAX_PATH
    if (mkstemp(tmp_path.data()))
    {
        return errno; // TODO: map OS-specific FS errors to portable, strings-enabled AO errors
    }
    return no_err;
}


inline ao_err rename_file(const ao_path& from_path, const ao_path& to_path)
{
    std::error_code code;
    std::filesystem::rename(from_path, to_path, code);
    return code.value(); // TODO: remap OS errs to AO errs
}




#endif /* __cspaths_hpp__ */
