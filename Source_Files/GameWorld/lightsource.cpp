/*
LIGHTSOURCE.C

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

Wednesday, February 1, 1995 4:21:43 AM  (Jason')

Monday, March 6, 1995 9:41:50 PM  (Jason')
	nearly finished; looking toward cataclysm.  we need a good interface for editing intensities:
	a _normal_light only has two intensities, but each is found three places.
Thursday, April 27, 1995 11:00:36 AM  (Jason')
	functions with zero periods are skipped.
Tuesday, June 13, 1995 6:13:29 PM  (Jason)
	support for phases greater than a light’s initial period
Monday, July 10, 1995 5:20:26 PM  (Jason)
	stateless (six phase) lights.

June 2, 2000 (Loren Petrich):
	Added fallback for absent lights

July 1, 2000 (Loren Petrich):
	Modified light accessors to be more C++-like

Aug 29, 2000 (Loren Petrich):
	Added packing routines for the light data; also moved old light stuff (M1) here

Jul 3, 2002 (Loren Petrich):
	Added support for Pfhortran Procedure: light_activated
*/

#include "cseries.h"

#include "map.h"
#include "lightsource.h"
#include "Packing.h"
#include "wad.h" // M1_MAP_WAD_VERSION

//MH: Lua scripting
#include "lua_script.h"

/* ---------- globals */

// Turned the list of lights into a variable array;
// took over their maximum number as how many of them

std::vector<LightState> LightList;

// struct light_data *lights = NULL;

/* ---------- private prototypes */

static void rephase_light(short light_index);

static lighting_function_specification* get_lighting_function_specification(m2_static_light_data_t* data, short state);

static _fixed lighting_function_dispatch(short function_index, _fixed initial_intensity,
                                         _fixed final_intensity, short phase, short period);

/* ---------- structures */

struct light_definition
{
	short on_sound, off_sound; // it remains unclear where these sounds should come from; TODO: it is also unclear why it's not on the fucking light_data struct
    m2_static_light_data_t defaults;
};

/* ---------- globals */

struct light_definition light_definitions[NUMBER_OF_LIGHT_TYPES]=
{
	// _normal_light
	{
		NONE, NONE, // on, off sound
		{
			_normal_light, // type
			FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0, // flags, phase
			
			{_constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0}, // primary_active
			{_constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0}, // secondary_active
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0}, // becoming_active

			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // primary_inactive
			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // secondary_inactive
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // becoming_inactive
		}
	},

	// _strobe_light
	{
		NONE, NONE, // on, off sound
		{
			_normal_light, // type
			FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0, // flags, phase
			
			{_constant_lighting_function, TICKS_PER_SECOND/2, 0, FIXED_ONE, 0}, // primary_active
			{_constant_lighting_function, TICKS_PER_SECOND/2, 0, FIXED_ONE_HALF, 0}, // secondary_active
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE_HALF, 0}, // becoming_active

			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // primary_inactive
			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // secondary_inactive
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // becoming_inactive
		}
	},

	// _lava_light
	{
		NONE, NONE, // on, off sound
		{
			_normal_light, // type
			FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0, // flags, phase
			
			{_smooth_lighting_function, 10*TICKS_PER_SECOND, 0, FIXED_ONE, 0}, // primary_active
			{_smooth_lighting_function, 10*TICKS_PER_SECOND, 0, 0, 0}, // secondary_active
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE_HALF, 0}, // becoming_active

			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // primary_inactive
			{_constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // secondary_inactive
			{_smooth_lighting_function, TICKS_PER_SECOND, 0, 0, 0}, // becoming_inactive
		}
	},
};

static light_definition *get_light_definition(short type);

/* ---------- code */


LightState *get_light_data(size_t light_index)
{
    if (light_index >= LightList.size()) return nullptr;
    LightState* light = &LightList[light_index];
    if (!SLOT_IS_USED(light)) return nullptr;
	return light;
}


// LP change: moved down here because it uses light definitions
light_definition *get_light_definition(short type)
{
	return GetMemberWithBounds(light_definitions,type,NUMBER_OF_LIGHT_TYPES);
}


short new_light(m2_static_light_data_t *data)
{
	if (!data) return NONE;
	
	for (int32_t light_index = 0; light_index < short(LightList.size()); light_index++)
	{
        LightState* light = &LightList[light_index];
		if (SLOT_IS_FREE(light)) // TODO: is this still needed/appropriate? if we stop treating vector as fixed-size array, we can clear and reserve memory from what's in WAD, then append new lights to it when needed; when implementing map editor, any unused slots can go in a pool to be reused
		{
			light->static_data= *data;
//			light->flags= 0;
			MARK_SLOT_AS_USED(light);
			
			light->intensity= 0;
			change_light_state(light_index, LIGHT_IS_INITIALLY_ACTIVE(data) ? _light_secondary_active : _light_secondary_inactive);
			light->intensity= light->final_intensity;
			change_light_state(light_index, LIGHT_IS_INITIALLY_ACTIVE(data) ? _light_primary_active : _light_primary_inactive);
					light->phase= data->phase;
			rephase_light(light_index);		
			
			light->intensity= lighting_function_dispatch(get_lighting_function_specification(&light->static_data, light->state)->function,
				light->initial_intensity, light->final_intensity, light->phase, light->period);
			
            return light_index;
		}
	}
	return NONE;
}


struct m2_static_light_data_t *get_defaults_for_light_type(short type)
{
	struct light_definition *definition= get_light_definition(type);
	// LP addition: idiot-proofing
	if (!definition) return NULL;
	
	return &definition->defaults;
}


void update_lights()
{
	for (int32_t light_index = 0; light_index < LightList.size(); light_index++)
	{
        LightState* light = &LightList[light_index];
		if (SLOT_IS_USED(light))
		{
			/* update light phase; if we’ve overflowed our period change to the next state */
			light->phase+= 1;
			rephase_light(light_index);
			
			/* calculate and remember intensity for this ii, fi, phase, period */
			light->intensity= lighting_function_dispatch(get_lighting_function_specification(&light->static_data, light->state)->function,
				light->initial_intensity, light->final_intensity, light->phase, light->period);
		}
	}
}


bool get_light_status(size_t light_index)
{
    LightState* light= get_light_data(light_index);
    if (!light) return false;
    return light->is_active();
}
    

bool set_light_status(size_t light_index, bool new_status)
{
	struct LightState *light= get_light_data(light_index);
	// LP change: idiot-proofing
	if (!light) return false;
	
	bool old_status= get_light_status(light_index);
	bool changed= false;
	
	if ((new_status&&!old_status) || (!new_status&&old_status))
	{
		if (!LIGHT_IS_STATELESS(light))
		{
			change_light_state(light_index, new_status ? _light_becoming_active : _light_becoming_inactive);
			assert_fail(light_index == static_cast<size_t>(static_cast<short>(light_index)), "");
                        //MH: Lua script hook
                        L_Call_Light_Activated(light_index);
			assume_correct_switch_position(_panel_is_light_switch, static_cast<short>(light_index), new_status);
			changed= true;
		}
	}
	
	return changed;
}


bool set_tagged_light_statuses(short tag, bool new_status)
{
	bool changed = false;
	if (tag)
	{
		for (int32_t light_index = 0; light_index < LightList.size(); light_index++)
		{
            LightState* light = &LightList[light_index];
			if (light->static_data.tag == tag && set_light_status(light_index, new_status)) { changed = true; }
		}
	}
	return changed;
}


_fixed get_light_intensity(size_t light_index)
{
	// LP change: idiot-proofing / fallback
	LightState *light = get_light_data(light_index);
	if (!light) return 0;	// Blackness
	
	return light->intensity;
}

/* ---------- private code */

/* given a state, initialize .phase, .period, .initial_intensity, and .final_intensity */
void change_light_state(size_t light_index, short new_state)
{
	struct LightState *light= get_light_data(light_index);
	// LP change: idiot-proofing
	if (!light) return;
	struct lighting_function_specification *function= get_lighting_function_specification(&light->static_data, new_state);
	
	light->phase= 0;
	light->period= function->period + global_random()%(function->delta_period+1);
	
	light->initial_intensity= light->intensity;
	light->final_intensity= function->intensity + global_random()%(function->delta_intensity+1);
	
	light->state= new_state;
}

static struct lighting_function_specification *get_lighting_function_specification(
	struct m2_static_light_data_t *data,
	short state)
{
	struct lighting_function_specification *function;
	
	switch (state)
	{
		case _light_becoming_active:
            function = &data->becoming_active;
            break;
		case _light_primary_active:
            function = &data->primary_active;
            break;
		case _light_secondary_active:
            function = &data->secondary_active;
            break;
		case _light_becoming_inactive:
            function = &data->becoming_inactive;
            break;
		case _light_primary_inactive:
            function = &data->primary_inactive;
            break;
		case _light_secondary_inactive:
            function = &data->secondary_inactive;
            break;
		default:
            throw_bug_report_f("invalid light state: #%d", state);
	}
	
	return function;
}

static void rephase_light(short light_index)
{
	struct LightState *light= get_light_data(light_index);
	// LP change: idiot-proofing
	if (!light) return;
	short phase= light->phase;
	
	while (phase>=light->period)
	{
		short new_state;
		
		phase-= light->period;
		
		switch (light->state)
		{
			case _light_becoming_active:
                new_state = _light_primary_active;
                break;
			case _light_primary_active:
                new_state = _light_secondary_active;
                break;
			case _light_secondary_active:
                new_state = LIGHT_IS_STATELESS(light) ? _light_becoming_inactive : _light_primary_active;
                break;
			case _light_becoming_inactive:
                new_state = _light_primary_inactive;
                break;
			case _light_primary_inactive:
                new_state = _light_secondary_inactive;
                break;
			case _light_secondary_inactive:
                new_state = LIGHT_IS_STATELESS(light) ? _light_becoming_active : _light_primary_inactive;
                break;
			default:
                throw_bug_report_f("invalid light state #%d", light->state);
		}
		
		change_light_state(light_index, new_state);
	}
	light->phase= phase;
}
				
/* ---------- lighting functions */

static _fixed constant_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);
static _fixed linear_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);
static _fixed smooth_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);
static _fixed flicker_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);
static _fixed random_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);
static _fixed fluorescent_lighting_proc(_fixed initial_intensity, _fixed final_intensity, short phase, short period);

typedef _fixed (*lighting_function)(_fixed initial_intensity, _fixed final_intensity,
	short phase, short period);

static lighting_function lighting_functions[NUMBER_OF_LIGHTING_FUNCTIONS]=
{
	constant_lighting_proc,
	linear_lighting_proc,
	smooth_lighting_proc,
	flicker_lighting_proc,
	random_lighting_proc,
	fluorescent_lighting_proc,
};

static _fixed lighting_function_dispatch(
	short function_index,
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	assert_fail(function_index>=0 && function_index<NUMBER_OF_LIGHTING_FUNCTIONS, "");
	
	return lighting_functions[function_index](initial_intensity, final_intensity, phase, period);
}

static _fixed constant_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	(void) (initial_intensity);
	(void) (phase);
	(void) (period);
	
	return final_intensity;
}

static _fixed linear_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	return initial_intensity + ((final_intensity-initial_intensity)*phase)/period;
}

static _fixed smooth_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	return initial_intensity + (((final_intensity-initial_intensity)*(cosine_table[(phase*HALF_CIRCLE)/period+HALF_CIRCLE]+TRIG_MAGNITUDE))>>(TRIG_SHIFT+1));
}

static _fixed flicker_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	_fixed smooth_intensity= smooth_lighting_proc(initial_intensity, final_intensity, phase, period);
	_fixed delta= final_intensity-smooth_intensity;
	
	return smooth_intensity + (delta ? global_random()%delta : 0);
}

static _fixed random_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	_fixed delta;
	if (final_intensity > initial_intensity) 
	{
		delta = final_intensity - initial_intensity;
		return initial_intensity + (delta ? global_random()%delta : 0);
	} else {
		delta = initial_intensity - final_intensity;
		return final_intensity + (delta ? global_random()%delta : 0);	
	}
}

// should the probability of final_intensity increase with phase?
static _fixed fluorescent_lighting_proc(
	_fixed initial_intensity,
	_fixed final_intensity,
	short phase,
	short period)
{
	return (global_random()%2 ? final_intensity : initial_intensity);
}



static void FixIntensity(lighting_function_specification& LightState, m1_static_light_data_t& OldLight)
{
    LightState.intensity = LightState.intensity > 0 ? OldLight.maximum_intensity : OldLight.minimum_intensity;
}

m2_static_light_data_t old_light_definitions[NUMBER_OF_OLD_LIGHTS] =
{
    // _light_is_normal
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, 1, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, 1, 0, FIXED_ONE, 0 }
    },

    // _light_is_rheostat
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _smooth_lighting_function, 3 * TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _smooth_lighting_function, 3 * TICKS_PER_SECOND, 0, 0, 0 }
    },

    // _light_is_flourescent
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _fluorescent_lighting_function, 3 * TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, 1, 0, 0, 0 }
    },

    // _light_is_strobe
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, 1, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, 1, 0, 0, 0 }
    },

    // _light_flickers
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _flicker_lighting_function, 3 * TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, 1, 0, 0, 0 }
    },

    // _light_pulsates
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _smooth_lighting_function, 2*TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _smooth_lighting_function, 2*TICKS_PER_SECOND-1, 0, 0, 0 },
        { _smooth_lighting_function, 2*TICKS_PER_SECOND-1, 0, FIXED_ONE, 0 },
        
        { _smooth_lighting_function, 2*TICKS_PER_SECOND, 0, 0, 0 },
        { _smooth_lighting_function, 2*TICKS_PER_SECOND-1, 0, FIXED_ONE, 0 },
        { _smooth_lighting_function, 2*TICKS_PER_SECOND, 0, 0, 0 }
    },

    // _light_is_annoying
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _random_lighting_function, 2, 1, FIXED_ONE, 0 },
        { _constant_lighting_function, 2, 0, 0, 0 },
        { _random_lighting_function, 1, 0, FIXED_ONE, 0 },
        
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 }
    },

    // _light_is_energy_efficient
    {
        _normal_light,
        FLAG(_light_is_initially_active)|FLAG(_light_has_slaved_intensities), 0,
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _linear_lighting_function, 2 * TICKS_PER_SECOND, 0, FIXED_ONE, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _constant_lighting_function, TICKS_PER_SECOND, 0, 0, 0 },
        { _linear_lighting_function, 2 * TICKS_PER_SECOND, 0, 0, 0 }
    }
};


void unpack_m1_static_light(uint8_t*& S, m2_static_light_data_t& m2_light)
{
    m1_static_light_data_t m1_light;
    
    StreamToValue(S, m1_light.flags);
    
    StreamToValue(S, m1_light.type);
    StreamToValue(S, m1_light.mode);
    StreamToValue(S, m1_light.phase);
    
    StreamToValue(S, m1_light.minimum_intensity);
    StreamToValue(S, m1_light.maximum_intensity);
    StreamToValue(S, m1_light.period);
    
    StreamToValue(S, m1_light.intensity);
    
    S += 5*2;
    
    // LP: code taken from map_wad.c and somewhat modified
    m2_light = old_light_definitions[m1_light.type];
    FixIntensity(m2_light.primary_active,     m1_light);
    FixIntensity(m2_light.secondary_active,   m1_light);
    FixIntensity(m2_light.becoming_active,    m1_light);
    FixIntensity(m2_light.primary_inactive,   m1_light);
    FixIntensity(m2_light.secondary_inactive, m1_light);
    FixIntensity(m2_light.becoming_inactive,  m1_light);
    
    if (m1_light.type == _light_is_strobe)
    {
        m2_light.primary_active.period     = m1_light.period / 4 + 1;
        m2_light.secondary_active.period   = m1_light.period / 4 + 1;
        m2_light.primary_inactive.period   = m1_light.period / 4 + 1;
        m2_light.secondary_inactive.period = m1_light.period / 4 + 1;
    }
    
    switch (m1_light.mode)
    {
        case _light_mode_on:
        case _light_mode_turning_on:
            SET_FLAG(m2_light.flags,FLAG(_light_is_initially_active),1);
            break;
        case _light_mode_off:
        default:
            SET_FLAG(m2_light.flags,FLAG(_light_is_initially_active),0);
            break;
    }
}


static void StreamToLightSpec(uint8* &S, lighting_function_specification& Object)
{
	StreamToValue(S,Object.function);
	
	StreamToValue(S,Object.period);
	StreamToValue(S,Object.delta_period);
	StreamToValue(S,Object.intensity);
	StreamToValue(S,Object.delta_intensity);
}

static void LightSpecToStream(uint8* &S, lighting_function_specification& Object)
{
	ValueToStream(S,Object.function);
	
	ValueToStream(S,Object.period);
	ValueToStream(S,Object.delta_period);
	ValueToStream(S,Object.intensity);
	ValueToStream(S,Object.delta_intensity);
}



static void unpack_m2_static_light(uint8_t*& S, m2_static_light_data_t& m2_light)
{
    StreamToValue(S, m2_light.type);
    StreamToValue(S, m2_light.flags);
    StreamToValue(S, m2_light.phase);

    StreamToLightSpec(S, m2_light.primary_active);
    StreamToLightSpec(S, m2_light.secondary_active);
    StreamToLightSpec(S, m2_light.becoming_active);
    StreamToLightSpec(S, m2_light.primary_inactive);
    StreamToLightSpec(S, m2_light.secondary_inactive);
    StreamToLightSpec(S, m2_light.becoming_inactive);

    StreamToValue(S, m2_light.tag);
    
    S += 4*2;
}


uint8 *unpack_static_light_data(uint8 *Stream, size_t count, int32_t version)
{
	uint8* S = Stream;
    
    LightList.clear();
    LightList.resize(count);
    
    auto unpack_static_light = version == M1_MAP_WAD_VERSION ? unpack_m1_static_light : unpack_m2_static_light;

	for (size_t k = 0; k < count; k++)
	{
        m2_static_light_data_t light_data;
        unpack_static_light(S, light_data);
        int32_t new_index = new_light(&light_data);
        assert_fail(new_index == k, "bad static light data");
	}
	return S;
}


uint8 *pack_static_light_data(uint8 *Stream, m2_static_light_data_t* Objects, size_t Count)
{
	uint8* S = Stream;
	m2_static_light_data_t* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->phase);
        
		LightSpecToStream(S,ObjPtr->primary_active);
		LightSpecToStream(S,ObjPtr->secondary_active);
		LightSpecToStream(S,ObjPtr->becoming_active);
		LightSpecToStream(S,ObjPtr->primary_inactive);
		LightSpecToStream(S,ObjPtr->secondary_inactive);
		LightSpecToStream(S,ObjPtr->becoming_inactive);
        
		ValueToStream(S,ObjPtr->tag);
		
		S += 4*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_m2_static_light_data), "");
	return S;
}


uint8 *unpack_dynamic_light_data(uint8 *Stream, size_t count, int32_t version)
{
    LightList.clear();
    LightList.resize(count);
    
	uint8* S = Stream;
    auto unpack_static_light = version == M1_MAP_WAD_VERSION ? unpack_m1_static_light : unpack_m2_static_light;
	
	for (size_t k = 0; k < count; k++)
	{
        LightState* ObjPtr = &LightList[k];
        
		StreamToValue(S,ObjPtr->flags);
		StreamToValue(S,ObjPtr->state);
		
		StreamToValue(S,ObjPtr->intensity);
		
		StreamToValue(S,ObjPtr->phase);
		StreamToValue(S,ObjPtr->period);
		StreamToValue(S,ObjPtr->initial_intensity);
		StreamToValue(S,ObjPtr->final_intensity);
		
		S += 4*2;
		
        unpack_static_light(S, ObjPtr->static_data);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_dynamic_light_data), "");
	return S;
}


uint8 *pack_dynamic_light_data(uint8 *Stream, LightState* Objects, size_t Count)
{
	uint8* S = Stream;
	LightState* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->state);
		
		ValueToStream(S,ObjPtr->intensity);
		
		ValueToStream(S,ObjPtr->phase);
		ValueToStream(S,ObjPtr->period);
		ValueToStream(S,ObjPtr->initial_intensity);
		ValueToStream(S,ObjPtr->final_intensity);
		
		S += 4*2;
		
		S = pack_static_light_data(S,&ObjPtr->static_data,1);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_dynamic_light_data), "");
	return S;
}


