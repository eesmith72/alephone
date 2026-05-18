/*
RENDER.C

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

#include "map.h"
#include "render.h"
#include "interface.hpp"
#include "lightsource.h"
#include "media.h"
#include "weapons.h"
#include "player.h"

#include "dynamic_limits.h"
#include "AnimatedTextures.h"


#include "automap.hpp"


#include "RenderVisTree.h"
#include "RenderSortPoly.h"
#include "RenderPlaceObjs.h"
#include "Renderer.h"
#include "ClassicRenderer.h"

#include "ClassicRasterizer.h"

#include "OGL_Render.h"
#include "OGLRenderer.h"
#include "OGLRasterizer.h"

#include "preferences.hpp"
#include "Screen.hpp"


/*
//render transparent walls (if a bit is set or if the transparent texture is non-NULL?)
//use side lightsources instead of taking them from their polygons
//respect dark side bit (darken light intensity by k)
//fix solid/opaque endpoint confusion (solidity does not imply opacity)

there exists a problem where an object can overlap into a polygon which is clipped by something
	behind the object but that will clip the object because clip windows are subtractive; how
	is this solved?
it’s still possible to get ambiguous clip flags, usually in very narrow (e.g., 1 pixel) windows
the renderer has a maximum range beyond which it shits bricks yet which it allows to be exceeded
it’s still possible, especially in high-res full-screen, for points to end up (slightly) off
	the screen (usually discarding these has no noticable effect on the scene)
whitespace results when two adjacent polygons are clipped to different vertical windows.  this
	is not trivially solved with the current implementation, and may be acceptable (?)

//build_base_polygon_index_list() should discard lower polygons for objects above the viewer and
//	higher polygons for objects below the viewer because we certainly don’t sort objects
//	correctly in these cases
//in strange cases, objects are sorted out of order.  this seems to involve players in some way
//	(i.e., parasitic objects).
*/



std::vector<uint16_t> RenderFlagList; // it might look private to this module but the get_render_flag macro which queries it is used all over (the set_render_flag is also used in a couple of places); ofc it'd be nice to know wtf it actually does...


// TODO: rename these too
static RenderVisTreeClass RenderVisTree;			// Visibility-tree object
static RenderSortPolyClass RenderSortPoly;			// Polygon-sorting object
static RenderPlaceObjsClass RenderPlaceObjs;		// Object-placement object


// the 3D worldview renderers (HUD, automap, etc will be separately rendered and composited in 2D)

static ClassicRenderer classic_renderer;
static OGLRenderer ogl_renderer;

static Renderer* active_renderer = nullptr; // one of the above, or nullptr when UI is active


// In Marathon 1-style exploration missions, we check each player's view for exploration polygons after this many ticks have elapsed
static const int TICKS_PER_EXPLORE = 4;

// M1 exploration mission helpers
static camera_settings_t explore_view;
static RenderVisTreeClass explore_tree;



// TODO: camera effects should move onto camera_settings_t struct

static void update_camera(camera_settings_t* view);


//-----------------------------------------------------------------------------
// configure Render3D/ for the loaded level


void allocate_render_memory(size_t endpoint_count, size_t line_count, size_t polygon_count)
{
    assert_fail(endpoint_count > 0 && line_count > 0 && polygon_count > 0, "");
    
    // TODO: not seeing how LineList and PolygonList could be larger than EndpointList?
	RenderFlagList.resize(MAX(MAX(endpoint_count, line_count), polygon_count));
    
    /* TODO: even by LP's bad standards this gem is 101% worthy of TheDailyWTF
	// LP addition: check out pointer-arithmetic hack
	assert(sizeof(void *) == sizeof(POINTER_DATA));
	*/
    
	RenderVisTree.Resize(endpoint_count, line_count);
	RenderSortPoly.Resize(polygon_count);
	
    
	// Reset to have the tree correctly resized if m1 exploration level
	explore_tree.view = nullptr;
	RenderSortPoly.RVPtr = &RenderVisTree;
	RenderPlaceObjs.RVPtr = &RenderVisTree;
	RenderPlaceObjs.RSPtr = &RenderSortPoly;
    
	classic_renderer.RSPtr = ogl_renderer.RSPtr = &RenderSortPoly;
}


//-----------------------------------------------------------------------------



bool classic_renderer_is_active()
{
    return active_renderer == &classic_renderer;
}


bool modern_renderer_is_active()
{
    return active_renderer == &ogl_renderer;
}


void load_gameworld_renderer(const SDL_Point& size, int32_t bit_depth)
{
    if (bit_depth == 32) // Modern
    {
        if (!modern_renderer_is_active())
        {
            unload_gameworld_renderer();
            active_renderer = &ogl_renderer;
        }
    }
    else // Classic
    {
        if (!classic_renderer_is_active())
        {
            unload_gameworld_renderer();
            active_renderer = &classic_renderer;
        }
    }
    // note: starting OGLRenderer may be quite slow ATM due to the lousy way Shapes and Shapes patches are loaded, managed, and activated/deactivated; that will improve once Shapes is overhauled
    
    // TODO: best to separate `activate`, `reconfigure`, `deactivate` from `initialize` and `shutdown` as we only want to unload textures when we're completely done, not when switching modes in-game. Mind you, we want to load as much as possible on a background thread while user is on splash screens/preferences/main menu. So we might load Shapes textures all the way into GPU.
    
    // TODO: FIX: shutting down and then reinitializing the OGL renderer while in-game (via F1+F2 mode keys) causes a crash as textures have been freed by OGL_StopTextures // it's not crashing ATM, may have bodged it
    
    active_renderer->initialize(size, bit_depth); // TODO: when changing size/bit_depth but keeping the existing renderer, instead of sending `initialize` send `reconfigure`; this'll save the OGLRenderer doing a full reinitialization
    
    main_camera_settings.initialize_for_game_view(size);
}


void unload_gameworld_renderer()
{
    if (active_renderer) { active_renderer->shutdown(); }
    active_renderer = nullptr;
}



//-----------------------------------------------------------------------------


void render_gameworld_view(camera_settings_t *view) // TODO: view should be const (assuming nothing modifies it here; if anything does, that should be documented/relocated)
{
    assert_fail(active_renderer, "");
    
    // camera view (origin, origin_polygon_index, yaw, pitch, roll, etc.) has probably changed since last call
	update_camera(view);
    
    // EES: there is something smelly about having a bunch of Render classes, yet a global list of bitflags; pseudo-encapsulation
    std::fill(RenderFlagList.begin(), RenderFlagList.end(), 0);
    
    // build the render tree, regardless of map mode, so the automap updates while active
    RenderVisTree.build_render_tree(view);
    
    // sort the render tree (so we have a depth-ordering of polygons) and accumulate clipping information for each polygon
    RenderSortPoly.sort_render_tree(view);
    
    // build the render object list by looking at the sorted render tree
    RenderPlaceObjs.build_render_object_list(view);
        
    // Start rendering main view
    active_renderer->Begin(view);
    
    // render the object list, back to front, doing clipping on each surface before passing it to the texture-mapping code
    active_renderer->render_tree();
    
    // Finish rendering main view
    active_renderer->End();
}




void check_m1_exploration(void)
{
	// Are we even on an exploration mission?
	if (!(static_world.mission_flags & _mission_exploration_m1)) return;

	// Are there still polygons to explore?
	bool need_exploring = false;
	for (const auto& polygon : PolygonList)
    {
		if (polygon.type == _polygon_must_be_explored)
		{
			need_exploring = true;
			break;
		}
	}
	if (!need_exploring) return;

	// All right, we need to do something.
	// First, make sure our data is set up.
	if (!explore_tree.view)
	{
		// We only need to initialize once, since nothing that we use changes.
        explore_view.initialize_for_m1_exploration();

		explore_tree.view = &explore_view;
		explore_tree.add_to_automap = false;
		explore_tree.mark_as_explored = true;
		explore_tree.Resize(EndpointList.size(), LineList.size()); // TODO: get these counts from RenderVisTree, which is local
	}

	// Check the relevant players' views for exploration polygons.
	// We check every TICKS_PER_EXLORE ticks, staggered by index.
	for (int i = (dynamic_world.tick_count % TICKS_PER_EXPLORE);
	     i < get_number_of_players();
	     i += TICKS_PER_EXPLORE)
	{
		Player* explore_player = &players[i];
		explore_view.yaw = explore_player->facing;
		explore_view.pitch = explore_player->elevation;
		explore_view.origin = explore_player->camera_location;
		explore_view.origin_polygon_index = explore_player->camera_polygon_index;

		update_camera(&explore_view);
		
		std::vector<uint16_t> saved_render_flags{RenderFlagList};
        //clear_render_flags();
        std::fill(RenderFlagList.begin(), RenderFlagList.end(), 0);
        
        // build_render_tree actually marks the polygons
		explore_tree.build_render_tree(&explore_view);

		RenderFlagList = std::move(saved_render_flags);
	}
}


/* ---------- private code */

static void update_camera(camera_settings_t* view) // TODO: move to camera.cpp
{
	// LP change: doing all the FOV changes here:
    view->update_fov();
	
	if (view->effect==NONE)
	{
		view->world_to_screen_x= view->real_world_to_screen_x;
		view->world_to_screen_y= view->real_world_to_screen_y;
	}
	else
	{
		view->update_effect();
	}
	
	/* calculate world_to_screen_y*tan(pitch) */
	view->dtanpitch= (view->world_to_screen_y*sine_table[view->pitch])/cosine_table[view->pitch];

	/* calculate left cone vector */
    angle theta= NORMALIZE_ANGLE(view->yaw-view->half_cone);
    view->left_edge.i= cosine_table[theta];
    view->left_edge.j= sine_table[theta];
	
	/* calculate right cone vector */
	theta= NORMALIZE_ANGLE(view->yaw+view->half_cone);
    view->right_edge.i= cosine_table[theta]; view->right_edge.j= sine_table[theta];
	
	/* if we’re sitting on one of the endpoints in our origin polygon, move us back slightly (±1) into
		that polygon.  when we split rays we’re assuming that we’ll never pass through a given
		vertex in different directions (because if we do the tree becomes a graph) but when
		we start on a vertex this can happen.  this is a destructive modification of the origin. */
    polygon_data *polygon = get_polygon_data(view->origin_polygon_index);
    
    for (int32_t i = 0; i < polygon->vertex_count; i++)
    {
        world_point2d *vertex= &get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
        
        if (vertex->x == view->origin.x && vertex->y == view->origin.y)
        {
            world_point2d *ccw_vertex= &get_endpoint_data(polygon->endpoint_indexes[WRAP_LOW(i, polygon->vertex_count-1)])->vertex;
            world_point2d *cw_vertex= &get_endpoint_data(polygon->endpoint_indexes[WRAP_HIGH(i, polygon->vertex_count-1)])->vertex;
            world_vector2d inset_vector;
            
            inset_vector.i= (ccw_vertex->x-vertex->x) + (cw_vertex->x-vertex->x);
            inset_vector.j= (ccw_vertex->y-vertex->y) + (cw_vertex->y-vertex->y);
            
            if (inset_vector.i == 0 && inset_vector.j == 0)
            {
                // This happens when the CW and CCW vertices are equidistant from and collinear with the origin;
                // we switch tactics and just move directly toward one of them
                inset_vector.i = cw_vertex->x - vertex->x;
                inset_vector.j = cw_vertex->y - vertex->y;
            }
            
            view->origin.x+= SGN(inset_vector.i);
            view->origin.y+= SGN(inset_vector.j);
            
            break;
        }
    }
    
    // Also check adjacent polygons' vertices in case a degenerate polygon has a vertex under us (on a side)
    {
        // Local index of the first side that connects to such a polygon, or else NONE
        // (if non-NONE, we're on this side or one collinear with it)
        const int side_to_poly_with_vertex_on_origin = [&]() -> int
        {
            for (int i = 0; i < polygon->vertex_count; ++i)
            {
                const int16 adj_poly_index = polygon->adjacent_polygon_indexes[i];
                if (adj_poly_index != NONE)
                {
                    const auto& adj_poly = *get_polygon_data(adj_poly_index);
                    for (int k = 0; k < adj_poly.vertex_count; ++k)
                    {
                        const auto v = get_endpoint_data(adj_poly.endpoint_indexes[k])->vertex;
                        if (v.x == view->origin.x && v.y == view->origin.y)
                            return i;
                    }
                }
            }
            return NONE;
        }();
        
        if (side_to_poly_with_vertex_on_origin != NONE)
        {
            // Scoot inward or along the side we're on (we're not on a corner because we handled that case already)
            const int vertex0_index = side_to_poly_with_vertex_on_origin;
            const int vertex1_index = WRAP_HIGH(side_to_poly_with_vertex_on_origin, polygon->vertex_count - 1);
            const world_distance vertex0_x = get_endpoint_data(polygon->endpoint_indexes[vertex0_index])->vertex.x;
            const world_distance vertex1_x = get_endpoint_data(polygon->endpoint_indexes[vertex1_index])->vertex.x;
            view->origin.y += (vertex1_x - vertex0_x >= 0) ? 1 : -1;
        }
    }
    
    /* determine whether we are under or over the media boundary of our polygon; we will see all
        other media boundaries from this orientation (above or below) or fail to draw them. */
    if (polygon->media_index==NONE)
    {
        view->under_media_boundary= false;
    }
    else
    {
        media_data *media= get_media_data(polygon->media_index);
        if (media)
        {
            view->under_media_boundary= UNDER_MEDIA(media, view->origin.z);
            view->under_media_index= polygon->media_index;
        } else {
            view->under_media_boundary= false;
        }
    }
}


/* ---------- transfer modes */

/* given a transfer mode and phase, cause whatever changes it should cause to a rectangle_definition
	structure */
void instantiate_rectangle_transfer_mode(camera_settings_t *view, billboard_t *rectangle, short transfer_mode, ao_fixed transfer_phase)
{
	// For the 3D-model code
	rectangle->HorizScale = 1;
	
	switch (transfer_mode)
	{
		case _xfer_invisibility:
		case _xfer_subtle_invisibility:
			if (view->shading_mode!=_shading_infravision)
			{
				rectangle->transfer_mode= _tinted_transfer;
				rectangle->shading_tables= get_global_shading_table();
				rectangle->transfer_data= (transfer_mode==_xfer_invisibility) ? 0x000f : 0x0018;
				break;
			}
			/* if we have infravision, fall through to _textured_transfer (i see you...) */
		case _xfer_normal:
			rectangle->transfer_mode= _textured_transfer;
			break;
		
		case _xfer_static:
		case _xfer_50percent_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= (transfer_mode==_xfer_static) ? 0x0000 : 0x8000;
			break;

		case _xfer_fade_out_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= transfer_phase;
			break;
			
		case _xfer_pulsating_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= 0x8000+((0x6000*sine_table[FIXED_INTEGERAL_PART(transfer_phase*NUMBER_OF_ANGLES)])>>TRIG_SHIFT);
			break;

		case _xfer_fold_in:
			transfer_phase= FIXED_ONE-transfer_phase; /* do everything backwards */
		case _xfer_fold_out:
			if (teleporting_uses_static_effect())
			{
				// Corrected the teleport shrinkage so that the sprite/object
				// shrinks to its object position and not to its sprite center
				short delta0 = FIXED_INTEGERAL_PART(int32(((1LL*rectangle->xc - rectangle->x0) - 1) * transfer_phase));
				short delta1 = FIXED_INTEGERAL_PART(int32(((1LL*rectangle->x1 - rectangle->xc) - 1) * transfer_phase));
				// short delta= FIXED_INTEGERAL_PART((((rectangle->x1-rectangle->x0)>>1)-1)*transfer_phase);
					
				rectangle->transfer_mode= _static_transfer;
				rectangle->transfer_data= (transfer_phase>>1);
				rectangle->x0+= delta0;
				rectangle->x1-= delta1;
				rectangle->HorizScale = 1 - float(transfer_phase)/float(FIXED_ONE);
			}
			else
				rectangle->transfer_mode= _textured_transfer;
			break;

#if 0		
		case _xfer_fade_out_to_black:
			rectangle->shading_tables= get_global_shading_table();
			if (transfer_phase<FIXED_ONE_HALF)
			{
				/* fade to black */
				rectangle->ambient_shade= (rectangle->ambient_shade*(transfer_phase-FIXED_ONE_HALF))>>(FIXED_FRACTIONAL_BITS-1);
				rectangle->transfer_mode= _textured_transfer;
			}
			else
			{
				/* vanish */
				rectangle->transfer_mode= _tinted_transfer;
				rectangle->transfer_data= 0x1f - ((0x1f*(FIXED_ONE_HALF-transfer_phase))>>(FIXED_FRACTIONAL_BITS-1));
			}
			break;
#endif
		
		// LP change: made an unrecognized mode act like normal
		default:
			rectangle->transfer_mode= _textured_transfer;
			break;
	}
}

/* given a transfer mode and phase, cause whatever changes it should cause to a polygon_definition
	structure (unfortunately we need to know whether this is a horizontal or vertical polygon) */
void instantiate_polygon_transfer_mode(
	camera_settings_t* view,
	struct polygon_definition *polygon,
	short transfer_mode,
	bool horizontal)
{
	world_distance x0, y0;
	world_distance vector_magnitude;
	short alternate_transfer_phase;
	short transfer_phase = view->effect_tick_count;

	polygon->transfer_mode= _textured_transfer;
	switch (transfer_mode)
	{
		case _xfer_fast_horizontal_slide:
		case _xfer_horizontal_slide:
		case _xfer_vertical_slide:
		case _xfer_fast_vertical_slide:
		case _xfer_wander:
		case _xfer_fast_wander:
		case _xfer_reverse_horizontal_slide:
		case _xfer_reverse_fast_horizontal_slide:
		case _xfer_reverse_vertical_slide:
		case _xfer_reverse_fast_vertical_slide:
			x0= y0= 0;
			switch (transfer_mode)
			{
				case _xfer_fast_horizontal_slide: transfer_phase<<= 1;
				case _xfer_horizontal_slide: x0= (transfer_phase<<2)&(WORLD_ONE-1); break;
				
				case _xfer_fast_vertical_slide: transfer_phase<<= 1;
				case _xfer_vertical_slide: y0= (transfer_phase<<2)&(WORLD_ONE-1); break;
					
				case _xfer_reverse_fast_horizontal_slide: transfer_phase<<= 1;
				case _xfer_reverse_horizontal_slide: x0 = WORLD_ONE - (transfer_phase<<2)&(WORLD_ONE-1); break;
					
		        case _xfer_reverse_fast_vertical_slide: transfer_phase<<= 1;
				case _xfer_reverse_vertical_slide: y0 = WORLD_ONE - (transfer_phase<<2)&(WORLD_ONE-1); break;
					
				case _xfer_fast_wander: transfer_phase<<= 1;
				case _xfer_wander:
					alternate_transfer_phase= transfer_phase%(10*FULL_CIRCLE);
					transfer_phase= transfer_phase%(6*FULL_CIRCLE);
					x0= (cosine_table[NORMALIZE_ANGLE(alternate_transfer_phase)] +
						(cosine_table[NORMALIZE_ANGLE(2*alternate_transfer_phase)]>>1) +
						(cosine_table[NORMALIZE_ANGLE(5*alternate_transfer_phase)]>>1))>>(WORLD_FRACTIONAL_BITS-TRIG_SHIFT+2);
					y0= (sine_table[NORMALIZE_ANGLE(transfer_phase)] +
						(sine_table[NORMALIZE_ANGLE(2*transfer_phase)]>>1) +
						(sine_table[NORMALIZE_ANGLE(3*transfer_phase)]>>1))>>(WORLD_FRACTIONAL_BITS-TRIG_SHIFT+2);
					break;
			}
			if (horizontal)
			{
				polygon->origin.x+= x0;
				polygon->origin.y+= y0;
			}
			else
			{
				vector_magnitude= isqrt(polygon->vector.i*polygon->vector.i + polygon->vector.j*polygon->vector.j);
				polygon->origin.x+= (polygon->vector.i*x0)/vector_magnitude;
				polygon->origin.y+= (polygon->vector.j*x0)/vector_magnitude;
				polygon->origin.z-= y0;
			}
			break;
		
		case _xfer_pulsate:
		case _xfer_wobble:
		case _xfer_fast_wobble:
			if (transfer_mode==_xfer_fast_wobble) transfer_phase*= 15;
			transfer_phase&= WORLD_ONE/16-1;
			transfer_phase= (transfer_phase>=WORLD_ONE/32) ? (WORLD_ONE/32+WORLD_ONE/64 - transfer_phase) : (transfer_phase - WORLD_ONE/64);
			if (horizontal)
			{
				polygon->origin.z+= transfer_phase;
			}
			else
			{
				if (transfer_mode==_xfer_pulsate) /* translate .origin perpendicular to .vector */
				{
					world_vector2d offset;
					world_distance vector_magnitude= isqrt(polygon->vector.i*polygon->vector.i + polygon->vector.j*polygon->vector.j);
	
					offset.i= (polygon->vector.j*transfer_phase)/vector_magnitude;
					offset.j= (polygon->vector.i*transfer_phase)/vector_magnitude;
	
					polygon->origin.x+= offset.i;
					polygon->origin.y+= offset.j;
				}
				else /* ==_xfer_wobble, wobble .vector */
				{
					polygon->vector.i+= transfer_phase;
					polygon->vector.j+= transfer_phase;
				}
			}
			break;

		case _xfer_normal:
			break;
		
		case _xfer_smear:
			polygon->transfer_mode= _solid_transfer;
			break;
			
		case _xfer_static:
			polygon->transfer_mode= _static_transfer;
			polygon->transfer_data= 0x0000;
			break;
		
		case _xfer_landscape:
			polygon->transfer_mode= _big_landscaped_transfer;
			break;
//		case _xfer_big_landscape:
//			polygon->transfer_mode= _big_landscaped_transfer;
//			break;
			
		default:
			// LP change: made an unrecognized mode act like normal
			break;
	}
}

