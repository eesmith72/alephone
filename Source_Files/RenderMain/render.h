#ifndef __RENDER_H
#define __RENDER_H

/*
RENDER.H -- gameworld renderer

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

Thursday, September 8, 1994 5:40:59 PM  (Jason)

Feb 18, 2000 (Loren Petrich):
	Added "show_weapons_in_hand" to "view_data", so as to have support third-person
	as well as first-person view

Feb 25, 2000 (Loren Petrich):
	Added support for tunnel-vision mode (sniper mode);
	this includes "tunnel_vision_active" in "view_data"

May 22, 2000 (Loren Petrich):
	Moved field-of-view definitions into ViewControl.c;
	using accessors to that file to get field-of-view parameters

Nov 12, 2000 (Loren Petrich):
	Added automap reset function
*/

#include "world.h"	
#include "textures.h"
#include "scottish_textures.h"
// Stuff to control the view
#include "ViewControl.h"

/* ---------- constants */

/* the distance behind which we are not required to draw objects */
#define MINIMUM_OBJECT_DISTANCE ((short)(WORLD_ONE/20))

// LP change: suppressed going/leaving states, because of alternative way of
// adjusting the FOV.
enum /* render effects */
{
	_render_effect_fold_in,
	_render_effect_fold_out,
	// _render_effect_going_fisheye,
	// _render_effect_leaving_fisheye,
	_render_effect_explosion,
	// LP additions:
	// _render_effect_going_tunnel,
	// _render_effect_leaving_tunnel
};

enum /* shading tables */
{
	_shading_normal, /* to black */
	_shading_infravision /* false color */
};

// LP change: using accessors instead
#define TUNNEL_VISION_FIELD_OF_VIEW View_FOV_TunnelVision()
#define NORMAL_FIELD_OF_VIEW View_FOV_Normal()
#define EXTRAVISION_FIELD_OF_VIEW View_FOV_ExtraVision()


/* ---------- structures */

struct definition_header
{
	short tag;
	short clip_left, clip_right;
};


/* ---------- render flags */

#define TEST_STATE_FLAG(i,f) TEST_RENDER_FLAG(i,f)
#define SET_STATE_FLAG(i,f,v) SET_RENDER_FLAG(i,f)

#define TEST_RENDER_FLAG(index, flag) (render_flags[index]&(flag))
#define SET_RENDER_FLAG(index, flag) render_flags[index]|= (flag)

#define RENDER_FLAGS_BUFFER_SIZE MAX(MAX(EndpointList.size(), LineList.size()),MAX(SideList.size(),PolygonList.size()))
//#define RENDER_FLAGS_BUFFER_SIZE (8*KILO)
enum /* render flags */
{
	_polygon_is_visible_bit, /* some part of this polygon is horizontally in the view cone */
	_endpoint_has_been_visited_bit, /* we’ve already tried to cast a ray out at this endpoint */
	_endpoint_is_visible_bit, /* this endpoint is horizontally in the view cone */
	_side_is_visible_bit, /* this side was crossed while building the tree and should be drawn */
	_line_has_clip_data_bit, /* this line has a valid clip entry */
	_endpoint_has_clip_data_bit, /* this endpoint has a valid clip entry */
	_endpoint_has_been_transformed_bit, /* this endpoint has been transformed into screen-space */
	NUMBER_OF_RENDER_FLAGS, // must be <= 16 as the flags' storage is std::vector<uint16_t>

	_polygon_is_visible= 1<<_polygon_is_visible_bit,
	_endpoint_has_been_visited= 1<<_endpoint_has_been_visited_bit,
	_endpoint_is_visible= 1<<_endpoint_is_visible_bit,
	_side_is_visible= 1<<_side_is_visible_bit,
	_line_has_clip_data= 1<<_line_has_clip_data_bit,
	_endpoint_has_clip_data= 1<<_endpoint_has_clip_data_bit,
	_endpoint_has_been_transformed= 1<<_endpoint_has_been_transformed_bit
};

/* ---------- globals */

extern std::vector<uint16> RenderFlagList;
#define render_flags (RenderFlagList.data())

// extern uint16 *render_flags;

/* ---------- prototypes/RENDER.C */

void allocate_render_memory(void);

void initialize_view_data(struct view_data *view, bool ignore_preferences = false);
void render_view(struct view_data *view, struct bitmap_definition *software_render_dest /*ignored under OpenGL*/);

void start_render_effect(struct view_data *view, short effect);

void check_m1_exploration(void);


/* ----------- prototypes/SCREEN.C */
void render_overhead_map(struct view_data *view);
void render_computer_interface(struct view_data *view);

// LP: definitions moved up here because they are referred to outside of render.c, where they are defined.

void instantiate_rectangle_transfer_mode(view_data *view, rectangle_definition *rectangle, short transfer_mode, _fixed transfer_phase);

void instantiate_polygon_transfer_mode(view_data *view, polygon_definition *polygon, short transfer_mode, bool horizontal);


// EES: yuck; In overhead_map.cpp:
void ResetOverheadMap();
void clear_automap();


#endif
