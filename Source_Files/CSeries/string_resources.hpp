/*
 string_resources.hpp
 
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
 
 Originally called "string sets", but who wants "ss_" as their prefix
 in this day and age.
 
 (Originally they were defined in the 'text' resource of the Marathon 2
 app's resource fork so this is technically correct, which is the best
 kind of correct. See also: ResEdit.)
 */


#ifndef string_resources_hpp
#define string_resources_hpp

#include "csstrings.hpp"


// TODO: Putting string_resources files in CSeries isn't ideal as parse_mml_strings() and reset_mml_strings() contain dependencies outside of CSeries: XML/ (though moving MML unpacking functions into MML/ would address that), Files and Scenario (needed for string var expansion).

// TODO: load_string_resources_from_scenario() which resets the current strings to the built-in defaults before loading scenario's MML-defined strings on top (this might be better over in Scenario/)


// -----------------------------------------------------------------------------------------
// Types

// * String key (resource ID + string ID are now combined as a single lookup key)

typedef int16_t resource_id_t, string_id_t; // TODO: int or uint?

typedef aoerr string_key_t; // resource + string IDs; also used as error codes

#define STRING_KEY(resource_id, string_id)  ((string_key_t)(((string_key_t)(resource_id)) << 16 | (string_key_t)(string_id)))


// * Strings resource (a collection of built-in or MML-defined strings)

typedef std::vector<std::string> strings_t; // as a naming convention we could call it `TYPE_vector_t`, but `TYPEs_t` is tidy
typedef std::map<std::string, std::string> strings_map_t;

// e.g. `w_select` uses this to populate a menu
typedef std::pair<string_key_t, std::string> keyed_string_t; // TODO: replace pair with struct to make code easier to read?
typedef std::vector<keyed_string_t> keyed_strings_t;
typedef std::map<string_key_t, std::string> keyed_strings_map_t;


// * String var tables are used to expand any "$NAME$" vars in a string.

// A function (typically a simple lambda) that returns a string when called. Used in string vars tables, e.g. to return the app's name
// "Classic Marathon" when "$appName$" is looked up. This extra indirection hides all the messiness of fetching these values from AO's
// many and varied APIs.
typedef std::function<std::string()> get_string_fn;

// Pair a string var's name with the function that returns its value.
typedef std::pair<std::string, get_string_fn> string_var_t;


// A string variable expansion table associates string var names - "$appName$", "$appVersion$", etc - with functions that
// return their values ("Classic Marathon", "1.11", etc).
//
// AO's standard table is built-in so those var names can be used in any resource string.
//
// To expand resource strings which contain custom var names, e.g. "Checkpoint $objectID$ was not found!", a custom expansion
// table with the mappings for those names must be passed to `get_resource_string`.
typedef std::vector<string_var_t> string_vars_t;


// -----------------------------------------------------------------------------------------
// Functions

// Expand a string which contains AO's "$NAME$" string variables using built-in and/or custom lookup tables. If the string doesn't contain any known vars, it is returned as-is.
const std::string expand_string_vars(const std::string &resource_string, const string_vars_t &custom_string_vars = {}, bool expand_built_in_vars = true);


// Load all strings from the given string set into the active resource strings table.
/*
 Important: Resource strings known to contain legacy '%' format codes will be automatically converted to use string var names, e.g.

   "Checkpoint %d was not found!"      => "Checkpoint $objectID$ was not found!"
   "Controlepunt %d is niet gevonden!" => "Controlepunt $objectID$ is niet gevonden!"
   etc.

 IMPORTANT SECURITY FIX: AO previously passed arbitrary MML-supplied format strings to vnprintf **unsanitized**, an
 accidental/malicious injection vector for exciting stack overruns depending on what '%' codes a given MML string contained!
 String vars do not have this flaw as their behavior is tightly controlled: 1. only known var names are processed and 2. their
 replacement values are always std::string so can be safely inserted as-is.
 */
void load_string_resources(resource_id_t resource_id, const strings_t strings);


// Get a string with the specified key (use the STRING_KEY macro to combine a resource ID with a string ID). Standard string vars,
// e.g. "$appName", are automatically recognized and replaced with their values. If a string requires one or more custom values,
// e.g. "Checkpoint ID $objectID$ not found!", the caller can pass a custom string vars table that supplies those vars' values.
// (If a string var isn't recognized for any reason, it will appear as-is, e.g. "Checkpoint ID $objecctID$ not found!".)
const std::string get_resource_string(string_key_t string_key, const string_vars_t custom_string_vars = {});


// w_select needs all the strings from a given set to build its dropdown menu
const keyed_strings_t get_strings_for_resource(resource_id_t resource_id, bool ignoring_empty = false, bool expanding_builtin_vars = false);


// expand a relative path that was stored in Prefs/MML as "$default$/custom-dir/my-data.file" to a full path, and vice-versa
const std::string expand_symbolic_path(const std::string& src);
const std::string contract_symbolic_path(const std::string& path);


// -----------------------------------------------------------------------------------------


class InfoTree;
void parse_mml_stringset(const InfoTree& root);
void reset_mml_stringset();


#endif /* string_resources_hpp */
