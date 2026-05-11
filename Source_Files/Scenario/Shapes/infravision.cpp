/*
 infravision.cpp
 
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

#include "infravision.hpp"

#include "shapes.h"
#include "Screen.hpp"

#include "InfoTree.h"


extern SDL_PixelFormat pixel_format_16;


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


static const std::array<tint_color8_data, NUMBER_OF_TINT_COLORS> collection_tint_colors_8_std = // TODO: is this not configurable in MML?
{
    tint_color8_data{45, 13},
    tint_color8_data{32, 13},
    tint_color8_data{96, 13},
    tint_color8_data{83, 13},
};


static const std::array<ao_rgb, NUMBER_OF_TINT_COLORS> collection_tint_colors_16_std =
{
    ao_rgb{65535,     0,     0},
    ao_rgb{    0, 65535,     0},
    ao_rgb{    0,     0, 65535},
    ao_rgb{65535, 65535,     0},
};


static std::array<int32_t, NUMBER_OF_COLLECTIONS> collection_tint_types_std =
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


static int32_t collection_tint_types[NUMBER_OF_COLLECTIONS];

static ao_rgb collection_tint_colors_16[NUMBER_OF_TINT_COLORS];




// Return intensity(base)*tint, with M2-style rounding behavior
inline ao_rgb m2_apply_tint(const shapes_color_t& base, ao_rgb tint)
{
#define SCALE(comp)  (uint16_t(int32_t(1LL * base_mag * comp) / 65535))
    
    const uint16_t base_mag = (int32_t(base.value.r) + base.value.g + base.value.b) / 3;
    return {SCALE(tint.r), SCALE(tint.g), SCALE(tint.b)};
    
#undef SCALE
}


static void build_tinting_table8(const shapes_colors_t& clut, shading_tables_8_t& tint_table, short tint_start, short tint_count)
{
    short start = 0, count = 0;
    while (get_next_color_run(clut, start, count))
    {
        for (short i = 0; i < count; i++)
        {
            short value  = (i * (tint_count + (start ? 0 : 1))) / count;
            tint_table[start + i]= (value >= tint_count) ? CLUT_8_BLACK : tint_start + value;
        }
    }
}


static void build_tinting_table16(const shapes_colors_t& clut, const ao_rgb& tint_color, shading_tables_16_t& tint_table)
{
    for (short i = 0; i < clut.size(); i++)
    {
        const ao_rgb tinted_color = m2_apply_tint(clut[i], tint_color);
        tint_table[i] = RGBCOLOR_TO_PIXEL16(tinted_color.r, tinted_color.g, tinted_color.b);
    }
}


static void build_tinting_table32(const shapes_colors_t& clut, const ao_rgb& tint_color, shading_tables_32_t& tint_table)
{
    for (short i = 0; i < clut.size(); i++)
    {
        const ao_rgb tinted_color = m2_apply_tint(clut[i], tint_color);
        tint_table[i] = RGBCOLOR_TO_PIXEL32(tinted_color.r, tinted_color.g, tinted_color.b);
    }
}




void build_collection_tinting_tables(ShapesCollection* collection, shapes_colors_t& colors)
{
    short tint_color = collection_tint_types[collection->index];
    assert_fail(tint_color >= 0 && tint_color < NUMBER_OF_TINT_COLORS, "");
    
    collection->infravision_tint = collection_tint_colors_16[tint_color];
    
    // 8-bit infravision isn't overrideable in MML
    build_tinting_table8(colors, collection->get_tint_table_8(0),
                         collection_tint_colors_8_std[tint_color].start, collection_tint_colors_8_std[tint_color].count);

    build_tinting_table16(colors, collection_tint_colors_16[tint_color], collection->get_tint_table_16(0));

    build_tinting_table32(colors, collection_tint_colors_16[tint_color], collection->get_tint_table_32(0));
}


//-----------------------------------------------------------------------------
// MML


void reset_mml_infravision()
{
    std::copy(collection_tint_types_std.begin(), collection_tint_types_std.end(), collection_tint_types);
    std::copy(collection_tint_colors_16_std.begin(), collection_tint_colors_16_std.end(), collection_tint_colors_16);
}


void parse_mml_infravision(const InfoTree& root)
{
    for (const InfoTree &color : root.children_named("color"))
    {
        int16 index;
        if (color.read_indexed("index", index, NUMBER_OF_TINT_COLORS))
        {
            color.read_color(collection_tint_colors_16[index]);
        }
    }
    
    for (const InfoTree &assign : root.children_named("assign"))
    {
        int16 coll, color = 0;
        if (assign.read_indexed("coll", coll, NUMBER_OF_COLLECTIONS) && assign.read_indexed("color", color, NUMBER_OF_TINT_COLORS))
        {
            collection_tint_types[coll] = color;
        }
    }
}
