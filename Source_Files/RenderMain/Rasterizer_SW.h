/*

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
	
	Rasterizer software impementation (plugs into scottish_textures files)
	by Loren Petrich,
	August 7, 2000
	
*/

#ifndef _RASTERIZER_SOFTWARE_CLASS_
#define _RASTERIZER_SOFTWARE_CLASS_

#include "Rasterizer.h"


class Rasterizer_SW_Class: public RasterizerClass
{
public:

	// Pointers to stuff used in scottish_textures:
	camera_settings_t *view;
    
	// Calling this one "screen" for scottish_textures convenience:
	bitmap_definition *screen;

	// Sets the rasterizer's view data; be sure to call it before doing any rendering
	void SetView(camera_settings_t& View) {view = &View;}
	
	// Rendering calls
	// These are defined in scottish_textures.c (too great a name to change) // EES: why? if scottish_textures.cpp is SW renderer only, move this class into it; if not, move its methods to Rasterizer_SW.cpp
	
    // draw ceiling/floor poly
	void texture_horizontal_polygon(polygon_definition& textured_polygon);
	
    // draw wall (side) poly
	void texture_vertical_polygon(polygon_definition& textured_polygon);
	
    // draw sprite (monsters, items; not sure about WIH)
	void texture_rectangle(rectangle_definition& textured_rectangle);
};


#endif
