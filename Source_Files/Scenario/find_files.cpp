/*
 find_files.cpp
 
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

#include "find_files.hpp"

#include "DataFile.hpp" // DataFile
#include "tags.h" // LINE_TAG, etc; used by get_file_type when sniffing a file's data to determine its type

#include "wad.h" // is_supported_wad_file_version

#ifdef HAVE_STEAM
#include "steamshim_child.h"
#endif


// -----------------------------------------------------------------------------------------
// file name extensions

// TODO: consolidate these tables

struct extension_map_t
{
    const std::string  extension;
    bool               case_sensitive; // ??? this is an attribute of the filesystem; it's the stupid ShpA vs ShPa nonsense below // TODO: get rid of this
    filetype_t         typecode;
};

static std::array<extension_map_t, 22> extensions = {
    // some common extensions, to speed up building map lists
    ".dds", false, _typecode_unknown, //
    ".jpg", false, _typecode_unknown,
    ".png", false, _typecode_unknown,
    ".bmp", false, _typecode_unknown,
    ".txt", false, _typecode_unknown,
    ".ttf", false, _typecode_unknown,

    ".lua", false, _typecode_netscript, // netscript, or unknown?
    ".mml", false, _typecode_unknown, // no type code for this yet
    
    
    ".sceA", false, _typecode_map,
    ".sgaA", false, _typecode_savegame,
    ".filA", false, _typecode_film,
    ".phyA", false, _typecode_physics,
    ".ShPa", true,  _typecode_shapespatch, // must come before shpA // TODO: idiocy that isn't portable to case-insensitive filesystems; presumably the only way to tell full Shapes files and patch files apart is to sniff their data (if there's distinguishing bytes at start) of read their headers for collection count
    ".shpA", false, _typecode_shapes,
    ".sndA", false, _typecode_sounds,

    ".scen", false, _typecode_map,
    ".shps", false, _typecode_shapes,
    ".phys", false, _typecode_physics,
    ".sndz", false, _typecode_sounds,

    ".mpg", false, _typecode_movie,

    ".appl", false, _typecode_m1_resources,
    ".imgA", false, _typecode_images,
    
    // TODO: what other types should be declared?
};


// these must match the enum names
static const std::array<std::string, 18> typecode_names = { // TODO: move to strings for l10n
    "invalid",              // not part of the enum but included in array so it can be localized
    "unknown",              // _typecode_unknown = -1
    "creator",              //
    "scenario",
    "saved game",
    "saved film",
    "physics",
    "shapes",
    "sounds",
    "patch",
    "images",
    "preferences",
    "music",
    "theme",
    "netscript",
    "shapes patch",
    "exported movie",
    "external resource",    // _typecode_m1_resources = 15
};


const std::string& get_filetype_name(filetype_t file_type)
{
    return typecode_names[(file_type >= _typecode_unknown && file_type <= _typecode_m1_resources) ? file_type + 2 : 0];
}


// functions

std::string hide_ao_filename_extension(const std::string& filename)
{
    // TODO: why only these extensions?
    static const strings_t alephone_extensions = {".sceA", ".sgaA", ".filA", ".phyA", ".shpA", ".sndA"};
    
    for (auto& extension : alephone_extensions)
    {
        if (boost::algorithm::ends_with(filename, extension)) // TODO: case-sensitivity?
        {
            return filename.substr(0, filename.length() - extension.size());
        }
    }
    
    return filename;
}

// Determine a file's type, preferably from its file name extension. If it doesn't have one (MacOS9-era
//  files usually didn't) this will try to infer file type by sniffing first few bytes of its content.
filetype_t get_type_of_file(const ao_path& path)
{
    assert_fail(!path.empty(), "an empty file path should never get this far");
    
    // Check the filename extension first.
    std::string ext = path.extension();
    if (!ext.empty())
    {
        for (auto& mapping : extensions)
        {
            // all our filename extensions are ASCII so a non-UTF8-aware string comparison is sufficient
            if (mapping.case_sensitive)
            {
                if (ext == mapping.extension) { return mapping.typecode; }
            }
            // TODO: this logic is subtly wrong on case-sensitive file systems: it always compares extensions case-insensitively whereas it should [presumably] compare the extension case-sensitively
            else
            {
                if (SDL_strcasecmp(path.extension().c_str(), mapping.extension.c_str()) == 0) { return mapping.typecode; }
            }
        }
    }
    
    // TODO: this should be pushed out into legacy converter:
    /*
    // No extension found or it wasn't recognized, so sniff start of file's data for magic/distinctive bytes:
    DataFile f;
    if (f.open(path)) { return _typecode_unknown; }
    
    SDL_RWops *p = f.borrow_rwops();
    int64_t file_length = f.get_length();
    
    // Check for Sounds file
    {
        f.set_position(0);
        uint32 version = SDL_ReadBE32(p);
        uint32 tag = SDL_ReadBE32(p);
        if ((version == 0 || version == 1) && tag == FOUR_CHARS_TO_INT('s', 'n', 'd', '2')) { return _typecode_sounds; }
    }
    
    // Check for Map/Physics WAD file
    {
        f.set_position(0);
        int file_version = SDL_ReadBE16(p);
        int data_version = SDL_ReadBE16(p);
        if (is_supported_wad_file_version(file_version, data_version))
        {
            SDL_RWseek(p, 68, SEEK_CUR);
            int32 directory_offset = SDL_ReadBE32(p);
            if (directory_offset >= file_length)
                goto not_map;
            f.set_position(128);
            uint32 tag = SDL_ReadBE32(p);
            // ghs: I do not believe this list is comprehensive
            //      I think it's just what we've seen so far?
            switch (tag) {
                case LINE_TAG:
                case POINT_TAG:
                case SIDE_TAG:
                    return _typecode_map;
                    break;
                case MONSTER_PHYSICS_TAG:
                    return _typecode_physics;
                    break;
            }
            
        }
    }
    
not_map:
    // Check for Shapes file // EES: IIRC Shapes files are WAD-like but not 100%
    {
        f.set_position(0);
        for (int i=0; i<32; i++) {
            uint32 status_flags = SDL_ReadBE32(p);
            int32 offset = SDL_ReadBE32(p);
            int32 length = SDL_ReadBE32(p);
            int32 offset16 = SDL_ReadBE32(p);
            int32 length16 = SDL_ReadBE32(p);
            if (status_flags != 0
                || (offset != NONE && (offset >= file_length || offset + length > file_length))
                || (offset16 != NONE && (offset16 >= file_length || offset16 + length16 > file_length))) { goto not_shapes; }
            SDL_RWseek(p, 12, SEEK_CUR);
        }
        return _typecode_shapes;
    }
    */
not_shapes:
    // Not identified
    return _typecode_unknown;
}



// -----------------------------------------------------------------------------------------
// find default scenario files

// From shell_sdl.cpp
std::vector<ao_path> scenario_data_search_paths; // List of directories in which data files are searched for


#ifdef HAVE_STEAM
extern std::vector<item_subscribed_query_result::item> subscribed_workshop_items;

// TODO: not clear what the purpose of defining yet another type is
ItemType typecode_to_item_type(filetype_t file_type)
{
    switch (file_type)
    {
        case _typecode_map:
            return ItemType::Map;
        case _typecode_physics:
            return ItemType::Physics;
        case _typecode_shapes:
            return ItemType::Shapes;
        case _typecode_sounds:
            return ItemType::Sounds;
        default:
            throw_bug_report_f("Invalid file type: %d", file_type);
    }
}
#endif


// this and macros in find_files.hpp replaces get_default_spec
const ao_path find_file_at_subpath(const ao_path& sub_path, filetype_t file_type) 
{
    assert_fail_f(!sub_path.empty(), "invalid file type %d or missing file name '%s'", file_type, sub_path.c_str()); // TODO: should probably be permanent error, or log error and return empty path
    for (const auto& dir : scenario_data_search_paths)
    {
        ao_path path = dir / sub_path;
        if (std::filesystem::is_regular_file(path)) { return path; }
    }
 //   log_warning_f("Didn't find %s file: '%s'", get_filetype_name(file_type).c_str(), sub_path.c_str());
    return ao_path("");
}


// -----------------------------------------------------------------------------------------
// and this replaces the old FindFiles classes

std::filesystem::file_time_type convert_time_to_file_time(time_t t) // TODO: hope this is right
{
    auto timePoint = std::chrono::system_clock::from_time_t(t);
    return std::filesystem::file_time_type::clock::now() + (timePoint - std::chrono::system_clock::now());
}


match_file_proc match_modification_date(std::filesystem::file_time_type time)
{
    return [time](const ao_path& path){
        std::error_code code;
        auto found_time = std::filesystem::last_write_time(path, code);
        return !code && found_time == time;
    };
}


match_file_proc match_modification_date(time_t modification_date)
{
    return match_modification_date(convert_time_to_file_time(modification_date));
}


match_file_proc match_checksum(uint32_t checksum)
{
    return [checksum](const ao_path& path){
        DataFile file;
        if (file.open(path)) { return false; }
        return file.get_map_checksum() == checksum;
    };
}


match_file_proc match_file_type(filetype_t file_type)
{
    return [file_type](const ao_path& path){
        DataFile file;
        uint32_t found_checksum;
        return get_type_of_file(path) == file_type;
    };
}


match_file_proc match_all(std::vector<match_file_proc> procs)
{
    return [procs](const ao_path& path){
        for (const auto& proc : procs)
        {
            if (!proc(path)) return false;
        }
        return true;
    };
}


typedef std::pair<bool, ao_path> depth_directory_t;


const void find_files(std::vector<ao_path>& result, const ao_path& search_dir,
                               match_file_proc proc, bool stop_after_first_match, bool is_recursive)
{
    if (!std::filesystem::is_directory(search_dir))
    {
       // log_warning_f("No directory found at: '%s'", search_dir.c_str());
        return;
    }
    
    std::queue<depth_directory_t> directories;
    directories.push({true, search_dir});
    
    while (!directories.empty())
    {
        auto [is_top_level, dir] = directories.front();
        directories.pop();
        
        // directory iterators do not guarantee order but std::set is ordered
        std::set<depth_directory_t> sub_directories;
        std::set<ao_path> found_files;
        for (const ao_path& path : std::filesystem::directory_iterator(dir)) // TODO: check other directory iterators re. hidden items; consolidate inside a function
        {
            std::string name = path.filename();
            if (name.front() == '.' || name.back() == '~') continue; // ignore hidden items
            if (std::filesystem::is_regular_file(path))
            {
                found_files.insert(path);
            }
            else if (is_recursive && std::filesystem::is_directory(path))
            {
                // ignore top-level Plugins and Scenarios directories
                if (is_top_level && (name == "Plugins" || name == "Scenarios")) continue;
                sub_directories.insert({false, path});
            }
        }
        for (const depth_directory_t& it : sub_directories) { directories.push(it); }
        
        for (const ao_path& path : found_files)
        {
            if (proc(path))
            {
                result.push_back(path);
                if (stop_after_first_match) { return; }
            }
        }
    }
}


const void find_files(std::vector<ao_path>& result,
                      match_file_proc proc, bool stop_after_first_match, bool is_recursive)
{
    for (const ao_path& dir : scenario_data_search_paths)
    {
        find_files(result, dir, proc, stop_after_first_match, is_recursive);
    }
}


ao_path find_scenario_file(match_file_proc proc)
{
    std::vector<ao_path> result;
#ifdef HAVE_STEAM
    auto item_type = typecode_to_item_type(file_type);
    for (const auto& item : subscribed_workshop_items)
    {
        if (item_type == item.item_type)
        {
            find_file_with_type_and_proc(result, item.install_folder_path, proc);
            if (!result.empty()) { return result[0]; }
        }
    }
#endif
    find_files(result, proc);
    return result.empty() ? ao_path("") : result[0]; 
}


void find_mml_files_in_directory(std::set<ao_path>& result, const ao_path& dir)
{
    result.clear();
    // search the MML files in (always case-sensitive) filename order
    // it is unclear why this extra sort is done when it's just looking for first match, but that's how it did it historically
    if (!std::filesystem::is_directory(dir))
    {
      //  log_warning_f("No directory found at: '%s'", dir.c_str());
        return;
    }
    for (const ao_path& path : std::filesystem::directory_iterator(dir))
    {
        // TODO: this conditional is bizarre: since goal appears to be loading .mml files, why not just test for ".mml" extension? (this current test will accept all files except hidden, .lua, or Windows backup)
        if (std::filesystem::is_regular_file(path) && path.extension() == ".mml") // since boost is being sticky, let's try that // TODO: will need to find out why it's so weirdly permissive and if tightening it will cause problems (also, case sensitive, but what's new)
            //&& !boost::algorithm::ends_with(path.filename(), "~")
            //&& path.extension() != ".lua") // people stick Lua scripts in Scripts/
        {
            result.insert(path);
        }
    }
}




ao_path get_random_demo_file()
{
    std::vector<ao_path> demo_files;
    
    // search the Demos/ folder for *.filA files
    for (auto& dir : scenario_data_search_paths)
    {
        ao_path demos_dir = dir / "Demos";
        if (std::filesystem::is_directory(demos_dir))
        {
            for (const ao_path& path : std::filesystem::directory_iterator(demos_dir))
            {
                if (path.extension() == ".filA") { demo_files.push_back(path); }
            }
        }
    }

    if (demo_files.empty())
    {
        return "";
    }
    else
    {
        static auto last_played_index = -1;
        auto index = 0;
        if (demo_files.size() > 1)
        {
            do
            {
                index = local_random() % demo_files.size();
            }
            while (index == last_played_index);
        }
        
        last_played_index = index;
        
        return demo_files[index];
    }
}



// -----------------------------------------------------------------------------------------
// temporarily extend the existing search paths


ScopedSearchPath::ScopedSearchPath(const ao_path& dir) : d{dir}
{
    scenario_data_search_paths.insert(scenario_data_search_paths.begin(), dir);
}

ScopedSearchPath::~ScopedSearchPath()
{
    assert_warn(scenario_data_search_paths.size() && scenario_data_search_paths.front() == d,
                "scenario_data_search_paths is mis-ordered: the path that will be removed is not the path that was added.");
    scenario_data_search_paths.erase(scenario_data_search_paths.begin());
}
