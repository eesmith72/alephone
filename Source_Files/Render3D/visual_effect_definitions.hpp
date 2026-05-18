/*
 visual_effect_definitions.hpp
 
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

#ifndef visual_effect_definitions_hpp
#define visual_effect_definitions_hpp

#include "cseries.hpp"


// TODO: currently, the tint effect is only applied when submerged under liquid; in future, it'd be nice if Modern could apply (e.g.) a dynamic orange heat haze on lava levels (this'd probably require extended Map format to support properly as how much to apply would need to be set per-polygon; alternatively, it could find the nearest poly with lava in it and calculate tint strength based on player distance) or a colored lighting effect (e.g. strobing red warning light in Marathon corridors). I think there's some support for environment tinting in OGL, but it'd be nice to have one standard system. (Now the code for applying media tint is well-defined, turning that into a vector of effect definitions and making those tint effects dynamic should be straightforward.)


//-----------------------------------------------------------------------------
// fade types


enum
{
    // user interface fades (no longer used; UI fades are hardcoded in OGL now)
    _start_cinematic_fade_in, // force all colors to black immediately
    _cinematic_fade_in, // fade in from black
    _long_cinematic_fade_in,
    _cinematic_fade_out, // fade out from black
    _end_cinematic_fade_out, // force all colors from black immediately
    
    // damage/pickup effects
    _fade_red, // bullets and fist
    _fade_big_red, // bigger bullets and fists
    _fade_bonus, // picking up items
    _fade_bright, // teleporting
    _fade_long_bright, // nuclear monster detonations
    _fade_yellow, // explosions
    _fade_big_yellow, // big explosions
    _fade_purple, // ?
    _fade_cyan, // fighter staves and projectiles
    _fade_white, // absorbed
    _fade_big_white, // rocket (probably) absorbed
    _fade_orange, // flamethrower
    _fade_long_orange, // marathon lava
    _fade_green, // hunter projectile
    _fade_long_green, // alien green goo
    _fade_static, // compiler projectile
    _fade_negative, // minor fusion projectile
    _fade_big_negative, // major fusion projectile
    _fade_flicker_negative, // hummer projectile
    _fade_dodge_purple, // alien weapon
    _fade_burn_cyan, // armageddon beast electricity
    _fade_dodge_yellow, // armageddon beast projectile
    _fade_burn_green, // hunter projectile
    
    // under liquid tints
    _fade_tint_pfhor_goo,
    _fade_tint_water,
    _fade_tint_lava,
    _fade_tint_sewage,
    _fade_tint_jjaro_goo,
    
    NUMBER_OF_VIEW_EFFECT_TYPES
};


enum class view_tint_t : int32_t
{
    water,
    lava,
    sewage,
    jjaro,
    goo,
};
#define NUMBER_OF_VIEW_TINT_TYPES (5)




typedef void (*classic_fade_proc)(const ao_rgb& color, ao_fixed transparency, color_table_t& animated_colors);

typedef void (*modern_fade_proc)(const ao_rgb& color, ao_fixed transparency);


// a tint may be applied to the original color table before an effect fade

struct view_tint_definition_t // was `fade_effect_definition`
{
    int16_t fade_type;
    ao_fixed tint_opacity;
};


enum
{
    _full_screen_flag         = 0x0001,
    _random_transparency_flag = 0x0002,
};


struct view_effect_definition_t // was `fade_definition`
{
    modern_fade_proc modern_fader;
    classic_fade_proc classic_fader;
    
    ao_rgb color;
    
    ao_fixed initial_opacity; // 0...FIXED_ONE
    ao_fixed final_opacity; // unused (only user interface fades used non-zero values) // TODO: repurpose as tint_opacity and get rid of the separate view_tint_definition_t[] table? or can we just use initial_opacity for tint? (btw, something that might make a nice effect is if hue/lighting varies slightly according to depth)
    
    int16_t period;
    
    uint16_t flags;
    int16_t priority; // higher is higher
};


extern uint16_t fades_random_seed; // also used in fades.cpp

#define FADES_RANDOM()  ((fades_random_seed & 0x01) ? (fades_random_seed = (fades_random_seed >> 1) ^ 0xb400) : (fades_random_seed >>= 1))



// in-game hit/pickup effect or UI fade in/out
view_effect_definition_t* get_view_effect_definition(int16_t index);


// submerged under liquid
view_tint_definition_t* get_view_tint_definition(int16_t index);


//-----------------------------------------------------------------------------
// MML


class InfoTree;
void parse_mml_faders(const InfoTree& root);
void reset_mml_faders();



#endif /* visual_effect_definitions_hpp */
