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

#include "ShapesCollection.hpp"

#include "textures.h"


//-----------------------------------------------------------------------------
// accessors


shapes_color_t* ShapesCollection::get_clut(short clut_number)
{
    if (clut_number < 0 || clut_number >= clut_count) { throw_out_of_bounds_f("Bad clut: %d", clut_number); }
    return &cluts[clut_number * color_count];
}


// an animated (or stationary) sequence viewable from 1-8 sides
shapes_animation_t* ShapesCollection::get_animation(short high_level_shape_index)
{
    if (high_level_shape_index < 0 || high_level_shape_index >= animation_sequences.size()) return nullptr;
    if (animation_sequences[high_level_shape_index].frames_per_view == 0) return nullptr;
    return &animation_sequences[high_level_shape_index];
}


// a single frame in the above
shapes_frame_t* ShapesCollection::get_frame(short low_level_shape_index)
{
    if (low_level_shape_index < 0 || low_level_shape_index >= animation_frames.size()) return nullptr;
    return &animation_frames[low_level_shape_index];
}


// Which bitmap index for a frame (used in OpenGL texture rendering)
short ShapesCollection::get_bitmap_index_for_frame(short low_level_shape_index)
{
    shapes_frame_t* low_level_shape = get_frame(low_level_shape_index);
    return low_level_shape ? low_level_shape->bitmap_index : NONE;
}


bitmap_definition_t* ShapesCollection::get_bitmap_definition(short bitmap_index)
{
    if (bitmap_index < 0 || bitmap_index >= bitmaps.size() || bitmaps[bitmap_index].width == 0) return nullptr;
    return &bitmaps[bitmap_index];
}




void* ShapesCollection::get_shading_table(int32_t clut_index, int32_t bit_depth)
{
    switch (bit_depth)
    {
        case 8:
            return get_shading_table_8(clut_index);
        case 16:
            return get_shading_table_16(clut_index);
        case 32:
            return get_shading_table_32(clut_index);
        default:
            throw_bug_report("Unsupported bit_depth");
    }
}


void* ShapesCollection::get_tint_table(int32_t tint_index, int32_t bit_depth)
{
    switch (bit_depth)
    {
        case 8:
            return get_tint_table_8(tint_index);
        case 16:
            return get_tint_table_16(tint_index);
        case 32:
            return get_tint_table_32(tint_index);
        default:
            throw_bug_report("Unsupported bit_depth");
    }
}


//-----------------------------------------------------------------------------
// parsing


void shapes_frame_t::read(SDL_RWops* p) // 36 bytes
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


#define HIGH_LEVEL_SHAPE_NAME_LENGTH  (32)

void shapes_animation_t::read(SDL_RWops* p)
{
    SDL_ReadBE16(p); // type
    SDL_ReadBE16(p); // flags
    SDL_RWseek(p, HIGH_LEVEL_SHAPE_NAME_LENGTH + 2, SEEK_CUR); // name
    
    number_of_views         = SDL_ReadBE16(p);
    frames_per_view         = SDL_ReadBE16(p);
    ticks_per_frame         = SDL_ReadBE16(p);
    key_frame               = SDL_ReadBE16(p);
    transfer_mode           = SDL_ReadBE16(p);
    transfer_mode_period    = SDL_ReadBE16(p);
    first_frame_sound       = SDL_ReadBE16(p);
    key_frame_sound         = SDL_ReadBE16(p);
    last_frame_sound        = SDL_ReadBE16(p);
    pixels_to_world         = SDL_ReadBE16(p);
    loop_frame              = SDL_ReadBE16(p);
     
    SDL_RWseek(p, 14 * sizeof(int16_t), SEEK_CUR);

    // Convert low-level shape index list
    int32_t total_number_of_views;
    switch (number_of_views)
    {
        case _unanimated:
        case _animated1:
            total_number_of_views = 1;
            break;
        case _animated3to4:
        case _animated4:
            total_number_of_views = 4;
            break;
        case _animated3to5:
        case _animated5:
            total_number_of_views = 5;
            break;
        case _animated2to8:
        case _animated5to8:
        case _animated8:
            total_number_of_views = 8;
            break;
        default:
            total_number_of_views = number_of_views;
    }
    low_level_shape_indexes.resize(total_number_of_views * frames_per_view);
    for (int32_t i = 0; i < low_level_shape_indexes.size(); i++)
    {
        low_level_shape_indexes[i] = SDL_ReadBE16(p);
    }
}


void ShapesCollection::read_m2_header(DataFile& shapes_file)
{
    shapes_file.skip(2 * 2); // status, flags (no longer used)
    offset8  = shapes_file.read_i32();
    length8  = shapes_file.read_i32();
    offset16 = shapes_file.read_i32();
    length16 = shapes_file.read_i32();
    shapes_file.skip(6 * 2);
}


void ShapesCollection::read(SDL_RWops* p, int32_t src_offset)
{
    SDL_RWseek(p, src_offset, RW_SEEK_SET);
    
    // const int SIZEOF_ShapesCollection = 544;
    version                                         = SDL_ReadBE16(p);
    type                                            = SDL_ReadBE16(p);
    SDL_ReadBE16(p); // skip `flags` (unused)
    color_count                                     = SDL_ReadBE16(p);
    clut_count                                      = SDL_ReadBE16(p);
    int32_t color_table_offset                      = SDL_ReadBE32(p);
    int32_t high_level_shape_count                  = SDL_ReadBE16(p);
    int32_t high_level_shape_offset_table_offset    = SDL_ReadBE32(p);
    int32_t low_level_shape_count                   = SDL_ReadBE16(p);
    int32_t low_level_shape_offset_table_offset     = SDL_ReadBE32(p);
    int32_t bitmap_count                            = SDL_ReadBE16(p);
    int32_t bitmap_offset_table_offset              = SDL_ReadBE32(p);
    pixels_to_world                                 = SDL_ReadBE16(p);
    SDL_ReadBE32(p); // skip size
    SDL_RWseek(p, 253 * sizeof(int16), SEEK_CUR); // unused
    
    // color tables
    cluts.resize(clut_count * color_count);
    if (cluts.size() > 0)
    {
        SDL_RWseek(p, src_offset + color_table_offset, RW_SEEK_SET);
        
        for (int i = 0; i < cluts.size(); i++)
        {
            cluts[i].read(p);
        }
    }
    
    // high-level shape definitions (animation sequences)
    animation_sequences.resize(high_level_shape_count);
    if (high_level_shape_count > 0)
    {
        SDL_RWseek(p, src_offset + high_level_shape_offset_table_offset, RW_SEEK_SET);
        std::vector<uint32> t(high_level_shape_count);
        SDL_RWread(p, &t[0], sizeof(uint32), high_level_shape_count);
        swap_array_BE32(&t[0], high_level_shape_count);
        
        for (int i = 0; i < high_level_shape_count; i++)
        {
            SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
            animation_sequences[i].read(p);
        }
    }
    
    // low-level shape definitions (animation frames)
    animation_frames.resize(low_level_shape_count);
    if (low_level_shape_count > 0)
    {
        SDL_RWseek(p, src_offset + low_level_shape_offset_table_offset, RW_SEEK_SET);
        std::vector<uint32> t(low_level_shape_count);
        SDL_RWread(p, &t[0], sizeof(uint32), low_level_shape_count);
        swap_array_BE32(&t[0], low_level_shape_count);
        
        for (int i = 0; i < low_level_shape_count; i++)
        {
            SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
            animation_frames[i].read(p);
        }
    }
    
    // bitmap definitions (sprites)
    bitmaps.resize(bitmap_count);
    if (bitmap_count > 0)
    {
        SDL_RWseek(p, src_offset + bitmap_offset_table_offset, RW_SEEK_SET);
        std::vector<uint32> t(bitmap_count);
        SDL_RWread(p, &t[0], sizeof(uint32), bitmap_count);
        swap_array_BE32(&t[0], bitmap_count);
        
        // bitmap_definition_t::read wants to know so it can fix M1's problem (non-128x128) wall texture[s]
        bool is_m1_wall_collection = false;
        if (is_m1)
        {
            switch (index)
            {
                case 2:
                case 8:
                case 17:
                case 18:
                case 19:
                case 24:
                    is_m1_wall_collection = true;
                default:
                {}
            }
        }
        for (int i = 0; i < bitmap_count; i++)
        {
            SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
            bitmaps[i].read(p, is_m1, is_m1_wall_collection);
        }
    }
    
    shading_tables_8.resize(PIXEL8_MAXIMUM_COLORS  * number_of_shading_tables_8  * (clut_count + NUMBER_OF_TINT_TABLES));
    shading_tables_16.resize(PIXEL8_MAXIMUM_COLORS * number_of_shading_tables_16 * (clut_count + NUMBER_OF_TINT_TABLES));
    shading_tables_32.resize(PIXEL8_MAXIMUM_COLORS * number_of_shading_tables_32 * (clut_count + NUMBER_OF_TINT_TABLES));
}

