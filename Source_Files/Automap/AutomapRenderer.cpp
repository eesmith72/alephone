/*
 AutomapRenderer.cpp -- code from automap.c and overhead_map_macintosh.c
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

#include "AutomapRenderer.hpp"

#include "flood_map.h"
#include "media.h"
#include "platforms.h"
#include "player.h"
#include "render.h"




enum /* render flags */
{
	_endpoint_on_automap = 0x2000,
	_line_on_automap     = 0x4000,
	_polygon_on_automap  = 0x8000,
};

/* ---------- macros */

#define WORLD_TO_SCREEN_SCALE_ONE 8
#define WORLD_TO_SCREEN(x, x0, scale) (((x)-(x0))>>(WORLD_TO_SCREEN_SCALE_ONE-(scale)))


// Externals:
// Changed to link properly with code in pathfinding.c

// TODO: rename these so it's clear these are pathfinding paths
extern world_point2d* path_peek(short path_index, short *step_count);

extern short GetNumberOfPaths();





void render_overhead_map() // TODO: redo; there are 3 maps needed: player, terminal checkpoint, quicksave thumbnail; the player and quicksave maps share visibility state while the terminal map shows all polys around the checkpoint except those flagged as secret (it applies a flood fill to visibility which, if I understand it, means only the current "floor" shows when there are overlapping polys that aren't closely connected)
{
    
#ifdef AUTOMAP_DEBUG
         clear_automap();
#endif
    //    ResetOverheadMap();

    
  //  SDL_Rect MapRect = main_screen.virtual_automap_rect();
  //  main_screen.set_virtual_drawing_rect(MapRect); // drawing is relative to MapRect's origin
  //  OGL_SetWindow(MapRect);

    //SDL_FillRect(Map_Buffer, NULL, SDL_MapRGB(Map_Buffer->format, 0, 0, 0));

    //overhead_map_data overhead_data;
    //overhead_data.configure_for_player_map();

    //_set_port_to_map();
    //render_overhead_map(&overhead_data);
    //_restore_port();
}




void AutomapRenderer::render(const SDL_Rect& view_rect, const world_point2d& origin, short origin_polygon_index, short scale)
{
    this->view_rect = view_rect;
    this->origin = origin;
    this->origin_polygon_index = origin_polygon_index;
    this->scale = scale;
    
	world_distance x0 = origin.x, y0 = origin.y;
    int32_t xoff = view_rect.x + (view_rect.w / 2), yoff = view_rect.y + (view_rect.h / 2);
	world_point2d location;
	
	canvas.begin_overall();
		
	// LP addition: stuff for setting the game options, since they get defaulted to 0 // TODO: whyyyy, sort it out
	// Made compatible with map cheat
	assert_fail(appearance, "");
	if (appearance->ShowAliens) GET_GAME_OPTIONS() |= _overhead_map_shows_monsters;
	if (appearance->ShowItems) GET_GAME_OPTIONS() |= _overhead_map_shows_items;
	if (appearance->ShowProjectiles) GET_GAME_OPTIONS() |= _overhead_map_shows_projectiles;
	
	if (map_type == automap_type_t::terminal_checkpoint)
    {
        flood_fill_starting_at_polygon(origin_polygon_index);
    }
    
	transform_endpoints_for_overhead_map();
	
	// shade all visible polygons
    canvas.begin_polygons();
	for (int32_t i = 0; i < PolygonList.size(); i++)
	{
		struct polygon_data *polygon= get_polygon_data(i);
		if (player_automap_visibility.has_polygon(i) && get_render_flag(i, _polygon_on_automap)
			&&(polygon->floor_transfer_mode!=_xfer_landscape||polygon->ceiling_transfer_mode!=_xfer_landscape))
		{
			
			if (!POLYGON_IS_DETACHED(polygon))
			{
				short color;
				
				switch (polygon->type)
				{
					case _polygon_is_platform:
						color= PLATFORM_IS_SECRET(get_platform_data(polygon->permutation)) ?
							_polygon_color : _polygon_platform_color;
						if (PLATFORM_IS_FLOODED(get_platform_data(polygon->permutation)))
						{
							short adj_index = find_flooding_polygon(i);
							if (adj_index != NONE)
							{
								switch (get_polygon_data(adj_index)->type)
								{
									case _polygon_is_minor_ouch:
										color = _polygon_minor_ouch_color;
										break;
									case _polygon_is_major_ouch:
										color = _polygon_major_ouch_color;
										break;
								}
							}
						}
						break;
					
					case _polygon_is_minor_ouch:
						color = _polygon_minor_ouch_color;
						break;
					
					case _polygon_is_major_ouch:
						color = _polygon_major_ouch_color;
						break;
                        
					case _polygon_is_teleporter:
						color = _polygon_teleporter_color;
						break;
                        
				case _polygon_is_hill:
					color = _polygon_hill_color;
					break;
					
					default:
						color= _polygon_color;
						break;
				}

				if (polygon->media_index!=NONE)
				{
					struct media_data *media= get_media_data(polygon->media_index);
					
					// LP change: idiot-proofing
					if (media)
					{
						if (media->height>=polygon->floor_height)
						{
							switch (media->type)
							{
								case _media_water: color= _polygon_water_color; break;
								case _media_lava: color= _polygon_lava_color; break;
								case _media_goo: color= _polygon_goo_color; break;
								// LP change: separated sewage and JjaroGoo
								case _media_sewage: color= _polygon_sewage_color; break;
								case _media_jjaro: color = _polygon_jjaro_color; break;
							}
						}
					}
				}
				
				draw_polygon(polygon->vertex_count, polygon->endpoint_indexes, color, scale);
			}
		}
	}
    canvas.end_polygons();

	// draw all visible lines
    canvas.begin_lines();
	for (int32_t i=0; i < LineList.size(); i++)
	{
		short line_color= NONE;
		struct line_data *line= get_line_data(i);
		
		if (player_automap_visibility.has_line(i))
		{
			if ((line->clockwise_polygon_owner!=NONE && get_render_flag(line->clockwise_polygon_owner, _polygon_on_automap)) ||
				(line->counterclockwise_polygon_owner!=NONE && get_render_flag(line->counterclockwise_polygon_owner, _polygon_on_automap)))
			{
				struct polygon_data *clockwise_polygon= line->clockwise_polygon_owner==NONE ? NULL : get_polygon_data(line->clockwise_polygon_owner);
				struct polygon_data *counterclockwise_polygon= line->counterclockwise_polygon_owner==NONE ? NULL : get_polygon_data(line->counterclockwise_polygon_owner);

				if (LINE_IS_SOLID(line) || LINE_IS_VARIABLE_ELEVATION(line))
				{
					if (LINE_IS_LANDSCAPED(line))
					{
						if ((!clockwise_polygon||clockwise_polygon->floor_transfer_mode!=_xfer_landscape) &&
							(!counterclockwise_polygon||counterclockwise_polygon->floor_transfer_mode!=_xfer_landscape))
						{
							line_color= _elevation_line_color;
						}
					}
					else
					{
						line_color= _solid_line_color;
					}
				}
				else
				{
					if (clockwise_polygon->floor_height!=counterclockwise_polygon->floor_height)
					{
						line_color= LINE_IS_LANDSCAPED(line) ? NONE : static_cast<short>(_elevation_line_color);
					}
				}
			}
			
			if (line_color!=NONE) draw_line(i, line_color, scale);
		}
	}
    canvas.end_lines();
	
	// print all visible tags
	if (scale != OVERHEAD_MAP_MINIMUM_SCALE)
	{
		struct map_annotation *annotation;
		
		int16_t i = 0;
		while ((annotation= get_next_map_annotation(&i)))
		{
			if (player_automap_visibility.has_polygon(annotation->polygon_index) &&
                get_render_flag(annotation->polygon_index, _polygon_on_automap))
			{
				location.x= xoff + WORLD_TO_SCREEN(annotation->location.x, x0, scale);
				location.y= yoff + WORLD_TO_SCREEN(annotation->location.y, y0, scale);
				
                draw_annotation(&location, annotation->type, annotation->text.c_str(), scale);
			}
		}
	}

	if (appearance->ShowPaths)
	{
        canvas.begin_path(appearance->path_color);
		
		for (short path_index = 0; path_index < GetNumberOfPaths(); path_index++)
		{
			world_point2d *points;
			short step, count;
			
			points= path_peek(path_index, &count);
			if (points)
			{
				for (step= 0; step<count; ++step)
				{
					location.x= xoff + WORLD_TO_SCREEN(points[step].x, x0, scale);
					location.y= yoff + WORLD_TO_SCREEN(points[step].y, y0, scale);
                    canvas.draw_path_point(step,location);
				}
			}
            canvas.end_path();
		}
	}
	
    if (map_type != automap_type_t::terminal_checkpoint)
	{
        for (int32_t i = 0; i < ObjectList.size(); i++)
		{
            object_data* object = &ObjectList[i];
            
			if (SLOT_IS_USED(object))
			{
				if (!OBJECT_IS_INVISIBLE(object))
				{
					short thing_type= NONE;
					
					switch (GET_OBJECT_OWNER(object))
					{
						case _object_is_monster:
						{
							struct monster_data *monster= get_monster_data(object->permutation);
							
							if (MONSTER_IS_PLAYER(monster))
							{
								Player* player= get_player_data(monster_index_to_player_index(object->permutation));
	
								if ((GET_GAME_OPTIONS()&_overhead_map_is_omniscient) || local_player->team==player->team)
								{
									location.x= xoff + WORLD_TO_SCREEN(object->location.x, x0, scale);
									location.y= yoff + WORLD_TO_SCREEN(object->location.y, y0, scale);
									
									draw_player(&location, object->facing, player->team, scale);
								}
							}
							else
							{
								switch (appearance->monster_displays[monster->type])
								{
									case _civilian_thing:
										thing_type= _civilian_thing;
										break;
									
									case _monster_thing:
										if (GET_GAME_OPTIONS()&_overhead_map_shows_monsters)
											thing_type= _monster_thing;
										break;
								}
							}
							break;
						}
	
						case _object_is_projectile:
							if ((GET_GAME_OPTIONS()&_overhead_map_shows_projectiles) && object->shape!=UNONE)
							{
								thing_type= _projectile_thing;
							}
							break;
						
						case _object_is_item:
							if (GET_GAME_OPTIONS()&_overhead_map_shows_items)
							{
								thing_type= _item_thing;
							}
							break;
							
						case _object_is_garbage:
							// LP change: making this more general
							switch (appearance->dead_monster_displays[GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(object->shape))])
							{
							case _civilian_thing:
								thing_type= _civilian_thing;
								break;
							
							case _monster_thing:
								if (GET_GAME_OPTIONS()&_overhead_map_shows_monsters)
									thing_type= _monster_thing;
								break;
							}
							/*
							if (GET_COLLECTION_INDEX(GET_DESCRIPTOR_COLLECTION(object->shape))==_collection_civilian)
							{
								thing_type= _civilian_thing;
							}
							*/
							break;
					}
					
					if (thing_type != NONE)
					{
						// Making this more general, in case we want to see monsters and stuff
						if (thing_type ==_projectile_thing || ((dynamic_world.tick_count + i) & 8))
						{
							location.x = xoff + WORLD_TO_SCREEN(object->location.x, x0, scale);
							location.y = yoff + WORLD_TO_SCREEN(object->location.y, y0, scale);
							
							draw_thing(&location, object->facing, thing_type, scale);
						}
					}
				}
			}
		}
	}
	else
	{
        for (auto& saved_object : SavedObjectList)
		{
			if (saved_object.type == _saved_goal && saved_object.location.x == origin.x && saved_object.location.y == origin.y)
			{
				location.x = xoff + WORLD_TO_SCREEN(saved_object.location.x, x0, scale);
				location.y = yoff + WORLD_TO_SCREEN(saved_object.location.y, y0, scale);
				draw_thing(&location, 0, _checkpoint_thing, scale);
			}
		}
	}
    
    if (map_type == automap_type_t::player_in_game) { draw_map_name(static_world.level_name); }
    
    canvas.end_overall();
}


void AutomapRenderer::transform_endpoints_for_overhead_map()
{
	world_distance x0 = origin.x, y0 = origin.y;
	int32_t xoff = view_rect.x + (view_rect.w / 2), yoff = view_rect.y + (view_rect.h / 2);
    
	// transform all our endpoints into screen space, remembering which ones are visible
	for (short i=0;i<EndpointList.size();++i)
	{
		struct endpoint_data *endpoint= get_endpoint_data(i);
		
		endpoint->transformed.x= xoff + WORLD_TO_SCREEN(endpoint->vertex.x, x0, scale);
		endpoint->transformed.y= yoff + WORLD_TO_SCREEN(endpoint->vertex.y, y0, scale);

		if (endpoint->transformed.x >= view_rect.x && endpoint->transformed.y >= view_rect.y &&
            endpoint->transformed.y <= view_rect.y + view_rect.h && endpoint->transformed.x <= view_rect.x + view_rect.w)
		{
            set_render_flag(i, _endpoint_on_automap);
		}
	}

	// sweep the polygon array, determining which polygons are visible based on their endpoints
	for (short i=0;i<PolygonList.size();++i)
	{
		struct polygon_data *polygon= get_polygon_data(i);
		short j;
		
		for (j=0;j<polygon->vertex_count;++j)
		{
			if (get_render_flag(polygon->endpoint_indexes[j], _endpoint_on_automap))
			{
                set_render_flag(i, _polygon_on_automap);
				break;
			}
		}
	}
}


// terminal checkpoint maps


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


void AutomapRenderer::flood_fill_starting_at_polygon(short polygon_index)
{
    visibility->clear_all();
    visibility->fill_polygon(polygon_index);
    
    polygon_index = flood_map(polygon_index, INT32_MAX, automap_flood_fill_cost_proc, _breadth_first, visibility);
    do
    {
        polygon_index = flood_map(NONE, INT32_MAX, automap_flood_fill_cost_proc, _breadth_first, visibility);
    }
    while (polygon_index != NONE);
}



