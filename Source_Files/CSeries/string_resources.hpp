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

// * String ID -- this combines a resource ID and a string index into a unique lookup key for any predefined string

typedef int16_t resource_id_t, string_index_t; // TODO: int or uint?

typedef aoerr strid_t; // aliasing to aoerr (uint32_t) allows human-readable error messages to be defined as string resources, providing string var expansion and localization support


#define STRID(resource_id, string_index)  ((strid_t)(((strid_t)(resource_id)) << 16 | (((string_index) & 0xFFFF))))

#define get_string_index(string_id) ((string_index_t)((string_id) & 0xFFFF))
#define get_resource_id(string_id) ((resource_id)((string_id) >> 16))

// * Strings resource (a collection of built-in or MML-defined strings)

typedef std::vector<std::string> strings_t; // as a naming convention we could call it `TYPE_vector_t`, but `TYPEs_t` is tidy
typedef std::map<std::string, std::string> strings_map_t;

// e.g. `w_select` uses this to populate a menu
typedef std::pair<strid_t, std::string> id_string_t; // TODO: replace pair with struct to make code easier to read?
typedef std::vector<id_string_t> id_strings_t;
typedef std::map<strid_t, std::string> id_strings_map_t;


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
// table with the mappings for those names must be passed to `get_string`.
typedef std::vector<string_var_t> string_vars_t;


// -----------------------------------------------------------------------------------------
// Functions


// Expand a string which contains AO's "$NAME$" string variables using built-in and/or custom lookup tables. If the string doesn't contain any known vars, it is returned as-is.
const std::string expand_string_vars(const std::string &string,
                                     const string_vars_t &custom_string_vars = {},
                                     bool expand_built_in_vars = true);


/*
 
 When loading resource strings, strings which are known to contain legacy '%' format codes
 will be automatically converted to use $NAME$ string vars, e.g.

   "Checkpoint %d was not found!"      => "Checkpoint $objectID$ was not found!"
 
   "Controlepunt %d is niet gevonden!" => "Controlepunt $objectID$ is niet gevonden!"
 
   etc.

 IMPORTANT SECURITY FIX:
 
 AO previously passed MML-supplied format strings (e.g. "Checkpoint %d was not found!")
 to vnprintf **unsanitized**. This is an exciting injection vector for accidental/deliberate
 stack overruns according to whatever arbitrary '%' codes an MML string contained!
 
 String vars do not have this flaw as their expansion behavior is tightly controlled:
 
 1. only known var names are processed; unsupported names and random '$' chars are left as-is
 
 2. replacement values must be std::string, so do not require any conversion to insert.
 */


// The supplied strings MUST have contiguous string keys from 0 to N.
void set_strings_for_resource(resource_id_t resource_id, const strings_t& strings);

// Get
// e.g. w_select needs all the strings from a given set to build its dropdown menu
const id_strings_t get_strings_for_resource(resource_id_t resource_id, bool ignoring_empty = false, bool expanding_builtin_vars = false);


// MML can define strings non-contiguously
void set_string(strid_t string_id, const std::string& string);

// Get a string with the unique id.
//
// Standard string vars, e.g. "$appName", are automatically recognized and replaced with their values. If a string requires one or more custom values,
// e.g. "Checkpoint ID $objectID$ not found!", the caller can pass a custom string vars table that supplies those vars' values.
// (If a string var isn't recognized for any reason, it will appear as-is, e.g. "Checkpoint ID $objecctID$ not found!".)
const std::string get_string(strid_t string_id, const string_vars_t custom_string_vars = {});



// expand a relative path that was stored in Prefs/MML as "$default$/custom-dir/my-data.file" to a full path, and vice-versa
const std::string expand_symbolic_path(const std::string& src);
const std::string contract_symbolic_path(const std::string& path);


// -----------------------------------------------------------------------------------------


class InfoTree;
void parse_mml_stringset(const InfoTree& root);
void reset_mml_stringset();


#endif /* string_resources_hpp */
