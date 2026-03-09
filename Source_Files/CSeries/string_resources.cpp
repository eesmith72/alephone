/*
 string_resources.cpp
 
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

#include "string_resources.hpp"

#include "cspaths.hpp"      // get_application_name, get_local_storage_dir, etc
#include "alephversion.h" // A1_DISPLAY_VERSION, etc      // loggingFileName
#include "Scenario.h"     // Scenario->GetName, GetVersion

#include "InfoTree.h"


// TODO: oh dear, looks like someone forgot to update the localized InfoPlist.strings when they renamed "Marathon [...]" to "Classic Marathon [...]"; another haphazard inconsistency to standardize out and PR. Best to do rest of plist cleanup at same time, which may fix file drag-n-drop and make the Xcode proj as minimally painful as it must unavoidably be.


// -----------------------------------------------------------------------------------------
// "$NAME$ string variables


// Redesign the old AO "$NAME$" substitution code:
static const string_vars_t standard_string_vars = {
    {"$appName$",           []{ return A1_DISPLAY_NAME;                     }},
    {"$appVersion$",        []{ return A1_DISPLAY_VERSION;                  }},
    {"$appLongVersion$",    []{ return A1_VERSION_STRING;                   }},
    {"$appDate$",           []{ return A1_DISPLAY_DATE_VERSION;             }},
    {"$appPlatform$",       []{ return A1_DISPLAY_PLATFORM;                 }},
    {"$appURL$",            []{ return A1_HOMEPAGE_URL;                     }},
  //{"$appLogFile$",        []{ return loggingFileName();                   }},
    {"$scenarioName$",      []{ return Scenario::instance()->GetName();     }},
    {"$scenarioVersion$",   []{ return Scenario::instance()->GetVersion();  }},
};


// Auto-convert the unsafe "%FMT" codes into safe "$NAME$" codes for backwards compatibility with existing MMLs:
// The good news is that none of the original strings contain duplicate escapes (e.g. "%d:%02d" but never "%d/%d"),
// so we can hardcode the exact escape sequences used in the original strings (e.g. "%s", "%02d") and match those
// sequences only (e.g. the original "%.2f" will match in kills/deaths/suicides-per-minute strings, thougn not "%.1f").
struct compatibility_escape_t
{
    strid_t string_id; // e.g. STRID(strERRORS,checkpointNotFound)
    std::string escape;      // e.g. "%d"         // the precise printf code to replace
    std::string string_var;  // e.g. "$objectID$" // the string var to use
};

static const std::array<compatibility_escape_t, 24> compatibility_escapes = {
    STRID(strERRORS, checkpointNotFound),                          "%d",   "$objectID$",
    STRID(strERRORS, pictureNotFound),                             "%d",   "$objectID$",
    
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%H",   "$hour$",
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%M",   "$minute$",
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%S",   "$second$",
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%m",   "$month$",
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%d",   "$day$",
    STRID(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%Y",   "$year$",
    
    STRID(strNETWORK_GAME_STRINGS, flagPullsFormatString),         "%d",   "$count$",
    STRID(strNETWORK_GAME_STRINGS, minutesPossessedFormatString),  "%d",   "$minute$",
    STRID(strNETWORK_GAME_STRINGS, minutesPossessedFormatString),  "%02d", "$second$",
    STRID(strNETWORK_GAME_STRINGS, pointsFormatString),            "%d",   "$count$",
    
    STRID(strJOIN_NETWORK_STRINGS, _standard_format),              "%s",   "$type$",
    
                                                                        // TODO: decide namings: generic or specific, e.g. "count" or "suicideCount"?
    STRID(strNET_STATS_STRINGS, strKILLS_STRING),                  "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strDEATHS_STRING),                 "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strSUICIDES_STRING),               "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strTOTAL_KILLS_STRING),            "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strTOTAL_KILLS_STRING),            "%.2f", "$frequency$",
    STRID(strNET_STATS_STRINGS, strTOTAL_DEATHS_STRING),           "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strTOTAL_DEATHS_STRING),           "%.2f", "$frequency$",
    STRID(strNET_STATS_STRINGS, strINCLUDING_SUICIDES_STRING),     "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strINCLUDING_SUICIDES_STRING),     "%.2f", "$frequency$",
    
    STRID(strNET_STATS_STRINGS, strFRIENDLY_FIRE_STRING),          "%d",   "$count$",
    STRID(strNET_STATS_STRINGS, strTEAM_CARNAGE_STRING),           "%s",   "$team$",
};


const std::string expand_string_var(const string_vars_t& string_vars, const std::string& key)
{
    for (const string_var_t& pair : string_vars)
    {
        if (key.compare(pair.first) == 0) { return pair.second(); }
    }
    return "";
}


#define NO_ESCAPE (-1)

const std::string expand_string_vars(const std::string &string,
                                     const string_vars_t &custom_string_vars,
                                     bool expand_built_in_vars)
{
    std::string result;
    result.reserve(string.size() * 2);
    
    int32_t var_name_start_index = -1;
    for (char c : string)
    {
        switch (c)
        {
            case '$':
                result += c; // The characters will be copied over in case it's not a [known] var name.
                if (var_name_start_index == NO_ESCAPE) // found start of a possible "$...$" var name sequence
                {
                    var_name_start_index = (int32_t)result.size() - 1;
                }
                else // found end of a "$...$" sequence, so attempt to expand it
                {
                    // process escape; the custom lookup table (if given) has precedence over the built-in table
                    int32_t var_name_end_index = (int32_t)result.size() - 1;
                    std::string key = result.substr(var_name_start_index, var_name_end_index + 1); // "$NAME$"
                    std::string value = expand_string_var(custom_string_vars, key);
                    if (expand_built_in_vars && value.empty())
                    {
                        value = expand_string_var(standard_string_vars, key);
                    }
                    //std::cout << "get_string found key: '" << key << "' => '" << value << "'\n";
                    if (value.empty()) // If this "$...$" sequence isn't a recognized var name (or just ordinary '$' chars)...
                    {
                        var_name_start_index = var_name_end_index + 1; // ...start a new match on its trailing "$".
                    }
                    else // The var name was successfully expanded, so replace it in the output string:
                    {
                        result.resize(var_name_start_index);
                        result += value;
                        var_name_start_index = NO_ESCAPE;
                    }
                }
                break;
                
            case '\r':
            case '\n':
                result += '\n';
                break;
                
            case '\t':
                result += "  "; // not sure what to do with tabs
                break;
                
            default:
                if (c >= 0x20 && c != 0x7f)
                {
                    result += c;
                }
        }
    }
    return result;
}


// -----------------------------------------------------------------------------------------
// the custom and/or default strings currently loaded by/for the current scenario 

static id_strings_map_t strings_by_id; // every loaded resource string has a unique ID of form `STRID(resource_id,string_index)`, e.g. `STRID(strERRORS,badProcessor)` for one-stop lookup


// add a single string to the global table
void set_string(strid_t string_id, const std::string& string)
{
    std::string tmp = string; // replace_printf_codes will modify the string in-place so make a copy first
    
    // repeatedly calling replace_all is lazy and not entirely robust, but on a small number of short strings with expected structure it's "good enough"
    for (const compatibility_escape_t& conversion : compatibility_escapes)
    {
        if (conversion.string_id == string_id)
        {
            boost::replace_all(tmp, conversion.escape, conversion.string_var);
        }
    }
    boost::replace_all(tmp, "%%", "%");

    strings_by_id[string_id] = tmp;
}


const std::string get_string(strid_t string_id, const string_vars_t custom_string_vars) // e.g. STRID(strCOMPUTER_TERMINAL_LABELS, _m1_marathon_name)
{
    id_strings_map_t::const_iterator it = strings_by_id.find(string_id);
    //std::cout << "get_string: " << (string_id >> 16) << ", " << (uint16_t)string_id << ": ";
    if (it == strings_by_id.end())
    {
        log_warning_f("string not found: %d %d", string_id >> 16, string_id & 0xffff);
        return "";
    }
    std::string result = expand_string_vars(it->second, custom_string_vars);
    return result;
}


// -----------------------------------------------------------------------------------------
// get all the strings in a resource, e.g. to populate a `w_select` menu

// MML imposes reasonable limits on max number of resources and max number of strings per resource // TODO: what's an appropriate limit here?
#define MAX_RESOURCE_IDS    (2048)
#define MAX_STRING_INDEXES  (255)

// primarily used in string_resources_std to add the default strings, which must be done at startup and before switching scenarios; could also be used to add e.g. menu labels; string keys MUST be contiguous, 0..N
void set_strings_for_resource(resource_id_t resource_id, const strings_t& strings)
{
    for (int32_t string_id = 0; string_id < strings.size(); string_id++)
    {
        set_string(STRID(resource_id, string_id), strings[string_id]);
    }
}

// w_select needs all the strings from a given set to build its dropdown menu
const id_strings_t get_strings_for_resource(resource_id_t resource_id, bool ignoring_empty, bool expanding_builtin_vars) // expands standard vars only
{
    id_strings_t result;
    result.reserve(32);
    for (int32_t i = 0; i < MAX_RESOURCE_IDS; i++)
    {
        strid_t key = STRID(resource_id, i);
        id_strings_map_t::const_iterator it = strings_by_id.find(key);
        if (it == strings_by_id.end()) { break; } // built-in string resources are contiguous
        const std::string& value = it->second;
        if (value.empty() && ignoring_empty) { continue; } // ignore any empty entries
        result.push_back({key, (expanding_builtin_vars ? get_string(key) : it->second)});
    }
    return result;
}


const int32_t count_strings_for_resource(resource_id_t resource_id) // expands standard vars only
{
    for (int32_t i = 0; i < MAX_RESOURCE_IDS; i++)
    {
        if (strings_by_id.find(STRID(resource_id, i)) == strings_by_id.end()) { return i; }
    }
    return 0;
}


//-----------------------------------------------------------------------------------------
// TODO: move these to cspaths or Files/find_file


static strings_map_t expanded_paths;


const std::string expand_symbolic_path(const std::string& path)
{
    std::string result = expand_string_vars(path, {
#if defined(HAVE_BUNDLE_NAME)
        {"$bundle$",  []{ return get_macos_app_bundle_game_data_dir(); }},
#endif
        {"$default$", []{ return get_default_game_data_dir();          }},
        {"$local$",   []{ return get_local_storage_dir();              }},
    }, false);
    expanded_paths[result] = path;
    return result;
}


const std::string contract_symbolic_path(const std::string& path)
{
    strings_map_t::const_iterator it = expanded_paths.find(path);
    return it != expanded_paths.end() ? it->second : path;
}


//-----------------------------------------------------------------------------------------
// deserialization


void reset_mml_stringset()
{
    load_standard_strings(); // EES: we gonna reset this bad boy now, oh yes
}


// TODO: what about sanitizing strFILENAMES' strings? (must be valid filenames, non-empty, no leading period)

void parse_mml_stringset(const InfoTree& root)
{
    int16_t resource_id;
    if (root.read_indexed("index", resource_id, MAX_RESOURCE_IDS)) // TODO: what about -ve IDs (assuming int16)? e.g. might want to reserve those for Lua scripts' use (not going to support in XML)
    {
        for (const InfoTree& child : root.children_named("string"))
        {
            int16_t string_index;
            if (child.read_indexed("index", string_index, MAX_STRING_INDEXES))
            {
                std::string value = child.get_value<std::string>("");
                set_string(STRID(resource_id, string_index), value);
            }
            else
            {
                log_warning_f("Ignoring bad string index (should be 0-%d) in resource %d.",
                              MAX_STRING_INDEXES, resource_id);
            }
        }
    }
    else
    {
        std::ostringstream stream;
        root.save_xml(stream);
        std::string s;
        stream.str(s);
        log_warning_f("Ignoring bad string resource ID (should be 0-%d): %s", MAX_RESOURCE_IDS, s.c_str());
    }
}
