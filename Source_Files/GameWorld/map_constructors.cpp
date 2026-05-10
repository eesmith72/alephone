/*
MAP_CONSTRUCTORS.C

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

#if defined(NEW_AND_BROKEN) || defined(WITH_ORIGINAL_DATA_STRUCTURES)
const bool DoIncorrectCountVWarn = true;
#endif


#include "cseries.hpp"
#include "map.h"
#include "map_wad.h"
#include "flood_map.h"
#include "platforms.h"
#include "Packing.h"

#include "automap_data.hpp"

/*
maps of one polygon don’t have their impassability information computed

//detached polygons (i.e., shadows) and their twins will not have their neighbor polygon lists correctly computed
//adjacent polygons should be precalculated in the polygon structure
//intersecting_flood_proc can’t store side information (using sign) with line index zero
//keep_line_segment_out_of_walls() can’t use precalculated height information and should do weird things next to elevators and doors
*/

/* ---------- structures */

#define MAXIMUM_INTERSECTING_INDEXES 64

struct intersecting_flood_data
{
	// This stuff now global:
	/*
	short *line_indexes;
	short line_count;
	
	short *endpoint_indexes;
	short endpoint_count;
	
	short *polygon_indexes;
	short polygon_count;
	*/
	
	short original_polygon_index;
	world_point2d center;
	
	int32 minimum_separation_squared;
};

/* ---------- globals */
static int32 map_index_buffer_count= 0l; /* Added due to the dynamic nature of maps */

// LP: Temporary areas for nearby endpoint/line/polygon finding;
// OK for this to be global since they replace only single instances.
static std::vector<short> LineIndices(MAXIMUM_INTERSECTING_INDEXES);
static std::vector<short> EndpointIndices(MAXIMUM_INTERSECTING_INDEXES);
static std::vector<short> PolygonIndices(MAXIMUM_INTERSECTING_INDEXES);


/* ---------- private prototypes */

static short calculate_clockwise_endpoints(short polygon_index, short *buffer);
static void calculate_adjacent_polygons(short polygon_index, short *polygon_indexes);
static void calculate_adjacent_sides(short polygon_index, short *side_indexes);
static int32 calculate_polygon_area(short polygon_index);

static void add_map_index(short index, short *count);
static void find_intersecting_endpoints_and_lines(short polygon_index, world_distance minimum_separation);
static int32 intersecting_flood_proc(short source_polygon_index, short line_index,
	short destination_polygon_index, void *data);

static void precalculate_polygon_sound_sources(void);

/* ---------- code */

void recalculate_side_type(short side_index)
{
	side_data *side = get_side_data(side_index);
	short opposite_index = find_adjacent_polygon(side->polygon_index, side->line_index);
	polygon_data *polygon = get_polygon_data(side->polygon_index);
	if (opposite_index != NONE)
	{
		polygon_data *opposite = get_polygon_data(opposite_index);
		int ceiling_height, floor_height, opposite_ceiling_height, opposite_floor_height;
		if (polygon->type == _polygon_is_platform)
		{
			platform_data *platform = get_platform_data(polygon->permutation);
			ceiling_height = platform->maximum_ceiling_height;
			floor_height = platform->minimum_floor_height;
		}
		else
		{
			ceiling_height = polygon->ceiling_height;
			floor_height = polygon->floor_height;
		}

		if (opposite->type == _polygon_is_platform)
		{
			platform_data *platform = get_platform_data(opposite->permutation);
			opposite_ceiling_height = platform->minimum_ceiling_height;
			opposite_floor_height = platform->maximum_floor_height;
		}
		else
		{
			opposite_ceiling_height = opposite->ceiling_height;
			opposite_floor_height = opposite->floor_height;
		}

		if (opposite_ceiling_height < ceiling_height && opposite_floor_height > floor_height)
		{
			side->type = _split_side;
		}
		else if (opposite_floor_height > floor_height)
		{
			side->type = _low_side;
		}
		else if (opposite_ceiling_height < ceiling_height)
		{
			side->type = _high_side;
		}
		else
			side->type = _full_side;
		
	}
	else
	{
		side->type = _full_side;
	}
}

short new_side(short polygon_index, short line_index)
{
	line_data *line = get_line_data(line_index);
	polygon_data *polygon = get_polygon_data(polygon_index);

	assert_fail((line->clockwise_polygon_owner == polygon_index && line->clockwise_polygon_side_index == NONE )|| (line->counterclockwise_polygon_owner == polygon_index && line->counterclockwise_polygon_side_index == NONE), "");

	side_data side;
	obj_clear(side);
	side.primary_texture.texture = UNONE;
	side.secondary_texture.texture = UNONE;
	side.transparent_texture.texture = UNONE;

	short side_index = SideList.size();
	SideList.push_back(side);

	if (line->clockwise_polygon_owner == polygon_index) 
		line->clockwise_polygon_side_index = side_index;
	else
		line->counterclockwise_polygon_side_index = side_index;
	recalculate_redundant_side_data(side_index, line_index);
	calculate_adjacent_sides(polygon_index, polygon->side_indexes);

	recalculate_side_type(side_index);
	return side_index;
}


/* calculates area, clockwise endpoint list, adjacent polygons */
void recalculate_redundant_polygon_data(short polygon_index)
{
	struct polygon_data *polygon= get_polygon_data(polygon_index);

	if (!POLYGON_IS_DETACHED(polygon))
	{
		calculate_clockwise_endpoints(polygon_index, polygon->endpoint_indexes);
		calculate_adjacent_polygons(polygon_index, polygon->adjacent_polygon_indexes);
		polygon->area= calculate_polygon_area(polygon_index);

		find_center_of_polygon(polygon_index, &polygon->center);
		calculate_adjacent_sides(polygon_index, polygon->side_indexes);
	}

	// TEMPORARY UNTIL THE EDITOR SETS THESE FIELDS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	polygon->media_lightsource_index= polygon->floor_lightsource_index;
//	polygon->ambient_sound_image_index= NONE;
//	polygon->random_sound_image_index= NONE;
}

/* calculates solidity, highest adjacent floor and lowest adjacent ceiling; not to be called
	at runtime. */
void recalculate_redundant_endpoint_data(short endpoint_index)
{
	struct endpoint_data *endpoint= get_endpoint_data(endpoint_index);
	world_distance highest_adjacent_floor_height= INT16_MIN;
	world_distance lowest_adjacent_ceiling_height= INT16_MAX;
	short supporting_polygon_index= NONE;
	short line_index;
	bool solid= false;
	bool elevation= false;
	bool transparent= true;
	
    for (int32_t line_index = 0; line_index < LineList.size(); line_index++)
	{
        line_data *line = &LineList[line_index];
		/* does this line contain our endpoint? */
		if (line->endpoint_indexes[0]==endpoint_index||line->endpoint_indexes[1]==endpoint_index)
		{
			short polygon_index;
			struct polygon_data *polygon;

			/* if this line is solid, so is the endpoint */			
			if (LINE_IS_SOLID(line)) solid= true;
			if (!LINE_IS_TRANSPARENT(line)) transparent= false;
			if (LINE_IS_ELEVATION(line)) elevation= true;
			
			/* look at adjacent polygons to determine highest floor and lowest ceiling */
			polygon_index= line->clockwise_polygon_owner;
			if (polygon_index!=NONE)
			{
				polygon= get_polygon_data(polygon_index);
				if (highest_adjacent_floor_height<polygon->floor_height)
                    {
                        highest_adjacent_floor_height= polygon->floor_height;
                        supporting_polygon_index= polygon_index;
                    }
				if (lowest_adjacent_ceiling_height>polygon->ceiling_height) lowest_adjacent_ceiling_height= polygon->ceiling_height;
			}
			polygon_index= line->counterclockwise_polygon_owner;
			if (polygon_index!=NONE)
			{
				polygon= get_polygon_data(polygon_index);
                   if (highest_adjacent_floor_height<polygon->floor_height)
                   {
                       highest_adjacent_floor_height= polygon->floor_height;
                       supporting_polygon_index= polygon_index;
                   }
				if (lowest_adjacent_ceiling_height>polygon->ceiling_height) lowest_adjacent_ceiling_height= polygon->ceiling_height;
			}
		}
	}

	SET_ENDPOINT_SOLIDITY(endpoint, solid);
	SET_ENDPOINT_TRANSPARENCY(endpoint, transparent);
	SET_ENDPOINT_ELEVATION(endpoint, elevation);
	endpoint->highest_adjacent_floor_height= highest_adjacent_floor_height;
	endpoint->lowest_adjacent_ceiling_height= lowest_adjacent_ceiling_height;
	endpoint->supporting_polygon_index= supporting_polygon_index;
}

/* calculates line length, highest adjacent floor and lowest adjacent ceiling and calls
	recalculate_redundant_side_data() on the line’s sides */
void recalculate_redundant_line_data(
	short line_index)
{
	struct line_data *line= get_line_data(line_index);
	struct side_data *clockwise_side= NULL, *counterclockwise_side= NULL;
	bool elevation= false;
	bool landscaped= false;
	bool variable_elevation= false;
	bool transparent_texture= false;
	
	/* recalculate line length */
	line->length= distance2d(&(get_endpoint_data(line->endpoint_indexes[0])->vertex),
		&(get_endpoint_data(line->endpoint_indexes[1])->vertex));

	/* find highest adjacent floor and lowest adjacent ceiling */
	{
		struct polygon_data *polygon1, *polygon2;
		
		polygon1= (line->clockwise_polygon_owner == NONE) ? (struct polygon_data *) NULL : get_polygon_data(line->clockwise_polygon_owner);
		polygon2= (line->counterclockwise_polygon_owner == NONE) ? (struct polygon_data *) NULL : get_polygon_data(line->counterclockwise_polygon_owner);
		
		if ((polygon1&&polygon1->type==_polygon_is_platform) || (polygon2&&polygon2->type==_polygon_is_platform)) variable_elevation= true;
		
		if (polygon1&&polygon2)
		{
			line->highest_adjacent_floor= MAX(polygon1->floor_height, polygon2->floor_height);
			line->lowest_adjacent_ceiling= MIN(polygon1->ceiling_height, polygon2->ceiling_height);
			if (polygon1->floor_height != polygon2->floor_height) elevation= true;
		}
		else
		{
			elevation= true;
			
			if (polygon1)
			{
				line->highest_adjacent_floor= polygon1->floor_height;
				line->lowest_adjacent_ceiling= polygon1->ceiling_height;
			}
			else
			{
				if (polygon2)
				{
					line->highest_adjacent_floor= polygon2->floor_height;
					line->lowest_adjacent_ceiling= polygon2->ceiling_height;
				}
				else
				{
					line->highest_adjacent_floor= line->lowest_adjacent_ceiling= 0;
				}
			}
		}
	}
	
	if (line->clockwise_polygon_side_index!=NONE)
	{
		recalculate_redundant_side_data(line->clockwise_polygon_side_index, line_index);
		clockwise_side= get_side_data(line->clockwise_polygon_side_index);
	}
	if (line->counterclockwise_polygon_side_index!=NONE)
	{
		recalculate_redundant_side_data(line->counterclockwise_polygon_side_index, line_index);
		counterclockwise_side= get_side_data(line->counterclockwise_polygon_side_index);
	}

	if ((clockwise_side&&clockwise_side->primary_transfer_mode==_xfer_landscape) ||
		(counterclockwise_side&&counterclockwise_side->primary_transfer_mode==_xfer_landscape))
	{
		landscaped= true;
	}
	
	if ((clockwise_side && clockwise_side->transparent_texture.texture!=UNONE) ||
		(counterclockwise_side && counterclockwise_side->transparent_texture.texture!=UNONE))
	{
		transparent_texture= true;
	}

	SET_LINE_ELEVATION(line, elevation);
	SET_LINE_VARIABLE_ELEVATION(line, variable_elevation && !LINE_IS_SOLID(line));
	SET_LINE_LANDSCAPE_STATUS(line, landscaped);
	SET_LINE_HAS_TRANSPARENT_SIDE(line, transparent_texture);
}

void recalculate_redundant_side_data(
	short side_index,
	short line_index)
{
	struct side_data *side= get_side_data(side_index);
	struct line_data *line= get_line_data(line_index);
	world_point2d *e0 = NULL , *e1 = NULL;

	// TEMPORARY UNTIL THE EDITOR SETS THESE FIELDS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	side->transparent_texture.texture= NONE; // no transparent texture
//	side->ambient_delta= 0;

	if (line->clockwise_polygon_side_index==side_index)
	{
		e0= &get_endpoint_data(line->endpoint_indexes[0])->vertex;
		e1= &get_endpoint_data(line->endpoint_indexes[1])->vertex;
		side->polygon_index= line->clockwise_polygon_owner;
	}
	else
	{
		assert_fail(side_index==line->counterclockwise_polygon_side_index, "");

		e0= &(get_endpoint_data(line->endpoint_indexes[1])->vertex);
		e1= &(get_endpoint_data(line->endpoint_indexes[0])->vertex);
		side->polygon_index= line->counterclockwise_polygon_owner;
	}

//	if (line_index==98) ao__dprintf__("line sides: %d,%d side_index==%d", line->clockwise_polygon_side_index, line->counterclockwise_polygon_side_index, side_index);
	
	side->exclusion_zone.e0= side->exclusion_zone.e2= *e0;
	side->exclusion_zone.e1= side->exclusion_zone.e3= *e1;
	push_out_line(&side->exclusion_zone.e0, &side->exclusion_zone.e1, MINIMUM_SEPARATION_FROM_WALL, line->length);
	
	side->line_index= line_index;
//	side->direction= arctangent(e0->x - e1->x, e0->y - e1->y);
	
//	if (line_index==98||line_index==64)
//		ao__dprintf__("e0(%d,%d) e1(%d,%d) e2(%d,%d) e3(%d,%d)", impassable_side->e0.x, impassable_side->e0.y,
//		 impassable_side->e1.x, impassable_side->e1.y, impassable_side->e2.x, impassable_side->e2.y,
//		 impassable_side->e3.x, impassable_side->e3.y);
	
	// TEMPORARY UNTIL THE EDITOR SETS THESE FIELDS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	guess_side_lightsource_indexes(side_index);
}

void calculate_endpoint_polygon_owners(short endpoint_index, short *first_index, short *index_count)
{
	*first_index = MapIndexList.size();
	*index_count= 0;

    for (short polygon_index = 0; polygon_index < PolygonList.size(); polygon_index++)
	{
        polygon_data *polygon = &PolygonList[polygon_index];
		for (unsigned short i= 0; i<polygon->vertex_count; ++i)
		{
			if (endpoint_index==polygon->endpoint_indexes[i])
			{
				add_map_index(polygon_index, index_count);
			}
		}
	}
}


void calculate_endpoint_line_owners(short endpoint_index, short *first_index, short *index_count)
{
	*first_index = MapIndexList.size();
	*index_count= 0;
	
    for (short line_index = 0; line_index < LineList.size(); line_index++)
	{
        line_data* line = &LineList[line_index];

		if (line->endpoint_indexes[0] == endpoint_index || line->endpoint_indexes[1] == endpoint_index)
		{
			add_map_index(line_index, index_count);
		}
	}
}


#define CONTINUOUS_SPLIT_SIDE_HEIGHT WORLD_ONE

void guess_side_lightsource_indexes(short side_index)
{
	struct side_data *side= get_side_data(side_index);
	if (side->line_index < 0 ||
		side->line_index >= LineList.size() ||
		side->polygon_index < 0 ||
		side->polygon_index >= PolygonList.size())
	{
		// apparently some M1 net maps have orphan sides
		return;
	}
	
	struct line_data *line= get_line_data(side->line_index);
	struct polygon_data *polygon= get_polygon_data(side->polygon_index);
    
	short ceiling_index = polygon->ceiling_lightsource_index;
	short floor_index = polygon->floor_lightsource_index;
	// change floor lighting if poly is a flooded platform
	if (polygon->type == _polygon_is_platform)
	{
		struct platform_data *platform= get_platform_data(polygon->permutation);
		if (platform && PLATFORM_IS_FLOODED(platform))
		{
			short adj_index = find_flooding_polygon(side->polygon_index);
			if (adj_index != NONE)
			{
				struct polygon_data *adj_polygon= get_polygon_data(adj_index);
				floor_index = adj_polygon->floor_lightsource_index;
			}
		}
	}
	
	switch (side->type)
	{
		case _full_side:
			side->primary_lightsource_index= ceiling_index;
			break;
		case _split_side:
			side->secondary_lightsource_index= (line->lowest_adjacent_ceiling-line->highest_adjacent_floor>CONTINUOUS_SPLIT_SIDE_HEIGHT) ?
				floor_index : ceiling_index;
			/* fall through to high side */
		case _high_side:
			side->primary_lightsource_index= ceiling_index;
			break;
		case _low_side:
			side->primary_lightsource_index= floor_index;
			break;
		
		default:
            throw_ao_exception_f("bad poly side type: %x", 1, side->type);
			break;
	}
	
	side->transparent_lightsource_index= ceiling_index;
}

/* Since the map_index buffer is no longer statically sized. */
void set_map_index_buffer_size(
	int32 length)
{
	map_index_buffer_count= length/sizeof(short);
}

/* ---------- private code */

/* given a polygon, return its endpoints in clockwise order; always returns polygon->vertex_count */
static short calculate_clockwise_endpoints(
	short polygon_index,
	short *buffer)
{
	struct polygon_data *polygon = get_polygon_data(polygon_index);
	
	for (unsigned short i=0;i<polygon->vertex_count;++i)
	{
		*buffer++ = clockwise_endpoint_in_line(polygon_index, polygon->line_indexes[i], 0);
	}
	
	return polygon->vertex_count;
}

static void calculate_adjacent_sides(short polygon_index, short *side_indexes)
{
	struct polygon_data *polygon= get_polygon_data(polygon_index);
	
	for (unsigned short i=0;i<polygon->vertex_count;++i)
	{
		struct line_data *line= get_line_data(polygon->line_indexes[i]);
		short side_index;
		
		if (line->clockwise_polygon_owner==polygon_index)
		{
			side_index= line->clockwise_polygon_side_index;
		}
		else
		{
			// LP change: get around some Pfhorte bugs
			side_index= line->counterclockwise_polygon_side_index;
		}
		
		*side_indexes++= side_index;
	}
}

static void calculate_adjacent_polygons(
	short polygon_index,
	short *polygon_indexes)
{
	struct polygon_data *polygon = get_polygon_data(polygon_index);

     for (unsigned short i=0;i<polygon->vertex_count;++i)
	{
		struct line_data *line= get_line_data(polygon->line_indexes[i]);
		short adjacent_polygon_index= NONE;
		
		if (polygon_index==line->clockwise_polygon_owner)
		{
			adjacent_polygon_index= line->counterclockwise_polygon_owner;
		}
		else
		{
			// LP change: get around some Pfhorte bugs
			adjacent_polygon_index= line->clockwise_polygon_owner;
		}
		
		*polygon_indexes++= adjacent_polygon_index;
	}
}

/* returns area of the given polygon */
static int32 calculate_polygon_area(
	short polygon_index)
{
	int32 area= 0;
	world_point2d *first_point, *point, *next_point;
	struct polygon_data *polygon= get_polygon_data(polygon_index);

	first_point= &(get_endpoint_data(polygon->endpoint_indexes[0])->vertex);	
	for (unsigned short vertex=1;vertex<polygon->vertex_count-1;++vertex)
	{
		point= &(get_endpoint_data(polygon->endpoint_indexes[vertex])->vertex);
		next_point= &(get_endpoint_data(polygon->endpoint_indexes[vertex+1])->vertex);

		area+= (((first_point->x) * (point->y)) - ((point->x) * (first_point->y))) +
			(((point->x) * (next_point->y)) - ((next_point->x) * (point->y))) +
			(((next_point->x) * (first_point->y)) - ((first_point->x) * (next_point->y)));
	}
	
	/* real area is absolute value of calculated area divided by two */
     area= (std::abs(area) >> 1);
	
	return area;
}

/* ---------- precalculate map indexes */

void precalculate_map_indexes()
{
    for (short polygon_index = 0; polygon_index < PolygonList.size(); polygon_index++)
    {
        polygon_data *polygon = &PolygonList[polygon_index];
		if (!POLYGON_IS_DETACHED(polygon)) /* we’ll handle detached polygons during the second pass */
		{
			// short line_indexes[MAXIMUM_INTERSECTING_INDEXES], endpoint_indexes[MAXIMUM_INTERSECTING_INDEXES],
			// 	polygon_indexes[MAXIMUM_INTERSECTING_INDEXES];
			// short line_count, endpoint_count, polygon_count;
	
//			if (polygon_index==17) ao__dprintf__("polygon #%d at %p", polygon_index, polygon);
						
			polygon->first_exclusion_zone_index= MapIndexList.size();
			polygon->line_exclusion_zone_count= polygon->point_exclusion_zone_count= 0;
			find_intersecting_endpoints_and_lines(polygon_index, MINIMUM_SEPARATION_FROM_WALL);
			//	line_indexes, &line_count, endpoint_indexes, &endpoint_count, polygon_indexes,
			//	&polygon_count);
			
			size_t line_count = LineIndices.size();
			size_t endpoint_count = EndpointIndices.size();
			
			for (size_t i=0;i<line_count;++i)	
			{
				add_map_index(LineIndices[i], &polygon->line_exclusion_zone_count);
			}
			
			for (size_t i=0;i<endpoint_count;++i)
			{
				add_map_index(EndpointIndices[i], &polygon->point_exclusion_zone_count);
			}
			
			polygon->first_neighbor_index= MapIndexList.size();
			polygon->neighbor_count= 0;
			find_intersecting_endpoints_and_lines(polygon_index, MINIMUM_SEPARATION_FROM_PROJECTILE);
			//	line_indexes, &line_count, endpoint_indexes, &endpoint_count, polygon_indexes,
			//	&polygon_count);
			
//			if (polygon_index==155) ao__dprintf__("polygon index #%d has %d neighbors:;dm %x %x;", polygon_index, polygon_count, polygon_indexes, sizeof(short)*polygon_count);
			
			size_t polygon_count = PolygonIndices.size();

			for (size_t i=0;i<polygon_count;++i)
			{
				add_map_index(PolygonIndices[i], &polygon->neighbor_count);
			}
		}
	}

	precalculate_polygon_sound_sources();
}

static void find_intersecting_endpoints_and_lines(
	short polygon_index,
	world_distance minimum_separation)
{
	struct intersecting_flood_data data;

	data.original_polygon_index= polygon_index;
	LineIndices.clear();
	EndpointIndices.clear();
	PolygonIndices.clear();

	data.minimum_separation_squared= minimum_separation*minimum_separation;
	find_center_of_polygon(polygon_index, &data.center);
	
	if (film_profile.adjacent_polygons_always_intersect)
	{
		polygon_data* polygon = get_polygon_data(polygon_index);
		for (int i = 0; i < polygon->vertex_count; ++i)
		{
			short adjacent_polygon_index = find_adjacent_polygon(polygon_index, polygon->line_indexes[i]);
			if (adjacent_polygon_index != NONE)
			{
				PolygonIndices.push_back(adjacent_polygon_index);
			}
		}
	}

	polygon_index= flood_map(polygon_index, INT32_MAX, intersecting_flood_proc, _breadth_first, &data);
	while (polygon_index!=NONE)
	{
		polygon_index= flood_map(NONE, INT32_MAX, intersecting_flood_proc, _breadth_first, &data);
	}
}

#ifdef NEW_AND_BROKEN
//AS: this confuses PB, so comment it out for now
/*
static long intersecting_flood_proc(
	short source_polygon_index,
	short line_index,
	short destination_polygon_index,
	void *vdata)
{
	struct intersecting_flood_data *data=vdata;
	struct polygon_data *polygon= get_polygon_data(source_polygon_index);
	struct polygon_data *original_polygon= get_polygon_data(data->original_polygon_index);
	bool keep_searching= false; // don’t flood any deeper unless we find something close enough
	short i = 0, j = 0;
     unsigned long new_broken_note;
	//(void) (line_index,destination_polygon_index);
#pragma unused (line_index,destination_polygon_index)
	// we only care about this polygon if it intersects us in z 
	if ((polygon->floor_height<=original_polygon->ceiling_height)&&(polygon->ceiling_height>=original_polygon->floor_height))
	{
		// check each endpoint to see if it is within the critical distance of any line within our original polygon 
		for (i= 0; i<polygon->vertex_count; ++i)
		{
			short endpoint_index= polygon->endpoint_indexes[i];
			world_point2d *p= &(get_endpoint_data(endpoint_index)->vertex);
			
			for (j= 0; j<original_polygon->vertex_count; ++j)
			{
				short line_index= polygon->line_indexes[i];
				struct line_data *line= get_line_data(line_index);
				
				if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
				{
					keep_searching |= try_and_add_endpoint(endpoint_index);
					keep_searching |= try_and_add_line(polygon->line_indexes[i]);
					keep_searching |= try_and_add_line(polygon->line_indexes[i ? i-1 : polygon->vertex_count-1);
					break;
				}
			}
		}
	}

	// if any part of this polygon is close enough to our original polygon, remember it’s index
	if (keep_searching)
	{
		for (j=0;j<data->polygon_count;++j)
		{
			if (data->polygon_indexes[j]==source_polygon_index)
			{
				break; // found duplicate, ignore
			}
		}
		if (j==data->polygon_count && data->polygon_count<MAXIMUM_INTERSECTING_INDEXES)
		{
			short detached_twin_index= NONE; //find_undetached_polygons_twin(source_polygon_index);
			
			if (DoIncorrectCountVWarn)
               {
                   assert_warn_f(data->polygon_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete neighbor list for polygon#%d", data->original_polygon_index);
               }
			data->polygon_indexes[data->polygon_count++]= source_polygon_index;
			
			// if this polygon has a detached twin, add it too 
			if ((detached_twin_index!=NONE) && (data->polygon_count<MAXIMUM_INTERSECTING_INDEXES))
			{
				if (DoIncorrectCountVWarn)
                    {
                        assert_warn_f(data->polygon_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete neighbor list for polygon#%d", data->original_polygon_index);
                    }
				data->polygon_indexes[data->polygon_count++]= detached_twin_index;
			}
		}
	}

	// return area of source polygon as cost
	return keep_searching ? 1 : -1;
}

void try_and_add_line(
	struct intersecting_flood_data *data,
	short line_index)
{
	struct polygon_data *original_polygon= get_polygon_data(data->original_polygon_index);
	struct line_data *line= get_line_data(line_index);
	bool keep_searching= false;
	short i = 0;
	
	if ((LINE_IS_SOLID(line)) ||
		(line_has_variable_height(line_index)) ||
		(line->lowest_adjacent_ceiling<original_polygon->ceiling_height) ||
     (line->highest_adjacent_floor>original_polygon->floor_height))
	{
		// make sure this line isn’t already in the line list
		for (i=0; i<data->line_count; ++i)
		{
			if (data->line_indexes[i]==line_index)
			{
				keep_searching= true;
				break; // found duplicate, ignore (but keep looking for others) 
			}
		}
		if (i==data->line_count && data->line_count<MAXIMUM_INTERSECTING_INDEXES)
		{
              bool clockwise= !!((((b->x-a->x)*(data->center.y-b->y)) - ((b->y-a->y)*(data->center.x-b->x)))>0);

			if (DoIncorrectCountVWarn)
               {
                   assert_warn_f(data->line_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete line list for polygon#%d", data->original_polygon_index);
               }
			data->line_indexes[data->line_count++]= clockwise ? polygon->line_indexes[i] : (-polygon->line_indexes[i]-1);
//			if (data->original_polygon_index==23) ao__dprintf__("found line %d (%s)", polygon->line_indexes[i], clockwise ? "clockwise" : "counterclockwise");
			keep_searching= true;
			break;
		}
	}
	
	return keep_searching;
}
			// add this endpoint if it isn’t already in the intersecting endpoint list
			for (j=0;j<data->endpoint_count;++j)
			{
				if (data->endpoint_indexes[j]==polygon->endpoint_indexes[i])
				{
					keep_searching= true;
					break; // found duplicate, ignore (but keep looking for others) 
				}
			}
			if (j==data->endpoint_count && data->endpoint_count<MAXIMUM_INTERSECTING_INDEXES)
			{
				world_point2d *p= &get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
				
				// check and see if this endpoint is close enough to any line in our original polygon to care about; if it is, add it to our list 
				for (j=0;j<original_polygon->vertex_count;++j)
				{
					struct line_data *line= get_line_data(original_polygon->line_indexes[j]);
					world_point2d *a= &get_endpoint_data(line->endpoint_indexes[0])->vertex;
					world_point2d *b= &get_endpoint_data(line->endpoint_indexes[1])->vertex;
		
					if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
					{
						if (DoIncorrectCountVWarn)
							assert_warn_f(data->endpoint_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete endpoint list for polygon#%d", data->original_polygon_index);
						data->endpoint_indexes[data->endpoint_count++]= polygon->endpoint_indexes[i];
//						if (data->original_polygon_index==23) ao__dprintf__("found endpoint %d", data->endpoint_indexes[data->endpoint_count-1]);
//						switch (data->endpoint_indexes[data->endpoint_count-1])
//						{
//							case 34:
//							case 35:
//								ao__dprintf__("found endpoint#%d from polygon#%d", data->endpoint_indexes[data->endpoint_count-1], data->original_polygon_index);
//						}
						break;
					}
				}
			}
		}
	}
*/
#endif

static int32 intersecting_flood_proc(
	short source_polygon_index,
	short line_index,
	short destination_polygon_index,
	void *vdata)
{
	struct intersecting_flood_data *data=(struct intersecting_flood_data *)vdata;
	struct polygon_data *polygon= get_polygon_data(source_polygon_index);
	struct polygon_data *original_polygon= get_polygon_data(data->original_polygon_index);
	bool keep_searching= false; /* don’t flood any deeper unless we find something close enough */
	unsigned short i, j;
	(void) (line_index);
	(void) (destination_polygon_index);

	/* we only care about this polygon if it intersects us in z */
	if ((polygon->floor_height<=original_polygon->ceiling_height)&&(polygon->ceiling_height>=original_polygon->floor_height))
	{
		/* update our running line and endpoint lists */	
		for (i=0;i<polygon->vertex_count;++i)
		{
			/* add this line if it isn’t already in the intersecting line list */
			for (j=0;j<LineIndices.size();++j)
			{
				if (LineIndices[j]==polygon->line_indexes[i] ||
					-LineIndices[j]-1==polygon->line_indexes[i])
				{
					keep_searching= true;
					break; /* found duplicate, stop */
				}
			}
			if (j==LineIndices.size())
			{
				short line_index= polygon->line_indexes[i];
				struct line_data *line= get_line_data(line_index);
				
				if (LINE_IS_SOLID(line) ||
					line_has_variable_height(line_index) ||
					line->lowest_adjacent_ceiling<original_polygon->ceiling_height ||
					line->highest_adjacent_floor>original_polygon->floor_height)
				{
					world_point2d *a= &(get_endpoint_data(line->endpoint_indexes[0])->vertex);
					world_point2d *b= &(get_endpoint_data(line->endpoint_indexes[1])->vertex);
		
					/* check and see if this line is close enough to any point in our original polygon
						to care about; if it is, add it to our list */
					for (j=0;j<original_polygon->vertex_count;++j)
					{
						world_point2d *p= &(get_endpoint_data(original_polygon->endpoint_indexes[j])->vertex);
			
						if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
						{
							bool clockwise= !!((((b->x-a->x)*(data->center.y-b->y)) - ((b->y-a->y)*(data->center.x-b->x)))>0);
							
							LineIndices.push_back(clockwise ? polygon->line_indexes[i] : (-polygon->line_indexes[i]-1));
							keep_searching= true;
							break;
						}
					}
				}
			}
			
			/* add this endpoint if it isn’t already in the intersecting endpoint list */
			for (j=0;j<EndpointIndices.size();++j)
			{
				if (EndpointIndices[j]==polygon->endpoint_indexes[i])
				{
					keep_searching= true;
					break; /* found duplicate, ignore (but keep looking for others) */
				}
			}
			if (j==EndpointIndices.size())
			{
				world_point2d *p= &(get_endpoint_data(polygon->endpoint_indexes[i])->vertex);
				
				/* check and see if this endpoint is close enough to any line in our original polygon
					to care about; if it is, add it to our list */
				for (j=0;j<original_polygon->vertex_count;++j)
				{
					struct line_data *line= get_line_data(original_polygon->line_indexes[j]);
					world_point2d *a= &get_endpoint_data(line->endpoint_indexes[0])->vertex;
					world_point2d *b= &get_endpoint_data(line->endpoint_indexes[1])->vertex;
		
					if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
					{
						EndpointIndices.push_back(polygon->endpoint_indexes[i]);
						break;
					}
				}
			}
		}
	}

	/* if any part of this polygon is close enough to our original polygon, remember it’s index */
	if (keep_searching)
	{
		for (j=0;j<PolygonIndices.size();++j)
		{
			if (PolygonIndices[j]==source_polygon_index)
			{
				break; /* found duplicate, ignore */
			}
		}
		if (j==PolygonIndices.size())
		{
			short detached_twin_index= NONE; //find_undetached_polygons_twin(source_polygon_index);
			
			PolygonIndices.push_back(source_polygon_index);
			
			/* if this polygon has a detached twin, add it too */
			if (detached_twin_index!=NONE)
			{
				PolygonIndices.push_back(detached_twin_index);
			}
		}
	}

	/* return area of source polygon as cost */
	return keep_searching ? 1 : -1;
}


#ifdef WITH_ORIGINAL_DATA_STRUCTURES
/*
static long intersecting_flood_proc(
	short source_polygon_index,
	short line_index,
	short destination_polygon_index,
	void *vdata)
{
	struct intersecting_flood_data *data=(struct intersecting_flood_data *)vdata;
	struct polygon_data *polygon= get_polygon_data(source_polygon_index);
	struct polygon_data *original_polygon= get_polygon_data(data->original_polygon_index);
	bool keep_searching= false; // don’t flood any deeper unless we find something close enough
     short i, j;
	(void) (line_index);
	(void) (destination_polygon_index);

	// we only care about this polygon if it intersects us in z 
	if (polygon->floor_height<=original_polygon->ceiling_height&&polygon->ceiling_height>=original_polygon->floor_height)
	{
		// update our running line and endpoint lists 
		for (i=0;i<polygon->vertex_count;++i)
		{
			// add this line if it isn’t already in the intersecting line list
			for (j=0;j<data->line_count;++j)
			{
				if (data->line_indexes[j]==polygon->line_indexes[i] ||
					-data->line_indexes[j]-1==polygon->line_indexes[i])
				{
					keep_searching= true;
					break; // found duplicate, stop 
				}
			}
			if (j==data->line_count && data->endpoint_count<MAXIMUM_INTERSECTING_INDEXES)
			{
				short line_index= polygon->line_indexes[i];
				struct line_data *line= get_line_data(line_index);
				
//				if (data->original_polygon_index==23&&line_index==104) ao__dprintf__("line#%d @ %p", line_index, line);
				
				if (LINE_IS_SOLID(line) ||
					line_has_variable_height(line_index) ||
					line->lowest_adjacent_ceiling<original_polygon->ceiling_height ||
					line->highest_adjacent_floor>original_polygon->floor_height)
				{
					world_point2d *a= &get_endpoint_data(line->endpoint_indexes[0])->vertex;
					world_point2d *b= &get_endpoint_data(line->endpoint_indexes[1])->vertex;
		
					// check and see if this line is close enough to any point in our original polygon to care about; if it is, add it to our list 
					for (j=0;j<original_polygon->vertex_count;++j)
					{
						world_point2d *p= &get_endpoint_data(original_polygon->endpoint_indexes[j])->vertex;
			
						if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
						{
							bool clockwise= ((b->x-a->x)*(data->center.y-b->y) - (b->y-a->y)*(data->center.x-b->x)>0) ? true : false;
							
							if (DoIncorrectCountVWarn)
								assert_warn_f(data->line_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete line list for polygon#%d", data->original_polygon_index);
							data->line_indexes[data->line_count++]= clockwise ? polygon->line_indexes[i] : (-polygon->line_indexes[i]-1);
//							if (data->original_polygon_index==23) ao__dprintf__("found line %d (%s)", polygon->line_indexes[i], clockwise ? "clockwise" : "counterclockwise");
							keep_searching= true;
							break;
						}
					}
				}
			}
			
			// add this endpoint if it isn’t already in the intersecting endpoint list
			for (j=0;j<data->endpoint_count;++j)
			{
				if (data->endpoint_indexes[j]==polygon->endpoint_indexes[i])
				{
					keep_searching= true;
					break; // found duplicate, ignore (but keep looking for others) 
				}
			}
			if (j==data->endpoint_count && data->endpoint_count<MAXIMUM_INTERSECTING_INDEXES)
			{
				world_point2d *p= &get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
				
				// check and see if this endpoint is close enough to any line in our original polygon to care about; if it is, add it to our list 
				for (j=0;j<original_polygon->vertex_count;++j)
				{
					struct line_data *line= get_line_data(original_polygon->line_indexes[j]);
					world_point2d *a= &get_endpoint_data(line->endpoint_indexes[0])->vertex;
					world_point2d *b= &get_endpoint_data(line->endpoint_indexes[1])->vertex;
		
					if (point_to_line_segment_distance_squared(p, a, b)<data->minimum_separation_squared)
					{
						if (DoIncorrectCountVWarn)
							assert_warn_f(data->endpoint_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete endpoint list for polygon#%d", data->original_polygon_index);
						data->endpoint_indexes[data->endpoint_count++]= polygon->endpoint_indexes[i];
//						if (data->original_polygon_index==23) ao__dprintf__("found endpoint %d", data->endpoint_indexes[data->endpoint_count-1]);
//						switch (data->endpoint_indexes[data->endpoint_count-1])
//						{
//							case 34:
//							case 35:
//								ao__dprintf__("found endpoint#%d from polygon#%d", data->endpoint_indexes[data->endpoint_count-1], data->original_polygon_index);
//						}
						break;
					}
				}
			}
		}
	}

	// if any part of this polygon is close enough to our original polygon, remember it’s index
	if (keep_searching)
	{
		for (j=0;j<data->polygon_count;++j)
		{
			if (data->polygon_indexes[j]==source_polygon_index)
			{
				break; // found duplicate, ignore 
			}
		}
		if (j==data->polygon_count && data->polygon_count<MAXIMUM_INTERSECTING_INDEXES)
		{
			short detached_twin_index= NONE; //find_undetached_polygons_twin(source_polygon_index);
			
			if (DoIncorrectCountVWarn)
				assert_warn_f(data->polygon_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete neighbor list for polygon#%d", data->original_polygon_index);
			data->polygon_indexes[data->polygon_count++]= source_polygon_index;
			
			// if this polygon has a detached twin, add it too 
			if (detached_twin_index!=NONE && data->polygon_count<MAXIMUM_INTERSECTING_INDEXES)
			{
				if (DoIncorrectCountVWarn)
					assert_warn_f(data->polygon_count!=MAXIMUM_INTERSECTING_INDEXES-1, "incomplete neighbor list for polygon#%d", data->original_polygon_index);
				data->polygon_indexes[data->polygon_count++]= detached_twin_index;
			}
		}
	}

	// return area of source polygon as cost
	return keep_searching ? 1 : -1;
}
*/
#endif

static void add_map_index(short index, short *count)
{
	assert_fail(MapIndexList.size() < UINT16_MAX, "");
	MapIndexList.push_back(index);
	*count += 1;
}


#define ZERO_VOLUME_DISTANCE (10*WORLD_ONE)

static void precalculate_polygon_sound_sources()
{
	
    for (short polygon_index = 0; polygon_index < PolygonList.size(); polygon_index++)
	{
        polygon_data* polygon = &PolygonList[polygon_index];
        
		short sound_sources = 0;
		
		polygon->sound_source_indexes = MapIndexList.size();
		
        for (short object_index = 0; object_index< SavedObjectList.size(); object_index++)
		{
            map_object *object = &SavedObjectList[object_index];

			if (object->type==_saved_sound_source)
			{
				short i;
				bool close= false;
				
				for (i= 0; i<polygon->vertex_count; ++i)
				{
					struct endpoint_data *endpoint= get_endpoint_data(polygon->endpoint_indexes[i]);
					struct line_data *line= get_line_data(polygon->line_indexes[i]);
					
					if (guess_distance2d((world_point2d *)&object->location, &endpoint->vertex)<ZERO_VOLUME_DISTANCE ||
						point_to_line_segment_distance_squared((world_point2d *)&object->location,
							&get_endpoint_data(line->endpoint_indexes[0])->vertex,
							&get_endpoint_data(line->endpoint_indexes[1])->vertex)<ZERO_VOLUME_DISTANCE)
					{
						close= true;
						break;
					}
				}
				
				if (close) add_map_index(object_index, &sound_sources);
			}
		}
		
		add_map_index(NONE, &sound_sources);
	}
}




uint8 *unpack_endpoint_data(uint8 *Stream, size_t count)
{
    EndpointList.resize(count);
    
	uint8* S = Stream;
     
     for (size_t k = 0; k < count; k++)
	{
        endpoint_data* ObjPtr = &EndpointList[k];
        
		StreamToValue(S,ObjPtr->flags);
		StreamToValue(S,ObjPtr->highest_adjacent_floor_height);
		StreamToValue(S,ObjPtr->lowest_adjacent_ceiling_height);
		
		StreamToValue(S,ObjPtr->vertex.x);
		StreamToValue(S,ObjPtr->vertex.y);
		StreamToValue(S,ObjPtr->transformed.x);
		StreamToValue(S,ObjPtr->transformed.y);
		
		StreamToValue(S,ObjPtr->supporting_polygon_index);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_endpoint_data), "");
	return S;
}


uint8 *pack_endpoint_data(uint8 *Stream, endpoint_data *Objects, size_t Count)
{
	uint8* S = Stream;
	endpoint_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->highest_adjacent_floor_height);
		ValueToStream(S,ObjPtr->lowest_adjacent_ceiling_height);
		
		ValueToStream(S,ObjPtr->vertex.x);
		ValueToStream(S,ObjPtr->vertex.y);
		ValueToStream(S,ObjPtr->transformed.x);
		ValueToStream(S,ObjPtr->transformed.y);
		
		ValueToStream(S,ObjPtr->supporting_polygon_index);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_endpoint_data), "");
	return S;
}


uint8 *unpack_line_data(uint8 *Stream, size_t count)
{
    LineList.resize(count);

	uint8* S = Stream;
	
	for (size_t k = 0; k < count; k++)
	{
        line_data* ObjPtr = &LineList[k];
        
		StreamToList(S,ObjPtr->endpoint_indexes,2);
		StreamToValue(S,ObjPtr->flags);

		StreamToValue(S,ObjPtr->length);
		StreamToValue(S,ObjPtr->highest_adjacent_floor);
		StreamToValue(S,ObjPtr->lowest_adjacent_ceiling);
		
		StreamToValue(S,ObjPtr->clockwise_polygon_side_index);
		StreamToValue(S,ObjPtr->counterclockwise_polygon_side_index);
		
		StreamToValue(S,ObjPtr->clockwise_polygon_owner);
		StreamToValue(S,ObjPtr->counterclockwise_polygon_owner);
		
		S += 6*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_line_data), "");
	return S;
}


uint8 *pack_line_data(uint8 *Stream, line_data *Objects, size_t Count)
{
	uint8* S = Stream;
	line_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ListToStream(S,ObjPtr->endpoint_indexes,2);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->length);
		ValueToStream(S,ObjPtr->highest_adjacent_floor);
		ValueToStream(S,ObjPtr->lowest_adjacent_ceiling);
		
		ValueToStream(S,ObjPtr->clockwise_polygon_side_index);
		ValueToStream(S,ObjPtr->counterclockwise_polygon_side_index);
		
		ValueToStream(S,ObjPtr->clockwise_polygon_owner);
		ValueToStream(S,ObjPtr->counterclockwise_polygon_owner);
		
		S += 6*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_line_data), "");
	return S;
}


// TODO: move these onto automap_visibility_t

void unpack_automap_line_data(uint8 *Stream, size_t count)
{
    assert_fail(player_automap_visibility.lines.size() == count, "should be resized when reading geometry");
    memcpy(player_automap_visibility.lines.data(), Stream, count); // it's a vector<uint8_t> (bitflags) so simple memcpy is safe
}


void unpack_automap_polygon_data(uint8 *Stream, size_t count)
{
    assert_fail(player_automap_visibility.polygons.size() == count, "should be resized when reading geometry");
    memcpy(player_automap_visibility.polygons.data(), Stream, count); // it's a vector<uint8_t> (bitflags) so simple memcpy is safe
}


void unpack_map_index_data(uint8 *Stream, size_t count)
{
    MapIndexList.resize(count);
    StreamToList(Stream, MapIndexList.data(), count);
}



inline void StreamToSideTxtr(uint8* &S, side_texture_definition& Object)
{
	StreamToValue(S,Object.x0);
	StreamToValue(S,Object.y0);
	StreamToValue(S,Object.texture);
}

inline void SideTxtrToStream(uint8* &S, side_texture_definition& Object)
{
	ValueToStream(S,Object.x0);
	ValueToStream(S,Object.y0);
	ValueToStream(S,Object.texture);	
}


void StreamToSideExclZone(uint8* &S, side_exclusion_zone& Object)
{
	StreamToValue(S,Object.e0.x);
	StreamToValue(S,Object.e0.y);
	StreamToValue(S,Object.e1.x);
	StreamToValue(S,Object.e1.y);
	StreamToValue(S,Object.e2.x);
	StreamToValue(S,Object.e2.y);
	StreamToValue(S,Object.e3.x);
	StreamToValue(S,Object.e3.y);
}

void SideExclZoneToStream(uint8* &S, side_exclusion_zone& Object)
{
	ValueToStream(S,Object.e0.x);
	ValueToStream(S,Object.e0.y);
	ValueToStream(S,Object.e1.x);
	ValueToStream(S,Object.e1.y);
	ValueToStream(S,Object.e2.x);
	ValueToStream(S,Object.e2.y);
	ValueToStream(S,Object.e3.x);
	ValueToStream(S,Object.e3.y);
}


void unpack_point_data(uint8 *S, size_t count)
{
    EndpointList.resize(count);
    
    for (size_t k = 0; k < count; k++)
    {
        world_point2d& vertex = EndpointList[k].vertex;
        StreamToValue(S, vertex.x);
        StreamToValue(S, vertex.y);
    }
}


uint8 *unpack_side_data(uint8 *Stream, size_t count, int16_t version)
{
	uint8* S = Stream;
	
    SideList.resize(count);
    
	for (size_t k = 0; k < count; k++)
	{
        side_data* ObjPtr = &SideList[k];
        
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToSideTxtr(S,ObjPtr->primary_texture);
		StreamToSideTxtr(S,ObjPtr->secondary_texture);
		StreamToSideTxtr(S,ObjPtr->transparent_texture);
		
		StreamToSideExclZone(S,ObjPtr->exclusion_zone);
		
		StreamToValue(S,ObjPtr->control_panel_type);
		StreamToValue(S,ObjPtr->control_panel_permutation);
		
		StreamToValue(S,ObjPtr->primary_transfer_mode);
		StreamToValue(S,ObjPtr->secondary_transfer_mode);
		StreamToValue(S,ObjPtr->transparent_transfer_mode);
		
		StreamToValue(S,ObjPtr->polygon_index);
		StreamToValue(S,ObjPtr->line_index);
		
		StreamToValue(S,ObjPtr->primary_lightsource_index);
		StreamToValue(S,ObjPtr->secondary_lightsource_index);
		StreamToValue(S,ObjPtr->transparent_lightsource_index);
		
		StreamToValue(S,ObjPtr->ambient_delta);
		
		S += 1*2;
        
        if (version == M1_MAP_WAD_VERSION)
        {
            // some editors set unused flags; clear them out
            static constexpr int m1_side_flags_mask = 0x0007;
            
            ObjPtr->transparent_texture.texture= UNONE;
            ObjPtr->ambient_delta= 0;
            ObjPtr->flags &= m1_side_flags_mask;
            ObjPtr->flags |= _side_item_is_optional;
        }
        else
        {
            // some editors set unused flags; clear them out
            static constexpr int m2_side_flags_mask = 0x007f;
            ObjPtr->flags &= m2_side_flags_mask;
        }
	}
    
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_side_data), "");
	return S;
}

uint8 *pack_side_data(uint8 *Stream, side_data *Objects, size_t Count)
{
	uint8* S = Stream;
	side_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->flags);
		
		SideTxtrToStream(S,ObjPtr->primary_texture);
		SideTxtrToStream(S,ObjPtr->secondary_texture);
		SideTxtrToStream(S,ObjPtr->transparent_texture);
		
		SideExclZoneToStream(S,ObjPtr->exclusion_zone);
		
		ValueToStream(S,ObjPtr->control_panel_type);
		ValueToStream(S,ObjPtr->control_panel_permutation);
		
		ValueToStream(S,ObjPtr->primary_transfer_mode);
		ValueToStream(S,ObjPtr->secondary_transfer_mode);
		ValueToStream(S,ObjPtr->transparent_transfer_mode);
		
		ValueToStream(S,ObjPtr->polygon_index);
		ValueToStream(S,ObjPtr->line_index);
		
		ValueToStream(S,ObjPtr->primary_lightsource_index);
		ValueToStream(S,ObjPtr->secondary_lightsource_index);
		ValueToStream(S,ObjPtr->transparent_lightsource_index);
		
		ValueToStream(S,ObjPtr->ambient_delta);
		
		S += 1*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_side_data), "");
	return S;
}


uint8 *unpack_polygon_data(uint8 *Stream, size_t Count, int16_t version)
{
	uint8* S = Stream;
    
    PolygonList.resize(Count);
    
	for (size_t k = 0; k < Count; k++)
	{
        polygon_data* ObjPtr = &PolygonList[k];
        
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->flags);
		StreamToValue(S,ObjPtr->permutation);
		
		StreamToValue(S,ObjPtr->vertex_count);
		StreamToList(S,ObjPtr->endpoint_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		StreamToList(S,ObjPtr->line_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		StreamToValue(S,ObjPtr->floor_texture);
		StreamToValue(S,ObjPtr->ceiling_texture);
		StreamToValue(S,ObjPtr->floor_height);
		StreamToValue(S,ObjPtr->ceiling_height);
		StreamToValue(S,ObjPtr->floor_lightsource_index);
		StreamToValue(S,ObjPtr->ceiling_lightsource_index);
		
		StreamToValue(S,ObjPtr->area);
		
		StreamToValue(S,ObjPtr->first_object);
		
		StreamToValue(S,ObjPtr->first_exclusion_zone_index);
		StreamToValue(S,ObjPtr->line_exclusion_zone_count);
		StreamToValue(S,ObjPtr->point_exclusion_zone_count);
		
		StreamToValue(S,ObjPtr->floor_transfer_mode);
		StreamToValue(S,ObjPtr->ceiling_transfer_mode);
		
		StreamToList(S,ObjPtr->adjacent_polygon_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		StreamToValue(S,ObjPtr->first_neighbor_index);
		StreamToValue(S,ObjPtr->neighbor_count);
		
		StreamToValue(S,ObjPtr->center.x);
		StreamToValue(S,ObjPtr->center.y);
		
		StreamToList(S,ObjPtr->side_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		StreamToValue(S,ObjPtr->floor_origin.x);
		StreamToValue(S,ObjPtr->floor_origin.y);
		StreamToValue(S,ObjPtr->ceiling_origin.x);
		StreamToValue(S,ObjPtr->ceiling_origin.y);
		
		StreamToValue(S,ObjPtr->media_index);
		StreamToValue(S,ObjPtr->media_lightsource_index);
		
		StreamToValue(S,ObjPtr->sound_source_indexes);
		
		StreamToValue(S,ObjPtr->ambient_sound_image_index);
		StreamToValue(S,ObjPtr->random_sound_image_index);
		
		S += 1*2;
        
        if (version == M1_MAP_WAD_VERSION)
        {
            ObjPtr->media_index = NONE;
            ObjPtr->floor_origin.x   = ObjPtr->floor_origin.y   = 0;
            ObjPtr->ceiling_origin.x = ObjPtr->ceiling_origin.y = 0;
            
            switch (ObjPtr->type)
            {
                case _polygon_is_hill:
                    ObjPtr->type = _polygon_is_minor_ouch;
                    break;
                case _polygon_is_base:
                    ObjPtr->type = _polygon_is_major_ouch;
                    break;
                case _polygon_is_zone_border:
                    ObjPtr->type = _polygon_is_glue;
                    break;
                case _polygon_is_goal:
                    ObjPtr->type = _polygon_is_glue_trigger;
                    break;
                case _polygon_is_visible_monster_trigger:
                    ObjPtr->type = _polygon_is_superglue;
                    break;
                case _polygon_is_invisible_monster_trigger:
                    ObjPtr->type = _polygon_must_be_explored;
                    break;
                case _polygon_is_dual_monster_trigger:
                    ObjPtr->type = _polygon_is_automatic_exit;
                    break;
            }

            // This is set on some m1 maps, but it's unknown what the flag does. Operating on the assumption that
            // old m1 editors didn't clear out flags, just unset the flag. Otherwise the map will assert out later.
            ObjPtr->flags &= ~POLYGON_IS_DETACHED_BIT;
        }
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_polygon_data), "");
	return S;
}

uint8 *pack_polygon_data(uint8 *Stream, polygon_data *Objects, size_t Count)
{
	uint8* S = Stream;
	polygon_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->permutation);
		
		ValueToStream(S,ObjPtr->vertex_count);
		ListToStream(S,ObjPtr->endpoint_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		ListToStream(S,ObjPtr->line_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		ValueToStream(S,ObjPtr->floor_texture);
		ValueToStream(S,ObjPtr->ceiling_texture);
		ValueToStream(S,ObjPtr->floor_height);
		ValueToStream(S,ObjPtr->ceiling_height);
		ValueToStream(S,ObjPtr->floor_lightsource_index);
		ValueToStream(S,ObjPtr->ceiling_lightsource_index);
		
		ValueToStream(S,ObjPtr->area);
		
		ValueToStream(S,ObjPtr->first_object);
		
		ValueToStream(S,ObjPtr->first_exclusion_zone_index);
		ValueToStream(S,ObjPtr->line_exclusion_zone_count);
		ValueToStream(S,ObjPtr->point_exclusion_zone_count);
		
		ValueToStream(S,ObjPtr->floor_transfer_mode);
		ValueToStream(S,ObjPtr->ceiling_transfer_mode);
		
		ListToStream(S,ObjPtr->adjacent_polygon_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		ValueToStream(S,ObjPtr->first_neighbor_index);
		ValueToStream(S,ObjPtr->neighbor_count);
		
		ValueToStream(S,ObjPtr->center.x);
		ValueToStream(S,ObjPtr->center.y);
		
		ListToStream(S,ObjPtr->side_indexes,MAXIMUM_VERTICES_PER_POLYGON);
		
		ValueToStream(S,ObjPtr->floor_origin.x);
		ValueToStream(S,ObjPtr->floor_origin.y);
		ValueToStream(S,ObjPtr->ceiling_origin.x);
		ValueToStream(S,ObjPtr->ceiling_origin.y);
		
		ValueToStream(S,ObjPtr->media_index);
		ValueToStream(S,ObjPtr->media_lightsource_index);
		
		ValueToStream(S,ObjPtr->sound_source_indexes);
		
		ValueToStream(S,ObjPtr->ambient_sound_image_index);
		ValueToStream(S,ObjPtr->random_sound_image_index);
		
		S += 1*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_polygon_data), "");
	return S;
}


uint8 *unpack_map_annotations(uint8 *Stream, size_t count)
{
	uint8* S = Stream;
    MapAnnotationList.resize(count);
	
	for (size_t k = 0; k < count; k++)
	{
        map_annotation* ObjPtr = &MapAnnotationList.emplace_back();
		StreamToValue(S,ObjPtr->type);
		
		StreamToValue(S,ObjPtr->location.x);
		StreamToValue(S,ObjPtr->location.y);
		StreamToValue(S,ObjPtr->polygon_index);
        
        assert_fail(MAXIMUM_ANNOTATION_TEXT_LENGTH == 64, "");
        read_macroman_string(S, ObjPtr->text, MAXIMUM_ANNOTATION_TEXT_LENGTH);
	}
	return S;
}

uint8 *pack_map_annotation(uint8 *Stream, map_annotation* Objects, size_t Count)
{
	uint8* S = Stream;
	map_annotation* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		
		ValueToStream(S,ObjPtr->location.x);
		ValueToStream(S,ObjPtr->location.y);
		ValueToStream(S,ObjPtr->polygon_index);
        
        write_macroman_string(S, ObjPtr->text, MAXIMUM_ANNOTATION_TEXT_LENGTH);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_map_annotation), "");
	return S;
}


uint8 *unpack_map_objects(uint8 *Stream, size_t count, int version)
{
	uint8* S = Stream;
    
    SavedObjectList.resize(count);

	for (size_t k = 0; k < count; k++)
	{
        map_object* ObjPtr = &SavedObjectList[k];
        
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->index);
		StreamToValue(S,ObjPtr->facing);
		StreamToValue(S,ObjPtr->polygon_index);
		StreamToValue(S,ObjPtr->location.x);
		StreamToValue(S,ObjPtr->location.y);
		if (version == M1_MAP_WAD_VERSION &&
			film_profile.m1_object_unused)
		{
		    ObjPtr->location.z = 0;
		    ObjPtr->flags = 0;
		    S += 2*2; // short unused[2]
		}
		else
		{
		    StreamToValue(S,ObjPtr->location.z);
		    StreamToValue(S,ObjPtr->flags);
		}
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_map_object), "");
	return S;
}

uint8 *pack_map_object(uint8 *Stream, map_object* Objects, size_t Count)
{
	uint8* S = Stream;
	map_object* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->index);
		ValueToStream(S,ObjPtr->facing);
		ValueToStream(S,ObjPtr->polygon_index);
		ValueToStream(S,ObjPtr->location.x);
		ValueToStream(S,ObjPtr->location.y);
		ValueToStream(S,ObjPtr->location.z);
		
		ValueToStream(S,ObjPtr->flags);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_map_object), "");
	return S;
}


uint8 *unpack_object_frequency_definition(uint8 *Stream, object_frequency_definition* Objects, size_t Count) // Objects = items_/monsters_placement_info array
{
	uint8* S = Stream;
	object_frequency_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->initial_count);
		StreamToValue(S,ObjPtr->minimum_count);
		StreamToValue(S,ObjPtr->maximum_count);
		
		StreamToValue(S,ObjPtr->random_count);
		StreamToValue(S,ObjPtr->random_chance);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_object_frequency_definition), "");
	return S;
}


uint8 *pack_object_frequency_definition(uint8 *Stream, object_frequency_definition* Objects, size_t Count)
{
	uint8* S = Stream;
	object_frequency_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->initial_count);
		ValueToStream(S,ObjPtr->minimum_count);
		ValueToStream(S,ObjPtr->maximum_count);
		
		ValueToStream(S,ObjPtr->random_count);
		ValueToStream(S,ObjPtr->random_chance);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_object_frequency_definition), "");
	return S;
}



uint8 *unpack_ambient_sound_image_data(uint8 *Stream, size_t count)
{
	uint8* S = Stream;
    
    AmbientSoundImageList.resize(count);
    
	for (size_t k = 0; k < count; k++)
	{
        ambient_sound_image_data& ObjPtr = AmbientSoundImageList[k];

		StreamToValue(S, ObjPtr.flags);
		StreamToValue(S, ObjPtr.sound_index);
		StreamToValue(S, ObjPtr.volume);
		
		S += 5*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_ambient_sound_image_data), "");
	return S;
}


uint8 *pack_ambient_sound_image_data(uint8 *Stream, ambient_sound_image_data* Objects, size_t Count)
{
	uint8* S = Stream;
	ambient_sound_image_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, k++)
	{
		ValueToStream(S,ObjPtr->flags);
    
		ValueToStream(S,ObjPtr->sound_index);
		ValueToStream(S,ObjPtr->volume);
		
		S += 5*2;
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_ambient_sound_image_data), "");
	return S;
}


uint8 *unpack_random_sound_image_data(uint8 *Stream, size_t count)
{
	uint8* S = Stream;
    
    RandomSoundImageList.resize(count);
    
	for (size_t k = 0; k < count; k++)
	{
        random_sound_image_data& ObjPtr = RandomSoundImageList[k];
        
		StreamToValue(S,ObjPtr.flags);
		StreamToValue(S,ObjPtr.sound_index);

		StreamToValue(S,ObjPtr.volume);
		StreamToValue(S,ObjPtr.delta_volume);
		StreamToValue(S,ObjPtr.period);
		StreamToValue(S,ObjPtr.delta_period);
		StreamToValue(S,ObjPtr.direction);
		StreamToValue(S,ObjPtr.delta_direction);
		StreamToValue(S,ObjPtr.pitch);
		StreamToValue(S,ObjPtr.delta_pitch);
		
		StreamToValue(S,ObjPtr.phase);
		
		S += 3*2;
	}
	
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_random_sound_image_data), "");
	return S;
}

uint8 *pack_random_sound_image_data(uint8 *Stream, random_sound_image_data* Objects, size_t Count)
{
	uint8* S = Stream;
	random_sound_image_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->sound_index);

		ValueToStream(S,ObjPtr->volume);
		ValueToStream(S,ObjPtr->delta_volume);
		ValueToStream(S,ObjPtr->period);
		ValueToStream(S,ObjPtr->delta_period);
		ValueToStream(S,ObjPtr->direction);
		ValueToStream(S,ObjPtr->delta_direction);
		ValueToStream(S,ObjPtr->pitch);
		ValueToStream(S,ObjPtr->delta_pitch);
		
		ValueToStream(S,ObjPtr->phase);
		
		S += 3*2;
	}
	
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_random_sound_image_data), "");
	return S;
}


uint8 *unpack_object_data(uint8 *Stream, size_t count)
{
    if (count > get_objects_limit())
    {
        throw_out_of_bounds_f("Number of map objects %zu > limit %u", count, get_objects_limit());
    }
	uint8* S = Stream;
    
    ObjectList.resize(count);
    
	for (size_t k = 0; k < count; k++)
	{
        object_data* ObjPtr = &ObjectList[k];
        
		StreamToValue(S,ObjPtr->location.x);
		StreamToValue(S,ObjPtr->location.y);
		StreamToValue(S,ObjPtr->location.z);
		StreamToValue(S,ObjPtr->polygon);
		
		StreamToValue(S,ObjPtr->facing);
		
		StreamToValue(S,ObjPtr->shape);
		
		StreamToValue(S,ObjPtr->sequence);
		StreamToValue(S,ObjPtr->flags);
		StreamToValue(S,ObjPtr->transfer_mode);
		StreamToValue(S,ObjPtr->transfer_period);
		StreamToValue(S,ObjPtr->transfer_phase);
		StreamToValue(S,ObjPtr->permutation);
		
		StreamToValue(S,ObjPtr->next_object);
		StreamToValue(S,ObjPtr->parasitic_object);
		
		StreamToValue(S,ObjPtr->sound_pitch);
	}
	
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(count*SIZEOF_object_data), "");
	return S;
}

uint8 *pack_object_data(uint8 *Stream, object_data* Objects, size_t Count)
{
	uint8* S = Stream;
	object_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->location.x);
		ValueToStream(S,ObjPtr->location.y);
		ValueToStream(S,ObjPtr->location.z);
		ValueToStream(S,ObjPtr->polygon);
		
		ValueToStream(S,ObjPtr->facing);
		
		ValueToStream(S,ObjPtr->shape);
		
		ValueToStream(S,ObjPtr->sequence);
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->transfer_mode);
		ValueToStream(S,ObjPtr->transfer_period);
		ValueToStream(S,ObjPtr->transfer_phase);
		ValueToStream(S,ObjPtr->permutation);
		
		ValueToStream(S,ObjPtr->next_object);
		ValueToStream(S,ObjPtr->parasitic_object);
		
		ValueToStream(S,ObjPtr->sound_pitch);
	}
	
    assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_object_data), "");
	return S;
}


uint8 *unpack_damage_definition(uint8 *Stream, damage_definition* Objects, size_t Count)
{
	uint8* S = Stream;
	damage_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->base);
		StreamToValue(S,ObjPtr->random);
		StreamToValue(S,ObjPtr->scale);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_damage_definition), "");
	return S;
}

uint8 *pack_damage_definition(uint8 *Stream, damage_definition* Objects, size_t Count)
{
	uint8* S = Stream;
	damage_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->base);
		ValueToStream(S,ObjPtr->random);
		ValueToStream(S,ObjPtr->scale);
	}
	
	assert_fail((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_damage_definition), "");
	return S;
}
