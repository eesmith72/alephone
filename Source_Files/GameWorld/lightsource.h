/*
 lightsource.H
 
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

#ifndef __LIGHTSOURCE_H
#define __LIGHTSOURCE_H

#include "cseries.h"


// M2 lights

enum // default light types
{
	_normal_light,
	_strobe_light,
	_media_light,
	NUMBER_OF_LIGHT_TYPES
};

enum // modes (states)
{
	_light_becoming_active,
	_light_primary_active,
	_light_secondary_active,
	_light_becoming_inactive,
	_light_primary_inactive,
	_light_secondary_inactive
};

// static light data

enum // lighting functions
{
	_constant_lighting_function, // maintain final intensity for period
	_linear_lighting_function, // linear transition between initial and final intensity over period
	_smooth_lighting_function, // sine transition between initial and final intensity over period
	_flicker_lighting_function, // intensity in [smooth_intensity(t),final_intensity]
	_random_lighting_function, // random intensity between initial and final,
	_fluorescent_lighting_function, // random on/off
	NUMBER_OF_LIGHTING_FUNCTIONS
};

// as intensities, transition functions are given the primary periods of the active and inactive state,
// plus the intensity at the time of transition
struct lighting_function_specification /* 7*2 == 14 bytes */
{
	int16 function;
	
	int16 period, delta_period;
	_fixed intensity, delta_intensity;
};

enum // static flags
{
	_light_is_initially_active,
	_light_has_slaved_intensities,
	_light_is_stateless,
	NUMBER_OF_STATIC_LIGHT_FLAGS /* <=16 */
};

#define LIGHT_IS_INITIALLY_ACTIVE(s) TEST_FLAG16((s)->flags, _light_is_initially_active)
#define LIGHT_IS_STATELESS(s) TEST_FLAG16((s)->flags, _light_is_stateless)

#define SET_LIGHT_IS_INITIALLY_ACTIVE(s, v) SET_FLAG16((s)->flags, _light_is_initially_active, (v))
#define SET_LIGHT_IS_STATELESS(s, v) SET_FLAG16((s)->flags, _light_is_stateless, (v))


struct m2_static_light_data_t
{
	int16 type;
	uint16 flags;
	int16 phase; // initializer, so lights may start out-of-phase with each other
	
    lighting_function_specification primary_active, secondary_active, becoming_active;
    lighting_function_specification primary_inactive, secondary_inactive, becoming_inactive;
	
	int16 tag;
	
	int16 unused[4];
};
const int SIZEOF_m2_static_light_data = 100;


// M1 lights

enum // type
{
    _light_is_normal,
    _light_is_rheostat,
    _light_is_flourescent,
    _light_is_strobe,
    _light_flickers,
    _light_pulsates,
    _light_is_annoying,
    _light_is_energy_efficient,
    NUMBER_OF_OLD_LIGHTS
};

enum // mode
{
    _light_mode_turning_on,
    _light_mode_on,
    _light_mode_turning_off,
    _light_mode_off,
    _light_mode_toggle
};


struct m1_static_light_data_t
{
    uint16 flags;
    
    int16 type;
    int16 mode; // on/off/turning
    int16 phase;
    
    _fixed minimum_intensity, maximum_intensity;
    int16 period; // on, in ticks (turning on and off periods are always the same for a given light type, or else are some function of this period)
    
    _fixed intensity; // current intensity
    
    int16 unused[5];
};
const int SIZEOF_m1_static_light_data = 32;



// LightState = dynamic + static light data


struct LightState
{
    // dynamic state
	uint16 flags;
	int16 state;
	
	_fixed intensity; // result of lighting function
	
	// data recalculated each function changed; passed to lighting_function each update
	int16 phase, period;
	_fixed initial_intensity, final_intensity;
    
	int16 unused[4];
    
    // static state
    m2_static_light_data_t static_data;
    
    bool is_active() const
    {
        switch (state)
        {
            case _light_becoming_active:
            case _light_primary_active:
            case _light_secondary_active:
                return true;
            default:
                return false;
        }
    }
};
const int SIZEOF_dynamic_light_data = 128;


/* --------- globals */


extern std::vector<LightState> LightList;
//#define lights (LightList.data())


/* --------- prototypes/LIGHTSOURCE.C */

short new_light(struct m2_static_light_data_t *data);
struct m2_static_light_data_t *get_defaults_for_light_type(short type);

void update_lights(void);

bool get_light_status(size_t light_index);
bool set_light_status(size_t light_index, bool active);
bool set_tagged_light_statuses(short tag, bool new_status);

_fixed get_light_intensity(size_t light_index);

LightState *get_light_data(const size_t light_index);


uint8 *unpack_static_light_data(uint8 *Stream, size_t Count, int32_t version);

uint8 *pack_static_light_data(uint8 *Stream, m2_static_light_data_t* Objects, size_t Count);

uint8 *unpack_dynamic_light_data(uint8 *Stream, size_t Count, int32_t version);

uint8 *pack_dynamic_light_data(uint8 *Stream, LightState* Objects, size_t Count);


#endif
