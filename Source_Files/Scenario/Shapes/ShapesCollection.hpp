/*
 ShapesCollection.hpp
 
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

#ifndef ShapesCollection_hpp
#define ShapesCollection_hpp


// EES: data structures used in shapes.cpp implementation, so really belongs in shapes.h


#include "cseries.hpp"

#include "DataFile.hpp"

#include "textures.h" // bitmap_definition_t



//-----------------------------------------------------------------------------

// number of levels per color channel
#define PIXEL8_MAXIMUM_COLORS      (256)
#define PIXEL16_MAXIMUM_COMPONENT  (32)
#define PIXEL32_MAXIMUM_COMPONENT  (256)


// shading tables are fixed sizes

#define number_of_shading_tables_8        (32)
#define shading_table_fractional_bits_8   (5)
#define shading_table_byte_size_8         (PIXEL8_MAXIMUM_COLORS * sizeof(pixel8))

#define number_of_shading_tables_16       (64)
#define shading_table_fractional_bits_16  (6)
#define shading_table_byte_size_16        (PIXEL8_MAXIMUM_COLORS * sizeof(pixel16))

#define number_of_shading_tables_32       (256)
#define shading_table_fractional_bits_32  (8)
#define shading_table_byte_size_32        (PIXEL8_MAXIMUM_COLORS * sizeof(pixel32))


enum // shading tables
{
    _shading_normal, // color-to-black ramps
    _shading_infravision // "false color"
};


// each collection also has a tint table which recolors its entire clut for infravision
#define NUMBER_OF_TINT_TABLES  (1)


// there are 3 reserved transparency colors at the beginning of every clut (typically cyan, magenta, blue)
#define NUMBER_OF_PRIVATE_COLORS  (3)


struct shapes_color_t
{
    bool luminescent; // was `uint8 flags` but luminescent was the only flag
    uint8_t index;    // was `uint8 value` but it's the index at which this color appears in the clut
    ao_rgb value;
    
    void read(SDL_RWops* p) // 8 bytes
    {
        luminescent = SDL_ReadU8(p);
        index       = SDL_ReadU8(p);
        value.r     = SDL_ReadBE16(p);
        value.g     = SDL_ReadBE16(p);
        value.b     = SDL_ReadBE16(p);
    }
};


typedef std::vector<shapes_color_t> shapes_colors_t;


//-----------------------------------------------------------------------------
// low-level shape definition


// TODO: deal with these in unpacking
#define _X_MIRRORED_BIT         (0x8000)
#define _Y_MIRRORED_BIT         (0x4000)
#define _KEYPOINT_OBSCURED_BIT  (0x2000)


struct shapes_frame_t
{
    uint16 flags; // [x-mirror.1] [y-mirror.1] [keypoint_obscured.1] [unused.13] // TODO: use bools

    ao_fixed minimum_light_intensity; // 0...FIXED_ONE

    int16 bitmap_index;
    
    // (x,y) in pixel coordinates of origin
    int16 origin_x, origin_y;
    
    // (x,y) in pixel coordinates of key point
    int16 key_x, key_y;

    int16 world_left, world_right, world_top, world_bottom;
    int16 world_x0, world_y0;
    
    
    void read(SDL_RWops* p); // 36 bytes
};

#define shapes_frame_data_size  (36)


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


struct shapes_animation_t
{
    int16 number_of_views; // must be 1, 4, 5 or 8 (see below)
    
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
    std::vector<int16_t> low_level_shape_indexes;
    
    bool empty() const { return frames_per_view == 0; }
    
    void read(SDL_RWops* p);
};

const int SIZEOF_high_level_shape_definition = 90;


//-----------------------------------------------------------------------------
// an M1/M2 Shapes collection


enum // collection types
{
    _unused_collection    = 0, // raw
    _wall_collection      = 1, // raw
    _object_collection    = 2, // rle
    _interface_collection = 3, // raw
    _scenery_collection   = 4, // rle
};


struct ShapesCollection
{
    // from the M2 Shapes header: locations of 8-bit collection data and optional 16-bit collection data in file
    int32_t offset8, length8;
    int32_t offset16, length16;
    
    // collection data
    
    bool loaded = false;
    
    int16_t index; // collection_index
    
    bool is_m1;
        
	int16 version;
	
	int16 type; // collection types above
	
	int16 pixels_to_world; // used to shift pixel values into world coordinates // pretty sure this means scaling (it's also in low-level collection) but I'm not seeing code that uses it?

    int16 color_count, clut_count; // number of colors per-clut

	std::vector<shapes_color_t> cluts;
    
    std::vector<shapes_animation_t> animation_sequences; // = 1 animation sequence seen from 1-8 angles
    
	std::vector<shapes_frame_t> animation_frames; // = 1 animation frame
    
    std::vector<bitmap_definition_t> bitmaps;
    
    ao_rgb infravision_tint; // (replaces OGL_SetInfravisionTint in OGL_TextureManager.cpp)
    
    std::vector<pixel8>  shading_tables_8;
    std::vector<pixel16> shading_tables_16;
    std::vector<pixel32> shading_tables_32; // EES: I have no idea why the OGL code needs a shading table, but it does
    
    // accessors
    
    // (clut_count = number of cluts in this table, color_count = number of colors per clut)
    shapes_color_t* get_clut(short clut_number);
    
    // an animated (or stationary) sequence viewable from 1-8 sides
    shapes_animation_t* get_animation(short high_level_shape_index);
    
    // a single frame in the above
    shapes_frame_t* get_frame(short low_level_shape_index);
    
    short get_bitmap_index_for_frame(short low_level_shape_index);
    
    bitmap_definition_t* get_bitmap_definition(short bitmap_index);
    
    
    // color mapping tables
    
    void* get_shading_table(int32_t clut_index, int32_t bit_depth);
    void* get_tint_table(int32_t tint_index, int32_t bit_depth);
    
    pixel8*  get_shading_table_8(short clut_index) { return &shading_tables_8[clut_index * PIXEL8_MAXIMUM_COLORS]; }
    pixel16* get_shading_table_16(short clut_index) { return &shading_tables_16[clut_index * PIXEL8_MAXIMUM_COLORS]; }
    pixel32* get_shading_table_32(short clut_index) { return &shading_tables_32[clut_index * PIXEL8_MAXIMUM_COLORS]; }
    
    pixel8*  get_tint_table_8(short tint_index) { return get_shading_table_8(clut_count + tint_index); }
    pixel16* get_tint_table_16(short tint_index) { return get_shading_table_16(clut_count + tint_index); }
    pixel32* get_tint_table_32(short tint_index) { return get_shading_table_32(clut_count + tint_index); }
    
    
    // read collections from M1/M2 Shapes file
    
    void read_m2_header(DataFile& shapes_file); // (M1 Shapes is resource-based and doesn't have a header)
    
    void read(SDL_RWops* p, int32_t src_offset); // both M1 and M2
    
};



#endif /* ShapesCollection_hpp */
