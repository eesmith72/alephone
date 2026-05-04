/*
 Canvas_OGL.hpp -- general-purpose 2D drawing // TODO: merge the various bits of 2D drawing functionality into one coherent class
 
 This repurposes OverheadMap_OGL.h/.cpp code by Loren Petrich, August 3, 2000.
 
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

#ifndef _OVERHEAD_MAP_OPENGL_CLASS_
#define _OVERHEAD_MAP_OPENGL_CLASS_


#include "cseries.hpp"

#include "fonts.hpp"





enum // TODO:
{
    _justify_left,
    _justify_center
};




class Canvas_OGL
{
public:
    
	void begin_overall();
	void end_overall();
	
	void begin_polygons();
	void draw_polygon(short vertex_count, const short* vertices, const rgb_color& color); // TODO: use `std::vector<short> vertices` (unless we're populating the vertices one at a time in which case `draw_polygon_point`); or maybe take any iterable?
	void end_polygons();
	
	void begin_lines();
	void draw_line(const short* vertices, const rgb_color& color, short pen_size); // ditto
	void end_lines();
    
    void draw_square(const world_point2d& center, const rgb_color& color, short radius);

    void draw_circle(const world_point2d& center, const rgb_color& color, short radius);
	
	void draw_triangle(const world_point2d& center, angle facing, const rgb_color& color,
                       short shrink, short front, short rear, short rear_theta);
	
	// Text justification: 0=left, 1=center // TODO: enum?
	void draw_text(const world_point2d& location, const rgb_color& color, const std::string& text, const font_t* FontData, short justify);
	
    // TODO: how does this compare to draw_line? do we need one/other/both?
	void begin_path(const rgb_color& color);
    void draw_path_point(short step, const world_point2d& location); // step 0 = first point -- TODO: if step always starts at 0 and increments with each point plotted, set it to 0 in begin_path and increment automatically in draw_
	void end_path();
	
private:
    
    void DrawCachedPolygons();
    
    void DrawCachedLines();
    
	// Cached polygons and their color
    std::vector<unsigned short> PolygonCache;
	rgb_color SavedColor;

	// Cached polygon lines and their width
    std::vector<world_point2d> LineCache;
	short SavedPenSize;
	
	// Cached lines For drawing monster paths
    std::vector<world_point2d> PathPoints;

};

#endif
