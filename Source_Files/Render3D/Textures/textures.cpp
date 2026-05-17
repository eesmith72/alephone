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


// bytes_per_row and height must be set first
void bitmap_definition_t::precalculate_bitmap_row_addresses(pixel8* row_address)
{
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


void remap_bitmap(bitmap_definition_t* bitmap, pixel8* table)
{
    int16_t rows    = (bitmap->flags & _COLUMN_ORDER_BIT) ? bitmap->width  : bitmap->height;
    int16_t columns = (bitmap->flags & _COLUMN_ORDER_BIT) ? bitmap->height : bitmap->width;
    
    if (bitmap->bytes_per_row != NONE)
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
            // CB: first/last are stored in big-endian order // EES: this is some subtle shit, presumably for RLE where row lengths are variable; in moving to sprite sheets RLE will have to go away
            uint16_t first = *pixels++ << 8;
            first         |= *pixels++;
            uint16_t  last = *pixels++ << 8;
            last          |= *pixels++;
            
            map_bytes(pixels, table, last - first);
            pixels += last - first;
        }
    }
}


void map_bytes(uint8_t* buffer, uint8_t* table, int32_t size)
{
    while ((size -= 1) >= 0 )
    {
        *buffer = table[*buffer];
        buffer += 1;
    }
}


//-----------------------------------------------------------------------------


// TODO: what is bitmap originally?
static void convert_m1_rle(SDL_RWops* p, int rows, int columns, std::vector<uint8_t>& bitmap)
{
    for (int32_t scanline = 0; scanline < rows; scanline++)
    {
        std::vector<uint8_t> scanline_data(columns + 1);
        uint8_t* dst = &scanline_data[0];
        uint8_t* sentry = &scanline_data[columns];
        
        int16_t opcode;
        while ((opcode = SDL_ReadBE16(p)))
        {
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
        }

        assert (dst == sentry);

        // Find M2/oo-format RLE compression;
        // it needs the first nonblank pixel and the last nonblank one + 1
        int16 first = 0;
        int16 last = 0;
        for (int i = 0; i < columns; ++i)
        {
            if (scanline_data[i] != 0)
            {
                first = i;
                break;
            }
        }

        for (int i = columns - 1; i >= 0; --i)
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
    
    if (bytes_per_row == NONE)
    {
        if (is_m1)
        {
            // make enough room for the definition, then append as we convert RLE
            bitmap.resize(rows * sizeof(pixel8*)); // TODO: don't think this is right
        }
        else
        {
            // ugly--figure out how big it's going to be
            int32 size = 0;
            for (int j = 0; j < rows; j++)
            {
                int16 first = SDL_ReadBE16(p);
                int16 last  = SDL_ReadBE16(p);
                size += 4;
                SDL_RWseek(p, last - first, SEEK_CUR);
                size += last - first;
            }
            
            bitmap.resize(rows * sizeof(pixel8*) + size);

            // Now, seek back
            SDL_RWseek(p, -size, SEEK_CUR);
        }
    }
    else
    {
        bitmap.resize(rows * sizeof(pixel8*) + rows * bytes_per_row);
    }
    
    // Copy bitmap data
    uint8_t* c = bitmap.data();
    if (bytes_per_row == NONE) // RLE format
    {
        if (is_m1)
            convert_m1_rle(p, rows, columns, bitmap);
        else
            convert_m2_rle(p, rows, bitmap);
    }
    else
    {
        SDL_RWread(p, c, bytes_per_row, rows);
        c += rows * bytes_per_row;
    }
    
    precalculate_bitmap_row_addresses(bitmap.data());
    
    if (is_m1_wall_texture && (width > 128 || height > 128))
    {
       width = height = 128; // correct for M1's dodgy oversized wall texture; must be done AFTER precalculating row addresses
    }
}
