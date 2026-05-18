/*
SHAPES.C

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

#include "shapes.h"

#include "render.h"
#include "interface.hpp"
#include "Screen.hpp"
#include "DataFile.hpp"
#include "images.h"

#include "infravision.hpp"

#include "map.h" // ??

#include "graphics_preferences.hpp"

#include "infravision.hpp"

#include "OGL_Setup.h" // OGL_LoadModelsImages

#include "Packing.h"

#include "Plugins.h"


// TODO: FIX: green fighters turn blue when switching from 8-bit to 16-bit and 32-bit

// TODO: FIX: clicking on window's title bar (gaining focus) causes window to jump position, eventually off bottom of screen

// TODO: FIX: fighter melee impact isn't playing sound (can't recall if impact sound is defined in Shapes/Physics)

// TODO: FIX: FOV is a bit fisheye in Classic modes


// TODO: support M2's optional 16-bit collections (landscapes, WIH)


//-----------------------------------------------------------------------------


color_table_t gameworld_color_table_8; // this is the gameworld's indexed color table constructed from Shapes collections; used in Classic 8


static std::array<ShapesCollection, MAXIMUM_COLLECTIONS> shapes_collections_8;

static std::array<ShapesCollection, MAXIMUM_COLLECTIONS> shapes_collections_16; // TODO: put any 16-bit collections in here; Classic16 will use preferentially these collections over their 8-bit equivalents

// TODO: shapes_collections_32, shapes_collections_3D for Modern RGBA32 sprite sheets and 3D models (AO's replacement collections will need auto-converted to sprite sheets; not sure how we'll handle 3D models but may be worth considering future SDL_gpu needs); also need to decide how best to support alternate colors (e.g. separate monster "uniform" into its own monochrome sheet, which can be colorized and overlaid on the base sprite similar to glow images?)


std::vector<uint8> shapes_patch; // TODO: will be simplest to import patches over existing collections, then immediately export to new ShapesCollection packages


SDL_PixelFormat pixel_format_8, pixel_format_16, pixel_format_32;


#define NUMBER_OF_RGB_COMPONENTS  (3)

static pixel8  global_shading_table_8[PIXEL8_MAXIMUM_COLORS];
                                                                                                                  
static pixel16 global_shading_table_16[number_of_shading_tables_16 * NUMBER_OF_RGB_COMPONENTS * PIXEL16_MAXIMUM_COMPONENT];

static pixel32 global_shading_table_32[number_of_shading_tables_32 * NUMBER_OF_RGB_COMPONENTS * PIXEL32_MAXIMUM_COMPONENT];


//-----------------------------------------------------------------------------


// TODO: these need to move to Scenario/
static DataFile ShapesFile_M2;

ResourceFile ShapesFile_M1;

static bool is_shapes_file_m1;

bool shapes_file_is_m1() { return is_shapes_file_m1; }


static void close_shapes_file();



void load_shapes_patch(SDL_RWops *p, bool override_replacements);



static void update_color_environment();




void initialize_shapes()
{
    SDL_PixelFormat *pf;
    pf = SDL_AllocFormat(AO_PIXEL_FORMAT_8);
    pixel_format_8 = *pf;
    SDL_FreeFormat(pf);
    pf = SDL_AllocFormat(AO_PIXEL_FORMAT_16);
    pixel_format_16 = *pf;
    SDL_FreeFormat(pf);
    pf = SDL_AllocFormat(AO_PIXEL_FORMAT_32);
    pixel_format_32 = *pf;
    SDL_FreeFormat(pf);
    
    atexit(close_shapes_file);
}


//-----------------------------------------------------------------------------


static void load_m1_collections()
{
    for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        ShapesCollection* collection = &shapes_collections_8.at(collection_index);
        collection->is_m1 = true;
        collection->index = collection_index;
        
        // Collections are stored in .256 resources
        LoadedResource rsrc;
        if (ShapesFile_M1.Get('.', '2', '5', '6', 128 + collection_index, rsrc))
        {
            SDL_RWops* p = SDL_RWFromConstMem(rsrc.GetPointer(), (int32_t)rsrc.get_length());
            collection->read(p, 0); // src_offset = 0 (start of resource)
            SDL_FreeRW(p);
            
            collection->loaded = true;
        }
        else
        {
            collection->loaded = false;
        }
        
        shapes_collections_16.at(collection_index).loaded = false;
    }
}


static void load_m2_collections()
{
    // TODO: not at all sure this is correct
    
    ShapesFile_M2.set_position(0);
    
    // read the data file headers to get locations of collections in file
    for (int collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        ShapesCollection* collection = &shapes_collections_8[collection_index];
        collection->is_m1 = false;
        collection->index = collection_index;
        
        collection->read_m2_header(ShapesFile_M2);
    }
    
    //read the collections
    for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        ShapesCollection* collection = &shapes_collections_8.at(collection_index);

        SDL_RWops* p = ShapesFile_M2.borrow_rwops();

        if (collection->offset8 != NONE)
        {
            // Get offset of data in source file from header
            int32_t src_offset = collection->offset8 + (int32_t)SDL_RWtell(p);
            collection->read(p, src_offset);
            
            OGL_LoadModelsImages(collection_index); // TODO: think this is okay here

            collection->loaded = true;
        }
        else
        {
            collection->loaded = false;
        }
        
        collection = &shapes_collections_16.at(collection_index);

        if (collection->offset16 != NONE)
        {
            // Get offset of data in source file from header
            int32_t src_offset = collection->offset16 + (int32_t)SDL_RWtell(p);
            collection->read(p, src_offset);
            collection->loaded = true;
        }
        else
        {
            collection->loaded = false;
        }
    }
}


bool collection_loaded(short collection_index)
{
    return get_shapes_collection(collection_index)->loaded;
}


bool shapes_collection_exists(short collection_index)
{
    return collection_index >= 0 && collection_index < NUMBER_OF_COLLECTIONS && get_shapes_collection(collection_index)->loaded;
}


/*
void unload_collections()
{
    for (short i = 0; i < MAXIMUM_COLLECTIONS; i++)
    {
        ShapesCollection* header = &shapes_collections[i];
        if (header->collection) { unload_collection(header); }
        OGL_UnloadModelsImages(i);
    }
}
*/


//-----------------------------------------------------------------------------


// opening the Shapes file now reads all collections fully into memory
void open_shapes_file(const ao_path& path)
{
    // TODO: if an M1/M2 Shapes is already loaded, make sure it gets unloaded
    
    if (ShapesFile_M1.open(path) == no_err && ShapesFile_M1.Check('.','2','5','6', 128))
    {
        is_shapes_file_m1 = true;
        load_m1_collections();
    }
    else
    {
        is_shapes_file_m1 = false;
        ShapesFile_M1.Close();
        
        ao_err err = ShapesFile_M2.open(path);
        if (err) { throw_ao_exception_f("Can't open Shapes file: %s", err, path.c_str()); } // TODO: open_shapes_file should return ao_err
        
        load_m2_collections();
    }
    
    update_color_environment();
    
    open_shapes_file_resources(path); // crap that's over in images.cpp for reading resource fork
}


static void close_shapes_file()
{
    if (is_shapes_file_m1)
    {
        ShapesFile_M1.Close();
    }
    else
    {
        ShapesFile_M2.close();
    }
}


//-----------------------------------------------------------------------------
// shapes patch


/*
void load_collections()
{
    for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        load_collection_content(collection_index);
    }
    
    // TODO: ignore patches for now; need to get the basics working right first
    // TODO: nasty snaky global coupling; also, what about Plugins::load_shapes_patches?
    Plugins::instance()->load_shapes_patches(is_opengl);
    if (shapes_patch.size())
    {
        SDL_RWops *f = SDL_RWFromMem(&shapes_patch[0], (int32_t)shapes_patch.size());
        load_shapes_patch(f, true);
        SDL_RWclose(f);
    }
    update_color_environment();
}
 */

#define ENDC_TAG FOUR_CHARS_TO_INT('e', 'n', 'd', 'c')
#define CLDF_TAG FOUR_CHARS_TO_INT('c', 'l', 'd', 'f')
#define HLSH_TAG FOUR_CHARS_TO_INT('h', 'l', 's', 'h')
#define LLSH_TAG FOUR_CHARS_TO_INT('l', 'l', 's', 'h')
#define BMAP_TAG FOUR_CHARS_TO_INT('b', 'm', 'a', 'p')
#define CTAB_TAG FOUR_CHARS_TO_INT('c', 't', 'a', 'b')


void set_shapes_patch_data(uint8* data, size_t length)
{
	if (!length) 
	{
		shapes_patch.clear();
	}
	else
	{
		shapes_patch.resize(length);
		memcpy(&shapes_patch[0], data, length);
	}
}


uint8* get_shapes_patch_data(size_t &length)
{
	length = shapes_patch.size();
	return length > 0 ? &shapes_patch[0] : 0;
}


void load_shapes_patch(SDL_RWops *p, bool override_replacements)
{
    std::vector<int16_t> color_counts(MAXIMUM_COLLECTIONS);
    int32_t start = (int32_t)SDL_RWtell(p);
    SDL_RWseek(p, 0, SEEK_END);
    int32_t end = (int32_t)SDL_RWtell(p);
    
    SDL_RWseek(p, start, SEEK_SET);
    
    while (SDL_RWtell(p) < end)
    {
        int32 collection_index = SDL_ReadBE32(p);
        int32 patch_bit_depth = SDL_ReadBE32(p);
        
        bool collection_end = false;
        while (!collection_end)
        {
            // read a tag
            int32 tag = SDL_ReadBE32(p);
            switch (tag)
            {
                case ENDC_TAG:
                {
                    collection_end = true;
                    break;
                }
                case CLDF_TAG:
                {
                    TODO("Shapes patch");
                    /*
                     // a collection follows directly
                     ShapesCollection *collection = get_shapes_collection(collection_index);
                     if (collection->loaded && patch_bit_depth == 8)
                     {
                     load_ShapesCollection(collection, p);
                     color_counts[collection_index] = collection->color_count;
                     allocate_shading_tables(collection_index);
                     } else {
                     // get the color count (it's the only way to skip the CTAB_TAG
                     SDL_RWseek(p, 6, SEEK_CUR);
                     color_counts[collection_index] = SDL_ReadBE16(p);
                     SDL_RWseek(p, 544 - 8, SEEK_CUR);
                     }
                     */
                    break;
                }
                case HLSH_TAG:
                {
                    ShapesCollection *collection = get_shapes_collection(collection_index);
                    int32 high_level_shape_index = SDL_ReadBE32(p);
                    int32 size = SDL_ReadBE32(p);
                    int32 pos = (int32_t)SDL_RWtell(p);
                    if (collection && patch_bit_depth == 8 && high_level_shape_index < collection->animation_sequences.size())
                    {
                        collection->animation_sequences[high_level_shape_index].read(p);
                        SDL_RWseek(p, pos + size, SEEK_SET);
                    }
                    else
                    {
                        SDL_RWseek(p, size, SEEK_CUR);
                    }
                    break;
                }
                case LLSH_TAG:
                {
                    ShapesCollection *collection = get_shapes_collection(collection_index);
                    int32 low_level_shape_index = SDL_ReadBE32(p);
                    if (collection && patch_bit_depth == 8 && low_level_shape_index < collection->animation_frames.size())
                    {
                        collection->animation_frames[low_level_shape_index].read(p);
                    }
                    else
                    {
                        SDL_RWseek(p, 36, SEEK_CUR);
                    }
                    break;
                }
                case BMAP_TAG:
                {
                    ShapesCollection *collection = get_shapes_collection(collection_index);
                    int32 bitmap_index = SDL_ReadBE32(p);
                    int32 size = SDL_ReadBE32(p);
                    if (collection && patch_bit_depth == 8 && bitmap_index < collection->bitmaps.size())
                    {
                        collection->bitmaps[bitmap_index].read(p, false, collection->type == _wall_collection);
                        if (override_replacements)
                        {
                            collection->get_bitmap_definition(bitmap_index)->flags |= _PATCHED_BIT; // TODO: question: why do we care?
                        }
                    }
                    else
                    {
                        SDL_RWseek(p, size, SEEK_CUR);
                    }
                    break;
                }
                case CTAB_TAG:
                {
                    ShapesCollection *collection = get_shapes_collection(collection_index);
                    int32 color_table_index = SDL_ReadBE32(p);
                    if (collection && patch_bit_depth == 8 && (color_table_index * collection->color_count < collection->cluts.size()))
                    {
                        for (int i = 0; i < collection->cluts.size(); i++)
                        {
                            collection->cluts[i].read(p);
                        }
                    }
                    else
                    {
                        SDL_RWseek(p, color_counts[collection_index] * sizeof(shapes_color_t), SEEK_CUR);
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Unrecognized tag in patch file '%c%c%c%c'\n %x", tag >> 24, tag >> 16, tag >> 8, tag, tag);
            }
        }
    }
}


//-----------------------------------------------------------------------------


static void build_global_shading_table_8()
{
    // return the last shading_table calculated
    for (short collection_index = MAXIMUM_COLLECTIONS - 1; collection_index >= 0; collection_index--)
    {
        ShapesCollection* collection = get_shapes_collection(collection_index);
        if (collection->loaded)
        {
            *global_shading_table_8 = *collection->get_shading_table_8(0);
            break;
        }
    }
}


static void build_global_shading_table_16()
{
    pixel16* write = global_shading_table_16;
    for (short shading_table = 0; shading_table < number_of_shading_tables_16; shading_table++)
    {
        // Under SDL, the components may have different widths and different shifts // EES: old comment; all we care about is what format the Shapes file uses for 16-bit (xRGB1555? RGB565?) and does it match what we're using as screen buffer (565)
        int shift = pixel_format_16.Rshift + (3 - pixel_format_16.Rloss);
        for (short value = 0; value < PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
        shift = pixel_format_16.Gshift + (3 - pixel_format_16.Gloss);
        for (short value = 0; value < PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
        shift = pixel_format_16.Bshift + (3 - pixel_format_16.Bloss);
        for (short value = 0; value < PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
    }
}


static void build_global_shading_table_32()
{
    pixel32* write = global_shading_table_32;
    for (short shading_table = 0; shading_table < number_of_shading_tables_32; shading_table++)
    {
        // Under SDL, the components may have different widths and different shifts
        int shift = pixel_format_32.Rshift - pixel_format_32.Rloss;
        for (short value = 0; value < PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * shading_table) / (number_of_shading_tables_32 - 1)) << shift;
        }
        shift = pixel_format_32.Gshift - pixel_format_32.Gloss;
        for (short value = 0; value < PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * shading_table) / (number_of_shading_tables_32 - 1)) << shift;
        }
        shift = pixel_format_32.Bshift - pixel_format_32.Bloss;
        for (short value = 0; value < PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * shading_table) / (number_of_shading_tables_32 - 1)) << shift;
        }
    }
}


// Given a list of RGBColors, find out which one, if any, match the given color.
// If there aren’t any matches, add a new entry and return that index.
 // update_flags is false in `shading_remapping_table[INDEX]=...`
static short find_or_add_color(const shapes_color_t& color, shapes_colors_t& found_colors, bool update_flags = true)
{
    for (short i = 1; i < found_colors.size(); i++) // i=1 to skip the transparent color
	{
        shapes_color_t& found_color = found_colors[i];
		if (found_color.value == color.value)
		{
            if (update_flags) { found_color.luminescent = color.luminescent; }
			return i;
		}
	}
	
    if (found_colors.size() < PIXEL8_MAXIMUM_COLORS)
    {
        found_colors.push_back(color);
        return found_colors.size() - 1;
    }
    else
	{
        // TODO: this needs rethought when decomposing monolithic Shapes file into ShapesCollection plugins which may exceed the 256 limit when mixed and matched: only 8-bit Classic needs to remain in 256 limit so simplest is to disable classic8 mode; alternatively, when reading the collections, group the color ramps and, when all ramps are collected, if total colors > 256 then determine which ramps are closest to each other and merge them until <=256 colors is achieved, then remap the bitmaps to that
        
        // LP change: added a fallback strategy; if there were too many colors, then find the closest one
		// Set up minimum distance, its index. Strictly speaking, the distance squared, since that will be
		// what we will be calculating. The color values are data type "word", which is unsigned short;
		// this explains the initial choice of minimum value -- as greater than any possible such value.
        double MinDiffSq = 3.0 * 65536 * 65536;
        short MinIndx = 0;
        
        // Rescan
        for (short i = 1; i < found_colors.size(); i++)
        {
            shapes_color_t& found_color = found_colors[i];
            double diff_r = double(color.value.r) - double(found_color.value.r);
            double diff_g = double(color.value.g) - double(found_color.value.g);
            double diff_b = double(color.value.b) - double(found_color.value.b);
            double DiffSq = (diff_r * diff_r) + (diff_g * diff_g) + (diff_b * diff_b);
            if (DiffSq < MinDiffSq)
            {
                MinIndx = i;
                MinDiffSq = DiffSq;
            }
        }
        return MinIndx;
	}
	
}


bool get_next_color_run(const shapes_colors_t& colors, short& start, short& count)
{
    bool not_done = false;
    
    start += count;
    
    if (start < colors.size())
    {
        ao_rgb last_color = colors[start].value;
        for (count = 1; start + count < colors.size(); count++)
        {
            ao_rgb this_color = colors[start + count].value;
            if (this_color > last_color) { break; } // if this color is brighter than previous color, it's the start of a new run
            last_color = this_color;
        }
        not_done = true;
    }
    
    return not_done;
}


static void build_shading_tables_8(const shapes_colors_t& colors, pixel8* shading_tables)
{
    // the first table is always all-black
    memset(shading_tables, CLUT_BLACK, PIXEL8_MAXIMUM_COLORS * sizeof(pixel8));
    
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            const shapes_color_t& color = colors[start + i];
            short adjust = start ? 1 : 0;

            for (short table_index = 0; table_index < number_of_shading_tables_8; table_index++)
            {
                short multiplier = color.luminescent ? (table_index >> 1) : table_index;

                short value = i + (multiplier * (count + adjust - i)) / (number_of_shading_tables_8 - 1);
                shading_tables[PIXEL8_MAXIMUM_COLORS * (number_of_shading_tables_8 - 1 - table_index) + start + i]
                        = (value >= count) ? CLUT_BLACK : start + value;
            }
        }
    }
}


static void build_shading_tables_16(const shapes_colors_t colors, const pixel8* remapping_table, pixel16* shading_tables)
{
    // the first table is always all-black
    memset(shading_tables, 0, PIXEL8_MAXIMUM_COLORS * sizeof(pixel16));
    
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            const shapes_color_t& color = colors[remapping_table ? remapping_table[start + i] : (start + i)];
            
            for (short table_index = 0; table_index < number_of_shading_tables_16; table_index++)
            {
                short multiplier = color.luminescent ? ((number_of_shading_tables_16 >> 1) + (table_index >> 1)) : table_index;
                
                // (SW) Find optimal pixel value for 16-bit video display
                shading_tables[PIXEL8_MAXIMUM_COLORS * table_index + start + i]
                        = SDL_MapRGB(&pixel_format_16, ((color.value.r * multiplier) / (number_of_shading_tables_16 - 1)) >> 8,
                                                       ((color.value.g * multiplier) / (number_of_shading_tables_16 - 1)) >> 8,
                                                       ((color.value.b * multiplier) / (number_of_shading_tables_16 - 1)) >> 8);
            }
        }
    }
}


static void build_shading_tables_32(const shapes_colors_t colors, const pixel8* remapping_table, pixel32* shading_tables)
{
    // the first table is always all-black
    memset(shading_tables, 0, PIXEL8_MAXIMUM_COLORS * sizeof(pixel32));
    
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            const shapes_color_t& color = colors[remapping_table ? remapping_table[start + i] : (start + i)];
            
            for (short table_index = 0; table_index < number_of_shading_tables_32; table_index++)
            {
                short multiplier = color.luminescent ? ((number_of_shading_tables_32 >> 1) + (table_index >> 1)) : table_index;
                
                // (OGL) Mac xRGB 8888 pixel format
                shading_tables[PIXEL8_MAXIMUM_COLORS * table_index + start + i]
                        = RGBCOLOR_TO_PIXEL32((color.value.r * multiplier) / (number_of_shading_tables_32 - 1),
                                              (color.value.g * multiplier) / (number_of_shading_tables_32 - 1),
                                              (color.value.b * multiplier) / (number_of_shading_tables_32 - 1));
            }
        }
    }
}




static void update_color_environment()
{
    // An aggregate clut containing colors from all collections. Used as the global color table in 8-bit (256-color) SW rendering mode.
    // All bitmaps' pixel colors are remapped to this table, so the same color value always appears at the same position in this clut.
    // When rendering a bitmap in SW mode, final pixel color values are looked up using color index + clut index * lighting strength.
    shapes_colors_t aggregate_clut;
    aggregate_clut.reserve(PIXEL8_MAXIMUM_COLORS);
    
    // Remaps each pixel color from its position in the current collection's clut to its position in the aggregate clut.
    pixel8 remapping_table[PIXEL8_MAXIMUM_COLORS];
    memset(remapping_table, 0, PIXEL8_MAXIMUM_COLORS * sizeof(pixel8));
    
    // dummy color to hold the first index (zero) for transparent pixels
    aggregate_clut.push_back({false, 0, {65535, 65535, 65535}});
    
    // Loop through the loaded collections, building a single 256-color clut to use for all bitmaps and remapping their indexed colors
    // to it. We depend on finding the gray run (white to black) first, so the lowest-numbered loaded collection MUST give us this.
    for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        ShapesCollection* collection = get_shapes_collection(collection_index);
        
        // TODO: what about 16-bit color collections? (landscapes, WIH)
        
        if (collection->is_m1 && collection->index == 10) continue; // don't add main menu collection to the gameworld palette
        
        if (collection->loaded && collection->bitmaps.size() > 0)
        {
            shapes_color_t* primary_colors = collection->get_clut(0) + NUMBER_OF_PRIVATE_COLORS;
            
            // add the colors from this collection’s primary clut to the aggregate clut and build the remapping table...
            for (short color_index = 0; color_index < collection->color_count - NUMBER_OF_PRIVATE_COLORS; color_index++)
            {
                primary_colors[color_index].index = remapping_table[primary_colors[color_index].index]
                                                  = find_or_add_color(primary_colors[color_index], aggregate_clut);
            }
            
            // ...then remap each bitmap's pixels so ALL bitmaps use the same indexed colors in the aggregate table
            for (short bitmap_index = 0; bitmap_index < collection->bitmaps.size(); bitmap_index++)
            {
                bitmap_definition_t* bitmap_definition = collection->get_bitmap_definition(bitmap_index);
                if (!bitmap_definition)
                {
                    throw_out_of_bounds_f("Bad bitmap index for collection %d: %d", collection_index, bitmap_index);
                }
                bitmap_definition->remap_colors(remapping_table);
            }
            
            // build the primary shading tables (all three adjust the original bitmap's 8-bit indexed colors for lighting...)
            build_shading_tables_8(aggregate_clut, collection->get_shading_table_8(0));
            // (...and these two also transform the original bitmap's pixel8 indexed colors to pixel16/pixel32 values)
            
            // TODO: FIX: these aren't right: green fighters are wearing blue
            build_shading_tables_16(aggregate_clut, nullptr, collection->get_shading_table_16(0));
            build_shading_tables_32(aggregate_clut, nullptr, collection->get_shading_table_32(0));
            
            // build a shading table for each alternate clut in this collection
            // (in addition to lighting, these shading tables convert an 8-bit sprite's "uniform" from its default to alternate color)
            for (short clut_index = 1; clut_index < collection->clut_count; clut_index++)
            {
                shapes_color_t* alternate_colors = collection->get_clut(clut_index) + NUMBER_OF_PRIVATE_COLORS;
                
                // build a remapping table for the primary shading table which we can use to calculate this alternate shading table
                pixel8 shading_remapping_table[PIXEL8_MAXIMUM_COLORS];
                memset(shading_remapping_table, 0, PIXEL8_MAXIMUM_COLORS * sizeof(pixel8));
                
                for (short color_index = 0; color_index < PIXEL8_MAXIMUM_COLORS; color_index++)
                {
                    shading_remapping_table[color_index] = pixel8(color_index);
                }
                for (short color_index = 0; color_index < collection->color_count - NUMBER_OF_PRIVATE_COLORS; color_index++)
                {
                    pixel8 new_index = find_or_add_color(alternate_colors[color_index], aggregate_clut);
                    shading_remapping_table[find_or_add_color(primary_colors[color_index], aggregate_clut, false)] = new_index;
                }
                
                // duplicate the primary 8-bit shading table and remap it
                pixel8* alternate_shading_table_8 = collection->get_shading_table_8(clut_index);
                memcpy(alternate_shading_table_8, collection->get_shading_table_8(0), shading_table_byte_size_8);
                remap_bytes(alternate_shading_table_8, shading_remapping_table, shading_table_byte_size_8);
                
                // build the 16-bit table
                build_shading_tables_16(aggregate_clut, shading_remapping_table, collection->get_shading_table_16(clut_index));
                
                // build the 32-bit table (we no longer support 32-bit SW mode but the OGL implementation apparently needs it, no idea why)
                build_shading_tables_32(aggregate_clut, shading_remapping_table, collection->get_shading_table_32(clut_index));
            }
            
            build_collection_tinting_tables(collection, aggregate_clut);
        }
    }
    
    // copy found colors to the global 8-bit color table and rebuild the shading tables
    gameworld_color_table_8.color_count = PIXEL8_MAXIMUM_COLORS;
    
    assert_fail_f(aggregate_clut.size() <= PIXEL8_MAXIMUM_COLORS, "Too many found colors for 8-bit table: %zu", aggregate_clut.size());
    
    short color_index = 0;
    for (; color_index < std::min((int32_t)aggregate_clut.size(), PIXEL8_MAXIMUM_COLORS); color_index++)
    {
        gameworld_color_table_8.colors[color_index] = aggregate_clut[color_index].value;
    }
    // fill unused entries with black
    for (; color_index < PIXEL8_MAXIMUM_COLORS; color_index++)
    {
        gameworld_color_table_8.colors[color_index] = {0, 0, 0};
    }
    
    build_global_shading_table_8();
    build_global_shading_table_16();
    build_global_shading_table_32();
}



//-----------------------------------------------------------------------------
//

ShapesCollection* get_shapes_collection(short collection_index)
{
    // This one is intended to bomb because collection indices can only be from 1 to 31,
    // short of drastic changes in how collection indices are specified (a bigger structure
    // than shape_descriptor, for example). // EES: except that it didn't always bomb, you twit, because asserts are DEBUG-only
    return &shapes_collections_8.at(collection_index); // NOW it bombs. Progress!
}


size_t number_of_shapes_collections()
{
    return shapes_collections_8.size();
}


shapes_animation_t* get_shapes_animation(shape_descriptor shape)
{
    ShapesCollection* collection = get_shapes_collection(GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(shape)));
    shapes_animation_t* high_level_shape = collection->get_animation(GET_DESCRIPTOR_SHAPE(shape));
    return high_level_shape ? (shapes_animation_t*)&high_level_shape->number_of_views : nullptr;
}


shapes_frame_t* get_shapes_frame(short collection_code, short low_level_shape_index)
{
    short collection_index = GET_COLLECTION_INDEX(collection_code);
    if (collection_index < 0 || collection_index >= NUMBER_OF_COLLECTIONS || low_level_shape_index < 0) return nullptr;
    return get_shapes_collection(collection_index)->get_frame(low_level_shape_index);
}


void* get_global_shading_table()
{
    switch (main_screen.bit_depth())
    {
        case 8:
            return global_shading_table_8;
        case 16:
            return global_shading_table_16;
        case 32:
            return global_shading_table_32;
        default:
            throw_bug_report("Unsupported bit depth.");
    }
}


void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition_t** bitmap, void** shading_tables, short shading_mode)
{
    //    if (collection_code==_collection_marathon_control_panels) collection_code= 30, low_level_shape_index= 0;
    short collection_index = GET_COLLECTION_INDEX(collection_code);
    short clut_index = GET_COLLECTION_CLUT(collection_code);
    
    // Forget about it if some one managed to call us with the NONE value
    assert_fail(!(clut_index+1 == MAXIMUM_CLUTS_PER_COLLECTION && collection_index+1 == MAXIMUM_COLLECTIONS && low_level_shape_index+1 == MAXIMUM_SHAPES_PER_COLLECTION), "");
    
    ShapesCollection* collection = get_shapes_collection(collection_index);
    
    shapes_frame_t* low_level_shape = collection->get_frame(low_level_shape_index);
    // Return NULL pointers for bitmap and shading table if the frame does not exist
    if (low_level_shape)
    {
        if (bitmap) { *bitmap = collection->get_bitmap_definition(low_level_shape->bitmap_index); }
        
        if (shading_tables)
        {
            switch (shading_mode)
            {
                case _shading_normal:
                    *shading_tables = collection->get_shading_table(clut_index, main_screen.bit_depth()); // TODO: probably not the right method
                    break;
                case _shading_infravision:
                    *shading_tables = collection->get_tint_table(0, main_screen.bit_depth()); // TODO: ditto
                    break;
                default:
                    throw_bug_report_f("Bad shading_mode: %d", shading_mode);
            }
        }
    }
    else
    {
        *bitmap = nullptr;
        if (shading_tables) { *shading_tables = nullptr; }
    }
}


void convert_ogl_color_to_infravision(short collection_index, GLfloat* color)
{
    ao_rgb tint = get_shapes_collection(collection_index)->infravision_tint;
    
    GLfloat average = (color[0] + color[1] + color[2]) / 3;
    color[0] = tint.r * average / 65535;
    color[1] = tint.g * average / 65535;
    color[2] = tint.b * average / 65535;
}

