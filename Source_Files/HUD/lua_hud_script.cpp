/*
 lua_hud_script.cpp
 
 Copyright (C) 2009 by Jeremiah Morris and the Aleph One developers
 
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

#include "cseries.hpp"

#include "mouse.h"
#include "interface.hpp"
#include "motion_sensor.hpp"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "hud_definitions.hpp"


#include "preferences.hpp"
#include "Plugins.h"

#include "lua_hud_script.h"
#include "lua_hud_objects.h"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream_buffer.hpp>
namespace io = boost::iostreams;



static const luaL_Reg lualibs[] = {
{"", luaopen_base},
{LUA_TABLIBNAME, luaopen_table},
{LUA_STRLIBNAME, luaopen_string},
{LUA_BITLIBNAME, luaopen_bit32},
{LUA_MATHLIBNAME, luaopen_math},
{LUA_DBLIBNAME, luaopen_debug},
{LUA_IOLIBNAME, luaopen_io},
{NULL, NULL}
};




HUDRenderer *Lua_HUDInstance()
{
    return nullptr; // TODO: FIX
}



void* L_Persistent_Table_Key();


class LuaHUDState
{
public:
	LuaHUDState() : running_(false), inited_(false), num_scripts_(0)
    {
		state_.reset(luaL_newstate(), lua_close);
	}

	virtual ~LuaHUDState() {
	}

public:

	bool Load(const std::string code);
    
	bool Loaded() { return num_scripts_ > 0; }
    
	bool Running() { return running_; }
    
	bool Run_LUA();
	
    void Stop() { running_ = false; }
	
    void MarkCollections(std::set<short>& collections);

	virtual void Initialize()
    {
		const luaL_Reg *lib = lualibs;
		for (; lib->func; lib++)
		{
			luaL_requiref(State(), lib->name, lib->func, 1);
			lua_pop(State(), 1);
		}
		
		RegisterFunctions();
	}

	void SetSearchPath(const ao_path& path)
    {
		L_Set_Search_Path(State(), path);
	}

protected:
	bool GetTrigger(const char *trigger);
	void CallTrigger(int numArgs = 0);

	virtual void RegisterFunctions();

	std::shared_ptr<lua_State> state_;
	lua_State* State() { return state_.get(); }

public:
	// triggers
	void Init();
	void Draw();
	void Resize();
	void Cleanup();

private:
	bool running_;
	int num_scripts_;
    bool inited_;
};


LuaHUDState *hud_state = NULL; // TODO: how does LuaHUDState state differ from LuaState? and why don't all the classes derive from a single, common base class?


bool LuaHUDState::GetTrigger(const char* trigger)
{
	if (!running_)
		return false;

	lua_getglobal(State(), "Triggers");
	if (!lua_istable(State(), -1))
	{
		lua_pop(State(), 1);
		return false;
	}

	lua_pushstring(State(), trigger);
	lua_gettable(State(), -2);
	if (!lua_isfunction(State(), -1))
	{
		lua_pop(State(), 2);
		return false;
	}

	lua_remove(State(), -2);
	return true;
}


void LuaHUDState::CallTrigger(int numArgs)
{
	if (lua_pcall(State(), numArgs, 0, 0) == LUA_ERRRUN)
		L_Error(lua_tostring(State(), -1));
}


void LuaHUDState::Init()
{
	if (GetTrigger("init"))
		CallTrigger();
    inited_ = true;
}


void LuaHUDState::Draw()
{
    if (!inited_)
        return;
	if (GetTrigger("draw"))
		CallTrigger();
}


void LuaHUDState::Resize()
{
    if (!inited_)
        return;
	if (GetTrigger("resize"))
		CallTrigger();
}


void LuaHUDState::Cleanup()
{
    if (!inited_)
        return;
	if (GetTrigger("cleanup"))
		CallTrigger();
    inited_ = false;
}


void LuaHUDState::RegisterFunctions()
{
	Lua_HUDObjects_register(State());
}


bool LuaHUDState::Load(const std::string code)
{
    // TODO: move these messages to string resource, return ao_err
	int status = luaL_loadbufferx(State(), code.data(), code.size(), "HUD Lua", "t");
	if (status == LUA_ERRRUN)
        log_warning("Lua loading failed: error running script.");
	if (status == LUA_ERRFILE)
        log_warning("Lua loading failed: error loading file.");
	if (status == LUA_ERRSYNTAX) {
        log_warning("Lua loading failed: syntax error.");
        log_warning(lua_tostring(State(), -1));
	}
	if (status == LUA_ERRMEM)
        log_warning("Lua loading failed: error allocating memory.");
	if (status == LUA_ERRERR)
        log_warning("Lua loading failed: unknown error.");

	num_scripts_ += ((status == 0) ? 1 : 0);
	return (status == 0);
}


bool LuaHUDState::Run_LUA()
{
	if (!Loaded()) return false;

	int result = 0;
	// Reverse the functions we're calling
	for (int i = 0; i < num_scripts_ - 1; ++i)
		lua_insert(State(), -(num_scripts_ - i));

	// Call 'em
	for (int i = 0; i < num_scripts_; ++i)
		result = result || lua_pcall(State(), 0, LUA_MULTRET, 0);
	
	if (result == 0) running_ = true;
	return (result == 0);
}


void LuaHUDState::MarkCollections(std::set<short>& collections)
{
	if (!running_)
		return;
    
	lua_getglobal(State(), "CollectionsUsed");
	
	if (lua_istable(State(), -1))
	{
		int i = 1;
		lua_pushnumber(State(), i++);
		lua_gettable(State(), -2);
		while (lua_isnumber(State(), -1))
		{
			short collection_index = static_cast<short>(lua_tonumber(State(), -1));
			if (collection_index >= 0 && collection_index < NUMBER_OF_COLLECTIONS)
			{
				mark_collection_for_loading(collection_index);
				collections.insert(collection_index);
			}
			lua_pop(State(), 1);
			lua_pushnumber(State(), i++);
			lua_gettable(State(), -2);
		}
        
		lua_pop(State(), 2);
	}
	else if (lua_isnumber(State(), -1))
	{
		short collection_index = static_cast<short>(lua_tonumber(State(), -1));
		if (collection_index >= 0 && collection_index < NUMBER_OF_COLLECTIONS)
		{
			mark_collection_for_loading(collection_index);
			collections.insert(collection_index);
		}
        
		lua_pop(State(), 1);
	}
	else
	{
		lua_pop(State(), 1);
	}
}


bool LuaHUDRunning()
{
	return (hud_state && hud_state->Running());
}



void Lua_DrawHUD(short time_elapsed)
{
    if (!hud_state) return;
    
    update_motion_sensor_blips(time_elapsed);
    
   // HUDRenderer* hud = Lua_HUDInstance();
  //  hud->start_draw();
  //  hud_state->Draw();
  //  hud->end_draw();
}


void L_Call_HUDResize()
{
	if (hud_state)
		hud_state->Resize();
}




void SetLuaHUDScriptSearchPath(const ao_path& directory)
{
	hud_state->SetSearchPath(directory);
}



void LoadLuaHUDScript()
{
    // TODO: there's several of these 'load script' functions, all very samey; would be nice to consolidate if practical
    const Plugin* hud_lua_plugin = Plugins::instance()->find_hud_lua();
    if (!hud_lua_plugin) return;
    
    ao_path path = expand_file_path(hud_lua_plugin->hud_lua, hud_lua_plugin->directory);
    
    DataFile file;
    ao_err err = file.open(path, DataFile::mode_text_read);
    if (err) return;
    
    int64_t script_length = file.get_length();
    
    std::string script_buffer;
    script_buffer.resize(script_length);
    file.read(script_length, &script_buffer[0]);
    
    //LoadLuaHUDScript(script_buffer);
    if (!hud_state)
    {
        hud_state = new LuaHUDState();
        hud_state->Initialize();
    }
    if (!hud_state->Load(script_buffer)) return; // Load can fail with a variety of errors; it would be good if those

    //was: L_Call_HUDInit();
    hud_state->Init(); // EES: TODO: moved here from start_game; pretty confident LuaHUDState::Initialize should be the one to call Load (loads the Lua script) and Init (calls the script's `init` hander), but unfucking the code paths in Lua support is a journey of its own
    
    if (!hud_lua_plugin->directory.empty())
    {
        SetLuaHUDScriptSearchPath(hud_lua_plugin->directory);
    }
    
    hud_state->Run_LUA();
}


void UnloadLuaHUDScript()
{
    //L_Call_HUDCleanup();
    if (hud_state) hud_state->Cleanup();
	delete hud_state;
	hud_state = NULL;
}


void MarkLuaHUDCollections(bool loading)
{
	static std::set<short> collections;
	if (loading)
	{
		collections.clear();
        if (hud_state)
            hud_state->MarkCollections(collections);
	}
	else
	{
		for (std::set<short>::iterator it = collections.begin(); it != collections.end(); it++)
		{
			mark_collection_for_unloading(*it);
		}
	}
}
