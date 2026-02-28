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

#include "cstypes.h"


// TODO: replace these with individual `get_NAME_path` functions+macros to get rid of get_data_path and minimize platform-specific code (cspaths should ONLY provide the base directory paths, with subpaths to "Screenshots", "Saved Games", et al being managed by Files/find_files)
enum cs_path_t
{
	kPathLocalData,
	kPathDefaultData,
	kPathLegacyData,
	kPathBundleData,
	kPathLogs,
	kPathPreferences,
	kPathLegacyPreferences,
	kPathScreenshots,
	kPathSavedGames,
	kPathQuickSaves,
	kPathImageCache,
	kPathRecordings
};


std::string get_data_path(cs_path_t type);
char get_path_list_separator();


std::string get_application_name();
std::string get_application_identifier();


#endif /* __cspaths_hpp__ */
