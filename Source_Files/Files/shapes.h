/*
 shapes.h
 
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

#ifndef __shapes_h__
#define __shapes_h__

#include "cseries.h"


// from SHAPE_DESCRIPTORS.H

struct collection_definition;

/* ---------- structures */

struct collection_header /* 32 bytes on disk */
{
	int16 status;
	uint16 flags;

	int32 offset, length;
	int32 offset16, length16;

	// LP: handles to pointers
	collection_definition *collection;
	std::vector<byte> shading_tables;
};
const int SIZEOF_collection_header = 32;


// TODO: this needs increased to uint32 or, better yet, made into struct to remove 32 collections limit
typedef uint16 shape_descriptor; /* [clut.3] [collection.5] [shape.8] */

#define DESCRIPTOR_SHAPE_BITS 8
#define DESCRIPTOR_COLLECTION_BITS 5
#define DESCRIPTOR_CLUT_BITS 3

#define MAXIMUM_COLLECTIONS (1<<DESCRIPTOR_COLLECTION_BITS)
#define MAXIMUM_SHAPES_PER_COLLECTION (1<<DESCRIPTOR_SHAPE_BITS)
#define MAXIMUM_CLUTS_PER_COLLECTION (1<<DESCRIPTOR_CLUT_BITS)

/* ---------- collections */

enum /* collection numbers */
{
    _collection_interface, // 0
    _collection_weapons_in_hand, // 1
    _collection_juggernaut, // 2
    _collection_tick, // 3
    _collection_rocket, // 4 -- LP: also known as "Explosion Effects"
    _collection_hunter, // 5
    _collection_player, // 6
    _collection_items, // 7
    _collection_trooper, // 8
    _collection_fighter, // 9
    _collection_defender, // 10
    _collection_yeti, // 11
    _collection_civilian, // 12
    _collection_civilian_fusion, // 13 -- LP: formerly _collection_madd
    _collection_enforcer, // 14
    _collection_hummer, // 15
    _collection_compiler, // 16
    _collection_walls1, // 17 -- LP: Lh'owon water
    _collection_walls2, // 18 -- LP: Lh'owon lava
    _collection_walls3, // 19 -- LP: Lh'owon sewage
    _collection_walls4, // 20 -- LP: Jjaro
    _collection_walls5, // 21 -- LP: Pfhor
    _collection_scenery1, // 22 -- LP: Lh'owon water
    _collection_scenery2, // 23 -- LP: Lh'owon lava
    _collection_scenery3, // 24 -- LP: Lh'owon sewage
    _collection_scenery4, // 25 pathways -- LP: Jjaro
    _collection_scenery5, // 26 alien -- LP: Pfhor
    _collection_landscape1, // 27 day -- LP: Lh'owon day
    _collection_landscape2, // 28 night -- LP: Lh'owon night
    _collection_landscape3, // 29 moon -- LP: Lh'owon moon
    _collection_landscape4, // 30 -- LP: outer space
    _collection_cyborg, // 31
    
    NUMBER_OF_COLLECTIONS
};

/* ---------- macros */

#define GET_DESCRIPTOR_SHAPE(d) ((d)&(uint16)(MAXIMUM_SHAPES_PER_COLLECTION-1))
#define GET_DESCRIPTOR_COLLECTION(d) (((d)>>DESCRIPTOR_SHAPE_BITS)&(uint16)((1<<(DESCRIPTOR_COLLECTION_BITS+DESCRIPTOR_CLUT_BITS))-1))
#define BUILD_DESCRIPTOR(collection,shape) (((collection)<<DESCRIPTOR_SHAPE_BITS)|(shape))

#define BUILD_COLLECTION(collection,clut) ((collection)|(uint16)((clut)<<DESCRIPTOR_COLLECTION_BITS))
#define GET_COLLECTION_CLUT(collection) (((collection)>>DESCRIPTOR_COLLECTION_BITS)&(uint16)(MAXIMUM_CLUTS_PER_COLLECTION-1))
#define GET_COLLECTION(collection) ((collection)&(MAXIMUM_COLLECTIONS-1))



void* get_global_shading_table();

short get_shape_descriptors(short shape_type, shape_descriptor *buffer);

#define get_shape_bitmap_and_shading_table(shape, bitmap, shading_table, shading_mode) \
    extended_get_shape_bitmap_and_shading_table(GET_DESCRIPTOR_COLLECTION(shape), \
                                                GET_DESCRIPTOR_SHAPE(shape), (bitmap), (shading_table), (shading_mode))

struct bitmap_definition; // in textures.h
void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition** bitmap, void** shading_tables, short shading_mode);

#define get_shape_information(shape) extended_get_shape_information(GET_DESCRIPTOR_COLLECTION(shape), GET_DESCRIPTOR_SHAPE(shape))

struct shape_information_data;
shape_information_data* extended_get_shape_information(short collection_code, short low_level_shape_index);

void get_shape_hotpoint(shape_descriptor texture, short *x0, short *y0);

struct shape_animation_data;
shape_animation_data* get_shape_animation_data(shape_descriptor texture);

void process_collection_sounds(short colleciton_code, void (*process_sound)(short sound_index));




//

#define mark_collection_for_loading(c) mark_collection((c), true)
#define mark_collection_for_unloading(c) mark_collection((c), false)
void mark_collection(short collection_code, bool loading);
void strip_collection(short collection_code);
void load_collections(bool with_progress_bar, bool is_opengl);
int count_replacement_collections();
void load_replacement_collections();
void unload_all_collections(void);

bool can_load_collection(short collection_index); // used by lua_script.cpp


void set_shapes_patch_data(uint8 *data, size_t length);
uint8* get_shapes_patch_data(size_t &length);


void initialize_shapes(void);


void open_shapes_file(const ao_path& path);


// ZZZ: this now works with RLE'd shapes, but needs extra storage.  Caller should
// be prepared to take a byte* if using an RLE shape (it will be set to NULL if
// shape is straight-coded); caller will need to free() that storage after freeing
// the SDL_Surface.
//
// If inIllumination is >= 0, it'd better be <= 1.  Shading tables are then used instead of the collection's CLUT.
// Among other effects (like being able to get darkened shapes), this lets player shapes be colorized according to
// team or player color.
//
// OK, yet another change... we now (optionally) take shape and collection separately, since there are too many
// low-level shapes in some collections to fit in the number of bits allotted.  If collection != NONE, it's taken
// as a collection and CLUT reference together; shape is (then) taken directly as a low-level shape index.
// If collection == NONE, shape is expected to convey information about all three elements (CLUT, collection,
// low-level shape index).
//
// Sigh, the extensions keep piling up... now we can also provide a quarter-sized surface from a shape.  It's hacky -
// the shape is shrunk by nearest-neighbor-style scaling (no smoothing), even at 16-bit and above, and it only works for RLE shapes.
//
SDL_Surface* get_shape_surface(int shape, int collection = NONE, byte** outPointerToPixelData = NULL, float inIllumination = -1.0f, bool inShrinkImage = false);



#endif /* __shapes_h__ */
