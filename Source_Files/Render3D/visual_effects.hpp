/*
 visual_effects.hpp (was fades.h) -- apply gameworld tint and damage effects
 
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

#ifndef visual_effects_hpp
#define visual_effects_hpp


#include "cseries.hpp"

#include "visual_effect_definitions.hpp"


//-----------------------------------------------------------------------------


// Set up a non-blocking damage/pickup effect (this may be ignored if a damage effect is already running)
void start_gameworld_damage_effect(short type);


void start_gameworld_tint_effect(short type); // player has submerged into liquid

void stop_gameworld_tint_effect(); // player has emerged from liquid


void update_gameworld_visual_effects(); // periodically called by `update_world` in marathon2.cpp

void reset_gameworld_view_effects(); // discard any active fades; called when entering/exiting level





void ogl_apply_visual_effects(float Left, float Top, float Right, float Bottom);



#endif /* visual_effects_hpp */
