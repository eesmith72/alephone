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

#include "ShapesCollection.hpp"


// the 8-bit color table constructed from Shapes file's collections
extern color_table_t gameworld_color_table_8;


extern SDL_PixelFormat pixel_format_8, pixel_format_16, pixel_format_32;


// collection 0's clut must start with a standard white-to-black ramp, with the black at index 18
#define CLUT_BLACK  (18)



// TODO: this needs increased to uint32/64 or made into struct to remove 32 collections limit (BTW, I think it's used in places as a lookup key)
typedef uint16 shape_descriptor; /* [clut.3] [collection.5] [shape.8] */

#define DESCRIPTOR_SHAPE_BITS       (8)
#define DESCRIPTOR_COLLECTION_BITS  (5)
#define DESCRIPTOR_CLUT_BITS        (3)

#define MAXIMUM_COLLECTIONS            (1 << DESCRIPTOR_COLLECTION_BITS) //  32
#define MAXIMUM_SHAPES_PER_COLLECTION  (1 << DESCRIPTOR_SHAPE_BITS)      // 256
#define MAXIMUM_CLUTS_PER_COLLECTION   (1 << DESCRIPTOR_CLUT_BITS)       //   8

/* ---------- collections */


// really belongs in definitions.h, along with projectile, effect, monster, and other standard M2 definition tables
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



// TODO: replace shape_descriptor with struct and unpack into that (caveat need to find anywhere that expects an integer value, e.g. as lookup key)

#define GET_DESCRIPTOR_SHAPE(d) ((d) & uint16_t(MAXIMUM_SHAPES_PER_COLLECTION - 1))

#define GET_DESCRIPTOR_COLLECTION(d) \
    (((d) >> DESCRIPTOR_SHAPE_BITS) & uint16_t((1 << (DESCRIPTOR_COLLECTION_BITS + DESCRIPTOR_CLUT_BITS)) - 1))

#define BUILD_DESCRIPTOR(collection, shape)  (((collection) << DESCRIPTOR_SHAPE_BITS) | (shape))


#define BUILD_COLLECTION(collection, clut)   ((collection) | uint16_t((clut) << DESCRIPTOR_COLLECTION_BITS))

#define GET_COLLECTION_CLUT(collection)      (((collection) >> DESCRIPTOR_COLLECTION_BITS) & uint16_t(MAXIMUM_CLUTS_PER_COLLECTION - 1))

#define GET_COLLECTION_INDEX(collection)     ((collection) & (MAXIMUM_COLLECTIONS - 1))


bool shapes_file_is_m1();


void convert_ogl_color_to_infravision(short collection_index, GLfloat* color); // used in OGL_Render.cpp and OGLRenderer.cpp


void* get_global_shading_table();


bool get_next_color_run(const shapes_colors_t& colors, short& start, short& count); // also used in infravision.cpp


// used by renderer

#define get_shape_bitmap_and_shading_table(shape, bitmap, shading_table, shading_mode) \
    extended_get_shape_bitmap_and_shading_table(GET_DESCRIPTOR_COLLECTION(shape), \
                                                GET_DESCRIPTOR_SHAPE(shape), (bitmap), (shading_table), (shading_mode))

void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition_t** bitmap, void** shading_tables, short shading_mode);



#define get_shape_information(shape)  get_shapes_frame(GET_DESCRIPTOR_COLLECTION(shape), GET_DESCRIPTOR_SHAPE(shape))


// weird API: not quite shape_descriptor
shapes_frame_t* get_shapes_frame(short collection_code, short low_level_shape_index);




ShapesCollection* get_shapes_collection(short collection_index);

size_t number_of_shapes_collections();

shapes_animation_t* get_shapes_animation(shape_descriptor texture);


//-----------------------------------------------------------------------------


void initialize_shapes();


void open_shapes_file(const ao_path& path);



bool shapes_collection_exists(short collection_index); // used by lua_script.cpp


void set_shapes_patch_data(uint8 *data, size_t length);

uint8* get_shapes_patch_data(size_t &length);



class InfoTree;
void parse_mml_infravision(const InfoTree& root); 
void reset_mml_infravision();



#endif /* __shapes_h__ */
