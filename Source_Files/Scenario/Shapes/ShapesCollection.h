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


// TODO: when unloading one Shapes file to load another, need to call OGL_UnloadModelsImages; at least until ShapesCollection manages all of a collection's MML/OGL patches too


#include "cseries.hpp"

#include "DataFile.hpp"


//-----------------------------------------------------------------------------




enum // collection types
{
	_unused_collection    = 0, // raw
	_wall_collection      = 1, // raw
	_object_collection    = 2, // rle
	_interface_collection = 3, // raw
	_scenery_collection   = 4, // rle
};


//-----------------------------------------------------------------------------
// shading and tint tables are fixed sizes

const int32_t number_of_shading_tables_8        = 32;
const int32_t shading_table_fractional_bits_8   = 5;
const int32_t shading_table_size_8              = PIXEL8_MAXIMUM_COLORS * sizeof(pixel8);

const int32_t number_of_shading_tables_16       = 64;
const int32_t shading_table_fractional_bits_16  = 6;
const int32_t shading_table_size_16             = PIXEL8_MAXIMUM_COLORS * sizeof(pixel16);

const int32_t number_of_shading_tables_32       = 256;
const int32_t shading_table_fractional_bits_32  = 8;
const int32_t shading_table_size_32             = PIXEL8_MAXIMUM_COLORS * sizeof(pixel32);

// each collection also has a tint table which recolors its entire clut for infravision
#define NUMBER_OF_TINT_TABLES  (1)


struct shapes_color_t
{
    bool luminescent; // was `uint8 flags` but luminescent was the only flag
    uint8_t index; // was `uint8 value` but looks like it's the clut index
    ao_rgb value;
};

const int SIZEOF_shapes_color_t = 8;


typedef std::vector<shapes_color_t> shapes_colors_t;


//-----------------------------------------------------------------------------
// high level shape definition


enum // animation types // TODO: where does this get used? find out and type it
{
    _animated1    = 1,
    _animated2to8 = 2, /* ?? */
    _animated3to4 = 3,
    _animated4    = 4,
    _animated5to8 = 5,
    _animated8    = 8,
    _animated3to5 = 9,
    _unanimated   = 10,
    _animated5    = 11,
};


#define HIGH_LEVEL_SHAPE_NAME_LENGTH 32

struct shapes_animation_t // see also shapes_animation_t in shapes.h, which is a subset of this info
{
    //char name[HIGH_LEVEL_SHAPE_NAME_LENGTH+2]; // it's in the Shapes file, but only used by Anvil so let's ignore it and consolidate
    
    int16 number_of_views; // must be 1, 2, 5 or 8 -- old comment // EES: I don't recall 2 being a thing; it can be 4 tho'
    
    int16 frames_per_view, ticks_per_frame;
    int16 key_frame;
    
    int16 transfer_mode;
    int16 transfer_mode_period; // in ticks
    
    int16 first_frame_sound, key_frame_sound, last_frame_sound;

    int16 pixels_to_world;

    int16 loop_frame;
    
    // N*frames_per_view indexes of low-level shapes follow:
    //
    //   N = 1 if number_of_views = _unanimated   / _animated1
    //   N = 4 if number_of_views = _animated3to4 / _animated4
    //   N = 5 if number_of_views = _animated3to5 / _animated5
    //   N = 8 if number_of_views = _animated2to8 / _animated5to8 / _animated8
    //
    int16 low_level_shape_indexes[1]; // oh joy, looks like another variable-length struct; replacing this with std::vector<int16_t> is the right way to move forward
};

const int SIZEOF_high_level_shape_definition = 90;


//-----------------------------------------------------------------------------
// low-level shape definition


#define _X_MIRRORED_BIT 0x8000
#define _Y_MIRRORED_BIT 0x4000
#define _KEYPOINT_OBSCURED_BIT 0x2000

struct shapes_frame_t
{
    uint16 flags; // [x-mirror.1] [y-mirror.1] [keypoint_obscured.1] [unused.13]

    ao_fixed minimum_light_intensity; // 0...FIXED_ONE

    int16 bitmap_index;
    
    // (x,y) in pixel coordinates of origin
    int16 origin_x, origin_y;
    
    // (x,y) in pixel coordinates of key point
    int16 key_x, key_y;

    int16 world_left, world_right, world_top, world_bottom;
    int16 world_x0, world_y0;
    
    
    void read(SDL_RWops* p) // 36 bytes
    {
        flags                   = SDL_ReadBE16(p);
        minimum_light_intensity = SDL_ReadBE32(p);
        bitmap_index            = SDL_ReadBE16(p);
        origin_x                = SDL_ReadBE16(p);
        origin_y                = SDL_ReadBE16(p);
        key_x                   = SDL_ReadBE16(p);
        key_y                   = SDL_ReadBE16(p);
        world_left              = SDL_ReadBE16(p);
        world_right             = SDL_ReadBE16(p);
        world_top               = SDL_ReadBE16(p);
        world_bottom            = SDL_ReadBE16(p);
        world_x0                = SDL_ReadBE16(p);
        world_y0                = SDL_ReadBE16(p);
        SDL_RWseek(p, 4 * sizeof(int16_t), SEEK_CUR);
    }
};

#define shapes_frame_data_size  (36)


//-----------------------------------------------------------------------------
// ShapesCollection


struct bitmap_definition_t; // defined in textures.h

typedef std::vector<uint8_t> bitmap_data_t; // this should become bitmap_definition once that struct is modernized

typedef std::array<pixel8,  PIXEL8_MAXIMUM_COLORS * number_of_shading_tables_8>  shading_tables_8_t;
typedef std::array<pixel16, PIXEL8_MAXIMUM_COLORS * number_of_shading_tables_16> shading_tables_16_t;
typedef std::array<pixel32, PIXEL8_MAXIMUM_COLORS * number_of_shading_tables_32> shading_tables_32_t;


struct ShapesCollection
{
    bool loaded = false;
    
    int16_t index;
    
    int16_t status; // unloaded, loaded, needs loaded, needs unloaded; though we'll probably dump the lot
    
    // locations of 8-bit and optional 16-bit [bitmap?] data in Shapes file
    int32_t offset8, length8;
    int32_t offset16, length16;
    
    std::vector<shading_tables_8_t> shading_tables_8;
    std::vector<shading_tables_16_t> shading_tables_16;
    std::vector<shading_tables_32_t> shading_tables_32;
    
	int16_t version;
	
	int16_t type; // this is the 'collection types' enum above
	
	int16_t color_count, clut_count;
	int32_t color_table_offset; // an array of clut_count arrays of color_count ColorSpec structures
    
	int16_t high_level_shape_count;
	int32_t high_level_shape_offset_table_offset;
    
	int16_t low_level_shape_count;
	int32_t low_level_shape_offset_table_offset;
    
	int16_t bitmap_count;
	int32_t bitmap_offset_table_offset;
    
	int16_t pixels_to_world; // used to shift pixel values into world coordinates // pretty sure this means scaling (it's also in low-level collection) but I'm not seeing code that uses it?!
    
	std::vector<shapes_color_t> color_tables; // EES: bit annoying that this is a flat array of all colors, not vector<vector<shapes_color_t>> which'd allow get_color_table to return a reference to the clut that comes with .size() and other niceties attached, paying down M2's vast quantities of cryptic pointer math
    
	std::vector<std::vector<uint8>> high_level_shapes; // really vector<high_level_shape_definition>; why isn't it better typed?
    
	std::vector<shapes_frame_t> low_level_shapes; // = 1 animation frame
    
	std::vector<bitmap_data_t> bitmaps; // hrmm; 1. an array of bitmap definitions, but currently vector<uint8> because they're variable-size structs that need converted to fixed-size structs with a std::vector for the variable data; 2. presumably when 16-bit collections are loaded, this contains the 16-bit bitmaps, not 8-bit, in which case we'll need another vector
    
    ao_rgb infravision_tint; // (replaces OGL_SetInfravisionTint in OGL_TextureManager.cpp)

    
    // accessors
    
    // (clut_count = number of cluts in this table, color_count = number of colors per clut)
    shapes_color_t* get_clut(short clut_number);
    
    // an animated (or stationary) sequence viewable from 1-8 sides
    shapes_animation_t* get_animation_sequence(short high_level_shape_index);
    
    // a single frame in the above
    shapes_frame_t* get_frame(short low_level_shape_index);
    
    short get_bitmap_index_for_frame(short low_level_shape_index);
    
    bitmap_definition_t* get_bitmap_definition(short bitmap_index);
    
    
    // color mapping tables
    
    void* get_shading_table(short clut_index); // for screen's current bit_depth
    void* get_tint_table(short tint_index);
    
    
    shading_tables_8_t&  get_shading_table_8(short  clut_index) { return shading_tables_8.at(clut_index); }
    shading_tables_16_t& get_shading_table_16(short clut_index) { return shading_tables_16.at(clut_index); }
    shading_tables_32_t& get_shading_table_32(short clut_index) { return shading_tables_32.at(clut_index); }
    
    
    shading_tables_8_t&  get_tint_table_8(short  tint_index) { return shading_tables_8.at(clut_count  + tint_index); }
    shading_tables_16_t& get_tint_table_16(short tint_index) { return shading_tables_16.at(clut_count + tint_index); }
    shading_tables_32_t& get_tint_table_32(short tint_index) { return shading_tables_32.at(clut_count + tint_index); }
    
    
    // read from Shapes file
    
    void read_header(DataFile& shapes_file);
    
    void read_content(SDL_RWops* p);
};


#endif
