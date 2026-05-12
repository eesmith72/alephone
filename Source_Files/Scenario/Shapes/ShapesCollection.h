/*
 COLLECTION_DEFINITION.H
 
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

#ifndef __COLLECTION_DEFINITION_H
#define __COLLECTION_DEFINITION_H


// EES: data structures used in shapes.cpp implementation, so really belongs in shapes.h


#include "cstypes.hpp"


/* ---------- collection definition structure */

/* 2 added pixels_to_world to ShapesCollection structure */
/* 3 added size to ShapesCollection structure */
#define COLLECTION_VERSION 3

/* at the beginning of the clut, used by the extractor for various opaque reasons */
#define NUMBER_OF_PRIVATE_COLORS  (3)

enum /* collection types */
{
	_unused_collection= 0, /* raw */
	_wall_collection, /* raw */
	_object_collection, /* rle */
	_interface_collection, /* raw */
	_scenery_collection /* rle */
};

struct high_level_shape_definition;
struct low_level_shape_definition;
struct bitmap_definition_t;





struct shapes_color_t
{
    bool luminescent; // was `uint8 flags` but luminescent was the only flag
    uint8_t index; // was `uint8 value` but looks like it's the clut index
    ao_rgb color;
};
const int SIZEOF_shapes_color_t = 8;



/* ---------- high level shape definition */

#define HIGH_LEVEL_SHAPE_NAME_LENGTH 32

struct high_level_shape_definition // Starting with number_of_views, this is a shape_animation_data structure
{
    int16 type; /* ==0 */
    uint16 flags; /* [unused.16] */
    
    char name[HIGH_LEVEL_SHAPE_NAME_LENGTH+2];
    
    int16 number_of_views;
    
    int16 frames_per_view, ticks_per_frame;
    int16 key_frame;
    
    int16 transfer_mode;
    int16 transfer_mode_period; /* in ticks */
    
    int16 first_frame_sound, key_frame_sound, last_frame_sound;

    int16 pixels_to_world;

    int16 loop_frame;

    int16 unused[14];

    /* see the interface.hpp/shape_animation_data for a decription of how many
       low-level indices follow (it's not simply number_of_view * frames_per_view) */
    int16 low_level_shape_indexes[1];
};

const int SIZEOF_high_level_shape_definition = 90;


/* --------- low-level shape definition */

#define _X_MIRRORED_BIT 0x8000
#define _Y_MIRRORED_BIT 0x4000
#define _KEYPOINT_OBSCURED_BIT 0x2000

struct low_level_shape_definition
{
    uint16 flags; /* [x-mirror.1] [y-mirror.1] [keypoint_obscured.1] [unused.13] */

    ao_fixed minimum_light_intensity; /* in [0,FIXED_ONE] */

    int16 bitmap_index;
    
    /* (x,y) in pixel coordinates of origin */
    int16 origin_x, origin_y;
    
    /* (x,y) in pixel coordinates of key point */
    int16 key_x, key_y;

    int16 world_left, world_right, world_top, world_bottom;
    int16 world_x0, world_y0;
    
    int16 unused[4];
};

const int SIZEOF_low_level_shape_definition = 36;


//-----------------------------------------------------------------------------
// a single Shapes file collection


struct ShapesCollection
{
	int16 version;
	
	int16 type; // used for get_shape_descriptors() // collection type above
	
	int16 color_count, clut_count;
	int32 color_table_offset; /* an array of clut_count arrays of color_count ColorSpec structures */

	int16 high_level_shape_count;
	int32 high_level_shape_offset_table_offset;

	int16 low_level_shape_count;
	int32 low_level_shape_offset_table_offset;

	int16 bitmap_count;
	int32 bitmap_offset_table_offset;

	int16 pixels_to_world; // used to shift pixel values into world coordinates // pretty sure this means scaling (it's also in low-level collection) but I'm not seeing code that uses it?!
    
	std::vector<shapes_color_t> color_tables; // bit annoying that this is a flat array of all colors, not vector<vector<shapes_color_t>> which'd allow get_color_table to return a reference to the clut
	std::vector<std::vector<uint8> > high_level_shapes; // really vector<high_level_shape_definition>; why isn't it better typed?
	std::vector<low_level_shape_definition> low_level_shapes; // = 1 animation frame
	std::vector<std::vector<uint8> > bitmaps; // ugh
    
    
    // accessors
    
    // (clut_count = number of cluts in this table, color_count = number of colors per clut)
    shapes_color_t* get_clut(short clut_number)
    {
        if (clut_number < 0 || clut_number >= clut_count) { throw_out_of_bounds_f("Bad clut: %d", clut_number); }
        return &color_tables[clut_number * color_count];
    }
    
    // an animated (or stationary) sequence viewable from 1-8 sides
    high_level_shape_definition* get_high_level_definition(short high_level_shape_index)
    {
        if (high_level_shape_index < 0 || high_level_shape_index >= high_level_shapes.size()) return nullptr;
        if (high_level_shapes[high_level_shape_index].empty()) return nullptr;
        return (high_level_shape_definition*)&high_level_shapes[high_level_shape_index][0];
    }
    
    // a single frame in the above
    low_level_shape_definition* get_frame(short low_level_shape_index)
    {
        if (low_level_shape_index < 0 || low_level_shape_index >= low_level_shapes.size()) return nullptr;
        return &low_level_shapes[low_level_shape_index];
    }
    
    short get_bitmap_index_for_frame(short low_level_shape_index)
    {
        low_level_shape_definition* low_level_shape = get_frame(low_level_shape_index);
        return low_level_shape ? low_level_shape->bitmap_index : NONE;
    }
    
    bitmap_definition_t* get_bitmap_definition(short bitmap_index)
    {
        if (bitmap_index < 0 || bitmap_index >= bitmaps.size()) return nullptr;
        if (bitmaps[bitmap_index].empty()) return nullptr;
        return (bitmap_definition_t*)&bitmaps[bitmap_index][0];
    }
};

const int SIZEOF_ShapesCollection = 544;




#endif
