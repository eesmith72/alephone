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

#include "cseries.hpp"

#include "ShapesCollection.h"


// pretty sure this is the index of the final black in the 8-bit clut's grayscale ramp
// (The first clut in first collection MUST start with this white-to-black ramp as code in shapes.cpp depends on its presence.)
#define CLUT_8_BLACK  (18)


// the 8-bit color table constructed from Shapes file's collections
extern color_table_t shapes_8_color_table;



// TODO: this needs increased to uint32/64 or made into struct to remove 32 collections limit (BTW, I think it's used in places as a lookup key); advantage of struct is we can get rid of these macros; TODO: rename frame_id_t? (depends what it's identifying)
typedef uint16 shape_descriptor; /* [clut.3] [collection.5] [shape.8] */

#define DESCRIPTOR_SHAPE_BITS       (8)
#define DESCRIPTOR_COLLECTION_BITS  (5)
#define DESCRIPTOR_CLUT_BITS        (3)

#define MAXIMUM_COLLECTIONS            (1 << DESCRIPTOR_COLLECTION_BITS) //  32
#define MAXIMUM_SHAPES_PER_COLLECTION  (1 << DESCRIPTOR_SHAPE_BITS)      // 256
#define MAXIMUM_CLUTS_PER_COLLECTION   (1 << DESCRIPTOR_CLUT_BITS)       //   8

/* ---------- collections */


// really belongs in definitions.h, along with projectile, effect, monster, and other standard M2 definition tables, though all of these might move to embedded config files anyway so leave for now
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



#define _X_MIRRORED_BIT 0x8000
#define _Y_MIRRORED_BIT 0x4000
#define _KEYPOINT_OBSCURED_BIT 0x2000


struct shape_information_data // TODO: how is this distinct from shapes_frame_t?
{
    uint16 flags; /* [x-mirror.1] [y-mirror.1] [keypoint_obscured.1] [unused.13] */

    ao_fixed minimum_light_intensity; /* in [0,FIXED_ONE] */

    short unused[5];

    short world_left, world_right, world_top, world_bottom;
    short world_x0, world_y0;
};


/* ---------- macros */

#define GET_DESCRIPTOR_SHAPE(d) ((d) & uint16_t(MAXIMUM_SHAPES_PER_COLLECTION - 1))

#define GET_DESCRIPTOR_COLLECTION(d) \
    (((d) >> DESCRIPTOR_SHAPE_BITS) & uint16_t((1 << (DESCRIPTOR_COLLECTION_BITS + DESCRIPTOR_CLUT_BITS)) - 1))

#define BUILD_DESCRIPTOR(collection, shape)  (((collection) << DESCRIPTOR_SHAPE_BITS) | (shape))


#define BUILD_COLLECTION(collection, clut)   ((collection) | uint16_t((clut) << DESCRIPTOR_COLLECTION_BITS))

#define GET_COLLECTION_CLUT(collection)      (((collection) >> DESCRIPTOR_COLLECTION_BITS) & uint16_t(MAXIMUM_CLUTS_PER_COLLECTION - 1))

#define GET_COLLECTION_INDEX(collection)     ((collection) & (MAXIMUM_COLLECTIONS - 1))




bool shapes_file_is_m1();


bool get_next_color_run(const shapes_colors_t& colors, short& start, short& count); // also used in infravision.cpp


void* get_global_shading_table();


#define get_shape_bitmap_and_shading_table(shape, bitmap, shading_table, shading_mode) \
    extended_get_shape_bitmap_and_shading_table(GET_DESCRIPTOR_COLLECTION(shape), \
                                                GET_DESCRIPTOR_SHAPE(shape), (bitmap), (shading_table), (shading_mode))


struct bitmap_definition_t; // in textures.h

void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition_t** bitmap, void** shading_tables, short shading_mode);

#define get_shape_information(shape) extended_get_shape_information(GET_DESCRIPTOR_COLLECTION(shape), GET_DESCRIPTOR_SHAPE(shape))


struct shape_information_data;

shape_information_data* extended_get_shape_information(short collection_code, short low_level_shape_index); // EES: 'extended'?


struct shapes_animation_t;

shapes_animation_t* get_shape_animation_data(shape_descriptor texture);


typedef void (*process_sound_proc)(short sound_index);

void process_collection_sounds(short colleciton_code, process_sound_proc);


ShapesCollection* get_shapes_collection(short collection_index);

size_t number_of_shapes_collections();


// Which bitmap index for a frame (good for OpenGL texture rendering)
short get_bitmap_index(short collection_index, short low_level_shape_index);



//

void load_shapes_collections(); // TODO: shapes need reloaded when file changes


void load_replacement_collections();

//void unload_all_collections();

bool collection_exists(short collection_index); // used by lua


void set_shapes_patch_data(uint8 *data, size_t length);

uint8* get_shapes_patch_data(size_t &length);


void initialize_shapes();


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



class InfoTree;
void parse_mml_infravision(const InfoTree& root); 
void reset_mml_infravision();



#endif /* __shapes_h__ */
