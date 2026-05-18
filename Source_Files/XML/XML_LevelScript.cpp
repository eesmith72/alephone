/*
 XML_LevelScript.cpp
 
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

#include "XML_LevelScript.h"


// TODO: the more MML can be replaced with Lua scripts, the better; we'll need some JSON for basics (plugin descriptions, manifests) and for scenario data (Shapes files should be converted to DDS/PNG sprite sheets and JSON for the collection data)

#include "map_wad.h"
#include "Music.h"
#include "XML_ParseTreeRoot.h"
#include "InfoTree.h"
#include "Plugins.h"
#include "images.h"
#include "lua_script.h"

#include "interface_support.hpp" // M1_/M2_EPILOGUE_SCREEN_BASE

#include "AStream.h"
#include "map.h"


// The "command" is an instruction to process a file/resource in a certain sort of way
struct LevelScriptCommand
{
	// Types of commands
	enum {
		MML,
		Music,
		Movie,
		Lua,
		LoadScreen, // no longer supported, but the enum remains so as not to break compatibility with existing MMLs
	};
	int Type;
	
	enum {
		UnsetResource = -32768
	};
	
	// Where to read the command data from:
	
	// This is a MacOS resource ID
	short RsrcID;
	
	bool RsrcPresent() {return RsrcID != UnsetResource;}
	
	// This is a Unix-style filespec, with form
	// <dirname>/<dirname>/<filename>
	// with the root directory being the map file's directory
	ao_path FileSpec; // TODO: ensure correct handling of path separators
	
	// Additional data:
	
	// Relative size for a movie (negative means don't use)
	float Size;

	// Additional load screen information:
	short T, L, B, R;
	ao_rgb Colors[2];
	bool Stretch;
	bool Scale;
	
	LevelScriptCommand(): RsrcID(UnsetResource), Size(NONE), L(0), T(0), R(0), B(0), Stretch(true), Scale(true) {
		memset(&Colors[0], 0, sizeof(ao_rgb));
		memset(&Colors[1], 0xff, sizeof(ao_rgb));
	}
};

struct LevelScriptHeader
{
	// Special pseudo-levels: restoration of previous parameters, defaults for this map file,
	// and the end of the game
	enum {
		Restore = -3,
		Default = -2,
		End = -1
	};
	
	std::vector<LevelScriptCommand> Commands;
	
	// Thanx to the Standard Template Library,
	// the copy constructor and the assignment operator will be automatically implemented
	
	// Whether the music files are to be played in random order;
	// it defaults to false (sequential order)
	bool RandomOrder;
	
	LevelScriptHeader(): RandomOrder(false) {}
};

// Scripts for current map file
static std::map<int, LevelScriptHeader> LevelScripts;







// For selecting the end-of-game screens - what fake level index for them, and how many to display
// (resource numbers increasing in sequence) 
// (defaults from interface.cpp)
static short EndScreenIndex = 99;
static short NumEndScreens = 1;


void get_epilogue_screen_base_id_and_count(int32_t& end_offset, int32_t& end_count)
{
    if (shapes_file_is_m1()) // should check map, but this is easier
    {
        // ignore M2 defaults set in read_scripts_from_current_map()
        end_offset = M1_EPILOGUE_SCREEN_BASE + 100;
        end_count  = 2;
    }
    else
    {
        end_offset = M2_EPILOGUE_SCREEN_BASE + EndScreenIndex;
        end_count  = NumEndScreens;
    }
}


// The level-script parsers are separate from the main MML ones,
// because they operate on per-level data.

// Parse marathon_levels script
static void parse_levels_xml(InfoTree& root);






static bool load_mml_files_from_directory(ao_path dir, bool load_menu_mml_only)
{
    // Get sorted list of files in directory
    std::set<ao_path> paths; // case-sensitive order
    find_mml_files_in_directory(paths, dir);
    if (paths.empty()) return false;
    
    // Parse each file
    for (const ao_path& path : paths)
    {
        ParseMMLFromFile(path, load_menu_mml_only);
    }
    
    return true;
}


void LoadBaseMMLScripts(bool load_menu_mml_only)
{
    for (const ao_path& path : scenario_data_search_paths)
    {
      //  log_note_f("searching for MML in: %s", path.c_str());
        load_mml_files_from_directory(path / "MML", load_menu_mml_only);
        load_mml_files_from_directory(path / "Scripts", load_menu_mml_only);
    }
}




// load scripts for the specified level; also works for pseudo-levels, e.g. Default
static void load_scripts_for_level(int LevelIndex);

// Defined in images.cpp
extern bool get_text_resource_from_map(int resource_number, LoadedResource& TextRsrc);


// Reads all those in resource 128 in a Map file (or some appropriate equivalent)
void read_scripts_from_current_map()
{
	// Get rid of the previous level script // TODO: wha...
	// ghs: unless it's the first time, in which case we would be clearing any external level scripts, so don't // EES: that doesn't make sense - level scripts external to the Map shouldn't be entangled with level scripts inside the map; and this is a static flag so it persists for app lifetime even if scenarios change; TODO: it'd be really nice if MML+Lua script loading+running+unloading behavior was documented - understanding how it behaves (or should behave) from modders' POV would make it much, much easier to clean up and organize
	static bool FirstTime = true;
	if (FirstTime)
		FirstTime = false;
	else
		LevelScripts.clear();
	
	// Set these to their defaults before trying to change them
	EndScreenIndex = 99;
	NumEndScreens = 1;
	
	// ResourceFile OFile;
	// if (!MapFile.Open(OFile)) return;
	
	// The script is stored at a special resource ID;
	// simply quit if it could not be found
	LoadedResource ScriptRsrc;
	
	// if (!OFile.Get('T','E','X','T',128,ScriptRsrc)) return;
	if (!get_text_resource_from_map(128,ScriptRsrc)) return;
	
	// Load the script
	std::istringstream strm(std::string((char *)ScriptRsrc.GetPointer(), ScriptRsrc.get_length()));
	try
    {
		InfoTree root = InfoTree::load_xml(strm).get_child("marathon_levels");
		parse_levels_xml(root);
	}
    catch (const InfoTree::Exception& e)
    {
        log_error_f("Error parsing map script in %s: %s", get_current_map_path().c_str(), e.what());
	}
}




// Reload base and default scripts initialize_level_from_wad_data
void load_base_and_default_scripts(int level_number)
{
    // TODO: weird place for this; presumably because MML specifies the music file to play (see load_scripts_for_level); surely we should fade out and clear level music in exit_gameworld so will probably move it there, and fading out startup music should be done elsewhere
    // if music is currently playing, quickly fade it out
    Music::instance()->QuickFade(); // 0.5sec fade
    while (Music::instance()->Playing()) { Music::instance()->Idle(); }
    
    
    Music::instance()->ClearLevelPlaylist();
    
    // reset values to engine defaults first
    ResetAllMMLValues(); // TODO: FIX: this is not good; it's resetting fonts and the dialog theme retains font_t*, which causes crash when dialog is next displayed as those have been freed; as a workaround, themes and dialogs should never retain font_t* but always look up; as a more permanent solution, separate theme MML state from other scenario MML state so that unloading and reloading is more granular (TBH, may want to rebuild scenario file format first, as that enables redesign of Scenario; once that design's right, write scripts to migrate the old MML data to new format; at least we're unknotting scenario loading now so how it works - and its problems - are clear)
    
    // then load the base stuff (from Scripts folder and whatnot)
    LoadBaseMMLScripts(false);
    Plugins::instance()->load_mml(false);
    
    // then the scripts for the new level
    load_scripts_for_level(LevelScriptHeader::Default); // scripts that run for all levels
	load_scripts_for_level(level_number); // scripts that run for this level only; TODO: this is confusing as there are scripts in Map's 'text' resource #128 and there may be scripts in the level WAD data which are loaded by initialize_level_from_wad_data; untwisting the logic suggests that a Map file may define MML/Lua scripts none/either/both ways
}


// Intended to be run at the end of a game
void load_epilogue_scripts() // TODO: unused in AO
{
	load_scripts_for_level(LevelScriptHeader::Default);
	load_scripts_for_level(LevelScriptHeader::End); // TODO: this is only place End scripts are loaded; are they no longer used? (i.e. do modders use the epilogue index instead?)
}


// TODO: because MML is shit
// Intended for restoring old parameter values, because MML sets values at a variety
// of different places, and it may be easier to simply set stuff back to defaults
// by including those defaults in the script.
void load_restore_level_scripts()
{
	load_scripts_for_level(LevelScriptHeader::Default);
	load_scripts_for_level(LevelScriptHeader::Restore);
}


// load the level scripts
void load_scripts_for_level(int LevelIndex)
{
    // Find the pointer to the current script
    auto it = LevelScripts.find(LevelIndex);
    if (it == LevelScripts.end()) return;
    LevelScriptHeader* CurrScriptPtr = &it->second;
    
    // Insures that this order is the last order set
    Music::instance()->SetPlaylistParameters(CurrScriptPtr->RandomOrder);
    
    for (LevelScriptCommand& Cmd : CurrScriptPtr->Commands)
    {
        switch (Cmd.Type)
        {
            case LevelScriptCommand::MML:
            case LevelScriptCommand::Lua:
            {
                LoadedResource ScriptRsrc;
                // if (Cmd.RsrcPresent() && OFile.Get('T','E','X','T',Cmd.RsrcID,ScriptRsrc))
                if (!Cmd.RsrcPresent() || !get_text_resource_from_map(Cmd.RsrcID, ScriptRsrc)) break;
                
                // yuck; really want std::string (utf8)
                char* Data = (char*)ScriptRsrc.GetPointer();
                size_t DataLen = ScriptRsrc.get_length();
                
                if (Cmd.Type == LevelScriptCommand::MML)
                {
                    ParseMMLFromData(Data, DataLen);
                }
                else
                {
                    LoadLuaScript(Data, DataLen, _embedded_lua_script);
                }
                break;
            }
            case LevelScriptCommand::Music:
            {
                ao_path MusicFile = find_file_at_subpath(Cmd.FileSpec);
                if (!MusicFile.empty()) { Music::instance()->PushBackLevelMusic(MusicFile); }
                break;
            }

            // EES: removed LevelScriptCommand::LoadScreen as AO does NOT need a level "Loading..." screen! It needs to load the full current scenario on a background thread while user is on startup screens/main menu, and no longer unload shapes and sounds between levels.
            
            // LevelScriptCommand::Movie is handled below in get_movie_path_for_level
        }
    }
}


ao_path get_movie_path_for_level(int LevelIndex)
{
    ao_path MovieFile;
    
    // Find the pointer to the current script
    if (LevelScripts.find(LevelIndex) != LevelScripts.end())
    {
        for (const LevelScriptCommand& Cmd : LevelScripts[LevelIndex].Commands)
        {
            if (Cmd.Type == LevelScriptCommand::Movie)
            {
                MovieFile = find_file_at_subpath(Cmd.FileSpec);
                if (!MovieFile.empty()) break;
            }
        }
        if (MovieFile.empty() && LevelIndex != LevelScriptHeader::Default)
        {
            MovieFile = get_movie_path_for_level(LevelScriptHeader::Default);
        }
    }
    return MovieFile;
}


//************************************************************************************************
//


// level scripts data gets cached and packed into Map WAD
std::vector<uint8> mml_level_scripts_data;

void unpack_mml_level_scripts_data(uint8_t* data, size_t length)
{
	if (length > 0)
	{
        mml_level_scripts_data.resize(length);
        memcpy(&mml_level_scripts_data[0], data, length);
        
        int32_t offset = 2;
        while (offset < mml_level_scripts_data.size())
        {
            if (offset + 8 + MAX_LEVEL_NAME_LENGTH > mml_level_scripts_data.size())
                break;
            
            AIStreamBE header(&mml_level_scripts_data[offset], 8 + MAX_LEVEL_NAME_LENGTH);
            offset += 8 + MAX_LEVEL_NAME_LENGTH;
            
            uint32 flags;
            char name[MAX_LEVEL_NAME_LENGTH];
            uint32 length;
            header >> flags;
            header.read(name, MAX_LEVEL_NAME_LENGTH);
            name[MAX_LEVEL_NAME_LENGTH - 1] = '\0';
            header >> length;
            if (offset + length > mml_level_scripts_data.size())
                break;
            
            if (length)
            {
                ParseMMLFromData(reinterpret_cast<char *>(&mml_level_scripts_data[offset]), length);
            }
            
            offset += length;
        }
	}
	else
	{
        mml_level_scripts_data.clear();
	}
}

void pack_mml_level_scripts_data(uint8_t* S)
{
    memcpy(S, mml_level_scripts_data.data(), mml_level_scripts_data.size());
    S += mml_level_scripts_data.size();
}

size_t get_length_of_mml_level_scripts_data()
{
    return mml_level_scripts_data.size();
}



std::vector<uint8> lua_level_scripts_data;

void unpack_lua_level_scripts_data(uint8_t* S, size_t length)
{
	if (length > 0)
	{
        lua_level_scripts_data.resize(length);
        memcpy(&lua_level_scripts_data[0], S, length);
        
        int32_t offset = 2;
        while (offset < lua_level_scripts_data.size())
        {
            if (offset + 8 + MAX_LEVEL_NAME_LENGTH > lua_level_scripts_data.size())
                break;

            AIStreamBE header(&lua_level_scripts_data[offset], 8 + MAX_LEVEL_NAME_LENGTH);
            offset += 8 + MAX_LEVEL_NAME_LENGTH;
            
            uint32 flags;
            char name[MAX_LEVEL_NAME_LENGTH];
            uint32 length;
            header >> flags;
            header.read(name, MAX_LEVEL_NAME_LENGTH);
            name[MAX_LEVEL_NAME_LENGTH - 1] = '\0';
            header >> length;
            if (offset + length > lua_level_scripts_data.size())
                break;

            LoadLuaScript(reinterpret_cast<char *>(&lua_level_scripts_data[offset]), length, _embedded_lua_script);
            offset += length;
        }
	}
	else
	{
        lua_level_scripts_data.clear(); // I assume resize(0) does the same?
	}
}

void pack_lua_level_scripts_data(uint8_t* S)
{
    memcpy(S, lua_level_scripts_data.data(), mml_level_scripts_data.size());
}

size_t get_length_of_lua_level_scripts_data()
{
    return lua_level_scripts_data.size();
}






void reset_mml_default_levels() {}


void parse_mml_default_levels(const InfoTree& root) // called by _ParseAllMML in XML_ParseTreeRoot.cpp
{
	LevelScriptHeader *ls_ptr = &LevelScripts[LevelScriptHeader::Default];
	
	for (const InfoTree& child : root.children_named("music"))
	{
		LevelScriptCommand cmd;
		cmd.Type = LevelScriptCommand::Music;
		
		if (!child.read_attr("file", cmd.FileSpec)) continue;
		
		ls_ptr->Commands.push_back(cmd);
	}
	
	for (const InfoTree& child : root.children_named("random_order"))
	{
		child.read_attr("on", ls_ptr->RandomOrder);
	}
}


static void parse_level_commands(InfoTree root, int index)
{
	// Find or create command list for this level
	LevelScriptHeader *ls_ptr = &LevelScripts[index];

	for (const InfoTree& child : root.children_named("mml"))
	{
		LevelScriptCommand cmd;
		cmd.Type = LevelScriptCommand::MML;
		
		if (!child.read_attr_bounded<int16>("resource", cmd.RsrcID, 0, SHRT_MAX))
			continue;
		
		ls_ptr->Commands.push_back(cmd);
	}

	for (const InfoTree& child : root.children_named("lua"))
	{
		LevelScriptCommand cmd;
		cmd.Type = LevelScriptCommand::Lua;
		
		if (!child.read_attr_bounded<int16>("resource", cmd.RsrcID, 0, SHRT_MAX))
			continue;
		
		ls_ptr->Commands.push_back(cmd);
	}
	
	for (const InfoTree& child : root.children_named("music"))
	{
		LevelScriptCommand cmd;
		cmd.Type = LevelScriptCommand::Music;
		
		if (!child.read_attr("file", cmd.FileSpec))
			continue;
		
		ls_ptr->Commands.push_back(cmd);
	}
	
	for (const InfoTree& child : root.children_named("random_order"))
	{
		child.read_attr("on", ls_ptr->RandomOrder);
	}
	
	for (const InfoTree& child : root.children_named("movie"))
	{
		LevelScriptCommand cmd;
		cmd.Type = LevelScriptCommand::Movie;
		
		if (!child.read_attr("file", cmd.FileSpec))
			continue;

		child.read_attr("size", cmd.Size);
		
		ls_ptr->Commands.push_back(cmd);
	}
}


static void parse_levels_xml(InfoTree& root)
{
	for (const InfoTree& child : root.children_named("level"))
	{
		int16 index;
		if (child.read_indexed("index", index, SHRT_MAX+1))
		{
			parse_level_commands(child, index);
		}
	}
	
	for (const InfoTree& child : root.children_named("end"))
	{
		parse_level_commands(child, LevelScriptHeader::End);
	}
	for (const InfoTree& child : root.children_named("default"))
	{
		parse_level_commands(child, LevelScriptHeader::Default);
	}
	for (const InfoTree& child : root.children_named("restore"))
	{
		parse_level_commands(child, LevelScriptHeader::Restore);
	}
	
	for (const InfoTree& child : root.children_named("end_screens"))
	{
		child.read_attr("index", EndScreenIndex);
		child.read_indexed("count", NumEndScreens, SHRT_MAX + 1);
	}
}
