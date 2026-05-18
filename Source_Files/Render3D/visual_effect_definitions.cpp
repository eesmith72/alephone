/*
 visual_effect_definitions_definitions.cpp
 
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

#include "visual_effect_definitions.hpp"

#include "graphics_preferences.hpp"


#define ADJUSTED_TRANSPARENCY_DOWNSHIFT  (8)


uint16_t fades_random_seed = 0x0001;



// Multiply a color by its alpha channel
inline void multiply_rgb_by_alpha(const GLfloat* color, GLfloat* result)
{
    result[0] = color[0] * color[3];
    result[1] = color[1] * color[3];
    result[2] = color[2] * color[3];
    result[3] = color[3];
}


// Take the complement of a color
inline void complement_of_rgb(const GLfloat* color, GLfloat* result)
{
    result[0] = 1 - color[0];
    result[1] = 1 - color[1];
    result[2] = 1 - color[2];
    result[3] = color[3];
}


//-----------------------------------------------------------------------------
// fade procs are used in the table below


// The original SW number crunchers

static void fade_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    short adjusted_transparency = transparency >> ADJUSTED_TRANSPARENCY_DOWNSHIFT;
    
    for (short i = 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
#define ADJUST(KEY) \
    (((color.KEY - adjusted.KEY) * adjusted_transparency) >> (FIXED_FRACTIONAL_BITS - ADJUSTED_TRANSPARENCY_DOWNSHIFT))
        
        adjusted.r += ADJUST(r);
        adjusted.g += ADJUST(g);
        adjusted.b += ADJUST(b);
    }
    
#undef ADJUST
}


// While AO implements newer effects in OGL_Shader.cpp, UI fades, liquid submersion, and player hit/pickup screen effects are still done in OGL; presumably when migrating to SDL_gpu these will be rewritten as shaders.

static void fade_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    // The simplest kind: fade to the fader color. // EES: moved here from OGL_Faders.cpp
    ao_rgbaf c = ao_rgb_to_rgbaf(color, transparency);
    glColor4fv(c);
    glDrawArrays(GL_POLYGON, 0, 4);
}


static void randomize_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    uint16 mask, adjusted_transparency = PIN(transparency, 0, 0xffff);
    
    (void) (color);

    // calculate a mask which has all bits including and lower than the high-bit in the transparency set
    for (mask = 0; ~mask & adjusted_transparency; mask = (mask << 1) | 1);
    
    for (short i = 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
        adjusted.r += (FADES_RANDOM() & mask);
        adjusted.g += (FADES_RANDOM() & mask);
        adjusted.b += (FADES_RANDOM() & mask);
    }
}


static void randomize_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    if (graphics_preferences.OGL_Flag_FlatStatic)
    {
        // Whether to make the "static" effect look flat
        // Static patterns; randomize all digits; the various offsets are to ensure that all the bits overlap.
        // Also do flat static if requested; done once per frame to avoid visual inconsistencies
        // Alternative: partially-transparent instead of the logic-op effect
        static GM_Random rng;
        uint16_t c[4];
        c[0] = rng.KISS() + rng.LFIB4();
        c[1] = rng.KISS() + rng.LFIB4();
        c[2] = rng.KISS() + rng.LFIB4();
        c[3] = PIN(int32_t(65535 * o2f(transparency) + 0.5), 0, 65535);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glColor4usv(c);
        glDrawArrays(GL_POLYGON, 0, 4);
    }
    else
    {
        // Create random colors, but transmit the opacity
        ao_rgbaf c = {FADES_RANDOM() / float(SHRT_MAX), FADES_RANDOM() / float(SHRT_MAX),
                      FADES_RANDOM() / float(SHRT_MAX), transparency / float(FIXED_ONE)};
        ao_rgbaf blend_color;
        multiply_rgb_by_alpha(c, blend_color);
        // Do random flipping of the lower bits of color values;
        // the stronger the opacity (alpha), the more bits to flip.
        glDisable(GL_BLEND);
        // Multiply color by its alpha channel
        glColor3fv(blend_color);
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_XOR);
        glDrawArrays(GL_POLYGON, 0, 4);
        // Revert to defaults
        glDisable(GL_COLOR_LOGIC_OP);
        glEnable(GL_BLEND);
    }
}


// unlike Pathways, all colors won’t pass through 50% gray at the same time
static void negate_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    transparency = FIXED_ONE - transparency;
    
    for (short i = 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
        adjusted.r = (adjusted.r > 0x8000) ? CEILING((adjusted.r ^ color.r) + transparency, (int32_t)adjusted.r)
                                           :   FLOOR((adjusted.r ^ color.r) - transparency, (int32_t)adjusted.r);
        adjusted.g = (adjusted.g > 0x8000) ? CEILING((adjusted.g ^ color.g) + transparency, (int32_t)adjusted.g)
                                           :   FLOOR((adjusted.g ^ color.g) - transparency, (int32_t)adjusted.g);
        adjusted.b = (adjusted.b > 0x8000) ? CEILING((adjusted.b ^ color.b) + transparency, (int32_t)adjusted.b)
                                           :   FLOOR((adjusted.b ^ color.b) - transparency, (int32_t)adjusted.b);
    }
}


static void negate_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    ao_rgbaf c = ao_rgb_to_rgbaf(color, transparency);
    ao_rgbaf blend_color;
    multiply_rgb_by_alpha(c, blend_color);
    glColor4fv(blend_color);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_POLYGON, 0, 4);
    // Revert to defaults
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static void dodge_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    for (short i = 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
#define ADJUST(KEY) (0xffff - (int32_t(1LL * (color.KEY ^ 0xffff) * adjusted.KEY) >> FIXED_FRACTIONAL_BITS) - transparency)
        
        adjusted.r = CEILING(adjusted.r, ADJUST(r));
        adjusted.g = CEILING(adjusted.g, ADJUST(g));
        adjusted.b = CEILING(adjusted.b, ADJUST(b));

#undef ADJUST
    }
}


static void dodge_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    ao_rgbaf c = ao_rgb_to_rgbaf(color, transparency);
    ao_rgbaf blend_color;
    complement_of_rgb(c, blend_color);
    multiply_rgb_by_alpha(blend_color, blend_color);
    glColor4fv(blend_color);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_POLYGON, 0, 4);
    glBlendFunc(GL_DST_COLOR,GL_ONE);
    glDrawArrays(GL_POLYGON, 0, 4);
    // Revert to defaults
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static void burn_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    transparency = FIXED_ONE - transparency;
    
    for (short i= 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
#define ADJUST(KEY) (((uint32_t(color.KEY) * adjusted.KEY) >> FIXED_FRACTIONAL_BITS) + transparency)
        
        adjusted.r = CEILING(adjusted.r, ADJUST(r));
        adjusted.g = CEILING(adjusted.g, ADJUST(g));
        adjusted.b = CEILING(adjusted.b, ADJUST(b));
        
#undef ADJUST
    }
}


static void burn_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    // Attempted to get that reversed-color effect at maximum intensity, with it being only near maximum intensity/
    // (MultAlpha + GL_SRC_ALPHA means opacity^2)
    ao_rgbaf c = ao_rgb_to_rgbaf(color, transparency);
    ao_rgbaf blend_color;
    multiply_rgb_by_alpha(c, blend_color);
    glColor4fv(blend_color);
    glBlendFunc(GL_DST_COLOR, GL_ONE);
    glDrawArrays(GL_POLYGON, 0, 4);
    complement_of_rgb(c, blend_color);
    multiply_rgb_by_alpha(blend_color, blend_color);
    glColor4fv(blend_color);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDrawArrays(GL_POLYGON, 0, 4);
    // Revert to defaults
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static void tint_color_sw(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors)
{
    uint16_t adjusted_transparency = transparency >> ADJUSTED_TRANSPARENCY_DOWNSHIFT;
    
    for (short i = 0; i < animated_colors.color_count; i++)
    {
        ao_rgb& adjusted = animated_colors.colors[i];
        
        uint16_t intensity = MAX(MAX(adjusted.r, adjusted.g), adjusted.b) >> ADJUSTED_TRANSPARENCY_DOWNSHIFT;

#define SHIFT  (FIXED_FRACTIONAL_BITS - ADJUSTED_TRANSPARENCY_DOWNSHIFT)
        
#define ADJUST(KEY) \
    (((((color.KEY * intensity) >> SHIFT) - adjusted.KEY) * adjusted_transparency) >> SHIFT)
        
        adjusted.r += ADJUST(r);
        adjusted.g += ADJUST(g);
        adjusted.b += ADJUST(b);
        
#undef ADJUST
#undef SHIFT
    }
}


static void tint_color_ogl(const ao_rgb& color, ao_fixed transparency)
{
    // Fade to the color multiplied by the fader color, as if the scene was illuminated by light with that fader color.
    ao_rgbaf c = ao_rgb_to_rgbaf(color, transparency);
    ao_rgbaf blend_color;
    multiply_rgb_by_alpha(c, blend_color);
    glColor4fv(blend_color);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_POLYGON, 0, 4);
    // Revert to defaults
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


//-----------------------------------------------------------------------------


static const std::array<view_effect_definition_t, NUMBER_OF_VIEW_EFFECT_TYPES> view_effects_std = {
    //(view_effect_definition_t)
    // user interface fade in/out (ignored)
    fade_color_ogl, fade_color_sw, {0, 0, 0}, FIXED_ONE, FIXED_ONE, 0, _full_screen_flag, 0, // _start_cinematic_fade_in
    fade_color_ogl, fade_color_sw, {0, 0, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0, // _cinematic_fade_in
    {fade_color_ogl, fade_color_sw, {0, 0, 0}, FIXED_ONE, 0, 3*MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0}, // _long_cinematic_fade_in
    {fade_color_ogl, fade_color_sw, {0, 0, 0}, 0, FIXED_ONE, MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0}, // _cinematic_fade_out
    {fade_color_ogl, fade_color_sw, {0, 0, 0}, 0, 0, 0, _full_screen_flag, 0}, // _end_cinematic_fade_out
    
    // gameworld damage/pickup effects
    {fade_color_ogl, fade_color_sw, {65535, 0, 0}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, // _fade_red
    {fade_color_ogl, fade_color_sw, {65535, 0, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/4, 0, 0}, // _fade_big_red
    {fade_color_ogl, fade_color_sw, {0, 65535, 0}, FIXED_ONE_HALF, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, // _fade_bonus
    {fade_color_ogl, fade_color_sw, {65535, 65535, 50000}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/3, 0, 0}, // _fade_bright
    {fade_color_ogl, fade_color_sw, {65535, 65535, 50000}, FIXED_ONE, 0, 4*MACHINE_TICKS_PER_SECOND, 0, 1}, // _fade_long_bright
    {fade_color_ogl, fade_color_sw, {65535, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, // _fade_yellow
    {fade_color_ogl, fade_color_sw, {65535, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_big_yellow
    {fade_color_ogl, fade_color_sw, {215*256, 107*256, 65535}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, // _fade_purple
    {fade_color_ogl, fade_color_sw, {169*256, 65535, 224*256}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, // _fade_cyan
    {fade_color_ogl, fade_color_sw, {65535, 65535, 65535}, FIXED_ONE_HALF, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, // _fade_white
    {fade_color_ogl, fade_color_sw, {65535, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, // _fade_big_white
    {fade_color_ogl, fade_color_sw, {65535, 32768, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, // _fade_orange
    {fade_color_ogl, fade_color_sw, {65535, 32768, 0}, FIXED_ONE/4, 0, 3*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_long_orange
    {fade_color_ogl, fade_color_sw, {0, 65535, 0}, 3*FIXED_ONE/4, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, // _fade_green
    {fade_color_ogl, fade_color_sw, {65535, 0, 65535}, FIXED_ONE/4, 0, 3*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_long_green
    
    {randomize_color_ogl, randomize_color_sw, {0, 0, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/8, 0, 0}, // _fade_static
    {negate_color_ogl, negate_color_sw, {65535, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, // _fade_negative
    {negate_color_ogl, negate_color_sw, {65535, 65535, 65535}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/2, 0, 0}, // _fade_big_negative
    {negate_color_ogl, negate_color_sw, {0, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, _random_transparency_flag, 0}, // _fade_flicker_negative
    {dodge_color_ogl, dodge_color_sw, {0, 65535, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/4, 0, 0}, // _fade_dodge_purple
    {burn_color_ogl, burn_color_sw, {0, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_burn_cyan
    {dodge_color_ogl, dodge_color_sw, {0, 0, 65535}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/2, 0, 0}, // _fade_dodge_yellow
    {burn_color_ogl, burn_color_sw, {0, 65535, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_burn_green
    
    // under liquid tints
    {tint_color_ogl, tint_color_sw, {137*256, 0, 137*256}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_tint_purple
    {tint_color_ogl, tint_color_sw, {0, 0, 65535}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_tint_water
    {tint_color_ogl, tint_color_sw, {65535, 16384, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_tint_lava
    {tint_color_ogl, tint_color_sw, {32768, 65535, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_tint_sewage
    {tint_color_ogl, tint_color_sw, {32768, 65535, 0}, /*initial_transparency*/ FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, // _fade_tint_jjaro_goo
};


// TODO: honestly, I'm not sure what this second table adds, beyond complexity; it's possible that the initial_transparency=FIXED_ONE above might have relevance elsewhere (e.g. what if a custom Physics uses _fade_tint_jjaro_goo as a damage effect?) which would explain why different transparencies are defined here or it could be the math is subtly difference (e.g. tint modifying hue and saturation but not brightness, whereas effect may affect all 3); if the numbers are distinct then it'd be better to add an extra field (`opacity`?) to view_effect_definition_t
static const std::array<view_tint_definition_t, NUMBER_OF_VIEW_TINT_TYPES> view_tints_std = {
    (view_tint_definition_t)
    {_fade_tint_water,     FIXED_THREE_QUARTER},
    {_fade_tint_lava,      FIXED_THREE_QUARTER},
    {_fade_tint_sewage,    FIXED_THREE_QUARTER},
    {_fade_tint_jjaro_goo, FIXED_THREE_QUARTER},
    {_fade_tint_pfhor_goo, FIXED_THREE_QUARTER},
};


//-----------------------------------------------------------------------------


std::array<view_effect_definition_t, NUMBER_OF_VIEW_EFFECT_TYPES> view_effects;

std::array<view_tint_definition_t, NUMBER_OF_VIEW_TINT_TYPES> view_tints;



view_effect_definition_t* get_view_effect_definition(int16_t index)
{
    if (index < 0 && index >= NUMBER_OF_VIEW_EFFECT_TYPES)
    {
        throw_out_of_bounds_f("Bad view effect definition: %d", index); // TODO: error code
    }
    return &view_effects[index];
}


view_tint_definition_t* get_view_tint_definition(int16_t index)
{
    if (index < 0 && index >= NUMBER_OF_VIEW_EFFECT_TYPES)
    {
        throw_out_of_bounds_f("Bad view tint definition: %d", index); // TODO: error code
    }
    return &view_tints[index];
}


//-----------------------------------------------------------------------------
// MML


enum // used below
{
    _tint_fader_type,
    _randomize_fader_type,
    _negate_fader_type,
    _dodge_fader_type,
    _burn_fader_type,
    _soft_tint_fader_type,
    NUMBER_OF_FADER_FUNCTIONS
};



void reset_mml_faders()
{
    view_effects = view_effects_std;
    view_tints = view_tints_std;
}


void parse_mml_faders(const InfoTree& root)
{
    for (const InfoTree &ftree : root.children_named("fader"))
    {
        int16 index;
        if (!ftree.read_indexed("index", index, NUMBER_OF_VIEW_EFFECT_TYPES)) continue;
        
        view_effect_definition_t& def = view_effects[index];
        int16 fade_type;
        if (ftree.read_indexed("type", fade_type, NUMBER_OF_VIEW_EFFECT_TYPES))
        {
            switch (fade_type)
            {
                case _tint_fader_type:
                    def.modern_fader = fade_color_ogl;
                    def.classic_fader = fade_color_sw;
                    break;
                case _randomize_fader_type:
                    def.modern_fader = randomize_color_ogl;
                    def.classic_fader = randomize_color_sw;
                    break;
                case _negate_fader_type:
                    def.modern_fader = negate_color_ogl;
                    def.classic_fader = negate_color_sw;
                    break;
                case _dodge_fader_type:
                    def.modern_fader = dodge_color_ogl;
                    def.classic_fader = dodge_color_sw;
                    break;
                case _burn_fader_type:
                    def.modern_fader = burn_color_ogl;
                    def.classic_fader = burn_color_sw;
                    break;
                case _soft_tint_fader_type:
                    def.modern_fader = tint_color_ogl;
                    def.classic_fader = tint_color_sw;
                    break;
                default:
                    break;
            }
        }
        
        ftree.read_fixed("initial_opacity", def.initial_opacity);
        //ftree.read_fixed("final_opacity", def.final_opacity);
        ftree.read_attr("flags", def.flags);
        ftree.read_attr("priority", def.priority);
        int16 period;
        if (ftree.read_attr("period", period))
        {
            def.period = static_cast<int32>(period) * 1000 / MACHINE_TICKS_PER_SECOND;
        }
        for (const InfoTree &color : ftree.children_named("color"))
        {
            color.read_color(def.color);
        }
    }
    
    for (const InfoTree &ltree : root.children_named("liquid"))
    {
        int16 index;
        if (!ltree.read_indexed("index", index, NUMBER_OF_VIEW_TINT_TYPES)) continue;
        
        view_tint_definition_t& def = view_tints[index];
        ltree.read_indexed("fader", def.fade_type, NUMBER_OF_VIEW_EFFECT_TYPES, true);
        ltree.read_fixed("opacity", def.tint_opacity);
    }
}
