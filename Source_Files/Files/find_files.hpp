/*
 find_files.hpp - Routines for finding files
 Written in 2000 by Christian Bauer
 
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

#ifndef __find_files_h__
#define __find_files_h__

#include "cseries.h"



std::filesystem::file_time_type convert_time_to_file_time(time_t t);


// -----------------------------------------------------------------------------------------
// file name extensions

// TODO: there is annoying inconsistency with the enum for strings; it'd be better to have a single enum for all

enum filetype_t // TODO: make enum class
{
    _typecode_unknown = NONE, // this seems to do double duty for 'unrecognized' and 'other' (.png, .dds, etc)
    _typecode_creator = 0,    // this code appears to be unused
    _typecode_map,            // This was named _typecode_map which was confusing as only Map and Physics were full WADs; other scenario files (Shapes, Sounds, etc) weren't full WADs so were always separate files. It would've been nice if early AO had made all files WAD-compatible, simplifying bundling and distribution, but it didn't. Anyway, renaming it now.
    _typecode_savegame,
    _typecode_film,
    _typecode_physics,
    _typecode_shapes,
    _typecode_sounds,
    _typecode_patch,
    _typecode_images,
    _typecode_preferences,
    _typecode_music,
    _typecode_theme,          // pseudo type code
    _typecode_netscript,      // ZZZ pseudo typecode
    _typecode_shapespatch,
    _typecode_movie,
    _typecode_m1_application_resources,
    NUMBER_OF_TYPECODES
};
// Finds every type of file
const filetype_t WILDCARD_TYPE = _typecode_unknown;


filetype_t get_type_of_file(const ao_path& path);

// hide extensions known to Aleph One for displaying file names in dialogs
std::string hide_ao_filename_extension(const std::string& filename);


// -----------------------------------------------------------------------------------------
// find default scenario files using a simple name search
// this is not recursive, looking for the named file in top-level of search directories


// Traverse search path, look for file given relative path name (EES: I'm guessing this predated FileFinder class)

// file_type should be given so better error messages can be generated
const ao_path find_file_at_subpath(const ao_path& sub_path, filetype_t file_type = WILDCARD_TYPE);


/*
 filenameSHAPES8,
 filenameSOUNDS8,
 filenamePREFERENCES,
 filenameDEFAULT_MAP,
 filenameDEFAULT_SAVE_GAME,
 filenameMARATHON_NAME,
 filenameMARATHON_RECORDING,
 filenamePHYSICS_MODEL,
 filenameMUSIC,
 filenameIMAGES,
 filenameMOVIE,
 filenameDEFAULT_THEME,
 filenameEXTERNAL_RESOURCES,
 */
inline const ao_path get_path_to_default_file(filetype_t file_type, string_index_t str129_index)
{
    // TODO: this returns empty if not found; that will need handled appropriately later
    return find_file_at_subpath(get_string(STRID(strFILENAMES, str129_index)), file_type);
}


// M1 uses exported resources (.appl) file; M2+ resources are in Images file
#define get_default_external_resources_path()  (get_path_to_default_file(_typecode_m1_application_resources, filenameEXTERNAL_RESOURCES))

#define get_default_images_path()              (get_path_to_default_file(_typecode_images, filenameIMAGES))

#define get_default_map_path()                 (get_path_to_default_file(_typecode_map, filenameDEFAULT_MAP))

#define get_default_physics_path()             (get_path_to_default_file(_typecode_physics, filenamePHYSICS_MODEL))

#define get_default_sounds_path()              (get_path_to_default_file(_typecode_sounds, filenameSOUNDS8))

#define get_default_shapes_path()              (get_path_to_default_file(_typecode_shapes, filenameSHAPES8))

#define get_default_music_path()               (get_path_to_default_file(_typecode_music, filenameMUSIC))

const ao_path get_default_theme_path()
{
    ao_path sub_path = "Themes"; // directory name is not configurable
    sub_path /= get_string(STRID(strFILENAMES, filenameDEFAULT_THEME));
    return find_file_at_subpath(sub_path, _typecode_theme); // TODO: this is problematic wrt path separator and may be wrong
}


// -----------------------------------------------------------------------------------------
// find scenario files by various search criteria

extern std::vector<ao_path> scenario_data_search_paths;


// callback for use in find_scenario_file/find_file
// called with each path of the specified type; return false to stop and return the current path
//
// note: to find all, capture a std::vector<ao_path> in the closure which it appends to, and have the closure always return false
//
typedef std::function<bool(const ao_path&)> match_file_proc;


static const match_file_proc match_all_files = [](const ao_path& path){ return true; };

match_file_proc match_modification_date(std::filesystem::file_time_type time);

match_file_proc match_modification_date(time_t modification_date);

match_file_proc match_checksum(uint32_t checksum);

match_file_proc match_file_type(filetype_t file_type);

match_file_proc match_all(std::vector<match_file_proc> procs);



// searches a given directory, breadth-first in case-sensitive sorted order
// (previously FileFinder::Find but lambda filters are simple and compose better)
const void find_files(std::vector<ao_path>& result, const ao_path& search_dir,
                      match_file_proc proc, bool stop_after_first_match = true, bool is_recursive = true);

const void find_files(std::vector<ao_path>& result,
                      match_file_proc proc, bool stop_after_first_match = true, bool is_recursive = true);


// recursively searches all of AO's search paths (if Steam is enabled, its workshop paths are searched first), e.g.
//
//    ao_path found_path = find_scenario_file({match_file_type(_typecode_map), match_checksum(checksum)});
//    if (found_path.empty()) { return file_not_found; }
//
ao_path find_scenario_file(match_file_proc proc);

// convenience function when specifying >1 match proc
inline ao_path find_scenario_file(std::vector<match_file_proc> procs)
{
    return find_scenario_file(match_all(procs));
}


void find_mml_files_in_directory(std::set<ao_path>& result, const ao_path& dir);



// search_path can be empty string, in which case all scenario dirs are searched; TODO: this pattern comes up repeatedly in Lua/ so pulled out here; note: if dir path is given and file_path is absolute, they will be joined which will presumably produce an invalid path (normally if file_path is absolute, it'd be left as-is; however, this is how AO did it so we're preserving it)
inline ao_path expand_file_path(const ao_path& file_path, const ao_path& dir_path)
{
    if (file_path.empty()) return "";
    ao_path path = dir_path.empty() ? find_file_at_subpath(file_path) : dir_path / file_path;
    return std::filesystem::is_regular_file(path) ? path : "";
}


// -----------------------------------------------------------------------------------------
// temporarily extend the existing search paths

// inserts the given dir before the global search paths, then restores when going out of scope // TODO: this would ideally fuck off, but that depends on how search paths are eventually stored and managed; thank goodness thread-safety is not a concern for AO!
class ScopedSearchPath
{
public:
    ScopedSearchPath(const ao_path& dir);
    ~ScopedSearchPath();

private:
    ScopedSearchPath(const ScopedSearchPath&) = delete;
    ScopedSearchPath& operator=(const ScopedSearchPath&) = delete;

    const ao_path d;
};



#endif /* __find_files_h__ */
