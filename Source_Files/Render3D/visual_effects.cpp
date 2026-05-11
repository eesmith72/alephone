/*
 visual_effects.cpp
 
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

#include "visual_effects.hpp"
//#include "Screen.hpp"
//#include "interface.hpp"

#include "render.h" // classic_renderer_is_active
#include "ClassicRasterizer.h" // [re]set_classic_color_map
//#include "graphics_preferences.hpp"

#include "Music.h"
#include "FilmExporter.h"


// How it works:
//
// 1. In Classic8/16 screen modes, synchronize_classic_color_map populates the 8- and 16-bit color maps in ClassicRasterizer.cpp. Those maps are applied when copying the 8/16-bit pixel values from the 'virtual screen' buffer to RGBA32, ready to upload to GPU texture.
//
// 2. In Modern 32-bit screen modes, OGL_EndMain calls ogl_apply_gameworld_visual_effects after the gameworld is drawn to apply any OGL fade functions to the viewport. (These functions predate modern shaders so use colors and blends.)


//-----------------------------------------------------------------------------
// liquid tint and damage/pickup effects apply to worldview

// these are pointers into the fade definitions table, one for liquid tint, the other for damage/pickup effect

static view_effect_definition_t* active_tint_effect = nullptr;
ao_fixed tint_effect_transparency; // EES: annoyingly, this is in a separate table to the effect definition (I suppose it's possible a tint fader could be used as an effect, in which case it needs its transparency fields to be separate to the tint transparency. But it would be nice if everything was defined in a single table.)

static view_effect_definition_t* active_damage_effect = nullptr;
ao_fixed damage_effect_transparency; // transparency or opacity?
uint64_t damage_effect_last_updated_tick;

#define MINIMUM_FADE_RESTART  (MACHINE_TICKS_PER_SECOND / 2)


//-----------------------------------------------------------------------------
// private


inline int16_t get_phase()
{
    return int16_t((dynamic_world.tick_count - damage_effect_last_updated_tick) * MACHINE_TICKS_PER_SECOND / TICKS_PER_SECOND);
}


// Applies Classic faders to the current Classic 8-bit clut/16-bit gamma curves and updates the ClassicRasterizer's color map.
static void synchronize_classic_color_map()
{
    if (classic_renderer_is_active())
    {
        color_table_t animated_color_table = *get_classic_color_table();
        
        if (active_tint_effect)
        {
            active_tint_effect->classic_fader(active_tint_effect->color, tint_effect_transparency, animated_color_table);
        }
        
        if (active_damage_effect)
        {
            active_damage_effect->classic_fader(active_damage_effect->color, damage_effect_transparency, animated_color_table);
        }
        
        set_classic_color_map(animated_color_table);
    }
}


//-----------------------------------------------------------------------------
// public


void update_gameworld_visual_effects() // called by update_world in marathon2.cpp, Screen::set_gameworld_gamma, and start_gameworld_damage_effect below
{
    if (active_damage_effect)
    {
        int16_t phase = get_phase();
        
        if (phase < active_damage_effect->period || active_damage_effect->period == 0) // effect is currently underway/is instantaneous
        {
            damage_effect_transparency = active_damage_effect->initial_transparency
                                       + (phase * (active_damage_effect->final_transparency - active_damage_effect->initial_transparency))
                                       / active_damage_effect->period;
            
            if (active_damage_effect->flags & _random_transparency_flag)
            {
                damage_effect_transparency += FADES_RANDOM() % (active_damage_effect->final_transparency - damage_effect_transparency);
            }
        }
        else // effect has finished
        {
            damage_effect_transparency = active_damage_effect->final_transparency;
            active_damage_effect = nullptr;
        }
        
        damage_effect_last_updated_tick = dynamic_world.tick_count;
        
        //FilmExporter::instance()->AddFrame(FilmExporter::FRAME_FADE); // TODO: smells; should be able to delete this (I think it's just pointless AO spaghetti) but need to confirm
    }

    synchronize_classic_color_map();
}


void start_gameworld_damage_effect(int16_t type)
{
    view_effect_definition_t* new_effect = get_view_effect_definition(type);
    
    // If a damage effect is already running, should the new fade replace it?
    if (!active_damage_effect || new_effect->priority > active_damage_effect->priority || (new_effect == active_damage_effect && get_phase() >= MINIMUM_FADE_RESTART))
    {
        view_effect_definition_t* previous_effect = active_damage_effect;
        active_damage_effect = new_effect;
        
        update_gameworld_visual_effects();
        
        // if the new effect is instantaneous (period=0), the existing effect needs restored // TODO: check original code, confirm this
        if (new_effect->period == 0) { active_damage_effect = previous_effect; }
    }
}


void start_gameworld_tint_effect(short type)
{
    view_tint_definition_t* new_tint = get_view_tint_definition(type); // this throws is type is out of range
    
    active_tint_effect = get_view_effect_definition(new_tint->fade_type);
    tint_effect_transparency = new_tint->transparency;
    
    synchronize_classic_color_map();
}


void stop_gameworld_tint_effect()
{
    active_tint_effect = nullptr;
    
    synchronize_classic_color_map();
}


void reset_gameworld_view_effects()
{
    if (active_damage_effect)
    {
        damage_effect_transparency = active_damage_effect->final_transparency;
        
        synchronize_classic_color_map(); // are there any faders whose final_transparency isn't 0?
        
        active_damage_effect = nullptr;
    }
    
    stop_gameworld_tint_effect();
}


// from OGL_Faders.cpp
void ogl_apply_gameworld_visual_effects(float left, float top, float right, float bottom)
{
    // Set up the vertices
    GLfloat vertices[4][2];
    vertices[0][0] = left;
    vertices[0][1] = top;
    vertices[1][0] = right;
    vertices[1][1] = top;
    vertices[2][0] = right;
    vertices[2][1] = bottom;
    vertices[3][0] = left;
    vertices[3][1] = bottom;
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices[0]);
    
    // Do real blending
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    if (active_tint_effect)
    {
        active_tint_effect->modern_fader(active_tint_effect->color, tint_effect_transparency);
    }
    
    if (active_damage_effect)
    {
        active_damage_effect->modern_fader(active_damage_effect->color, tint_effect_transparency);
    }
    
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

