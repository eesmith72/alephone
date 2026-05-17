/*
 shape_to_surface.cpp
 
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

#include "shape_to_surface.hpp"

#include "shapes.h"

#include "Screen.hpp"


// ZZZ extension: pass out (if non-NULL) a pointer to a block of pixel data -
// caller should free() that storage after freeing the returned surface.
// Only needed for RLE-encoded shapes.
// Note that default arguments are used to make this function
// source-code compatible with existing usage.
// Note also that inShrinkImage currently only applies to RLE shapes.



SDL_Surface* get_shape_surface(int shape, int inCollection,
                               byte** outPointerToPixelData, float inIllumination, bool inShrinkImage)
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
        
        uint32*    shading_tables    = (uint32*) shading_tables_as_void;
        shading_tables += 256 * (int)(inIllumination * (number_of_shading_tables_32 - 1));
        
        // Extract color table - ZZZ change to use shading table rather than CLUT.  Hope it works.
        for(int i = 0; i < 256; i++) {
            colors[i].r = RED32(shading_tables[i]);
            colors[i].g = GREEN32(shading_tables[i]);
            colors[i].b = BLUE32(shading_tables[i]);
            colors[i].a = 0xff;
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
        uint32    theNumberOfStorageBytes = bitmap->width * bitmap->height * sizeof(byte);
        byte*    pixel_storage = (byte*) malloc(theNumberOfStorageBytes);
        memset(pixel_storage, 0, theNumberOfStorageBytes);
        
        // Here, a "run" is a row or column.  An "element" is a single pixel's data.
        // We always go forward through the source data.  Thus, the offsets for where the next run
        // or element goes into the destination data area change depending on the circumstances.
        int16    theNumRuns;
        int16    theDestDataNextRunOffset;
        int16    theDestDataNextElementOffset;
        
        // Is this row-major or column-major?
        if(bitmap->flags & _COLUMN_ORDER_BIT) {
            theNumRuns                = bitmap->width;
            theDestDataNextRunOffset        = (low_level_shape->flags & _X_MIRRORED_BIT) ? -1 : 1;
            theDestDataNextElementOffset    = (low_level_shape->flags & _Y_MIRRORED_BIT) ? -bitmap->width : bitmap->width;
        }
        else {
            theNumRuns                = bitmap->height;
            theDestDataNextElementOffset    = (low_level_shape->flags & _X_MIRRORED_BIT) ? -1 : 1;
            theDestDataNextRunOffset        = (low_level_shape->flags & _Y_MIRRORED_BIT) ? -bitmap->width : bitmap->width;
        }
        
        // Figure out where our first byte will be written
        byte* theDestDataStartAddress = pixel_storage;
        
        if(low_level_shape->flags & _X_MIRRORED_BIT)
            theDestDataStartAddress += bitmap->width - 1;
        
        if(low_level_shape->flags & _Y_MIRRORED_BIT)
            theDestDataStartAddress += bitmap->width * (bitmap->height - 1);
        
        // Walk through runs, un-RLE'ing as we go
        for(int run = 0; run < theNumRuns; run++) {
            uint16*    theLengthData                     = (uint16*) bitmap->row_addresses[run];
            uint16    theFirstOpaquePixelElement             = SDL_SwapBE16(theLengthData[0]);
            uint16    theFirstTransparentAfterOpaquePixelElement    = SDL_SwapBE16(theLengthData[1]);
            uint16    theNumberOfOpaquePixels = theFirstTransparentAfterOpaquePixelElement - theFirstOpaquePixelElement;
            
            byte*    theOriginalPixelData = (byte*) &theLengthData[2];
            byte*    theUnpackedPixelData;
            
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
        int image_width        = bitmap->width;
        int image_height    = bitmap->height;
        
        if(inShrinkImage) {
            int        theLargerWidth        = bitmap->width;
            int        theLargerHeight        = bitmap->height;
            byte*    theLargerPixelStorage    = pixel_storage;
            int        theSmallerWidth        = theLargerWidth / 2 + theLargerWidth % 2;
            int        theSmallerHeight    = theLargerHeight / 2 + theLargerHeight % 2;
            byte*    theSmallerPixelStorage    = (byte*) malloc(theSmallerWidth * theSmallerHeight);
            
            for(int y = 0; y < theSmallerHeight; y++) {
                for(int x = 0; x < theSmallerWidth; x++) {
                    theSmallerPixelStorage[y * theSmallerWidth + x] =
                    theLargerPixelStorage[(y * theLargerWidth + x) * 2];
                }
            }
            
            free(pixel_storage);
            
            pixel_storage    = theSmallerPixelStorage;
            image_width        = theSmallerWidth;
            image_height    = theSmallerHeight;
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

