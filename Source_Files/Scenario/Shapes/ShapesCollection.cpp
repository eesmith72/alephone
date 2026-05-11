

#include "ShapesCollection.h"

#include "Screen.hpp"



// (clut_count = number of cluts in this table, color_count = number of colors per clut)
shapes_color_t* ShapesCollection::get_clut(short clut_number)
{
    if (clut_number < 0 || clut_number >= clut_count) { throw_out_of_bounds_f("Bad clut: %d", clut_number); }
    return &color_tables[clut_number * color_count];
}


// an animated (or stationary) sequence viewable from 1-8 sides
shapes_animation_t* ShapesCollection::get_animation_sequence(short high_level_shape_index)
{
    // TODO: inclined to throw on bad indexes: they shouldn't happen unless the shapes data is buggy (which can happen depending on how that data's constructed) but it's simpler to throw an error up the stack and tell the modder to fix their mod than do null checks (which AO tends to let fail silently)
    if (high_level_shape_index < 0 || high_level_shape_index >= high_level_shapes.size()
                                   || high_level_shapes[high_level_shape_index].empty()) return nullptr;
    return (shapes_animation_t*)&high_level_shapes[high_level_shape_index][0];
}


// a single frame in the above
shapes_frame_t* ShapesCollection::get_frame(short low_level_shape_index)
{
    if (low_level_shape_index < 0 || low_level_shape_index >= low_level_shapes.size()) return nullptr;
    return &low_level_shapes[low_level_shape_index];
}


short ShapesCollection::get_bitmap_index_for_frame(short low_level_shape_index)
{
    shapes_frame_t* low_level_shape = get_frame(low_level_shape_index);
    return low_level_shape ? low_level_shape->bitmap_index : NONE;
}


bitmap_definition_t* ShapesCollection::get_bitmap_definition(short bitmap_index)
{
    if (bitmap_index < 0 || bitmap_index >= bitmaps.size() || bitmaps[bitmap_index].empty()) return nullptr;
    return (bitmap_definition_t*)&bitmaps[bitmap_index][0];
}


// shading tables

void* ShapesCollection::get_shading_table(short clut_index)
{
    switch (main_screen.bit_depth())
    {
        case 8:
            return shading_tables_8[clut_index].data();
        case 16:
            return shading_tables_16[clut_index].data();
        case 32:
            return shading_tables_32[clut_index].data();
        default:
            throw_bug_report("Bad bit depth.");
    }
}


void* ShapesCollection::get_tint_table(short tint_index)
{
    return get_shading_table(clut_count + tint_index);
}



void ShapesCollection::read_header(DataFile& shapes_file)
{
    status   = shapes_file.read_i16();
    shapes_file.skip(2); // flags (unused)
    offset8  = shapes_file.read_i32();
    length8  = shapes_file.read_i32();
    offset16 = shapes_file.read_i32();
    length16 = shapes_file.read_i32();
    shapes_file.skip(6 * 2);
}


// const int SIZEOF_ShapesCollection = 544;
void ShapesCollection::read_content(SDL_RWops* p)
{
    version = SDL_ReadBE16(p);
    type = SDL_ReadBE16(p);
    SDL_ReadBE16(p); // skip `flags` (unused)
    color_count = SDL_ReadBE16(p);
    clut_count = SDL_ReadBE16(p);
    color_table_offset = SDL_ReadBE32(p);
    high_level_shape_count = SDL_ReadBE16(p);
    high_level_shape_offset_table_offset = SDL_ReadBE32(p);
    low_level_shape_count = SDL_ReadBE16(p);
    low_level_shape_offset_table_offset = SDL_ReadBE32(p);
    bitmap_count = SDL_ReadBE16(p);
    bitmap_offset_table_offset = SDL_ReadBE32(p);
    pixels_to_world = SDL_ReadBE16(p);
    SDL_ReadBE32(p); // skip size
    SDL_RWseek(p, 253 * sizeof(int16), SEEK_CUR); // unused
    
    // while these are now vectors, shapes.cpp still treats them as old-school C arrays with lots of old-school pointer math; improving typing is WIP
    color_tables.resize(clut_count * color_count);
    high_level_shapes.resize(high_level_shape_count);
    low_level_shapes.resize(low_level_shape_count);
    bitmaps.resize(bitmap_count);
    
    shading_tables_8.resize(shading_table_size_8 * clut_count + shading_table_size_8 * NUMBER_OF_TINT_TABLES);
    shading_tables_16.resize(shading_table_size_16 * clut_count + shading_table_size_16 * NUMBER_OF_TINT_TABLES);
    shading_tables_32.resize(shading_table_size_32 * clut_count + shading_table_size_32 * NUMBER_OF_TINT_TABLES);
}


