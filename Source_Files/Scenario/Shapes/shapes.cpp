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

// TODO: splitting this file would be really, really helpful to understanding it (at least some code is moving to ShapesCollection.cpp)

// TODO: merging header into collection simplifies code (we could still support unloading by yeeting all the bitmap data but inclined to keep them in memory as we're not short of RAM nowadays); once code is sufficiently clean, we can work on sprite sheets


#include "shapes.h"

//#include "shell.h"
#include "render.h"
#include "interface.hpp"
#include "Screen.hpp"
#include "DataFile.hpp"
#include "images.h"

#include "map.h"

#include "graphics_preferences.hpp"

#include "infravision.hpp"

#include "OGL_Render.h"

#include "InfoTree.h"

#include "Packing.h"

#include "Plugins.h"


// there are 3 reserved transparency colors at the beginning of every clut (typically cyan, magenta, blue)
#define NUMBER_OF_PRIVATE_COLORS  (3)


color_table_t shapes_8_color_table; // this is the gameworld's indexed color table constructed from Shapes collections; used in Classic 8


static std::array<ShapesCollection, MAXIMUM_COLLECTIONS> shapes_collections; // TODO: this may become a map in future (once the myriad calls to get_shapes_collection are reduced to minimum)


static DataFile ShapesFile_M2;

ResourceFile ShapesFile_M1; // M1 Trojan (to be awkward) put its 'term' resources in the Shapes.shps file, so ComputerTerminal.cpp has to fish them out of here or this would be static too

static bool is_shapes_file_m1;

bool shapes_file_is_m1() { return is_shapes_file_m1; }




SDL_PixelFormat pixel_format_16, pixel_format_32; // also used in infravision.cpp


static pixel8  global_shading_table_8[sizeof(pixel8) * PIXEL8_MAXIMUM_COLORS];
                                                                                                                  
static pixel16 global_shading_table_16[sizeof(pixel16) * number_of_shading_tables_16
                                       * NUMBER_OF_COLOR_COMPONENTS * (PIXEL16_MAXIMUM_COMPONENT + 1)];

static pixel32 global_shading_table_32[sizeof(pixel32) * number_of_shading_tables_32
                                       * NUMBER_OF_COLOR_COMPONENTS * (PIXEL32_MAXIMUM_COMPONENT + 1)];





static void update_color_environment();

static short find_or_add_color(shapes_color_t *color, shapes_color_t *colors, bool update_flags); // update_flags is false in `shading_remapping_table[INDEX]=...`


static void build_global_shading_table8();
static void build_global_shading_table16();
static void build_global_shading_table32();



static void load_collection(short collection_index);



static void shutdown_shape_handler();

static void close_shapes_file();


//-----------------------------------------------------------------------------



ShapesCollection* get_shapes_collection(int16_t collection_index);

size_t number_of_shapes_collections()
{
    return shapes_collections.size();
}



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
	
    shapes_frame_t* low_level_shape = collection->get_frame(low_level_shape_index);
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
                shading_tables += 256 * (int)(inIllumination * (number_of_shading_tables_16 - 1));
                
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
                shading_tables += 256 * (int)(inIllumination * (number_of_shading_tables_32 - 1));
                
                // Extract color table - ZZZ change to use shading table rather than CLUT.  Hope it works.
                for(int i = 0; i < 256; i++)
                {
                    // TODO: FIX: use the pixel32 bitshifts, Luke
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
            colors[src_colors[i].index] = SDL_Color(src_colors[i].value);
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




static void load_clut(shapes_color_t* r, int count, SDL_RWops* p)
{
	for (int i = 0; i < count; i++, r++) 
	{
        r->luminescent = SDL_ReadU8(p);
        r->index = SDL_ReadU8(p);
		r->value.r = SDL_ReadBE16(p);
		r->value.g = SDL_ReadBE16(p);
		r->value.b = SDL_ReadBE16(p);
	}
}


static void load_high_level_shape(std::vector<uint8>& shape, SDL_RWops* p)
{
    SDL_ReadBE16(p); // int16 type (unused)
    SDL_ReadBE16(p); // int16 flags (unused)
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

	shape.resize(sizeof(shapes_animation_t) + num_views * frames_per_view * sizeof(int16));
		
	shapes_animation_t *d = (shapes_animation_t *) &shape[0];
		
	//memcpy(d->name, name, HIGH_LEVEL_SHAPE_NAME_LENGTH + 2);
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
	b.width         = SDL_ReadBE16(p);
	b.height        = SDL_ReadBE16(p);
	b.bytes_per_row = SDL_ReadBE16(p);
	b.flags         = SDL_ReadBE16(p);
	b.bit_depth     = SDL_ReadBE16(p);

	// guess how big to make it
	int rows = (b.flags & _COLUMN_ORDER_BIT) ? b.width : b.height;
	int row_len = (b.flags & _COLUMN_ORDER_BIT) ? b.height : b.width;
		
	SDL_RWseek(p, 16, SEEK_CUR);
		
	// Skip row address pointers
	SDL_RWseek(p, (rows + 1) * sizeof(uint32), SEEK_CUR);
    
    // TODO: replace variable-size struct with fixed-size struct + std::vector at end
    
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



static void load_collection(short collection_index)
{
    ShapesCollection* collection = get_shapes_collection(collection_index);
    collection->index = collection_index;
    
    log_note_f("Loading Shapes collection %d", collection_index);
    
	SDL_RWops* p;
	std::shared_ptr<SDL_RWops> m1_p; // automatic deallocation
	LoadedResource r;
	int32 src_offset;
	
	if (is_shapes_file_m1)
	{
		// Collections are stored in .256 resources
		if (!ShapesFile_M1.Get('.', '2', '5', '6', 128 + collection_index, r))
		{
            log_error_f("Can't load M1 Shapes collection %d: not defined.", collection_index);
            collection->loaded = false;
            return;
		}

		m1_p.reset(SDL_RWFromConstMem(r.GetPointer(), (int32_t)r.get_length()), SDL_FreeRW);
		p = m1_p.get();
		src_offset = 0;
	}
	else
	{
		// Get offset and length of data in source file from header
		
		if (main_screen.bit_depth() == 8 || collection->offset16 == -1)
        {
			if (collection->offset8 == -1)
            {
                log_error_f("Can't load M2 Shapes collection %d: not defined.", collection_index);
                collection->loaded = false;
                return;
            }
			src_offset = collection->offset8;
		}
        else
        {
			src_offset = collection->offset16;
		}

		p = ShapesFile_M2.borrow_rwops();
		ShapesFile_M2.set_position(0);
		src_offset += SDL_RWtell(p);
	}

	// Read collection definition
	SDL_RWseek(p, src_offset, RW_SEEK_SET);
    
    collection->read_content(p);
    
//	collection->status &= ~markPATCHED;

	// Convert CLUTS
	if (collection->clut_count && collection->color_count)
    {
		SDL_RWseek(p, src_offset + collection->color_table_offset, RW_SEEK_SET);
		load_clut(&collection->color_tables[0], collection->clut_count * collection->color_count, p);
	}

	// Convert high-level shape definitions
	if (collection->high_level_shape_count)
    {
		SDL_RWseek(p, src_offset + collection->high_level_shape_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(collection->high_level_shape_count);
		SDL_RWread(p, &t[0], sizeof(uint32), collection->high_level_shape_count);
        swap_array_BE32(&t[0], collection->high_level_shape_count);

		for (int i = 0; i < collection->high_level_shape_count; i++)
        {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			load_high_level_shape(collection->high_level_shapes[i], p);
		}
	}

	// Convert low-level shape definitions
	if (collection->low_level_shape_count)
    {
		SDL_RWseek(p, src_offset + collection->low_level_shape_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(collection->low_level_shape_count);
		SDL_RWread(p, &t[0], sizeof(uint32), collection->low_level_shape_count);
        swap_array_BE32(&t[0], collection->low_level_shape_count);

		for (int i = 0; i < collection->low_level_shape_count; i++)
        {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			collection->low_level_shapes[i].read(p);
		}
	}

	// Convert bitmap definitions
	if (collection->bitmap_count)
    {
		SDL_RWseek(p, src_offset + collection->bitmap_offset_table_offset, RW_SEEK_SET);
		std::vector<uint32> t(collection->bitmap_count);
		SDL_RWread(p, &t[0], sizeof(uint32), collection->bitmap_count);
        swap_array_BE32(&t[0], collection->bitmap_count);

		for (int i = 0; i < collection->bitmap_count; i++)
        {
			SDL_RWseek(p, src_offset + t[i], RW_SEEK_SET);
			load_bitmap(collection->bitmaps[i], p, is_shapes_file_m1);
		}
	}
    
    collection->loaded = true;
    
    
    // TODO: replacement collections
    
    OGL_LoadModelsImages(collection_index);
}
			


//-----------------------------------------------------------------------------
// shapes patch

// EES: looks like this is for M2 Shapes patches; I don't know how common those are and don't recall offhand if they're full collections (good) or can be partial collections (PITA)


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
	int32 start = (int32_t)SDL_RWtell(p);
	SDL_RWseek(p, 0, SEEK_END);
	int32 end = (int32_t)SDL_RWtell(p);

	SDL_RWseek(p, start, SEEK_SET);
	
	bool done = false;
	while (!done)
	{
		// is there more data to read?
		if (SDL_RWtell(p) < end)
		{
			int32 collection_index = SDL_ReadBE32(p);
			int32 patch_bit_depth = SDL_ReadBE32(p);
            
            ShapesCollection *cd = get_shapes_collection(collection_index);
            
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
					if (cd->loaded && patch_bit_depth == 8)
					{
                        cd->read_content(p);
						color_counts[collection_index] = cd->color_count;
	//					cd->status|=markPATCHED;

					} else {
						// get the color count (it's the only way to skip the CTAB_TAG
						SDL_RWseek(p, 6, SEEK_CUR);
						color_counts[collection_index] = SDL_ReadBE16(p);
						SDL_RWseek(p, 544 - 8, SEEK_CUR);
					}
				} 
				else if (tag == HLSH_TAG)
				{
					int32 high_level_shape_index = SDL_ReadBE32(p);
					int32 size = SDL_ReadBE32(p);
					int32 pos = (int32_t)SDL_RWtell(p);
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
					int32 low_level_shape_index = SDL_ReadBE32(p);
					if (cd && patch_bit_depth == 8 && low_level_shape_index < cd->low_level_shapes.size())
					{
						cd->low_level_shapes[low_level_shape_index].read(p);
					}
					else
					{
						SDL_RWseek(p, shapes_frame_data_size, SEEK_CUR);
					}
				} 
				else if (tag == BMAP_TAG)
				{
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
    
    load_shapes_collections();
}





void open_shapes_file(const ao_path& File)
{
	if (ShapesFile_M1.open(File) == no_err && ShapesFile_M1.Check('.','2','5','6',128))
	{
		is_shapes_file_m1 = true; // TODO: so when does it get read?
	}
	else
	{
		ShapesFile_M1.Close();
        is_shapes_file_m1 = false;
        
        if (ShapesFile_M2.open(File) == no_err) // TODO: and what if it fails? error handling is awful
        {
            // Load the collection headers
            for (int i = 0; i < MAXIMUM_COLLECTIONS; i++)
            {
                shapes_collections[i].read_header(ShapesFile_M2);
            }
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



bool collection_exists(short collection_index) // used in lua_script.cpp, lua_map.cpp, lua_hud_objects.cpp
{
    return (collection_index >= 0 && collection_index < number_of_shapes_collections() && get_shapes_collection(collection_index)->loaded);
}


/*
void unload_all_collections() // TODO: needed? no, but the OGL_UnloadModelsImages call will be needed when replacing a collection
{
	for (short i = 0; i < MAXIMUM_COLLECTIONS; i++)
	{
         // TODO: FIX
        ShapesCollection* header = &shapes_collections[i];
         header->shading_tables_8.clear();
		OGL_UnloadModelsImages(i);
	}
}
 */


//-----------------------------------------------------------------------------


void extended_get_shape_bitmap_and_shading_table(short collection_code, short low_level_shape_index,
                                                 bitmap_definition_t** bitmap, void **shading_tables, short shading_mode)
{
//	if (collection_code==_collection_marathon_control_panels) collection_code= 30, low_level_shape_index= 0;
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
                    *shading_tables = collection->get_shading_table(clut_index);
                    break;
                case _shading_infravision:
                    *shading_tables = collection->get_tint_table(0);
                    break;
                default:
                    throw_bug_report_f("Bad shading_mode: %d", shading_mode);
            }
        }
    }
    else
    {
        if (bitmap) { *bitmap = nullptr; }
        if (shading_tables) { *shading_tables = nullptr; }
    }
}


shape_information_data* extended_get_shape_information(short collection_code, short low_level_shape_index)
{
    short collection_index = GET_COLLECTION_INDEX(collection_code);
    if (collection_index < 0 || collection_index >= NUMBER_OF_COLLECTIONS || low_level_shape_index < 0) return nullptr;
    return (shape_information_data*)get_shapes_collection(collection_index)->get_frame(low_level_shape_index);
}


void process_collection_sounds(short collection_code, process_sound_proc process_sound)
{
    ShapesCollection* collection = get_shapes_collection(GET_COLLECTION_INDEX(collection_code));
    if (!collection->loaded) return; // Skip over processing unloaded collections and sequences
	
	for (short high_level_shape_index = 0; high_level_shape_index < collection->high_level_shape_count; high_level_shape_index++)
	{
        shapes_animation_t* high_level_shape = collection->get_animation_sequence(high_level_shape_index);
		if (!high_level_shape) return;
		
		process_sound(high_level_shape->first_frame_sound);
		process_sound(high_level_shape->key_frame_sound);
		process_sound(high_level_shape->last_frame_sound);
	}
}


shapes_animation_t* get_shape_animation_data(shape_descriptor shape)
{
    ShapesCollection* collection = get_shapes_collection(GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(shape)));
    shapes_animation_t* high_level_shape = collection->get_animation_sequence(GET_DESCRIPTOR_SHAPE(shape));
    return high_level_shape ? (shapes_animation_t*)&high_level_shape->number_of_views : nullptr;
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
	}
    throw_bug_report("This should never happen.");
}


void load_shapes_collections()
{
    // now we load everything and never unload it (not unless a different scenario gets loaded)
    
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        load_collection(collection_index);
	}

	Plugins::instance()->load_shapes_patches(true); // TODO: get rid of is_opengl argument

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


// TODO: merge into load_collection

void load_replacement_collections() // called from OGLRenderer::initialize
{
	for (short collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
	{
        if (get_shapes_collection(collection_index)->loaded)
        {
            OGL_LoadModelsImages(collection_index);
        }
	}
}









		
//-----------------------------------------------------------------------------
// build color lookup tables


// Given a list of RGBColors, find out which one, if any, match the given color.
// If there aren’t any matches, add a new entry and return that index.
static short find_or_add_color(shapes_color_t* color, shapes_colors_t& found_colors, bool update_flags = true)
{
	// = 1 to skip the transparent color
    for (short i = 1; i < found_colors.size(); i++)
	{
        shapes_color_t& found_color = found_colors[i];
		if (color->value == found_color.value)
		{
            if (update_flags) { found_color.luminescent = color->luminescent; }
			return i;
		}
	}
	
	if (found_colors.size() < PIXEL8_MAXIMUM_COLORS)
    {
        found_colors.push_back(*color);
        return found_colors.size() - 1;
    }
    else
	{
        // LP change: added a fallback strategy; if there were too many colors, then find the closest one
        // EES: this doesn't find the closest color, just the first color that is 'close'. The smart strategy would be to collect all colors found in all [8-bit] collections, then remap them to averaged ramps... but that's more work than I am going to put in, so LP's weak algorithm remains until it irritates someone enough to redo it.
		// Set up minimum distance, its index
		// Strictly speaking, the distance squared, since that will be what we will be calculating.
		// The color values are unsigned short; this explains the initial choice of minimum value
        // -- as greater than any possible such value.
		double MinDiffSq = 3.0 * 65536 * 65536;
		short MinIndx = 0;
		
		// Rescan
        for (short i = 1; i < found_colors.size(); i++)
		{
            shapes_color_t& found_color = found_colors[i];
			double diff_r = double(color->value.r) - double(found_color.value.r);
			double diff_g = double(color->value.g) - double(found_color.value.g);
			double diff_b = double(color->value.b) - double(found_color.value.b);
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
    struct shapes_color_t last_color;
    
    if (start + count < colors.size())
    {
        start += count;
        for (count = 0; start + count < colors.size(); count++)
        {
            if (count > 0)
            {
                const shapes_color_t& this_color = colors[start + count];
                
                if ((int32_t(last_color.value.r) + int32_t(last_color.value.g) + int32_t(last_color.value.b)) <
                    (int32_t(this_color.value.r) + int32_t(this_color.value.g) + int32_t(this_color.value.b))) { break; }
            }
            last_color = colors[start + count];
        }
        
        not_done = true;
    }
    
    return not_done;
}


static void build_shading_tables8(shapes_colors_t colors, shading_tables_8_t& shading_tables)
{
    shading_tables.fill(CLUT_8_BLACK);
    
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            assert_fail(number_of_shading_tables_8 > 1, "");
            short adjust = start ? 1 : 0;

            for (short level = 0; level < number_of_shading_tables_8; level++)
            {
                shapes_color_t& color = colors[start + i];
                short multiplier = color.luminescent ? (level >> 1) : level;

                short value = i + (multiplier * (count + adjust - i)) / (number_of_shading_tables_8 - 1);
                value = (value >= count) ? CLUT_8_BLACK : start + value;
                shading_tables[(number_of_shading_tables_8 - level - 1) + start + i] = value;
            }
        }
    }
}


static void build_shading_tables16(const shapes_colors_t& colors, const shading_tables_8_t* remapping_table, shading_tables_16_t& shading_tables)
{
    shading_tables.fill(0); // Paint It Black
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            for (short level = 0; level < number_of_shading_tables_16; level++)
            {
                const shapes_color_t& color = colors[remapping_table ? (*remapping_table)[start + i] : (start + i)];
                short multiplier = color.luminescent ? ((number_of_shading_tables_16 >> 1) + (level >> 1)) : level;
                
                // (SW) Find optimal pixel value for 16-bit video display; EES: this seems to use 555, not 565, but it'll do
                shading_tables[level + start + i]
                        = RGBCOLOR_TO_PIXEL16(((color.value.r * multiplier) / (number_of_shading_tables_16 - 1)),
                                              ((color.value.g * multiplier) / (number_of_shading_tables_16 - 1)),
                                              ((color.value.b * multiplier) / (number_of_shading_tables_16 - 1)));
            }
        }
    }
}


static void build_shading_tables32(const shapes_colors_t& colors, const shading_tables_8_t* remapping_table, shading_tables_32_t& shading_tables)
{
    shading_tables.fill(0);
    short start = 0, count = 0;
    while (get_next_color_run(colors, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            for (short level = 0; level < number_of_shading_tables_32; ++level)
            {
                const shapes_color_t& color = colors[remapping_table ? (*remapping_table)[start + i] : (start + i)];
                short multiplier = color.luminescent ? ((number_of_shading_tables_32 >> 1) + (level >> 1)) : level;
                
                shading_tables[level + start + i]
                        = RGBCOLOR_TO_PIXEL32((color.value.r * multiplier) / (number_of_shading_tables_32 - 1),
                                              (color.value.g * multiplier) / (number_of_shading_tables_32 - 1),
                                              (color.value.b * multiplier) / (number_of_shading_tables_32 - 1));
            }
        }
    }
}


static void build_global_shading_table8()
{
    // use the last shading_table calculated
    for (int32_t collection_index = MAXIMUM_COLLECTIONS - 1; collection_index >= 0; collection_index--)
    {
        ShapesCollection* collection = get_shapes_collection(collection_index);
        if (collection->loaded)
        {
            memcpy(global_shading_table_8, collection->get_shading_table_8(0).data(), sizeof(pixel8) * PIXEL8_MAXIMUM_COLORS);
            return;
        }
    }

}


static void build_global_shading_table16()
{
    pixel16* write = global_shading_table_16;
    for (short shading_table = 0; shading_table < number_of_shading_tables_16; shading_table++)
    {
        // Under SDL, the components may have different widths and different shifts // EES: old comment; all we care about is what format the Shapes file uses for 16-bit (xRGB1555? RGB565?) and does it match what we're using as screen buffer (565)
        int shift = pixel_format_16.Rshift + (3 - pixel_format_16.Rloss);
        for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
        shift = pixel_format_16.Gshift + (3 - pixel_format_16.Gloss);
        for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
        shift = pixel_format_16.Bshift + (3 - pixel_format_16.Bloss);
        for (short value = 0; value <= PIXEL16_MAXIMUM_COMPONENT; value++)
        {
            *write++ = (value * shading_table / (number_of_shading_tables_16 - 1)) << shift;
        }
    }
}


static void build_global_shading_table32()
{
    pixel32* write = global_shading_table_32;
    for (short shading_table = 0; shading_table < number_of_shading_tables_32; shading_table++)
    {
        int shift = pixel_format_32.Rshift - pixel_format_32.Rloss;
        for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * (shading_table)) / (number_of_shading_tables_32 - 1)) << shift;
        }
        shift = pixel_format_32.Gshift - pixel_format_32.Gloss;
        for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * (shading_table)) / (number_of_shading_tables_32 - 1)) << shift;
        }
        shift = pixel_format_32.Bshift - pixel_format_32.Bloss;
        for (short value = 0; value <= PIXEL32_MAXIMUM_COMPONENT; value++)
        {
            *write++ = ((value * (shading_table)) / (number_of_shading_tables_32 - 1)) << shift;
        }
    }
}


// build all of the above tables for each collection
static void update_color_environment()
{
    shapes_colors_t found_colors_8;
    
    shading_tables_8_t remapping_table; //remap 8-bit collections' cluts; TODO: needs helpful explanation
    remapping_table.fill(0);
    
    // dummy color to hold the first index (zero) for transparent pixels
    shapes_color_t& transparency = found_colors_8.emplace_back();
    transparency.value.r = transparency.value.g = transparency.value.b = 65535;
    transparency.luminescent = false;
    transparency.index = 0;
    short color_count = 1;
    
    // Loop through all loaded collections. We’re depending on finding the gray run (white to black) first,
    // so it’s the responsibility of the lowest numbered loaded collection to give us this.
    for (int32_t collection_index = 0; collection_index < MAXIMUM_COLLECTIONS; collection_index++)
    {
        ShapesCollection* collection = get_shapes_collection(collection_index);
        
        if (collection->loaded && collection->bitmap_count)
        {
            shapes_color_t *primary_colors = collection->get_clut(0) + NUMBER_OF_PRIVATE_COLORS;
            
            // add the colors from this collection’s primary color table to the aggregate color table and build the remapping table
            for (int32_t color_index = 0; color_index < collection->color_count - NUMBER_OF_PRIVATE_COLORS; color_index++)
            {
                primary_colors[color_index].index = remapping_table[primary_colors[color_index].index]
                                                  = find_or_add_color(&primary_colors[color_index], found_colors_8);
            }
            
            // then remap the collection and recalculate the base addresses of each bitmap
            for (int32_t bitmap_index = 0; bitmap_index < collection->bitmap_count; bitmap_index++)
            {
                bitmap_definition_t* bitmap = collection->get_bitmap_definition(bitmap_index);
                if (!bitmap) { throw_out_of_bounds_f("Bad bitmap index for collection %d: %d", collection_index, bitmap_index); }
                
                // calculate row base addresses...
                bitmap->row_addresses[0] = calculate_bitmap_origin(bitmap);
                precalculate_bitmap_row_addresses(bitmap);
                // ...and remap it
                remap_bitmap(bitmap, remapping_table.data());
            }
            
            // TODO: what about weapons-in-hand and landscape?
            
            // build the primary shading table
            build_shading_tables8(found_colors_8, collection->get_shading_table_8(0));
            build_shading_tables16(found_colors_8, nullptr, collection->get_shading_table_16(0));
            build_shading_tables32(found_colors_8, nullptr, collection->get_shading_table_32(0));
            
            // build the alternate shading tables
            for (int32_t clut_index = 1; clut_index < collection->clut_count; clut_index++)
            {
                // build a remapping table for the primary shading table which we can use to calculate this alternate shading table
                shading_tables_8_t shading_remapping_table;
                
                for (int32_t color_index = 0; color_index < PIXEL8_MAXIMUM_COLORS; color_index++)
                {
                    shading_remapping_table[color_index] = pixel8(color_index);
                }
                
                shapes_color_t* alternate_colors = collection->get_clut(clut_index) + NUMBER_OF_PRIVATE_COLORS;
                for (int32_t color_index = 0; color_index < collection->color_count - NUMBER_OF_PRIVATE_COLORS; color_index++)
                {
                    int32_t new_index = find_or_add_color(&primary_colors[color_index], found_colors_8, false);
                    shading_remapping_table[new_index] = find_or_add_color(&alternate_colors[color_index], found_colors_8);
                }
                
                // duplicate the primary shading table and remap it
                shading_tables_8_t alternate_shading_table_8 = collection->get_shading_table_8(0);
                map_bytes(alternate_shading_table_8.data(), shading_remapping_table.data(), alternate_shading_table_8.size());
                collection->get_shading_table_8(clut_index) = alternate_shading_table_8;
                
                build_shading_tables16(found_colors_8, &shading_remapping_table, collection->get_shading_table_16(clut_index));
                
                build_shading_tables32(found_colors_8, &shading_remapping_table, collection->get_shading_table_32(clut_index));
            }
            
            build_collection_tinting_tables(collection, found_colors_8);
            
            // if we’re not in 8-bit, we don’t have to carry our colors over into the next collection
            //if (main_screen.bit_depth() != 8) color_count = 1;
        }
    }
    
    // rebuild our shading tables
    shapes_8_color_table.color_count = PIXEL8_MAXIMUM_COLORS;
    
    short color_index = 0;
    for (; color_index < color_count; color_index++)
    {
        shapes_8_color_table.colors[color_index] = found_colors_8[color_index].value;
    }
    // fill unused entries with black
    for (color_index = color_count; color_index < PIXEL8_MAXIMUM_COLORS; color_index++)
    {
        shapes_8_color_table.colors[color_index] = {0, 0, 0};
    }
    
    build_global_shading_table8();
    build_global_shading_table16();
    build_global_shading_table32();
}



//-----------------------------------------------------------------------------




ShapesCollection* get_shapes_collection(short collection_index)
{
	return &shapes_collections.at(collection_index);
}


// Which bitmap index for a frame (good for OpenGL texture rendering)
short get_bitmap_index(short collection_index, short low_level_shape_index)
{
	shapes_frame_t* low_level_shape = get_shapes_collection(collection_index)->get_frame(low_level_shape_index);
	return low_level_shape ? low_level_shape->bitmap_index : NONE;
}


