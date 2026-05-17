/*
 RENDER.H -- render the gameworld view, and other stuff which we are still disentangling
 
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

#ifndef __RENDER_H
#define __RENDER_H


#include "world.h"	
#include "textures.h"
#include "scottish_textures.h"
#include "camera.hpp"



// the distance behind which we are not required to draw objects
#define MINIMUM_OBJECT_DISTANCE ((short)(WORLD_ONE / 20))




struct definition_header
{
	short tag;
	short clip_left, clip_right;
};




enum /* render bitflags */
{
	_polygon_is_visible_bit, /* some part of this polygon is horizontally in the view cone */
	_endpoint_has_been_visited_bit, /* we’ve already tried to cast a ray out at this endpoint */
	_endpoint_is_visible_bit, /* this endpoint is horizontally in the view cone */
	_side_is_visible_bit, /* this side was crossed while building the tree and should be drawn */
	_line_has_clip_data_bit, /* this line has a valid clip entry */
	_endpoint_has_clip_data_bit, /* this endpoint has a valid clip entry */
	_endpoint_has_been_transformed_bit, /* this endpoint has been transformed into screen-space */
	NUMBER_OF_RENDER_FLAGS, // this MUST be <= 16 as the flags' storage is std::vector<uint16_t>

	_polygon_is_visible= 1<<_polygon_is_visible_bit,
	_endpoint_has_been_visited= 1<<_endpoint_has_been_visited_bit,
	_endpoint_is_visible= 1<<_endpoint_is_visible_bit,
	_side_is_visible= 1<<_side_is_visible_bit,
	_line_has_clip_data= 1<<_line_has_clip_data_bit,
	_endpoint_has_clip_data= 1<<_endpoint_has_clip_data_bit,
	_endpoint_has_been_transformed= 1<<_endpoint_has_been_transformed_bit
};

extern std::vector<uint16_t> RenderFlagList;

#define get_render_flag(index, flag)  (RenderFlagList[index] & (flag))
#define set_render_flag(index, flag)  (RenderFlagList[index] |= (flag))


// called by initialize_level_from_wad_data in map_wad
void allocate_render_memory(size_t endpoint_count, size_t line_count, size_t polygon_count);


//-----------------------------------------------------------------------------
// load and unload Classic/Modern 3D renderer as appropriate

// these 2 are called by start_/stop_gameworld_renderer; don't call them directly
void load_gameworld_renderer(const SDL_Point& size, int32_t bit_depth);
void unload_gameworld_renderer();

//
bool classic_renderer_is_active();
bool modern_renderer_is_active();


//-----------------------------------------------------------------------------

void render_gameworld_view(camera_settings_t* view);


void check_m1_exploration(void);



void instantiate_rectangle_transfer_mode(camera_settings_t *view, billboard_t *rectangle, short transfer_mode, ao_fixed transfer_phase);

void instantiate_polygon_transfer_mode(camera_settings_t *view, polygon_definition *polygon, short transfer_mode, bool horizontal);


#endif
