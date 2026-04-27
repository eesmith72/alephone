/*
 textures.cpp
 
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

#include "textures.h"


pixel8 *calculate_bitmap_origin(bitmap_definition *bitmap)
{
    pixel8* origin = (pixel8*)(((byte*)bitmap) + sizeof(struct bitmap_definition));
    return origin + (bitmap->flags & _COLUMN_ORDER_BIT ? bitmap->width : bitmap->height) * sizeof(pixel8*);
}


void remap_bitmap(bitmap_definition* bitmap, pixel8* table)
{
    int16_t rows    = (bitmap->flags & _COLUMN_ORDER_BIT) ? bitmap->width  : bitmap->height;
    int16_t columns = (bitmap->flags & _COLUMN_ORDER_BIT) ? bitmap->height : bitmap->width;
    
    if (bitmap->bytes_per_row!=NONE)
    {
        for (int16_t row = 0; row < rows; row++)
        {
            map_bytes(bitmap->row_addresses[row], table, columns * sizeof(pixel8));
        }
    }
    else
    {
        pixel8* pixels = bitmap->row_addresses[0];
        
        for (int16_t row = 0; row < rows; row++)
        {
            // CB: first/last are stored in big-endian order
            uint16_t first = *pixels++ << 8;
            first         |= *pixels++;
            uint16_t  last = *pixels++ << 8;
            last          |= *pixels++;
            
            map_bytes(pixels, table, last-first);
            pixels += last-first;
        }
    }
}


// must initialize bytes_per_row, height and row_address[0]
void precalculate_bitmap_row_addresses(bitmap_definition* bitmap)
{
    int16_t rows = (bitmap->flags & _COLUMN_ORDER_BIT) ? bitmap->width : bitmap->height;
    pixel8* row_address = bitmap->row_addresses[0];
    pixel8** table      = bitmap->row_addresses;
    
    int16_t bytes_per_row = bitmap->bytes_per_row;
    
    if (bytes_per_row != NONE)
    {
        for (int16_t row = 0; row < rows; row++)
        {
            *table     ++= row_address;
            row_address += bytes_per_row;
        }
    }
    else
    {
        for (int16_t row = 0; row < rows; row++)
        {
            *table       ++= row_address;
            
            // CB: first/last are stored in big-endian order
            uint16_t first = *row_address++ << 8;
            first         |= *row_address++;
            uint16_t  last = *row_address++ << 8;
            last          |= *row_address++;
            
            row_address   += last-first;
        }
    }
}


void map_bytes(uint8_t* buffer, uint8_t* table, int32_t size)
{
    while ((size -= 1) >=0 )
    {
        *buffer = table[*buffer];
        buffer += 1;
    }
}

