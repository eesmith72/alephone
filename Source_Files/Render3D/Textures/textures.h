/*
 TEXTURES.H
 
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

#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "cseries.hpp"

#include "cscolors.hpp" 


enum // bitmap flags
{
    _COLUMN_ORDER_BIT = 0x8000,
    _TRANSPARENT_BIT  = 0x4000,
    _PATCHED_BIT      = 0x2000, // the bitmap should take precedent over MML
};


struct bitmap_definition_t
{
    int16_t width, height; // in pixels
    int16_t bytes_per_row; // if NONE this is a transparent RLE shape (sprite)
    
    int16_t flags;         // [column_order.1] [unused.15]
    int16_t bit_depth;     // should always be ==8
    
    std::vector<uint8_t*> row_addresses;
    
    std::vector<uint8_t> bitmap; // holds the image's pixel data, or empty if an external buffer is used
    
    // initialize all of the above except row_addresses, then call this to populate it
    void initialize_row_addresses(pixel8* first_row_address = nullptr); // a pointer into an external buffer (e.g. external sprite sheet); if omitted, bitmap.data() is used
    
    void remap_colors(pixel8* remapping_table);
    
    void read(SDL_RWops* p, bool is_m1, bool is_wall_texture = false);
};

const int SIZEOF_bitmap_definition = 30;


void map_bytes(uint8_t* buffer, uint8_t* table, int32_t size); // used here and in shapes.cpp


#endif

