/*
 cspaths.cpp -- platform-agnostic functions and Win/Lin-specific implementations
 
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

#include "cspaths.hpp"

#include "csstrings.hpp"
#include "alephversion.h"

#ifdef HAVE_CONFIG_H
#include "confpaths.h"
#endif


// -----------------------------------------------------------------------------------------
// app's display name for use in string vars and dialogs

// TODO: this needs straightened out

#ifndef __MACOSX__

std::string get_application_name()
{
    return std::string(A1_DISPLAY_NAME);
}

#endif


// -----------------------------------------------------------------------------------------
// standard AO directories


// platform-specific implementations for Windows and Linux (Mac-specific is in cspaths.mm)

#if defined(__MACOSX__)

// defined in cspaths.mm

#elif defined(__WIN32__)


ao_path get_local_storage_dir()
{
	static ao_path dir;
	if (dir.empty())
	{
		wchar_t file_name[MAX_PATH];
		SHGetFolderPathW(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, file_name);
        dir = ao_path(wide_to_utf8(file_name)) / "AlephOne";
	}
	return dir;
}


ao_path get_default_game_data_dir()
{
	static ao_path dir;
	if (dir.empty())
	{
		wchar_t file_name[MAX_PATH];
		const DWORD r = GetModuleFileNameW(NULL, file_name, MAX_PATH); // can truncate
        if (r != 0 & r != MAX_PATH)
        {
            dir = ao_path(wide_to_utf8(file_name));
            dir.remove_filename();
        }
	}
	return dir;
}


ao_path get_preferences_dir()
{
	static ao_path dir;
	if (dir.empty())
	{
		wchar_t file_name[MAX_PATH];
		SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, file_name);
		dir = ao_path(wide_to_utf8(file_name)) / "AlephOne";
	}
	return dir;
}


ao_path get_logs_dir()
{
    return get_local_storage_dir();
}


#else /* end of Windows; start of Linux */


ao_path get_local_storage_dir() // "~/.alephone/" -- all AO support dirs+files live in here
{
	static ao_path dir = "";
	if (dir.empty())
	{
		const char *home = getenv("HOME");
        if (home) { dir = ao_path(home) / ".alephone"; }
	}
	return dir;
}


ao_path get_default_game_data_dir()
{
#ifdef PKGDATADIR
    return PKGDATADIR;
#else
    return get_local_storage_dir();
#endif
}


ao_path get_preferences_dir()
{
    return get_local_storage_dir();
}


ao_path get_logs_dir()
{
    return get_local_storage_dir();
}


#endif /* end of Linux */


// -----------------------------------------------------------------------------------------
// utilities


void ao_create_directories(const ao_path& dir)
{
    // TODO: this does nothing if directory already exists; however, it can throw OS exceptions so we should probably handle those here, calling notify_user with an appropriate error code+details
    std::filesystem::create_directories(dir); // TODO: use error code arg
}


// TODO: this is inadequate - ASCII-only, omits 99% of valid unicode chars, so if anyone ever makes a ME/FE scenario with localized name it's likely to return zero matches; don't know offhand the best way to do this so leaving for another day

static int char_is_not_filesafe(int c)
{
    return (c != ' ' && !std::isalnum(c));
}


void make_string_filesystem_safe(std::string& path_component)
{
    if (!path_component.empty())
    {
        path_component.erase(std::remove_if(path_component.begin(), path_component.end(), char_is_not_filesafe), path_component.end());
    }
    if (path_component.empty()) { path_component = "Unknown"; }
}

