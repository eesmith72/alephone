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

#include "cspaths.hpp"      // get_application_name
#include "alephversion.h" // A1_DISPLAY_VERSION, etc      // loggingFileName
#include "Scenario.h"     // Scenario->GetName, GetVersion

#include "InfoTree.h"


// TODO: oh dear, looks like someone forgot to update the localized InfoPlist.strings when they renamed "Marathon [...]" to "Classic Marathon [...]"; another haphazard inconsistency to standardize out and PR. Best to do rest of plist cleanup at same time, which may fix file drag-n-drop and make the Xcode proj as minimally painful as it must unavoidably be.


// -----------------------------------------------------------------------------------------
// "$NAME$ string variables


// Redesign the old AO "$NAME$" substitution code:
static const string_vars_t standard_string_vars = {
    {"$appName$",           []{ return get_application_name();              }},
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
    string_key_t string_key; // e.g. STRING_KEY(strERRORS,checkpointNotFound)
    std::string escape;      // e.g. "%d"         // the precise printf code to replace
    std::string string_var;  // e.g. "$objectID$" // the string var to use
};

static const std::array<compatibility_escape_t, 24> compatibility_escapes = {
    STRING_KEY(strERRORS, checkpointNotFound),                          "%d",   "$objectID$",
    STRING_KEY(strERRORS, pictureNotFound),                             "%d",   "$objectID$",
    
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%H",   "$hour$",
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%M",   "$minute$",
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%S",   "$second$",
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%m",   "$month$",
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%d",   "$day$",
    STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _date_format),              "%Y",   "$year$",
    
    STRING_KEY(strNETWORK_GAME_STRINGS, flagPullsFormatString),         "%d",   "$count$",
    STRING_KEY(strNETWORK_GAME_STRINGS, minutesPossessedFormatString),  "%d",   "$minute$",
    STRING_KEY(strNETWORK_GAME_STRINGS, minutesPossessedFormatString),  "%02d", "$second$",
    STRING_KEY(strNETWORK_GAME_STRINGS, pointsFormatString),            "%d",   "$count$",
    
    STRING_KEY(strJOIN_NETWORK_STRINGS, _standard_format),              "%s",   "$type$",
    
                                                                        // TODO: decide namings: generic or specific, e.g. "count" or "suicideCount"?
    STRING_KEY(strNET_STATS_STRINGS, strKILLS_STRING),                  "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strDEATHS_STRING),                 "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strSUICIDES_STRING),               "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strTOTAL_KILLS_STRING),            "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strTOTAL_KILLS_STRING),            "%.2f", "$frequency$",
    STRING_KEY(strNET_STATS_STRINGS, strTOTAL_DEATHS_STRING),           "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strTOTAL_DEATHS_STRING),           "%.2f", "$frequency$",
    STRING_KEY(strNET_STATS_STRINGS, strINCLUDING_SUICIDES_STRING),     "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strINCLUDING_SUICIDES_STRING),     "%.2f", "$frequency$",
    
    STRING_KEY(strNET_STATS_STRINGS, strFRIENDLY_FIRE_STRING),          "%d",   "$count$",
    STRING_KEY(strNET_STATS_STRINGS, strTEAM_CARNAGE_STRING),           "%s",   "$team$",
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

const std::string expand_string_vars(const std::string &resource_string, const string_vars_t &custom_string_vars, bool expand_built_in_vars)
{
    std::string result;
    result.reserve(resource_string.size() * 2);
    
    int32_t var_name_start_index = -1;
    for (char c : resource_string)
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
                    //std::cout << "get_resource_string found key: '" << key << "' => '" << value << "'\n";
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

static keyed_strings_map_t strings_by_key; // every loaded resource string has a key of form `STRING_KEY(resource_id,string_id)`, e.g. `STRING_KEY(strERRORS,badProcessor)` for one-stop lookup


inline void replace_printf_codes(const string_key_t& string_key, std::string& utf8_string)
{
    // repeatedly calling replace_all is a bit lazy but on a small number of short strings it's fast enough
    for (const compatibility_escape_t& conversion : compatibility_escapes)
    {
        if (conversion.string_key == string_key)
        {
            boost::replace_all(utf8_string, conversion.escape, conversion.string_var);
        }
    }
    boost::replace_all(utf8_string, "%%", "%");
}


void load_string_resources(resource_id_t resource_id, const std::vector<std::string> strings)
{
    for (int32_t i = 0; i < strings.size(); i++)
    {
        string_key_t key = STRING_KEY(resource_id, i);
        std::string s = strings[i];
        replace_printf_codes(key, s);
        strings_by_key[key] = s;
    }
    std::vector<std::string> x = strings;
}


static void set_string_resources(resource_id_t resource_id, int16_t string_id, const std::string& string)
{
    strings_by_key[STRING_KEY(resource_id, string_id)] = string;
}


// get the string for the given key, with string var expansion

const std::string get_resource_string(string_key_t string_key, const string_vars_t custom_string_vars) // e.g. STRING_KEY(strCOMPUTER_TERMINAL_LABELS, _m1_marathon_name)
{
    keyed_strings_map_t::const_iterator it = strings_by_key.find(string_key);
    std::cout << "get_resource_string: " << (string_key >> 16) << ", " << (uint16_t)string_key << ": ";
    if (it == strings_by_key.end())
    {
        std::cout << "not found.\n";
        return "";
    }
    std::string result = expand_string_vars(it->second, custom_string_vars);
    std::cout << " => '" << result << "'\n";
    return result;
}


// -----------------------------------------------------------------------------------------
// get all the strings in a resource, e.g. to populate a `w_select` menu

// MML imposes reasonable limits on max number of resources and max number of strings per resource
#define RESOURCE_STRINGS_MAX_SIZE (INT16_MAX)

typedef std::pair<string_key_t, std::string> keyed_string_t;
typedef std::vector<keyed_string_t> keyed_strings_t;


// w_select needs all the strings from a given set to build its dropdown menu
const keyed_strings_t get_strings_for_resource(resource_id_t resource_id, bool ignoring_empty, bool expanding_builtin_vars) // expands standard vars only
{
    keyed_strings_t result;
    result.reserve(32);
    for (int32_t i = 0; i < RESOURCE_STRINGS_MAX_SIZE; i++)
    {
        string_key_t key = STRING_KEY(resource_id, i);
        keyed_strings_map_t::const_iterator it = strings_by_key.find(key);
        if (it == strings_by_key.end()) { break; } // built-in string resources are contiguous
        const std::string& value = it->second;
        if (value.empty() && ignoring_empty) { continue; } // ignore any empty entries
        result.push_back({key, (expanding_builtin_vars ? get_resource_string(key) : it->second)});
    }
    return result;
}


const int32_t count_strings_for_resource(resource_id_t resource_id) // expands standard vars only
{
    for (int32_t i = 0; i < RESOURCE_STRINGS_MAX_SIZE; i++)
    {
        if (strings_by_key.find(STRING_KEY(resource_id, i)) == strings_by_key.end()) { return i; }
    }
    return 0;
}


//-----------------------------------------------------------------------------------------
// TODO: move these to cspaths or Files/find_file


extern DirectorySpecifier bundle_data_dir, default_data_dir, local_data_dir; // TODO: ugh, but it needs to relocate later


static strings_map_t expanded_paths;


const std::string expand_symbolic_path(const std::string& path)
{
    std::string result = expand_string_vars(path, {
#if defined(HAVE_BUNDLE_NAME)
        {"$bundle$",  []{ return bundle_data_dir.GetPath();  }},
#endif
        {"$default$", []{ return default_data_dir.GetPath(); }}, //default first in case user installed his game in his local data dir
        {"$local$",   []{ return local_data_dir.GetPath();   }},
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
    load_string_resources_builtin(); // EES: we gonna reset this bad boy now, oh yes
}


void parse_mml_stringset(const InfoTree& root)
{
    int16_t resource_id;
    if (root.read_indexed("index", resource_id, RESOURCE_STRINGS_MAX_SIZE)) // TODO: what about -ve IDs (assuming int16)? e.g. might want to reserve those for Lua scripts' use
    {
       // int32_t max_string_id = count_strings_for_resource(resource_id);
        
        for (const InfoTree &child : root.children_named("string"))
        {
            int16_t string_id;
            if (child.read_indexed("index", string_id, RESOURCE_STRINGS_MAX_SIZE))
            {
                set_string_resources(resource_id, string_id, child.get_value<std::string>(""));
            }
            else
            {
                log_warning_f("Ignoring invalid string ID %d (should be 0-127) in resource %d.", string_id, resource_id);
            }
        }
    }
    else
    {
        log_warning_f("Ignoring invalid string resource ID %d (should be 0-127).", resource_id);
    }
        
}
