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


//-----------------------------------------------------------------------------


void map_bytes(uint8_t* buffer, uint8_t* table, int32_t size)
{
    while ((size -= 1) >= 0 )
    {
        *buffer = table[*buffer];
        buffer += 1;
    }
}


void bitmap_definition_t::initialize_row_addresses(pixel8* row_address)
{
    if (!row_address)
    {
        assert_fail(!bitmap.empty(), "bitmap_definition_t's bitmap buffer must be initialized or an external buffer given.");
        row_address = bitmap.data();
    }
    
    int16_t rows = (flags & _COLUMN_ORDER_BIT) ? width : height;
    row_addresses.resize(rows);
    pixel8** table = row_addresses.data();
    
    if (bytes_per_row != NONE)
    {
        for (int16_t row = 0; row < rows; row++)
        {
            *table++ = row_address;
            row_address += bytes_per_row;
        }
    }
    else
    {
        for (int16_t row = 0; row < rows; row++)
        {
            *table++ = row_address;
            
            // CB: first/last are stored in big-endian order
            uint16_t first = *row_address++ << 8;
            first         |= *row_address++;
            
            uint16_t  last = *row_address++ << 8;
            last          |= *row_address++;
            
            row_address += last - first;
        }
    }
}


void bitmap_definition_t::remap_colors(pixel8* remapping_table)
{
    int16_t rows    = (flags & _COLUMN_ORDER_BIT) ? width  : height;
    int16_t columns = (flags & _COLUMN_ORDER_BIT) ? height : width;
    
    if (bytes_per_row != NONE)
    {
        for (int16_t row = 0; row < rows; row++)
        {
            map_bytes(row_addresses[row], remapping_table, columns * sizeof(pixel8));
        }
    }
    else
    {
        pixel8* pixels = row_addresses[0];
        
        for (int16_t row = 0; row < rows; row++)
        {
            // CB: first/last are stored in big-endian order // EES: this is some subtle shit, presumably for RLE where row lengths are variable; in moving to sprite sheets RLE will have to go away
            uint16_t first = *pixels++ << 8;
            first         |= *pixels++;
            uint16_t  last = *pixels++ << 8;
            last          |= *pixels++;
            
            map_bytes(pixels, remapping_table, last - first);
            pixels += last - first;
        }
    }
}


//-----------------------------------------------------------------------------
// read from Shapes file


static void convert_m1_rle(SDL_RWops* p, int scanlines, int scanline_length, std::vector<uint8_t>& bitmap)
{
    for (int scanline = 0; scanline < scanlines; ++scanline)
    {
        std::vector<uint8> scanline_data(scanline_length + 1);
        uint8* dst = &scanline_data[0];
        uint8* sentry = &scanline_data[scanline_length];

        int16 opcode;
        while ((opcode = SDL_ReadBE16(p)))
        {
            if (opcode > 0)
            {
                assert(dst + opcode <= sentry);
                SDL_RWread(p, dst, opcode, 1);
                dst += opcode;
            }
            else // if (opcode < 0)
            {
                assert(dst - opcode <= sentry);
                dst -= opcode;
            }
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


static void convert_m2_rle(SDL_RWops *p, int rows, std::vector<uint8_t>& bitmap)
{
    uint8_t* c = bitmap.data();
    for (int j = 0; j < rows; j++)
    {
        int16 first = SDL_ReadBE16(p);
        int16 last  = SDL_ReadBE16(p);
        *(c++) = (uint8_t)(first >> 8);
        *(c++) = (uint8_t)(first);
        *(c++) = (uint8_t)(last >> 8);
        *(c++) = (uint8_t)(last);
        SDL_RWread(p, c, 1, last - first);
        c += last - first;
    }
}


void bitmap_definition_t::read(SDL_RWops* p, bool is_m1, bool is_m1_wall_texture)
{
    // Convert bitmap definition
    width         = SDL_ReadBE16(p);
    height        = SDL_ReadBE16(p);
    bytes_per_row = SDL_ReadBE16(p);
    flags         = SDL_ReadBE16(p);
    bit_depth     = SDL_ReadBE16(p);
    
    // guess how big to make it
    int rows = (flags & _COLUMN_ORDER_BIT) ? width : height;
    int columns = (flags & _COLUMN_ORDER_BIT) ? height : width;
    
    SDL_RWseek(p, 16, SEEK_CUR);
        
    // Skip row address pointers
    SDL_RWseek(p, (rows + 1) * sizeof(uint32), SEEK_CUR);
    
    row_addresses.reserve(rows);
    
    // Copy bitmap data
    if (bytes_per_row == NONE) // RLE format
    {
        bitmap.reserve(rows * columns); // should be enough to avoid reallocs
        if (is_m1)
            convert_m1_rle(p, rows, columns, bitmap);
        else
            convert_m2_rle(p, rows, bitmap);
    }
    else
    {
        bitmap.resize(rows * sizeof(pixel8*) + rows * bytes_per_row);
        uint8_t* c = bitmap.data();
        SDL_RWread(p, c, bytes_per_row, rows);
        c += rows * bytes_per_row;
    }
    
    initialize_row_addresses();
    
    if (is_m1_wall_texture && (width > 128 || height > 128))
    {
       width = height = 128; // correct for M1's dodgy oversized wall texture; must be done AFTER precalculating row addresses
    }
}
