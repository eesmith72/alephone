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

// TODO: splitting this file would be really, really helpful to understanding it

#include "shapes.h"

#include "shell.h"
#include "render.h"
#include "interface.hpp"
#include "Screen.hpp"
#include "DataFile.hpp"
#include "images.h"

#include "map.h"

#include "graphics_preferences.hpp"

#include "OGL_Render.h"

// LP addition: infravision XML setup needs colors
#include "InfoTree.h"

#include "Packing.h"

#include "Plugins.h"



color_table_t shapes_8_color_table; // this is the gameworld's indexed color table constructed from Shapes collections; used in Classic 8


/* ---------- constants */

#define iWHITE 1
#define iBLACK 18

/* each collection has a tint table which (fully) tints the clut of that collection to whatever it
	looks like through the light enhancement goggles */
#define NUMBER_OF_TINT_TABLES 1

// Moved from shapes_macintosh.c:

// Possibly of historical interest:
// #define COLLECTIONS_RESOURCE_BASE 128
// #define COLLECTIONS_RESOURCE_BASE16 1128

enum /* collection status */
{
	markNONE,
	markLOAD= 1,
	markUNLOAD= 2,
	//markSTRIP = 4, // we don’t want bitmaps, just high/low-level shape data // EES: the only collection that ever got stripped was Player sprites in solo if the chase cam was disabled, and we've yeeted that fussiness already so this can go
	markPATCHED = 8, // force re-load
};


static std::array<collection_header, MAXIMUM_COLLECTIONS> collection_headers;


static SDL_PixelFormat pixel_format_16, pixel_format_32;


// TODO: use array/vector
static pixel16* global_shading_table16 = nullptr;
static pixel32* global_shading_table32 = nullptr;

// TODO: these are constant for 8, 16, 32
short number_of_shading_tables, shading_table_fractional_bits, shading_table_size;


static DataFile ShapesFile_M2;

ResourceFile ShapesFile_M1;


static bool is_shapes_file_m1;

bool shapes_file_is_m1() { return is_shapes_file_m1; }

/* ---------- private prototypes */

static void update_color_environment();
static short find_or_add_color(struct shapes_color_t *color, struct shapes_color_t *colors, short *color_count, bool update_flags); // update_flags is false in `shading_remapping_table[INDEX]=...`

static void build_shading_tables8(struct shapes_color_t *colors, short color_count, pixel8 *shading_tables);
static void build_shading_tables16(struct shapes_color_t *colors, short color_count, pixel16 *shading_tables, byte *remapping_table);
static void build_shading_tables32(struct shapes_color_t *colors, short color_count, pixel32 *shading_tables, byte *remapping_table);
static void build_global_shading_table16(void);
static void build_global_shading_table32(void);

static bool get_next_color_run(struct shapes_color_t *colors, short color_count, short *start, short *count);
static bool new_color_run(struct shapes_color_t *_new, struct shapes_color_t *last);

static int32 get_shading_table_size(short collection_code);

static void build_collection_tinting_table(struct shapes_color_t *colors, short color_count, short collection_index);
static void build_tinting_table8(struct shapes_color_t *colors, short color_count, pixel8 *tint_table, short tint_start, short tint_count);
static void build_tinting_table16(struct shapes_color_t *colors, short color_count, pixel16 *tint_table, struct ao_rgb *tint_color);
static void build_tinting_table32(struct shapes_color_t *colors, short color_count, pixel32 *tint_table, struct ao_rgb *tint_color);

static void precalculate_bit_depth_constants(void);

static bool collection_loaded(struct collection_header *header);
static void unload_collection(struct collection_header *header);
static bool load_collection(short collection_index);

static void shutdown_shape_handler(void);
static void close_shapes_file(void);

// static byte *make_stripped_collection(byte *collection);

/* --------- collection accessor prototypes */

// Modified to return NULL for unloaded collections and out-of-range indices for collection contents.
// This is to allow for more graceful degradation.

static struct collection_header *get_collection_header(short collection_index);
/*static*/ struct ShapesCollection *get_shapes_collection(short collection_index);
static void *get_collection_shading_tables(short collection_index, short clut_index);
static void *get_collection_tint_tables(short collection_index, short tint_index);


//-----------------------------------------------------------------------------
// Convert shape to surface // EES: should be useful when converting M2 Shapes file to sprite sheets
 

// ZZZ extension: pass out (if non-NULL) a pointer to a block of pixel data -
// caller should free() that storage after freeing the returned surface.
// Only needed for RLE-encoded shapes.
// Note that default arguments are used to make this function
// source-code compatible with existing usage.
// Note also that inShrinkImage currently only applies to RLE shapes.
SDL_Surface* get_shape_surface(int shape, int inCollection, byte** outPointerToPixelData, float inIllumination, bool inShrinkImage)
{
	// Get shape information
	int collection_index = GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(shape));
	int clut_index = GET_COLLECTION_CLUT(GET_DESCRIPTOR_COLLECTION(shape));
	int low_level_shape_index = GET_DESCRIPTOR_SHAPE(shape);
    
    if (inCollection != NONE)
    {
        collection_index = GET_COLLECTION_INDEX(inCollection);
        clut_index = GET_COLLECTION_CLUT(inCollection);
        low_level_shape_index = shape;
    }

	ShapesCollection *collection = get_shapes_collection(collection_index);
	
    low_level_shape_definition* low_level_shape = collection->get_frame(low_level_shape_index);
	if (!low_level_shape) return NULL;
    
	bitmap_definition_t* bitmap;
    
    SDL_Color colors[256];

    if (inIllumination >= 0)
    {
        assert_fail(inIllumination <= 1.0f, "");
    
        // ZZZ: get shading tables to use instead of CLUT, if requested
        void* shading_tables_as_void;
        extended_get_shape_bitmap_and_shading_table(BUILD_COLLECTION(collection_index, clut_index), low_level_shape_index,
                                                    &bitmap, &shading_tables_as_void, _shading_normal);
        if (!bitmap) return NULL;
        
        switch (main_screen.bit_depth())
        {
            case 16:
            {
                uint16*	shading_tables = (uint16*)shading_tables_as_void;
                shading_tables += 256 * (int)(inIllumination * (number_of_shading_tables - 1));
                
                // Extract color table - ZZZ change to use shading table rather than CLUT.  Hope it works.
                for (int i = 0; i < 256; i++)
                {
                    SDL_GetRGB(shading_tables[i], &pixel_format_16, &colors[i].r, &colors[i].g, &colors[i].b);
                    colors[i].a = 0xff;
                }
                break;
            }
            
            case 32:
            {
                uint32*	shading_tables	= (uint32*) shading_tables_as_void;
                shading_tables += 256 * (int)(inIllumination * (number_of_shading_tables - 1));
                
                // Extract color table - ZZZ change to use shading table rather than CLUT.  Hope it works.
                for(int i = 0; i < 256; i++) {
                    colors[i].r = RED32(shading_tables[i]);
                    colors[i].g = GREEN32(shading_tables[i]);
                    colors[i].b = BLUE32(shading_tables[i]);
                    colors[i].a = 0xff;
                }
                break;
            }
            
            default:
                throw_bug_report_f("Unsupported color bit depth %i for get_shape_surface with illumination", main_screen.bit_depth());
            break;
        }

    }
    else
    {
        bitmap = collection->get_bitmap_definition(low_level_shape->bitmap_index);
        if (!bitmap) return NULL;
        
        // Extract color table
        int num_colors = collection->color_count - NUMBER_OF_PRIVATE_COLORS;
        shapes_color_t* src_colors = collection->get_clut(clut_index) + NUMBER_OF_PRIVATE_COLORS;
        for (int i = 0; i < num_colors && src_colors; i++)
        {
            colors[src_colors[i].index] = SDL_Color(src_colors[i].color);
        }
    } // inIllumination < 0
    
        
	SDL_Surface *surface = NULL;
	if (bitmap->bytes_per_row == NONE)
    {
        
        // ZZZ: process RLE-encoded shape
        
        // Allocate storage for un-RLE'd pixels
        uint32	theNumberOfStorageBytes = bitmap->width * bitmap->height * sizeof(byte);
        byte*	pixel_storage = (byte*) malloc(theNumberOfStorageBytes);
        memset(pixel_storage, 0, theNumberOfStorageBytes);
        
        // Here, a "run" is a row or column.  An "element" is a single pixel's data.
        // We always go forward through the source data.  Thus, the offsets for where the next run
        // or element goes into the destination data area change depending on the circumstances.
        int16	theNumRuns;
        int16	theDestDataNextRunOffset;
        int16	theDestDataNextElementOffset;
        
        // Is this row-major or column-major?
        if(bitmap->flags & _COLUMN_ORDER_BIT) {
            theNumRuns				= bitmap->width;
            theDestDataNextRunOffset		= (low_level_shape->flags & _X_MIRRORED_BIT) ? -1 : 1;
            theDestDataNextElementOffset	= (low_level_shape->flags & _Y_MIRRORED_BIT) ? -bitmap->width : bitmap->width;
        }
        else {
            theNumRuns				= bitmap->height;
            theDestDataNextElementOffset	= (low_level_shape->flags & _X_MIRRORED_BIT) ? -1 : 1;
            theDestDataNextRunOffset		= (low_level_shape->flags & _Y_MIRRORED_BIT) ? -bitmap->width : bitmap->width;
        }
        
        // Figure out where our first byte will be written
        byte* theDestDataStartAddress = pixel_storage;
        
        if(low_level_shape->flags & _X_MIRRORED_BIT)
            theDestDataStartAddress += bitmap->width - 1;
        
        if(low_level_shape->flags & _Y_MIRRORED_BIT)
            theDestDataStartAddress += bitmap->width * (bitmap->height - 1);
        
        // Walk through runs, un-RLE'ing as we go
        for(int run = 0; run < theNumRuns; run++) {
            uint16*	theLengthData 					= (uint16*) bitmap->row_addresses[run];
            uint16	theFirstOpaquePixelElement 			= SDL_SwapBE16(theLengthData[0]);
            uint16	theFirstTransparentAfterOpaquePixelElement	= SDL_SwapBE16(theLengthData[1]);
            uint16	theNumberOfOpaquePixels = theFirstTransparentAfterOpaquePixelElement - theFirstOpaquePixelElement;
            
            byte*	theOriginalPixelData = (byte*) &theLengthData[2];
            byte*	theUnpackedPixelData;
            
            theUnpackedPixelData = theDestDataStartAddress + run * theDestDataNextRunOffset
            + theFirstOpaquePixelElement * theDestDataNextElementOffset;
            
            for(int i = 0; i < theNumberOfOpaquePixels; i++) {
                assert_fail(theUnpackedPixelData >= pixel_storage, "");
                assert_fail(theUnpackedPixelData < (pixel_storage + theNumberOfStorageBytes), "");
                *theUnpackedPixelData = *theOriginalPixelData;
                theUnpackedPixelData += theDestDataNextElementOffset;
                theOriginalPixelData++;
            }
        }
        
        // Let's shrink the image if the user wants us to.
        // We do this here by discarding every other pixel in each direction.
        // Really, I guess there's probably a library out there that would do nice smoothing
        // for us etc. that we should use here.  I just want to hack something out now and run with it.
        int image_width		= bitmap->width;
        int image_height	= bitmap->height;
        
        if(inShrinkImage) {
            int		theLargerWidth		= bitmap->width;
            int		theLargerHeight		= bitmap->height;
            byte*	theLargerPixelStorage	= pixel_storage;
            int		theSmallerWidth		= theLargerWidth / 2 + theLargerWidth % 2;
            int		theSmallerHeight	= theLargerHeight / 2 + theLargerHeight % 2;
            byte*	theSmallerPixelStorage	= (byte*) malloc(theSmallerWidth * theSmallerHeight);
            
            for(int y = 0; y < theSmallerHeight; y++) {
                for(int x = 0; x < theSmallerWidth; x++) {
                    theSmallerPixelStorage[y * theSmallerWidth + x] =
                    theLargerPixelStorage[(y * theLargerWidth + x) * 2];
                }
            }
            
            free(pixel_storage);
            
            pixel_storage	= theSmallerPixelStorage;
            image_width		= theSmallerWidth;
            image_height	= theSmallerHeight;
        }
        
        // Now we can create a surface from this new storage
        surface = SDL_CreateRGBSurfaceFrom(pixel_storage, image_width, image_height, 8, image_width, 0, 0, 0, 0);
        
        if(surface != NULL) {
            // If caller is not prepared to take this data, it's a coding error.
            assert_fail(outPointerToPixelData != NULL, "");
            *outPointerToPixelData = pixel_storage;
            
            // Set color table
            SDL_SetPaletteColors(surface->format->palette, colors, 0, 256);
            
            // Set transparent pixel (color #0)
            SDL_SetColorKey(surface, SDL_TRUE, 0);
        }
        
    }
    else
    {
        // Row-order shape, we can directly create a surface from it
        if (collection->type == _wall_collection)
        {
            surface = SDL_CreateRGBSurfaceFrom(bitmap->row_addresses[0], bitmap->height, bitmap->width, 8, bitmap->bytes_per_row, 0, 0, 0, 0);
        }
        else
        {
            surface = SDL_CreateRGBSurfaceFrom(bitmap->row_addresses[0], bitmap->width, bitmap->height, 8, bitmap->bytes_per_row, 0, 0, 0, 0);
        }
        // ZZZ: caller should not dispose of any additional data - just free the surface.
        if (outPointerToPixelData) { *outPointerToPixelData = nullptr; }

        if (surface) { SDL_SetPaletteColors(surface->format->palette, colors, 0, 256); } // Set color table
	}

	return surface;
}


static void load_ShapesCollection(ShapesCollection* cd, SDL_RWops* p)
{
	cd->version = SDL_ReadBE16(p);
	cd->type = SDL_ReadBE16(p);
    SDL_ReadBE16(p); // skip `flags` (unused)
	cd->color_count = SDL_ReadBE16(p);
	cd->clut_count = SDL_ReadBE16(p);
	cd->color_table_offset = SDL_ReadBE32(p);
	cd->high_level_shape_count = SDL_ReadBE16(p);
	cd->high_level_shape_offset_table_offset = SDL_ReadBE32(p);
	cd->low_level_shape_count = SDL_ReadBE16(p);
	cd->low_level_shape_offset_table_offset = SDL_ReadBE32(p);
	cd->bitmap_count = SDL_ReadBE16(p);
	cd->bitmap_offset_table_offset = SDL_ReadBE32(p);
	cd->pixels_to_world = SDL_ReadBE16(p);
	SDL_ReadBE32(p); // skip size
	SDL_RWseek(p, 253 * sizeof(int16), SEEK_CUR); // unused

	// resize members
	cd->color_tables.resize(cd->clut_count * cd->color_count);
	cd->high_level_shapes.resize(cd->high_level_shape_count);
	cd->low_level_shapes.resize(cd->low_level_shape_count);
	cd->bitmaps.resize(cd->bitmap_count);

}


static void load_clut(shapes_color_t* r, int count, SDL_RWops* p)
{
	for (int i = 0; i < count; i++, r++) 
	{
        r->luminescent = SDL_ReadU8(p);
        r->index = SDL_ReadU8(p);
		r->color.r = SDL_ReadBE16(p);
		r->color.g = SDL_ReadBE16(p);
		r->color.b = SDL_ReadBE16(p);
	}
}


static void load_high_level_shape(std::vector<uint8>& shape, SDL_RWops* p)
{
	int16 type = SDL_ReadBE16(p);
	int16 flags = SDL_ReadBE16(p);
	char name[HIGH_LEVEL_SHAPE_NAME_LENGTH + 2];
	SDL_RWread(p, name, 1, HIGH_LEVEL_SHAPE_NAME_LENGTH + 2);
	int16 number_of_views = SDL_ReadBE16(p);
	int16 frames_per_view = SDL_ReadBE16(p);

	// Convert low-level shape index list
	int num_views;
	switch (number_of_views) {
	case _unanimated:
	case _animated1:
		num_views = 1;
		break;
	case _animated3to4:
	case _animated4:
		num_views = 4;
		break;
	case _animated3to5:
	case _animated5:
		num_views = 5;
		break;
	case _animated2to8:
	case _animated5to8:
	case _animated8:
		num_views = 8;
		break;
	default:
		num_views = number_of_views;
		break;
	}

	shape.resize(sizeof(high_level_shape_definition) + num_views * frames_per_view * sizeof(int16));
		
	high_level_shape_definition *d = (high_level_shape_definition *) &shape[0];
		
	d->type = type;
	d->flags = flags;
	memcpy(d->name, name, HIGH_LEVEL_SHAPE_NAME_LENGTH + 2);
	d->number_of_views = number_of_views;
	d->frames_per_view = frames_per_view;
	d->ticks_per_frame = SDL_ReadBE16(p);
	d->key_frame = SDL_ReadBE16(p);
	d->transfer_mode = SDL_ReadBE16(p);
	d->transfer_mode_period = SDL_ReadBE16(p);
	d->first_frame_sound = SDL_ReadBE16(p);
	d->key_frame_sound = SDL_ReadBE16(p);
	d->last_frame_sound = SDL_ReadBE16(p);
	d->pixels_to_world = SDL_ReadBE16(p);
	d->loop_frame = SDL_ReadBE16(p);
	SDL_RWseek(p, 14 * sizeof(int16), SEEK_CUR);

	// Convert low-level shape index list
	for (int j = 0; j < num_views * d->frames_per_view; j++) {
		d->low_level_shape_indexes[j] = SDL_ReadBE16(p);
	}
}


static void load_low_level_shape(low_level_shape_definition* d, SDL_RWops* p)
{
	d->flags = SDL_ReadBE16(p);
	d->minimum_light_intensity = SDL_ReadBE32(p);
	d->bitmap_index = SDL_ReadBE16(p);
	d->origin_x = SDL_ReadBE16(p);
	d->origin_y = SDL_ReadBE16(p);
	d->key_x = SDL_ReadBE16(p);
	d->key_y = SDL_ReadBE16(p);
	d->world_left = SDL_ReadBE16(p);
	d->world_right = SDL_ReadBE16(p);
	d->world_top = SDL_ReadBE16(p);
	d->world_bottom = SDL_ReadBE16(p);
	d->world_x0 = SDL_ReadBE16(p);
	d->world_y0 = SDL_ReadBE16(p);
	SDL_RWseek(p, 4 * sizeof(int16), SEEK_CUR);
}


static void convert_m1_rle(std::vector<uint8>& bitmap, int scanlines, int scanline_length, SDL_RWops* p)
{
//	std::vector<uint8> bitmap;
	for (int scanline = 0; scanline < scanlines; ++scanline)
	{
		std::vector<uint8> scanline_data(scanline_length + 1);
		uint8* dst = &scanline_data[0];
		uint8* sentry = &scanline_data[scanline_length];

		while (true)
		{
			int16 opcode = SDL_ReadBE16(p);
			if (opcode > 0)
			{
				assert_fail(dst + opcode <= sentry, "");
				SDL_RWread(p, dst, opcode, 1);
				dst += opcode;
			}
			else if (opcode < 0)
			{
				assert_fail(dst - opcode <= sentry, "");
				dst -= opcode;
			}
			else
				break;
		}

		assert (dst == sentry);

		// Find M2/oo-format RLE compression;
		// it needs the first nonblank pixel and the last nonblank one + 1
		int16 first = 0;
		int16 last = 0;
		for (int i = 0; i < scanline_length; ++i)
		{
			if (scanline_data[i] != 0)
			{
				first = i;
				break;
			}
		}

		for (int i = scanline_length - 1; i >= 0; --i)
		{
			if (scanline_data[i] != 0)
			{
				last = i + 1;
				break;
			}
		}

		if (last < first) last = first;

		bitmap.push_back(first >> 8);
		bitmap.push_back(first & 0xff);
		bitmap.push_back(last >> 8);
		bitmap.push_back(last & 0xff);
		bitmap.insert(bitmap.end(), &scanline_data[first], &scanline_data[last]);
	}
}


static void load_bitmap(std::vector<uint8>& bitmap, SDL_RWops* p, bool is_m1)
{
	bitmap_definition_t b;

	// Convert bitmap definition
	b.width = SDL_ReadBE16(p);
	b.height = SDL_ReadBE16(p);
	b.bytes_per_row = SDL_ReadBE16(p);
	b.flags = SDL_ReadBE16(p);
	b.bit_depth = SDL_ReadBE16(p);

	// guess how big to make it
	int rows = (b.flags & _COLUMN_ORDER_BIT) ? b.width : b.height;
	int row_len = (b.flags & _COLUMN_ORDER_BIT) ? b.height : b.width;
		
	SDL_RWseek(p, 16, SEEK_CUR);
		
	// Skip row address pointers
	SDL_RWseek(p, (rows + 1) * sizeof(uint32), SEEK_CUR);

	if (b.bytes_per_row == NONE) 
	{
        if (is_m1)
		{
			// make enough room for the definition, then append as we convert RLE
			bitmap.resize(sizeof(bitmap_definition_t) + rows * sizeof(pixel8*));
		}
		else
		{
			// ugly--figure out how big it's going to be
			
			int32 size = 0;
			for (int j = 0; j < rows; j++) {
				int16 first = SDL_ReadBE16(p);
				int16 last = SDL_ReadBE16(p);
				size += 4;
				SDL_RWseek(p, last - first, SEEK_CUR);
				size += last - first;
			}
			
			bitmap.resize(sizeof(bitmap_definition_t) + rows * sizeof(pixel8*) + size);
			
			// Now, seek back
			SDL_RWseek(p, -size, SEEK_CUR);
		}
	} 
	else
	{
		bitmap.resize(sizeof(bitmap_definition_t) + rows * sizeof(pixel8*) + rows * b.bytes_per_row);
	}


	uint8* c = &bitmap[0];
	bitmap_definition_t *d = (bitmap_definition_t *) &bitmap[0];
	d->width = b.width;
	d->height = b.height;
	d->bytes_per_row = b.bytes_per_row;
	d->flags = b.flags;
	d->flags &= ~_PATCHED_BIT; // Anvil sets unused flags :( we'll set it later
	d->bit_depth = b.bit_depth;
	c += sizeof(bitmap_definition_t);

	// Skip row address pointers
	c += rows * sizeof(pixel8 *);

	// Copy bitmap data
	if (d->bytes_per_row == NONE) 
	{
		// RLE format

        if (is_m1)
		{
			convert_m1_rle(bitmap, rows, row_len, p);
		}
		else
		{
			for (int j = 0; j < rows; j++) {
				int16 first = SDL_ReadBE16(p);
				int16 last = SDL_ReadBE16(p);
				*(c++) = (uint8)(first >> 8);
				*(c++) = (uint8)(first);
				*(c++) = (uint8)(last >> 8);
				*(c++) = (uint8)(last);
				SDL_RWread(p, c, 1, last - first);
				c += last - first;
			}
		}
	} else {
		SDL_RWread(p, c, d->bytes_per_row, rows);
		c += rows * d->bytes_per_row;
	}

}


static void allocate_shading_tables(short collection_index)
{
	collection_header *header = get_collection_header(collection_index);
	// Allocate enough space for this collection's shading tables
    ShapesCollection *definition = get_shapes_collection(collection_index);
    header->shading_tables.resize(get_shading_table_size(collection_index) * definition->clut_count + shading_table_size * NUMBER_OF_TINT_TABLES);
}


static bool load_collection(short collection_index)
{
	SDL_RWops* p;
	std::shared_ptr<SDL_RWops> m1_p; // automatic deallocation
	LoadedResource r;
	int32 src_offset;

	collection_header *header = get_collection_header(collection_index);
	
	if (is_shapes_file_m1)
	{
		// Collections are stored in .256 resources
		if (!ShapesFile_M1.Get('.', '2', '5', '6', 128 + collection_index, r))
		{
			return false;
		}

		m1_p.reset(SDL_RWFromConstMem(r.GetPointer(), r.get_length()), SDL_FreeRW);
		p = m1_p.get();
		src_offset = 0;
	}
	else
	{
		// Get offset and length of data in source file from header
		
		if (main_screen.bit_depth() == 8 || header->offset16 == -1) {
			if (header->offset == -1)
			{
				return false;
			}
			src_offset = header->offset;
		} else {
			src_offset = header->offset16;
		}

		p = ShapesFile_M2.borrow_rwops();
		ShapesFile_M2.set_position(0);
		src_offset += SDL_RWtell(p);
	}

	// Read collection definition
	std::unique_ptr<ShapesCollection> cd(new ShapesCollection);
	SDL_RWseek(p, src_offset, RW_SEEK_SET);
	load_ShapesCollection(cd.get(), p);
	header->status &= ~markPATCHED;

	// Convert CLUTS
	if (cd->clut_count && cd->color_count) {
		SDL_RWseek(p, src_offset + cd->color_table_offset, RW_SEEK_SET);
		load_clut(&cd->color_tables[0], cd->clut_count * cd->color_count, p);
	}

	// Convert high-level shape definitions
	if (cd->high_level_shape_count) {
		SDL_RWseek(p, src_offset + cd->high_level_shape_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(cd->high_level_shape_count);
		SDL_RWread(p, &t[0], sizeof(uint32), cd->high_level_shape_count);
        swap_array_BE32(&t[0], cd->high_level_shape_count);

		for (int i = 0; i < cd->high_level_shape_count; i++) {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			load_high_level_shape(cd->high_level_shapes[i], p);
		}
	}

	// Convert low-level shape definitions
	if (cd->low_level_shape_count) {
		SDL_RWseek(p, src_offset + cd->low_level_shape_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(cd->low_level_shape_count);
		SDL_RWread(p, &t[0], sizeof(uint32), cd->low_level_shape_count);
        swap_array_BE32(&t[0], cd->low_level_shape_count);

		for (int i = 0; i < cd->low_level_shape_count; i++) {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			load_low_level_shape(&cd->low_level_shapes[i], p);
		}
	}

	// Convert bitmap definitions
	if (cd->bitmap_count) {
		SDL_RWseek(p, src_offset + cd->bitmap_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(cd->bitmap_count);
		SDL_RWread(p, &t[0], sizeof(uint32), cd->bitmap_count);
        swap_array_BE32(&t[0], cd->bitmap_count);

		for (int i = 0; i < cd->bitmap_count; i++) {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			load_bitmap(cd->bitmaps[i], p, is_shapes_file_m1);
		}
	}

	header->collection = cd.release();
	
	allocate_shading_tables(collection_index);
	
	if (header->shading_tables.empty())
    {
		delete header->collection;
		header->collection = NULL;
		return false;
	}
	return true;
}	
			


static void unload_collection(collection_header* header)
{
	assert_fail(header->collection, "");
	delete header->collection;
	header->shading_tables.clear();
	header->collection = NULL;
}


//-----------------------------------------------------------------------------
// shapes patch


#define ENDC_TAG FOUR_CHARS_TO_INT('e', 'n', 'd', 'c')
#define CLDF_TAG FOUR_CHARS_TO_INT('c', 'l', 'd', 'f')
#define HLSH_TAG FOUR_CHARS_TO_INT('h', 'l', 's', 'h')
#define LLSH_TAG FOUR_CHARS_TO_INT('l', 'l', 's', 'h')
#define BMAP_TAG FOUR_CHARS_TO_INT('b', 'm', 'a', 'p')
#define CTAB_TAG FOUR_CHARS_TO_INT('c', 't', 'a', 'b')


std::vector<uint8> shapes_patch;


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
	return length ? &shapes_patch[0] : 0;
}


void load_shapes_patch(SDL_RWops *p, bool override_replacements)
{
	std::vector<int16> color_counts(MAXIMUM_COLLECTIONS);
	int32 start = SDL_RWtell(p);
	SDL_RWseek(p, 0, SEEK_END);
	int32 end = SDL_RWtell(p);

	SDL_RWseek(p, start, SEEK_SET);
	
	bool done = false;
	while (!done)
	{
		// is there more data to read?
		if (SDL_RWtell(p) < end)
		{
			int32 collection_index = SDL_ReadBE32(p);
			int32 patch_bit_depth = SDL_ReadBE32(p);

			bool collection_end = false;
			while (!collection_end)
			{
				// read a tag
				int32 tag = SDL_ReadBE32(p);
				if (tag == ENDC_TAG) 
				{
					collection_end = true;
				}
				else if (tag == CLDF_TAG)
				{
					// a collection follows directly
					collection_header *header = get_collection_header(collection_index);
					if (collection_loaded(header) && patch_bit_depth == 8)
					{
						load_ShapesCollection(header->collection, p);
						color_counts[collection_index] = header->collection->color_count;
						allocate_shading_tables(collection_index);
						header->status|=markPATCHED;

					} else {
						// get the color count (it's the only way to skip the CTAB_TAG
						SDL_RWseek(p, 6, SEEK_CUR);
						color_counts[collection_index] = SDL_ReadBE16(p);
						SDL_RWseek(p, 544 - 8, SEEK_CUR);
					}
				} 
				else if (tag == HLSH_TAG)
				{
					ShapesCollection *cd = get_shapes_collection(collection_index);
					int32 high_level_shape_index = SDL_ReadBE32(p);
					int32 size = SDL_ReadBE32(p);
					int32 pos = SDL_RWtell(p);
					if (cd && patch_bit_depth == 8 && high_level_shape_index < cd->high_level_shapes.size())
					{
						load_high_level_shape(cd->high_level_shapes[high_level_shape_index], p);
						SDL_RWseek(p, pos + size, SEEK_SET);
						
					}
					else
					{
						SDL_RWseek(p, size, SEEK_CUR);
					}
				}
				else if (tag == LLSH_TAG)
				{
					ShapesCollection *cd = get_shapes_collection(collection_index);
					int32 low_level_shape_index = SDL_ReadBE32(p);
					if (cd && patch_bit_depth == 8 && low_level_shape_index < cd->low_level_shapes.size())
					{
						load_low_level_shape(&cd->low_level_shapes[low_level_shape_index], p);
					}
					else
					{
						SDL_RWseek(p, 36, SEEK_CUR);
					}
				} 
				else if (tag == BMAP_TAG)
				{
					ShapesCollection *cd = get_shapes_collection(collection_index);
					int32 bitmap_index = SDL_ReadBE32(p);
					int32 size = SDL_ReadBE32(p);
					if (cd && patch_bit_depth == 8 && bitmap_index < cd->bitmaps.size())
					{
						load_bitmap(cd->bitmaps[bitmap_index], p, false);
						if (override_replacements)
						{
                            cd->get_bitmap_definition(bitmap_index)->flags |= _PATCHED_BIT; // TODO: question: why do we care?
						}
					}
					else
					{
						SDL_RWseek(p, size, SEEK_CUR);
					}
				}
				else if (tag == CTAB_TAG)
				{
					ShapesCollection *cd = get_shapes_collection(collection_index);
					int32 color_table_index = SDL_ReadBE32(p);
					if (cd && patch_bit_depth == 8 && (color_table_index * cd->color_count < cd->color_tables.size())) 
					{
						load_clut(&cd->color_tables[color_table_index], cd->color_count, p);
					}
					else
					{
						SDL_RWseek(p, color_counts[collection_index] * sizeof(shapes_color_t), SEEK_CUR);
					}
				}
				else
				{
					fprintf(stderr, "Unrecognized tag in patch file '%c%c%c%c'\n %x", tag >> 24, tag >> 16, tag >> 8, tag, tag);
				}
			}
					

		} else {
			done = true;
		}
	}

	
}


//-----------------------------------------------------------------------------


void initialize_shapes()
{
    assert_fail(NUMBER_OF_COLLECTIONS <= MAXIMUM_COLLECTIONS, "");

	// M1 uses the resource fork, but M2 and Moo use the data fork

    ao_path File = get_scenario_shapes_path();

    open_shapes_file(File);
    
    if (!ShapesFile_M2.is_open() && !ShapesFile_M1.IsOpen())
    {
        exit(badExtraFileLocations);
    }
	else
		atexit(shutdown_shape_handler);
    
    SDL_PixelFormat *pf = SDL_AllocFormat(AO_PIXEL_FORMAT_16);
    pixel_format_16 = *pf; // only used by in shapes.cpp
    SDL_FreeFormat(pf);
    pf = SDL_AllocFormat(AO_PIXEL_FORMAT_32);
    pixel_format_32 = *pf;
    SDL_FreeFormat(pf);
}



void open_shapes_file(const ao_path& File)
{
    is_shapes_file_m1 = false;
	if (ShapesFile_M1.open(File) == no_err && ShapesFile_M1.Check('.','2','5','6',128))
	{
		is_shapes_file_m1 = true;
	}
	else
	{
		ShapesFile_M1.Close();
        
        if (ShapesFile_M2.open(File) == no_err) // TODO: and what if it fails? error handling is awful
        {
            // Load the collection headers;
            // need a buffer for the packed data
            int Size = MAXIMUM_COLLECTIONS*SIZEOF_collection_header;
            byte *CollHdrStream = new byte[Size];
            ShapesFile_M2.read(Size,CollHdrStream);
            //if (!ShapesFile_M2.read(Size,CollHdrStream))
            //{
            //	ShapesFile_M2.close();
            //	delete []CollHdrStream;
            //	return;
            //}
            
            // Unpack them
            uint8 *S = CollHdrStream;
            int Count = MAXIMUM_COLLECTIONS;
            
            for (int k = 0; k < Count; k++)
            {
                collection_header* ObjPtr = &collection_headers[k];
                StreamToValue(S,ObjPtr->status);
                S += 2; // StreamToValue(S,ObjPtr->flags);
                
                StreamToValue(S,ObjPtr->offset);
                StreamToValue(S,ObjPtr->length);
                StreamToValue(S,ObjPtr->offset16);
                StreamToValue(S,ObjPtr->length16);
                
                S += 6*2;
                
                ObjPtr->collection = NULL;	// so unloading can work properly
                ObjPtr->shading_tables.clear();	// so unloading can work properly
            }
            
            assert_fail((S - CollHdrStream) == Count*SIZEOF_collection_header, "");
            
            delete []CollHdrStream;

        }
	}
	open_shapes_file_resources(File);
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


static void shutdown_shape_handler()
{
	close_shapes_file();
}


//-----------------------------------------------------------------------------


static bool collection_loaded(collection_header* header)
{
	return header->collection ? true : false;
}


bool collection_loaded(short collection_index)
{
	collection_header *header = get_collection_header(collection_index);
	return collection_loaded(header);
}


bool can_load_collection(short collection_index)
{
	if (collection_index >= 0 && collection_index < NUMBER_OF_COLLECTIONS)
	{
		collection_header* header = get_collection_header(collection_index);
		if (header) {
			return (header->offset != -1 || header->offset16 != -1);
		}
	}

	return false;
}


void unload_all_collections()
{
	for (short i = 0; i < MAXIMUM_COLLECTIONS; i++)
	{
        collection_header* header = &collection_headers[i];
        if (header->collection) { unload_collection(header); }
		OGL_UnloadModelsImages(i);
	}
}


void mark_collection(short collection_code, bool loading)
{
	if (collection_code!=NONE)
	{
		short collection_index= GET_COLLECTION_INDEX(collection_code);
	
		assert_fail(collection_index>=0&&collection_index<MAXIMUM_COLLECTIONS, "");
		collection_headers[collection_index].status|= loading ? markLOAD : markUNLOAD;
	}
}


//-----------------------------------------------------------------------------



// returns count, doesn’t fill NULL buffer
short get_shape_descriptors(short shape_type, shape_descriptor *buffer)
{
	short collection_index, low_level_shape_index;
	short appropriate_type;
	short count;
    
    // huh?
	switch (shape_type)
	{
		case _wall_shape:
            appropriate_type = _wall_collection;
            break;
		case _floor_or_ceiling_shape:
            appropriate_type = _wall_collection;
            break;
		default:
			assert_fail(false, "");
			break;
	}

	count= 0;
	for (collection_index=0;collection_index<MAXIMUM_COLLECTIONS;++collection_index)
	{
        ShapesCollection* collection = get_shapes_collection(collection_index);
		// Skip over unloaded collections (and nonexistent frames and bitmaps).
		if (!collection) continue;
		
		if (collection && collection->type == appropriate_type)
		{
			for (low_level_shape_index = 0; low_level_shape_index < collection->low_level_shape_count; low_level_shape_index++)
			{
				low_level_shape_definition *low_level_shape = collection->get_frame(low_level_shape_index);
				if (!low_level_shape) continue;
                
				bitmap_definition_t* bitmap = collection->get_bitmap_definition(low_level_shape->bitmap_index);
				if (!bitmap) continue;
				
				count+= collection->clut_count;
				if (buffer)
				{
					short clut;
				
					for (clut=0;clut<collection->clut_count;++clut)
					{
						*buffer++= BUILD_DESCRIPTOR(BUILD_COLLECTION(collection_index, clut), low_level_shape_index);
					}
				}
			}
		}
	}
	
	return count;
}


void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition_t** bitmap, void **shading_tables, short shading_mode)
{
//	if (collection_code==_collection_marathon_control_panels) collection_code= 30, low_level_shape_index= 0;
	short collection_index = GET_COLLECTION_INDEX(collection_code);
	short clut_index = GET_COLLECTION_CLUT(collection_code);
	
	// Forget about it if some one managed to call us with the NONE value
	assert_fail(!(clut_index+1 == MAXIMUM_CLUTS_PER_COLLECTION && collection_index+1 == MAXIMUM_COLLECTIONS && low_level_shape_index+1 == MAXIMUM_SHAPES_PER_COLLECTION), "");
    
    ShapesCollection* collection = get_shapes_collection(collection_index);
	
    low_level_shape_definition* low_level_shape = collection->get_frame(low_level_shape_index);
	// Return NULL pointers for bitmap and shading table if the frame does not exist
	if (!low_level_shape)
	{
		*bitmap = NULL;
		if (shading_tables) *shading_tables = NULL;
		return;
	}
	
    if (bitmap) { *bitmap = collection->get_bitmap_definition(low_level_shape->bitmap_index); }
    
	if (shading_tables)
	{
		switch (shading_mode)
		{
			case _shading_normal:
				*shading_tables= get_collection_shading_tables(collection_index, clut_index);
				break;
			case _shading_infravision:
				*shading_tables= get_collection_tint_tables(collection_index, 0);
				break;
			
			default:
				assert_fail(false, "");
				break;
		}
	}
}


shape_information_data* extended_get_shape_information(short collection_code, short low_level_shape_index)
{
    short collection_index = GET_COLLECTION_INDEX(collection_code);
    if (collection_index < 0 || collection_index >= NUMBER_OF_COLLECTIONS || low_level_shape_index < 0) return nullptr;
    return (shape_information_data*)get_shapes_collection(collection_index)->get_frame(low_level_shape_index);
}


void process_collection_sounds(short collection_code, void (*process_sound)(short sound_index))
{
    ShapesCollection* collection = get_shapes_collection(GET_COLLECTION_INDEX(collection_code));
	if (!collection) return; // Skip over processing unloaded collections and sequences
	
	for (short high_level_shape_index = 0; high_level_shape_index < collection->high_level_shape_count; high_level_shape_index++)
	{
        high_level_shape_definition* high_level_shape = collection->get_high_level_definition(high_level_shape_index);
		if (!high_level_shape) return;
		
		process_sound(high_level_shape->first_frame_sound);
		process_sound(high_level_shape->key_frame_sound);
		process_sound(high_level_shape->last_frame_sound);
	}
}


shape_animation_data* get_shape_animation_data(shape_descriptor shape)
{
    ShapesCollection* collection = get_shapes_collection(GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(shape)));
    high_level_shape_definition* high_level_shape = collection->get_high_level_definition(GET_DESCRIPTOR_SHAPE(shape));
    return high_level_shape ? (shape_animation_data*)&high_level_shape->number_of_views : nullptr;
}


void *get_global_shading_table()
{
	void *shading_table= (void *) NULL;

	switch (main_screen.bit_depth())
	{
		case 8:
		{
			/* return the last shading_table calculated */
			short collection_index;
		
			for (collection_index=MAXIMUM_COLLECTIONS-1;collection_index>=0;--collection_index)
			{
				struct ShapesCollection *collection= get_shapes_collection(collection_index);
				
				if (collection)
				{
					shading_table= get_collection_shading_tables(collection_index, 0);
					break;
				}
			}
			
			break;
		}
		
		case 16:
			build_global_shading_table16();
			shading_table= global_shading_table16;
			break;
		
		case 32:
			build_global_shading_table32();
			shading_table= global_shading_table32;
			break;
		
		default:
			assert_fail(false, "");
			break;
	}
	assert_fail(shading_table, "");
	
	return shading_table;
}


void load_collections(bool is_opengl)
{
	precalculate_bit_depth_constants();
		
	// First go through our list of shape collections and dispose of any collections which were marked for unloading.
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        collection_header* header = &collection_headers[collection_index];
        if (collection_loaded(header))
        {
            unload_collection(header);
        }
        OGL_UnloadModelsImages(collection_index);
	}
	
	// ... then go back through the list of collections and load any that we were asked to
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        collection_header* header = &collection_headers[collection_index];
        if (header->status & markLOAD)
        {
            // load and decompress collection
            if (!load_collection(collection_index))
            {
                if (is_shapes_file_m1) { exit(outOfMemory); } // TODO: what is appropriate error?
            }
        }
		
		header->status = markNONE;
	}

	Plugins::instance()->load_shapes_patches(is_opengl);

	if (shapes_patch.size())
	{
		SDL_RWops *f = SDL_RWFromMem(&shapes_patch[0], (int32_t)shapes_patch.size());
		load_shapes_patch(f, true);
		SDL_RWclose(f);
	}

	/* remap the shapes, recalculate row base addresses, build our new world color table and
		(finally) update the screen to reflect our changes */
	update_color_environment();
}


int count_replacement_collections()
{
	int total_replacements = 0;
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
		if (collection_headers[collection_index].collection) // is it loaded?
		{
			total_replacements += OGL_CountModelsImages(collection_index);
		}
	}
	return total_replacements;
}


void load_replacement_collections()
{
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        if (collection_headers[collection_index].collection) // is it loaded?
		{
			OGL_LoadModelsImages(collection_index);
		}
	}
}

		
/* ---------- private code */

static void precalculate_bit_depth_constants()
{
	switch (main_screen.bit_depth())
	{
		case 8:
			number_of_shading_tables= 32;
			shading_table_fractional_bits= 5;
//			next_shading_table_shift= 8;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel8);
			break;
		case 16:
			number_of_shading_tables= 64;
			shading_table_fractional_bits= 6;
//			next_shading_table_shift= 9;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel16);
			break;
		case 32:
			number_of_shading_tables= 256;
			shading_table_fractional_bits= 8;
//			next_shading_table_shift= 10;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel32);
			break;
	}
}


// Given a list of RGBColors, find out which one, if any, match the given color.
// If there aren’t any matches, add a new entry and return that index.
static short find_or_add_color(shapes_color_t* color, shapes_color_t* colors, short* color_count, bool update_flags = true)
{
	// LP addition: save initial color-table pointer, just in case we overflow
    shapes_color_t *colors_saved = colors;
	
	// = 1 to skip the transparent color
    short i;
	for (i = 1, colors += 1; i < *color_count; i++, colors++)
	{
		if (colors->color.r == color->color.r && colors->color.g == color->color.g && colors->color.b == color->color.b)
		{
            if (update_flags) colors->luminescent = color->luminescent;
			return i;
		}
	}
	
	// LP change: added a fallback strategy; if there were too many colors, then find the closest one
	if (*color_count >= PIXEL8_MAXIMUM_COLORS)
	{
		// Set up minimum distance, its index
		// Strictly speaking, the distance squared, since that will be
		// what we will be calculating.
		// The color values are data type "word", which is unsigned short;
		// this explains the initial choice of minimum value --
		// as greater than any possible such value.
		double MinDiffSq = 3*double(65536)*double(65536);
		short MinIndx = 0;
		
		// Rescan
		colors = colors_saved;
        short i;
		for (i = 1, colors+= 1; i < *color_count; i++, colors++)
		{
			double RedDiff = double(color->color.r) - double(colors->color.r);
			double GreenDiff = double(color->color.g) - double(colors->color.g);
			double BlueDiff = double(color->color.b) - double(colors->color.b);
			double DiffSq = RedDiff*RedDiff + GreenDiff*GreenDiff + BlueDiff*BlueDiff;
			if (DiffSq < MinDiffSq)
			{
				MinIndx = i;
				MinDiffSq = DiffSq;
			}
		}
		return MinIndx;
	}
	
	// assert_fail(*color_count<PIXEL8_MAXIMUM_COLORS, "");
	*colors= *color;
	
	return (*color_count)++;
}



static void update_color_environment()
{
	pixel8 remapping_table[PIXEL8_MAXIMUM_COLORS];
    shapes_color_t colors[PIXEL8_MAXIMUM_COLORS];

	memset(remapping_table, 0, PIXEL8_MAXIMUM_COLORS * sizeof(pixel8));

	// dummy color to hold the first index (zero) for transparent pixels
	colors[0].color.r = colors[0].color.g = colors[0].color.b = 65535;
    colors[0].luminescent = false;
    colors[0].index = 0;
    short color_count = 1;

	/* loop through all collections, only paying attention to the loaded ones.  we’re
		depending on finding the gray run (white to black) first; so it’s the responsibility
		of the lowest numbered loaded collection to give us this */
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        ShapesCollection* collection = get_shapes_collection(collection_index);

//		ao__dprintf__("collection #%d", collection_index);
		
		if (collection && collection->bitmap_count)
		{
			struct shapes_color_t *primary_colors = collection->get_clut(0) + NUMBER_OF_PRIVATE_COLORS;
			short color_index, clut_index;

//			if (collection_index==15) ao__dprintf__("primary clut %p", primary_colors);
//			ao__dprintf__("primary clut %d entries;dm #%d #%d", collection->color_count, primary_colors, collection->color_count*sizeof(ColorSpec));

			/* add the colors from this collection’s primary color table to the aggregate color
				table and build the remapping table */
			for (color_index=0;color_index<collection->color_count-NUMBER_OF_PRIVATE_COLORS;++color_index)
			{
				primary_colors[color_index].index = remapping_table[primary_colors[color_index].index]
                                                  = find_or_add_color(&primary_colors[color_index], colors, &color_count);
			}
			
			/* then remap the collection and recalculate the base addresses of each bitmap */
			for (short bitmap_index= 0; bitmap_index<collection->bitmap_count; ++bitmap_index)
			{
				bitmap_definition_t* bitmap = collection->get_bitmap_definition(bitmap_index);
                if (!bitmap) { throw_out_of_bounds_f("Bad bitmap index for collection %d: %d", collection_index, bitmap_index); }
				
				/* calculate row base addresses ... */
				bitmap->row_addresses[0]= calculate_bitmap_origin(bitmap);
				precalculate_bitmap_row_addresses(bitmap);

				/* ... and remap it */
				remap_bitmap(bitmap, remapping_table);
			}
			
			/* build a shading table for each clut in this collection */
			for (clut_index= 0; clut_index<collection->clut_count; ++clut_index)
			{
				void *primary_shading_table= get_collection_shading_tables(collection_index, 0);
				short collection_bit_depth= collection->type==_interface_collection ? 8 : main_screen.bit_depth();

				if (clut_index)
				{
                    shapes_color_t* alternate_colors = collection->get_clut(clut_index) + NUMBER_OF_PRIVATE_COLORS;
					assert_fail(alternate_colors, "");
					void *alternate_shading_table= get_collection_shading_tables(collection_index, clut_index);
					pixel8 shading_remapping_table[PIXEL8_MAXIMUM_COLORS];
					
					memset(shading_remapping_table, 0, PIXEL8_MAXIMUM_COLORS*sizeof(pixel8));
					
//					ao__dprintf__("alternate clut %d entries;dm #%d #%d", collection->color_count, alternate_colors, collection->color_count*sizeof(ColorSpec));
					
					/* build a remapping table for the primary shading table which we can use to
						calculate this alternate shading table */
					for (color_index= 0; color_index<PIXEL8_MAXIMUM_COLORS; ++color_index) shading_remapping_table[color_index]= static_cast<pixel8>(color_index);
					for (color_index= 0; color_index<collection->color_count-NUMBER_OF_PRIVATE_COLORS; ++color_index)
					{
						shading_remapping_table[find_or_add_color(&primary_colors[color_index], colors, &color_count, false)]= 
							find_or_add_color(&alternate_colors[color_index], colors, &color_count);
					}
//					shading_remapping_table[iBLACK]= iBLACK; /* make iBLACK==>iBLACK remapping explicit */

					switch (collection_bit_depth)
					{
						case 8:
							// duplicate the primary shading table and remap it
							memcpy(alternate_shading_table, primary_shading_table, get_shading_table_size(collection_index));
                            map_bytes((pixel8*)alternate_shading_table, shading_remapping_table, get_shading_table_size(collection_index));
							break;
						
						case 16:
							build_shading_tables16(colors, color_count, (pixel16*)alternate_shading_table, shading_remapping_table);
                            break;
						
						case 32:
							build_shading_tables32(colors, color_count, (pixel32*)alternate_shading_table, shading_remapping_table);
							break;
					}
				}
				else
				{
					// build the primary shading table
					switch (collection_bit_depth)
					{
					case 8:
                            build_shading_tables8(colors, color_count, (pixel8*)primary_shading_table);
                            break;
                        case 16:
                            build_shading_tables16(colors, color_count, (pixel16*)primary_shading_table, nullptr);
                            break;
                        case 32:
                            build_shading_tables32(colors, color_count, (pixel32*)primary_shading_table, nullptr);
                            break;
					}
				}
			}
			
			build_collection_tinting_table(colors, color_count, collection_index);
            
			/* if we’re not in 8-bit, we don’t have to carry our colors over into the next collection */
			if (main_screen.bit_depth() != 8) color_count = 1;
		}
	}
        
	// rebuild our shading tables
    shapes_8_color_table.color_count = PIXEL8_MAXIMUM_COLORS;
	
    short color_index = 0;
	for (; color_index < color_count; color_index++)
	{
        shapes_8_color_table.colors[color_index] = *((ao_rgb*)&colors[color_index].color.r); // ick, copies red,green,blue values from shapes_color_t
	}
    // fill unused entries with black
	for (color_index = color_count; color_index < PIXEL8_MAXIMUM_COLORS; color_index++)
	{
        shapes_8_color_table.colors[color_index] = {0, 0, 0};
	}
}




static void build_shading_tables8(shapes_color_t* colors, short color_count, pixel8* shading_tables)
{
    memset(shading_tables, iBLACK, sizeof(shapes_color_t) * PIXEL8_MAXIMUM_COLORS);
	
    short start = 0, count = 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (short i = 0; i < count; i++)
		{
			assert_fail(number_of_shading_tables > 1, "");
			short adjust = start ? 1 : 0;

			for (short level = 0; level < number_of_shading_tables; level++)
			{
                shapes_color_t* color = colors + start + i;
                short multiplier = color->luminescent ? (level >> 1) : level;

				short value = i + (multiplier * (count + adjust - i)) / (number_of_shading_tables - 1);
				value = (value >= count) ? iBLACK : start + value;
				shading_tables[PIXEL8_MAXIMUM_COLORS * (number_of_shading_tables - level - 1) + start + i] = value;
			}
		}
	}
}


static void build_shading_tables16(shapes_color_t* colors, short color_count, pixel16* shading_tables, byte* remapping_table)
{
	objlist_set(shading_tables, 0, PIXEL8_MAXIMUM_COLORS);
	
    short start = 0, count = 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (short i = 0; i < count; i++)
		{
			assert_fail(number_of_shading_tables > 1, "");
			for (short level = 0; level < number_of_shading_tables; level++)
			{
                shapes_color_t* color = colors + (remapping_table ? remapping_table[start + i] : (start + i));
                short multiplier = color->luminescent ? ((number_of_shading_tables >> 1) + (level >> 1)) : level;
                
                // (SW) Find optimal pixel value for 16-bit video display
                shading_tables[PIXEL8_MAXIMUM_COLORS * level + start + i] = SDL_MapRGB(&pixel_format_16,
                               ((color->color.r * multiplier) / (number_of_shading_tables-1)) >> 8,
                               ((color->color.g * multiplier) / (number_of_shading_tables-1)) >> 8,
                               ((color->color.b * multiplier) / (number_of_shading_tables-1)) >> 8);
			}
		}
	}
}


static void build_shading_tables32(shapes_color_t* colors, short color_count, pixel32* shading_tables, byte* remapping_table)
{
	objlist_set(shading_tables, 0, PIXEL8_MAXIMUM_COLORS);
	
    short start = 0, count = 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (short i = 0; i<count; ++i)
		{
			assert_fail(number_of_shading_tables > 1, "");
			for (short level = 0; level<number_of_shading_tables; ++level)
			{
                shapes_color_t* color = colors + (remapping_table ? remapping_table[start + i] : (start + i));
                short multiplier = color->luminescent ? ((number_of_shading_tables >> 1) + (level >> 1)) : level;
				
				// (OGL) Mac xRGB 8888 pixel format
				shading_tables[PIXEL8_MAXIMUM_COLORS * level + start + i] = RGBCOLOR_TO_PIXEL32(
                                                                (color->color.r * multiplier) / (number_of_shading_tables - 1),
                                                                (color->color.g * multiplier) / (number_of_shading_tables - 1),
                                                                (color->color.b * multiplier) / (number_of_shading_tables - 1));
			}
		}
	}
}


static void build_global_shading_table16()
{
	if (!global_shading_table16)
	{
		global_shading_table16 = (pixel16*)ao_malloc(sizeof(pixel16) * number_of_shading_tables * NUMBER_OF_COLOR_COMPONENTS * (PIXEL16_MAXIMUM_COMPONENT + 1));
		
        pixel16* write = global_shading_table16;
		for (short shading_table = 0; shading_table < number_of_shading_tables; shading_table++)
		{
			// Under SDL, the components may have different widths and different shifts // EES: old comment; all we care about is what format the Shapes file uses for 16-bit (xRGB1555? RGB565?) and does it match what we're using as screen buffer (565)
			int shift = pixel_format_16.Rshift + (3 - pixel_format_16.Rloss);
			for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
            {
                *write++ = (value * shading_table / (number_of_shading_tables - 1)) << shift;
            }
			shift = pixel_format_16.Gshift + (3 - pixel_format_16.Gloss);
			for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
            {
                *write++ = (value * shading_table / (number_of_shading_tables - 1)) << shift;
            }
			shift = pixel_format_16.Bshift + (3 - pixel_format_16.Bloss);
			for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
            {
                *write++ = (value * shading_table / (number_of_shading_tables - 1)) << shift;
            }
		}
	}
}


static void build_global_shading_table32()
{
	if (!global_shading_table32)
	{
		global_shading_table32 = (pixel32*)ao_malloc(sizeof(pixel32) * number_of_shading_tables * NUMBER_OF_COLOR_COMPONENTS * (PIXEL32_MAXIMUM_COMPONENT + 1));
		
        pixel32* write = global_shading_table32;
		for (short shading_table = 0; shading_table < number_of_shading_tables; shading_table++)
		{
			// Under SDL, the components may have different widths and different shifts
			int shift = pixel_format_32.Rshift - pixel_format_32.Rloss;
			for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
            {
                *write++ = ((value * (shading_table)) / (number_of_shading_tables - 1)) << shift;
            }
			shift = pixel_format_32.Gshift - pixel_format_32.Gloss;
			for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
            {
                *write++ = ((value * (shading_table)) / (number_of_shading_tables - 1)) << shift;
            }
			shift = pixel_format_32.Bshift - pixel_format_32.Bloss;
			for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
            {
                *write++ = ((value * (shading_table)) / (number_of_shading_tables - 1)) << shift;
            }
		}
	}
}


static bool get_next_color_run(shapes_color_t* colors, short color_count, short* start, short* count)
{
	bool not_done= false;
	struct shapes_color_t last_color;
	
	if (*start+*count<color_count)
	{
		*start+= *count;
		for (*count=0;*start+*count<color_count;*count+= 1)
		{
			if (*count)
			{
				if (new_color_run(colors+*start+*count, &last_color))
				{
					break;
				}
			}
			last_color= colors[*start+*count];
		}
		
		not_done= true;
	}
	
	return not_done;
}


static bool new_color_run(shapes_color_t* _new, shapes_color_t* last)
{
	return ((int32)last->color.r + (int32)last->color.g + (int32)last->color.b
          < (int32)_new->color.r + (int32)_new->color.g + (int32)_new->color.b);
}


static int32 get_shading_table_size(short collection_code)
{
	int32 size;
	
	switch (main_screen.bit_depth())
	{
		case 8: size= number_of_shading_tables*shading_table_size; break;
		case 16: size= number_of_shading_tables*shading_table_size; break;
		case 32: size= number_of_shading_tables*shading_table_size; break;
		default:
			assert_fail(false, "");
			break;
	}
	
	return size;
}


/* --------- light enhancement goggles */

enum /* collection tint colors */
{
	_tint_collection_red,
	_tint_collection_green,
	_tint_collection_blue,
	_tint_collection_yellow,
	NUMBER_OF_TINT_COLORS
};

struct tint_color8_data
{
	short start, count;
};

static struct ao_rgb tint_colors16[NUMBER_OF_TINT_COLORS]=
{
	{65535, 0, 0},
	{0, 65535, 0},
	{0, 0, 65535},
	{65535, 65535, 0},
};

static struct tint_color8_data tint_colors8[NUMBER_OF_TINT_COLORS]=
{
	{45, 13},
	{32, 13},
	{96, 13},
	{83, 13},
};


static short CollectionTints[NUMBER_OF_COLLECTIONS] =
{
	// Interface
	NONE,
	// Weapons in hand
	_tint_collection_yellow,
	// Juggernaut, tick
	_tint_collection_red,
	_tint_collection_red,
	// Explosion effects
	_tint_collection_yellow,
	// Hunter	
	_tint_collection_red,
	// Player
	_tint_collection_yellow,
	// Items
	_tint_collection_green,
	// Trooper, Pfhor, S'pht'Kr, F'lickta
	_tint_collection_red,
	_tint_collection_red,
	_tint_collection_red,
	_tint_collection_red,
	// Bob and VacBobs
	_tint_collection_yellow,
	_tint_collection_yellow,
	// Enforcer, Drone
	_tint_collection_red,
	_tint_collection_red,
	// S'pht
	_tint_collection_blue,
	// Walls
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	// Scenery
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	// Landscape
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	_tint_collection_blue,
	// Cyborg
	_tint_collection_red
};


static void build_collection_tinting_table(shapes_color_t* colors, short color_count, short collection_index)
{
    ShapesCollection *collection= get_shapes_collection(collection_index);
	if (!collection) return;
	
	void *tint_table= get_collection_tint_tables(collection_index, 0);
	short tint_color;

	/* get the tint color */
	// LP change: look up a table
	tint_color = CollectionTints[collection_index];
	// Idiot-proofing:
	if (tint_color >= NUMBER_OF_TINT_COLORS)
		tint_color = NONE;
	else
		tint_color = MAX(tint_color,NONE);

	/* build the tint table */	
	if (tint_color!=NONE)
	{
		// LP addition: OpenGL support
		ao_rgb &Color = tint_colors16[tint_color];
		OGL_SetInfravisionTint(collection_index,true,Color.r/65535.0F,Color.g/65535.0F,Color.b/65535.0F);
		switch (main_screen.bit_depth())
		{
			case 8:
				build_tinting_table8(colors, color_count, (unsigned char *)tint_table, tint_colors8[tint_color].start, tint_colors8[tint_color].count);
				break;
			case 16:
				build_tinting_table16(colors, color_count, (pixel16 *)tint_table, tint_colors16+tint_color);
				break;
			case 32:
				build_tinting_table32(colors, color_count, (pixel32 *)tint_table, tint_colors16+tint_color);
				break;
		}
	}
	else
	{
		OGL_SetInfravisionTint(collection_index,false,1,1,1);
	}
}


static void build_tinting_table8(shapes_color_t* colors, short color_count, pixel8* tint_table, short tint_start, short tint_count)
{
	short start = 0, count = 0;
	
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		short i;

		for (i=0; i<count; ++i)
		{
			short adjust= start ? 0 : 1;
			short value= (i*(tint_count+adjust))/count;
			
			value= (value>=tint_count) ? iBLACK : tint_start + value;
			tint_table[start+i]= value;
		}
	}
}


// Return intensity(base)*tint, with M2-style rounding behavior
static ao_rgb m2_apply_tint(shapes_color_t base, ao_rgb tint)
{
	const uint16_t base_mag = (int32_t(base.color.r) + base.color.g + base.color.b) / 3;
#define SCALE(comp) (uint16_t(int32_t(1LL * base_mag * comp) / 65535))
	return {SCALE(tint.r), SCALE(tint.g), SCALE(tint.b)};
#undef SCALE
}


static void build_tinting_table16(shapes_color_t* colors, short color_count, pixel16* tint_table, ao_rgb* tint_color)
{
	for (short i = 0; i < color_count; i++, colors++)
	{
		const ao_rgb tinted_color = m2_apply_tint(*colors, *tint_color);
		
		// Find optimal pixel value for video display
		*tint_table++ = SDL_MapRGB(&pixel_format_16, tinted_color.r >> 8, tinted_color.g >> 8, tinted_color.b >> 8);
	}
}


static void build_tinting_table32(shapes_color_t* colors, short color_count, pixel32* tint_table, ao_rgb* tint_color)
{
	for (short i = 0; i < color_count; i++, colors++)
	{
		const ao_rgb tinted_color = m2_apply_tint(*colors, *tint_color);
		// Mac xRGB 8888 pixel format
        *tint_table++ = RGBCOLOR_TO_PIXEL32(tinted_color.r, tinted_color.g, tinted_color.b);
	}
}



// TODO: merging header into collection simplifies code (we could still support unloading by yeeting all the bitmap data but inclined to keep them in memory as we're not short of RAM nowadays); once code is sufficiently clean, we can work on sprite sheets


static collection_header* get_collection_header(short collection_index)
{
	// This one is intended to bomb because collection indices can only be from 1 to 31,
	// short of drastic changes in how collection indices are specified (a bigger structure
	// than shape_descriptor, for example). // EES: except that it didn't always bomb, you twit, because asserts are DEBUG-only
    return &collection_headers.at(collection_index); // NOW it bombs. Progress!
}


ShapesCollection* get_shapes_collection(short collection_index) // returns nullptr if the collection isn't loaded
{
	return get_collection_header(collection_index)->collection;
}







static void* get_collection_shading_tables(short collection_index, short clut_index)
{
	void *shading_tables= get_collection_header(collection_index)->shading_tables.data();
	shading_tables = (uint8 *)shading_tables + clut_index*get_shading_table_size(collection_index);
	return shading_tables;
}


static void* get_collection_tint_tables(short collection_index, short tint_index)
{
	struct ShapesCollection *definition= get_shapes_collection(collection_index);
	if (!definition) return NULL;
	
	void *tint_table= get_collection_header(collection_index)->shading_tables.data();

	tint_table = (uint8 *)tint_table + get_shading_table_size(collection_index)*definition->clut_count + shading_table_size*tint_index;
	
	return tint_table;
}







// Which bitmap index for a frame (good for OpenGL texture rendering)
short get_bitmap_index(short collection_index, short low_level_shape_index)
{
	low_level_shape_definition* low_level_shape = get_shapes_collection(collection_index)->get_frame(low_level_shape_index);
	return low_level_shape ? low_level_shape->bitmap_index : NONE;
}


// XML elements for parsing infravision specification
short *OriginalCollectionTints = NULL;
struct ao_rgb *original_tint_colors16 = NULL;


void reset_mml_infravision()
{
	if (original_tint_colors16) {
		for (int i = 0; i < NUMBER_OF_TINT_COLORS; i++)
			tint_colors16[i] = original_tint_colors16[i];
		free(original_tint_colors16);
		original_tint_colors16 = NULL;
	}

	if (OriginalCollectionTints) {
		for (int i = 0; i < NUMBER_OF_COLLECTIONS; i++)
			CollectionTints[i] = OriginalCollectionTints[i];
		free(OriginalCollectionTints);
		OriginalCollectionTints = NULL;
	}
}


void parse_mml_infravision(const InfoTree& root)
{
	// back up old values first
	if (!original_tint_colors16) {
		original_tint_colors16 = (struct ao_rgb *) malloc(sizeof(struct ao_rgb) * NUMBER_OF_TINT_COLORS);
		assert_fail(original_tint_colors16, "");
		for (int i = 0; i < NUMBER_OF_TINT_COLORS; i++)
			original_tint_colors16[i] = tint_colors16[i];
	}
	
	if (!OriginalCollectionTints) {
		OriginalCollectionTints = (short *) malloc(sizeof(short) * NUMBER_OF_COLLECTIONS);
		assert_fail(OriginalCollectionTints, "");
		for (int i = 0; i < NUMBER_OF_COLLECTIONS; i++)
			OriginalCollectionTints[i] = CollectionTints[i];
	}

	for (const InfoTree &color : root.children_named("color"))
	{
		int16 index;
		if (!color.read_indexed("index", index, NUMBER_OF_TINT_COLORS))
			continue;
		color.read_color(tint_colors16[index]);
	}
	
	for (const InfoTree &assign : root.children_named("assign"))
	{
		int16 coll, color;
		if (!assign.read_indexed("coll", coll, NUMBER_OF_COLLECTIONS) ||
			!assign.read_indexed("color", color, NUMBER_OF_TINT_COLORS))
			continue;
		CollectionTints[coll] = color;
	}
}
