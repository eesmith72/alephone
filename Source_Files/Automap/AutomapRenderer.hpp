/*
 AutomapRenderer.hpp -- code from automap.c and overhead_map_macintosh.c
 by Loren Petrich, August 3, 2000
 
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


#ifndef _OVERHEAD_MAP_CLASS_
#define _OVERHEAD_MAP_CLASS_



// TODO: for Modern, the player arrow should always to point upwards and the map rotates around the arrow as the player turns (this should reduce user motion-sickness when the player moves forward but the translucent map moves sideways)

// TODO: future Modern enhancement: in heavily layered maps it'd be useful for player's local polys to have high opacity while polys on other 'levels' are increasingly faint (would need to use flood algorithm to find the local polys and apply constant opacity to them, while non-local polys calculate opacity based on height delta of their floor to the floor of the player's current poly)

#include "cseries.hpp"

#include "world.h"
#include "map.h"
#include "monsters.h"
#include "shapes.h"
#include "screen_drawing.h" // get_player_color // TODO: finish relocating colors, rects
#include "fonts.hpp"
//#include "screen_drawing.h"

#include "automap_data.hpp"

#include "Canvas_OGL.hpp"



class AutomapRenderer
{
public:
    
    AutomapRenderer() : map_type(automap_type_t::player_in_game),
                        visibility(&player_automap_visibility),
                        appearance(&player_automap_appearance) {}
    
    virtual ~AutomapRenderer() {}
    
    
    void configure(automap_type_t map_type, automap_visibility_t* visibility, const automap_appearance_t* appearance)
    {
        this->map_type = map_type;
        this->visibility = visibility;
        this->appearance = appearance;
    }
        
    void render(const SDL_Rect& view_rect, const world_point2d& origin, short origin_polygon_index, short scale);
    
    
private:
    
    SDL_Rect view_rect;

    automap_visibility_t* visibility;
    const automap_appearance_t* appearance;
    automap_type_t map_type;
    
    world_point2d origin;
    short origin_polygon_index;
    short scale;

    
    Canvas_OGL canvas;
    
    // TODO: move the AutomapLines/Polygons from map_wad to here
    // For the false automap
    
    
    // TODO: what this actually does: generate the visibility for a terminal checkpoint map
    void flood_fill_starting_at_polygon(short polygon_index);
    
    void replace_real_automap(void);
    

    void transform_endpoints_for_overhead_map();
    
    
	// Auxiliary functions to be done inline; these are overloads of the corresponding graphics-API-specific virtual functions defined earlier.
	void draw_polygon(short vertex_count, short *vertices, short color, short scale)
    {
        if (!(color >= 0 && color < NUMBER_OF_POLYGON_COLORS)) return;
        canvas.draw_polygon(vertex_count, vertices, appearance->polygon_colors[color]);
    }
    
    
	void draw_line(short line_index, short color, short scale)
    {
        if (!(color >= 0 && color < NUMBER_OF_LINE_DEFINITIONS)) return;
        const automap_line_style_t& LineDef = appearance->line_definitions[color];
        canvas.draw_line(get_line_data(line_index)->endpoint_indexes, LineDef.color, LineDef.pen_sizes[scale - OVERHEAD_MAP_MINIMUM_SCALE]);
    }
    
    
	void draw_thing(world_point2d* center, angle facing, short color, short scale)
    {
        if (!(color >= 0 && color < NUMBER_OF_THINGS)) return;
        const automap_shape_style_t& ThingDef = appearance->thing_definitions[color];
        if (ThingDef.shape == _circle_thing)
        {
            canvas.draw_circle(*center, ThingDef.color, ThingDef.radii[scale - OVERHEAD_MAP_MINIMUM_SCALE]);
        }
        else
        {
            canvas.draw_square(*center, ThingDef.color, ThingDef.radii[scale - OVERHEAD_MAP_MINIMUM_SCALE]);
        }
    }
    
    
	void draw_player(world_point2d* center, angle facing, short color, short scale)
    {
        SDL_Color PlayerColor = get_player_color(color);
        
        // Changed to use only one entity shape
        const automap_player_style_t& EntityDef = appearance->player_entity;
        //canvas.draw_triangle(*center, facing, PlayerColor, OVERHEAD_MAP_MAXIMUM_SCALE - scale, EntityDef.front, EntityDef.rear, EntityDef.rear_theta); // TODO: FIX
    }
    
    
	void draw_annotation(world_point2d* location, short color, const std::string& text, short scale)
	{
		if (!(color >= 0 && color < NUMBER_OF_ANNOTATION_DEFINITIONS)) return;
		if (!(scale >= OVERHEAD_MAP_MINIMUM_SCALE && scale <= OVERHEAD_MAP_MAXIMUM_SCALE)) return;
		const automap_label_style_t& NoteDef = appearance->annotation_definitions[color];
		//canvas..draw_text(*location, NoteDef.color, text, NoteDef.Fonts[scale - OVERHEAD_MAP_MINIMUM_SCALE], _justify_left);  // TODO: FIX
	}
    
    
	void draw_map_name(const std::string& name)
	{
		const automap_title_style_t& map_name_data = appearance->map_name_data;
		world_point2d location;
        location.x = view_rect.x + (view_rect.w / 2);
        location.y = view_rect.y + appearance->map_name_data.offset_down;
		//canvas..draw_text(location, map_name_data.color, name, map_name_data.Font, _justify_center);  // TODO: FIX
	}
    
};


#endif
