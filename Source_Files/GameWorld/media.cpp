/*
MEDIA.C

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

Sunday, March 26, 1995 1:13:11 AM  (Jason')

May 17, 2000 (Loren Petrich):
	Added XML support, including a damage parser

May 26, 2000 (Loren Petrich):
	Added XML shapes support

June 3, 2000 (Loren Petrich):
	Added some idiot-proofing: if a liquid type is not found, then it is assumed
	to be nonexistent. This is implemented by returning the null pointer.

July 1, 2000 (Loren Petrich):
	Inlined the media accessors

Aug 29, 2000 (Loren Petrich):
	Added packing and unpacking routines

Feb 8, 2001 (Loren Petrich):
	Fixed liquid-count bug in parallel with similar bug in map.cpp
*/

#include "cseries.hpp"

#include "map.h"
#include "media.h"
#include "effects.h"
#include "visual_effects.hpp"
#include "lightsource.h"
#include "SoundManager.h"
#include "InfoTree.h"

#include "Packing.h"


#include "media_definitions.h"


#define CALCULATE_MEDIA_HEIGHT(m) ((m)->low + FIXED_INTEGERAL_PART(((m)->high-(m)->low)*get_light_intensity((m)->light_index)))


std::vector<media_data> MediaList;


void update_one_media(size_t media_index, bool force_update);





media_data *get_media_data(size_t media_index)
{
    if (media_index >= MediaList.size()) return nullptr;
	media_data* media = &MediaList[media_index];
    return SLOT_IS_USED(media) ? media : nullptr;
}


// LP change: moved down here because it uses liquid definitions
media_definition *get_media_definition(short type)
{
	return GetMemberWithBounds(media_definitions,type,NUMBER_OF_MEDIA_TYPES);
}


// light_index must be loaded
size_t new_media(struct media_data *initializer)
{
    for (size_t media_index= 0; media_index < MediaList.size(); media_index++)
	{
        media_data* media = &MediaList[media_index];
        
		if (SLOT_IS_FREE(media))
		{
			*media= *initializer;
			
			MARK_SLOT_AS_USED(media);
			
			media->origin.x= media->origin.y= 0;
			update_one_media(media_index, true);

            return media_index;
		}
	}
	return UNONE;
}


bool media_in_environment(short media_type, short environment_code)
{
    if (media_type >= MediaList.size()) return false;
	return collection_in_environment(get_media_definition(media_type)->collection, environment_code);
}


void update_medias()
{
    for (size_t media_index = 0; media_index < MediaList.size(); media_index++)
    {
        media_data* media = &MediaList[media_index];
		if (SLOT_IS_USED(media))
		{
			update_one_media(media_index, false);
			
			media->origin.x= WORLD_FRACTIONAL_PART(media->origin.x + ((cosine_table[media->current_direction] * media->current_magnitude) >> TRIG_SHIFT));
			media->origin.y= WORLD_FRACTIONAL_PART(media->origin.y + ((sine_table[media->current_direction]   * media->current_magnitude) >> TRIG_SHIFT));
		}
	}
}


void get_media_detonation_effect(
	short media_index,
	short type,
	short *detonation_effect)
{
	struct media_data *media= get_media_data(media_index);
	// LP change: idiot-proofing
	if (!media) return;
	
	struct media_definition *definition= get_media_definition(media->type);
	if (!definition) return;

	if (type!=NONE)
	{
		if (!(type>=0 && type<NUMBER_OF_MEDIA_DETONATION_TYPES)) return;
		
		if (definition->detonation_effects[type]!=NONE) *detonation_effect= definition->detonation_effects[type];
	}
}

short get_media_sound(
	short media_index,
	short type)
{
	struct media_data *media= get_media_data(media_index);
	// LP change: idiot-proofing
	if (!media) return NONE;
	
	struct media_definition *definition= get_media_definition(media->type);
	if (!definition) return NONE;

	if (!(type>=0 && type<NUMBER_OF_MEDIA_SOUNDS)) return NONE;

	return definition->sounds[type];
}

struct damage_definition *get_media_damage(
	short media_index,
	ao_fixed scale)
{
	struct media_data *media= get_media_data(media_index);
	// LP change: idiot-proofing
	if (!media) return NULL;
	
	struct media_definition *definition= get_media_definition(media->type);
	if (!definition) return NULL;
	
	struct damage_definition *damage= &definition->damage;

	damage->scale= scale;
		
	return (damage->type==NONE || (dynamic_world.tick_count&definition->damage_frequency)) ?
		(struct damage_definition *) NULL : damage;
}


short get_media_submerged_fade_effect(short media_index)
{
	media_data* media = get_media_data(media_index);
	if (!media) return NONE;
	
	media_definition* definition = get_media_definition(media->type);
	if (!definition) return NONE;
	
	return (int16_t)definition->submerged_fade_effect;
}


bool get_media_collection(short media_index, short& collection)
{
	media_data *media = get_media_data(media_index);

	if (!media) return false;

	media_definition *definition = get_media_definition(media->type);
	if (!definition) return false;

	collection = definition->collection;
	return true;
}

// LP addition:
bool IsMediaDangerous(short media_index)
{
	media_definition *definition = get_media_definition(media_index);
	if (!definition) return false;
	
	struct damage_definition *damage= &definition->damage;
	if (damage->type == NONE) return false;
	else return (damage->base > 0);
}

/* ---------- private code */

void update_one_media(size_t media_index, bool force_update)
{
	struct media_data *media= get_media_data(media_index);
	// LP change: idiot-proofing
	if (!media) return;
	struct media_definition *definition= get_media_definition(media->type);
	if (!definition) return;

	/* update height */
	media->height= (media->low + FIXED_INTEGERAL_PART((media->high-media->low)*get_light_intensity(media->light_index)));

	/* update texture */	
	media->texture= BUILD_DESCRIPTOR(definition->collection, definition->shape);
	media->transfer_mode= definition->transfer_mode;

	(void)force_update;
}


// LP addition: count number of media types used, for better Infinity compatibility when saving games.
size_t count_number_of_medias_used()
{
    // Look for the last used slot
	for (int32_t media_index = (int32_t)MediaList.size() - 1; media_index >= 0; media_index--)
	{
        if (SLOT_IS_USED(&MediaList[media_index])) { return (size_t)(media_index + 1); }
	}
	return 0;
}


uint8 *unpack_media_data(uint8 *Stream, size_t count, bool restoring_game)
{
	uint8* S = Stream;
    
    MediaList.resize(count);
	
	for (size_t k = 0; k < count; k++)
	{
        media_data* ObjPtr = &MediaList[k];
        
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->light_index);
		
		StreamToValue(S,ObjPtr->current_direction);
		StreamToValue(S,ObjPtr->current_magnitude);
		
		StreamToValue(S,ObjPtr->low);
		StreamToValue(S,ObjPtr->high);
		
		StreamToValue(S,ObjPtr->origin.x);
		StreamToValue(S,ObjPtr->origin.y);
		StreamToValue(S,ObjPtr->height);
		
		StreamToValue(S,ObjPtr->minimum_light_intensity);
		StreamToValue(S,ObjPtr->texture);
		StreamToValue(S,ObjPtr->transfer_mode);
		
		S += 2*2;
        
        if (!restoring_game) // if it's a new game, create the media too
        {
            size_t new_index = new_media(ObjPtr);
            assert_fail(new_index == k, "corrupt media");
        }
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_media_data), "");
	return S;
}


uint8 *pack_media_data(uint8 *Stream, media_data* Objects, size_t Count)
{
	uint8* S = Stream;
	media_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->light_index);
		
		ValueToStream(S,ObjPtr->current_direction);
		ValueToStream(S,ObjPtr->current_magnitude);
		
		ValueToStream(S,ObjPtr->low);
		ValueToStream(S,ObjPtr->high);
		
		ValueToStream(S,ObjPtr->origin.x);
		ValueToStream(S,ObjPtr->origin.y);
		ValueToStream(S,ObjPtr->height);
		
		ValueToStream(S,ObjPtr->minimum_light_intensity);
		ValueToStream(S,ObjPtr->texture);
		ValueToStream(S,ObjPtr->transfer_mode);
		
		S += 2*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_media_data), "");
	return S;
}


struct media_definition *original_media_definitions = NULL;

void reset_mml_liquids()
{
	if (original_media_definitions) {
		for (int i = 0; i < NUMBER_OF_MEDIA_TYPES; i++)
			media_definitions[i] = original_media_definitions[i];
		free(original_media_definitions);
		original_media_definitions = NULL;
	}
}

void parse_mml_liquids(const InfoTree& root)
{
	// back up old values first
	if (!original_media_definitions) {
		original_media_definitions = (struct media_definition *) malloc(sizeof(struct media_definition) * NUMBER_OF_MEDIA_TYPES);
		assert_fail(original_media_definitions, "");
		for (int i = 0; i < NUMBER_OF_MEDIA_TYPES; i++)
			original_media_definitions[i] = media_definitions[i];
	}
	
	for (const InfoTree &liquid : root.children_named("liquid"))
	{
		int16 index;
		if (!liquid.read_indexed("index", index, NUMBER_OF_MEDIA_TYPES))
			continue;
		media_definition& def = media_definitions[index];
		
		liquid.read_indexed("coll", def.collection, NUMBER_OF_COLLECTIONS);
		liquid.read_indexed("frame", def.shape, MAXIMUM_SHAPES_PER_COLLECTION);
		liquid.read_indexed("transfer", def.transfer_mode, NUMBER_OF_TRANSFER_MODES);
		liquid.read_attr("damage_freq", def.damage_frequency);
        int16_t fade;
		liquid.read_indexed("submerged", fade, NUMBER_OF_VIEW_TINT_TYPES);
        def.submerged_fade_effect = (view_tint_t)def.submerged_fade_effect;
		for (const InfoTree &sound : liquid.children_named("sound"))
		{
			int16 type;
			if (!sound.read_indexed("type", type, NUMBER_OF_MEDIA_SOUNDS))
				continue;
			sound.read_indexed("which", def.sounds[type], SHRT_MAX+1, true);
		}
		for (const InfoTree &effect : liquid.children_named("effect"))
		{
			int16 type;
			if (!effect.read_indexed("type", type, NUMBER_OF_MEDIA_DETONATION_TYPES))
				continue;
			effect.read_indexed("which", def.detonation_effects[type], NUMBER_OF_EFFECT_TYPES);
		}
		for (const InfoTree &dmg : liquid.children_named("damage"))
		{
			dmg.read_damage(def.damage);
		}
	}
}
