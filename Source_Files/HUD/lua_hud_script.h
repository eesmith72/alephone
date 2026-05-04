
/*
 lua_hud_script.h -- Implements Lua HUD state and trigger callbacks
 
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

#ifndef __lua_hud_hpp__
#define __lua_hud_hpp__

#include "cseries.hpp"

#include "Canvas.hpp"


// TODO: in future there might be >1 active screen renderer, e.g. 1 for HUD panels, 1 for crosshairs, 1 for on-screen messsages

class HUDRenderer // TODO: merge this shit
{
public:
    HUDRenderer() {}
    ~HUDRenderer() {}
    
    void start_draw() {}
    void end_draw() {}
    
    bool update_everything(short time_elapsed);
    
    Canvas* canvas; // may be SDL, may be OGL; all we care is it's ONE standard API
    
protected:
    bool ForceUpdate;
    
};



HUDRenderer* Lua_HUDInstance();

void Lua_DrawHUD(short time_elapsed);



//void L_Call_HUDInit();
void L_Call_HUDCleanup();
void L_Call_HUDResize();

bool LuaHUDRunning();

void LoadLuaHUDScript();
void UnloadLuaHUDScript();

void SetLuaHUDScriptPath(const std::string& path);
std::string GetLuaHUDScriptPath();

void MarkLuaHUDCollections(bool loading);


#endif /* __lua_hud_hpp__ */
