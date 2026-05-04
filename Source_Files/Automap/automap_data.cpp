/*
 automap_data.cpp
 
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

#include "cseries.hpp"

// TODO: cleanup #includes

#include "map.h"
#include "monsters.h"
#include "automap_data.hpp"
#include "player.h"
#include "render.h"
#include "flood_map.h"
#include "platforms.h"
#include "media.h"
#include "InfoTree.h"

#include "screen_drawing.h" // get_player_color

#include "graphics_preferences.hpp"
#include "Screen.hpp"
#include "camera.hpp"


#ifdef DEBUG
//#define PATH_DEBUG
//#define RENDER_DEBUG
#endif

#ifdef RENDER_DEBUG
extern camera_settings_t* world_view;
#endif


const int TOTAL_NUMBER_OF_COLORS = NUMBER_OF_POLYGON_COLORS + NUMBER_OF_LINE_DEFINITIONS + NUMBER_OF_THINGS + NUMBER_OF_ANNOTATION_DEFINITIONS + 2;
const int TOTAL_NUMBER_OF_FONTS = NUMBER_OF_ANNOTATION_DEFINITIONS * NUMBER_OF_ZOOM_LEVELS + 1;


automap_visibility_t player_automap_visibility;


// Constants moved out to OverheadMapRender.h
// Render flags now in OverheadMapRender.c

// TODO: eventually modernize this (it's a mess, but it'll do until a 2D map editor comes along)

// The configuration data
static const automap_appearance_t automap_style_std = {
	// Polygon colors
	{
		{0, 12000, 0},				// Plain polygon
		{30000, 0, 0},				// Platform
		{14*256, 37*256, 63*256},	// Water
		{76*256, 27*256, 0},		// Lava
		{70*256, 90*256, 0},		// Sewage
		{70*256, 90*256, 0},		// JjaroGoo
		{137*256, 0, 137*256},		// PfhorSlime
		{0, 12000, 0},			// Hill
		{76*256, 27*256, 0},		// Minor Damage
		{137*256, 0, 137*256},		// Major Damage
		{0, 12000, 0}			// Teleporter
	},
	// Line definitions (color, 4 widths)
	{
		{{0, 65535, 0}, {1, 2, 2, 4}},	// Solid
		{{0, 40000, 0}, {1, 1, 1, 2}},	// Elevation
		{{65535, 0, 0}, {1, 2, 2, 4}}	// Control-Panel
	},
	// Thing definitions (color, shape, 4 radii)
	{
		{{0, 0, 65535}, _rectangle_thing, {1, 2, 4, 8}}, /* civilian */
		{{65535, 0, 0}, _rectangle_thing, {1, 2, 4, 8}}, /* non-player monster */
		{{65535, 65535, 65535}, _rectangle_thing, {1, 2, 3, 4}}, /* item */
		{{65535, 65535, 0}, _rectangle_thing, {1, 1, 2, 3}}, /* projectiles */
		{{65535, 0, 0}, _circle_thing, {8, 16, 16, 16}}	// LP note: this is for checkpoint locations
	},
	// Live-monster type assignments
	{
		// Marine
		_civilian_thing,
		// Ticks
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// S'pht
		_monster_thing,
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// Pfhor
		_monster_thing,
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// Bob
		_civilian_thing,
		_civilian_thing,
		_civilian_thing,
		_civilian_thing,
		// Drone
		_monster_thing,
		_monster_thing,
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// Cyborg
		_monster_thing,
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// Enforcer
		_monster_thing,
		_monster_thing,
		// Hunter
		_monster_thing,
		_monster_thing,
		// Trooper
		_monster_thing,
		_monster_thing,
		// Big Cyborg, Hunter
		_monster_thing,
		_monster_thing,
		// F'lickta
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// S'pht'Kr
		_monster_thing,
		_monster_thing,
		// Juggernauts
		_monster_thing,
		_monster_thing,
		// Tiny ones
		_monster_thing,
		_monster_thing,
		_monster_thing,
		// VacBobs
		_civilian_thing,
		_civilian_thing,
		_civilian_thing,
		_civilian_thing,
	},
	// Dead-monster type assignments
	{
		NONE,				// Interface (what one sees in the HUD)
		NONE,				// Weapons in Hand
		
		_monster_thing,		// Juggernaut
		_monster_thing,		// Tick
		_monster_thing,		// Explosion effects
		_monster_thing,		// Hunter
		NONE,				// Player
	
		_monster_thing,		// Items
		_monster_thing,		// Trooper
		_monster_thing,		// Fighter
		_monster_thing,		// S'pht'Kr
		_monster_thing,		// F'lickta
		
		_civilian_thing,	// Bob
		_civilian_thing,	// VacBob
		_monster_thing,		// Enforcer
		_monster_thing,		// Drone
		_monster_thing,		// S'pht
		
		NONE,				// Water
		NONE,				// Lava
		NONE,				// Sewage
		NONE,				// Jjaro
		NONE,				// Pfhor
	
		NONE,				// Water Scenery
		NONE,				// Lava Scenery
		NONE,				// Sewage Scenery
		NONE,				// Jjaro Scenery
		NONE,				// Pfhor Scenery
		
		NONE,				// Day
		NONE,				// Night
		NONE,				// Moon
		NONE,				// Outer Space
		
		_monster_thing		// Cyborg
	},
	// Player-entity definition
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	// Annotations (color, 4 fonts)
	{
		{{0, 65535, 0},
		{
			kFontIDMonaco, styleBold,  5,
			kFontIDMonaco, styleBold,  9,
			kFontIDMonaco, styleBold, 12,
			kFontIDMonaco, styleBold, 18,
		}}
	},
	// Map name (color, font)
	{{0, 65535, 0}, {kFontIDMonaco, styleNormal, 18}, 25},
	// Path color
	{65535, 65535, 65535},
	// What to show (aliens, items, projectiles, paths)
	false, false, false, false
};


static bool MapFontsInited = false;


automap_appearance_t player_automap_appearance = automap_style_std;


//-----------------------------------------------------------------------------


void ResetOverheadMap() // TODO:
{
    /*
    switch (automap_visibility)
    {
        case automap_visibility_t::normal: // Default: nothing (mapping is cumulative)
            break;
        case automap_visibility_t::camera: // No previous visibility is carried over
            clear_automap();
            break;
        case automap_visibility_t::global: // Everything is assumed visible
            reveal_automap();
            break;
    };
     */
}

//-----------------------------------------------------------------------------
//


void automap_visibility_t::fill_polygon(short polygon_index)
{
    polygon_data* polygon = get_polygon_data(polygon_index);
    for (size_t i = 0; i < polygon->vertex_count; i++)
    {
        add_line(polygon->line_indexes[i]);
    }
    add_polygon(polygon_index);
}


static int32 automap_flood_fill_cost_proc(short source_polygon_index, short line_index,
                                          short destination_polygon_index, void* visibility)
{
    polygon_data* destination_polygon = get_polygon_data(destination_polygon_index);
    polygon_data* source_polygon = get_polygon_data(source_polygon_index);
    int32_t cost = 1;
    
    (void)(line_index);
    
    // can’t leave secret platforms
    if (source_polygon->type == _polygon_is_platform && PLATFORM_IS_SECRET(get_platform_data(source_polygon->permutation)))
    {
        cost = -1;
    }
    
    // can’t enter secret platforms which are also doors
    if (destination_polygon->type == _polygon_is_platform)
    {
        platform_data* platform = get_platform_data(destination_polygon->permutation);
        if (PLATFORM_IS_DOOR(platform) && PLATFORM_IS_SECRET(platform)) cost = -1;
    }
    
    // add the destination polygon and all its lines to the automap
    if (cost > 0) { ((automap_visibility_t*)visibility)->fill_polygon(destination_polygon_index); }
    
    return cost;
}


void automap_visibility_t::flood_fill_polygons(short polygon_index)
{
    clear_all();
    fill_polygon(polygon_index);
    
    polygon_index = flood_map(polygon_index, INT32_MAX, automap_flood_fill_cost_proc, _breadth_first, this);
    do
    {
        polygon_index = flood_map(NONE, INT32_MAX, automap_flood_fill_cost_proc, _breadth_first, this);
    }
    while (polygon_index != NONE);
}





//-----------------------------------------------------------------------------
// MML

// TODO: this needs more thought: separate appearance (theme) from visibility (cheats, customizations)


void reset_mml_overhead_map()
{
    player_automap_appearance = automap_style_std;
}

// automap_visibility_t::mode_t
#define NUMBER_OF_VISIBILITY_MODES (3)

void parse_mml_overhead_map(const InfoTree& root)
{
    /*
    int16_t visibility;
	root.read_indexed("mode", visibility, NUMBER_OF_VISIBILITY_MODES);
     
    
	root.read_attr("title_offset", automap_settings.map_name_data.offset_down);

	for (const InfoTree &assign : root.children_named("assign_live"))
	{
		int16 monster;
		if (!assign.read_indexed("monster", monster, NUMBER_OF_MONSTER_TYPES))
			continue;
		assign.read_attr_bounded<int16>("type", automap_settings.monster_displays[monster], -1, 1);
	}
	
	for (const InfoTree &assign : root.children_named("assign_dead"))
	{
		int16 coll;
		if (!assign.read_indexed("coll", coll, NUMBER_OF_COLLECTIONS))
			continue;
		assign.read_attr_bounded<int16>("type", automap_settings.dead_monster_displays[coll], -1, 1);
	}
	
	for (const InfoTree &child : root.children_named("aliens"))
	{
		child.read_attr("on", automap_settings.ShowAliens);
	}
	for (const InfoTree &child : root.children_named("items"))
	{
		child.read_attr("on", automap_settings.ShowItems);
	}
	for (const InfoTree &child : root.children_named("projectiles"))
	{
		child.read_attr("on", automap_settings.ShowProjectiles);
	}
	for (const InfoTree &child : root.children_named("paths"))
	{
		child.read_attr("on", automap_settings.ShowPaths);
	}

	for (const InfoTree &line : root.children_named("line_width"))
	{
		int16 index;
		if (!line.read_indexed("index", index, NUMBER_OF_LINE_DEFINITIONS))
			continue;
		
		int16 scale;
		if (!line.read_indexed("scale", scale, OVERHEAD_MAP_MAXIMUM_SCALE - OVERHEAD_MAP_MINIMUM_SCALE))
			continue;
		
		line.read_attr("width", automap_settings.line_definitions[index].pen_sizes[scale]);
	}

	for (const InfoTree &color : root.children_named("color"))
	{
		int16 index;
		if (!color.read_indexed("index", index, TOTAL_NUMBER_OF_COLORS))
			continue;
		
		if (index < NUMBER_OF_OLD_POLYGON_COLORS)
		{
			color.read_color(automap_settings.polygon_colors[index]);
			continue;
		}
		index -= NUMBER_OF_OLD_POLYGON_COLORS;
		
		if (index < NUMBER_OF_LINE_DEFINITIONS)
		{
			color.read_color(automap_settings.line_definitions[index].color);
			continue;
		}
		index -= NUMBER_OF_LINE_DEFINITIONS;
		
		if (index < NUMBER_OF_THINGS)
		{
			color.read_color(automap_settings.thing_definitions[index].color);
			continue;
		}
		index -= NUMBER_OF_THINGS;
		
		if (index < NUMBER_OF_ANNOTATION_DEFINITIONS)
		{
			color.read_color(automap_settings.annotation_definitions[index].color);
			continue;
		}
		index -= NUMBER_OF_ANNOTATION_DEFINITIONS;
		
		if (index == 0)
		{
			color.read_color(automap_settings.map_name_data.color);
			continue;
		}
		--index;

		if (index == 0)
		{
			color.read_color(automap_settings.path_color);
			continue;
		}
		--index;

		index += NUMBER_OF_OLD_POLYGON_COLORS;
		if (index < NUMBER_OF_POLYGON_COLORS)
		{
			color.read_color(automap_settings.polygon_colors[index]);
			continue;
		}
		index -= NUMBER_OF_POLYGON_COLORS;
	}
	
	for (const InfoTree &font : root.children_named("font"))
	{
		int16 index;
		if (!font.read_indexed("index", index, TOTAL_NUMBER_OF_FONTS)) continue;
		
		bool found = false;
		for (int i = 0; !found && i < NUMBER_OF_ANNOTATION_DEFINITIONS; ++i)
		{
			if (index < NUMBER_OF_ZOOM_LEVELS)
			{
				font.read_font(automap_settings.annotation_definitions[i].Fonts[index]);
				found = true;
			}
			index -= NUMBER_OF_ZOOM_LEVELS;
		}
		if (found)
			continue;
		
		if (index == 0)
		{
			font.read_font(automap_settings.map_name_data.key);
			continue;
		}
		--index;
	}
     */
}
