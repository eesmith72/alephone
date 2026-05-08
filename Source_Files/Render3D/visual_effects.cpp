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


// TODO: move UI fades to Screen, then rename this file `visual_effects.hpp/.cpp` and move it into Render3D/


#include "cseries.hpp"
#include "visual_effects.hpp"
#include "Screen.hpp"
#include "interface.hpp"
#include "map.h" // for TICKS_PER_SECOND
#include "InfoTree.h"
#include "render.h" // classic_renderer_is_active
#include "graphics_preferences.hpp"

#include "Music.h"
#include "FilmExporter.h"

#include "ClassicRasterizer.h" // [re]set_classic_color_map


// Be aware that we could try to change bit depths before a fade is completed // EES: TODO: color depths can change in prefs dialog or when changing screen size in-game via F-keys; how will this affect behaviour when switching between renderers or changing Classic bit depth?


//-----------------------------------------------------------------------------
// liquid tint and damage/pickup effects apply to worldview

// these are pointers into the fade definitions table, one for liquid tint, the other for damage/pickup effect

static view_effect_definition_t* active_tint_effect = nullptr;
ao_fixed tint_effect_transparency; // annoyingly, this is in a separate table to the effect definition (I suppose it's possible a tint fader could be used as an effect, in which case it needs its transparency fields to be separate to the tint transparency, but it would be nice if it was just one table)

static view_effect_definition_t* active_damage_effect = nullptr;
ao_fixed damage_effect_transparency; // transparency or opacity?
uint64_t damage_effect_last_updated_tick;

#define MINIMUM_FADE_RESTART  (MACHINE_TICKS_PER_SECOND / 2)


//-----------------------------------------------------------------------------


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


void update_gameworld_visual_effects() // called periodically by update_world in marathon2.cpp, and also by start_/stop_gameworld__effect
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


//-----------------------------------------------------------------------------



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


static void stop_gameworld_damage_effect()
{
    if (active_damage_effect)
    {
        damage_effect_transparency = active_damage_effect->final_transparency;
        
        synchronize_classic_color_map(); // are there any faders whose final_transparency isn't 0?
        
        active_damage_effect = nullptr;
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
}



void reset_gameworld_view_effects()
{
    stop_gameworld_damage_effect();
    stop_gameworld_tint_effect();
}





/* // dumping this here for now (I forget what I pulled it out of), just in case it's something important:
 glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glEnableClientState(GL_VERTEX_ARRAY);
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 OGL_DoFades(dst_rect.x, dst_rect.y, dst_rect.x + dst_rect.w, dst_rect.y + dst_rect.h);
 OGL_SwapBuffers();
 */


void OGL_DoFades(float Left, float Top, float Right, float Bottom)
{
    // Set up the vertices
    GLfloat Vertices[4][2];
    Vertices[0][0] = Left;
    Vertices[0][1] = Top;
    Vertices[1][0] = Right;
    Vertices[1][1] = Top;
    Vertices[2][0] = Right;
    Vertices[2][1] = Bottom;
    Vertices[3][0] = Left;
    Vertices[3][1] = Bottom;
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, Vertices[0]);
    
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



//-----------------------------------------------------------------------------
// gameworld brightness (gamma) adjustment (Classic rendering only; OGLRasterizer::End applies Shader::S_Gamma)

/*
// TODO: Figuring how to put fades code back together, it turns out gameworld's gamma and fades are closely connected:
 
 
 - For 8-bit gamma, set_gameworld_gamma (was `change_gamma_level`) adjusted the shapes' color table:
 
      gamma_correct_color_table(uncorrected_color_table, world_color_table, gamma_level);
 
   Any fades then operate on the world_color_table to create the visible_color_table.
 
   (Gamma almost never changes in-game so building the intermediate world_color_table is efficient.)

 
 - For 16/32-bit SW, AO uses apply_gamma function to twiddle every pixel in the Surface. This is also what applies the liquid tint and hit effects (this wasn't obvious): when a fade is active, update_color_map modifies the current_gamma_r/g/b tables, and apply_gamma applies those adjustments combined with the gamma shift.
 
   (I am unclear how gamma and effects originally applied in 16-bit, given that landscapes and WIH collections are 16-bit.)

 
 Two options:
 
 1. Always use OGL for Classic effects and gamma. Not 100% 'authentic' but if the visible difference is negligible then it simplifies the code.
 
 2. Modify `apply_gamma` to write to a 32-bit Surface pixel buffer, replacing the SDL_ConvertSurfaceFormat call in `ClassicRasterizer::End`. (I'll assume for 32-bit, the 32-bit m_surface can be passed as both args)
 */


