/*
 Rasterizer.h -- abstract base class for Classic (SW) and Modern (OGL) Rasterizers
 by Loren Petrich, August 7, 2000
 
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

#ifndef _RASTERIZER_CLASS_
#define _RASTERIZER_CLASS_


#include "render.h"
#include "OGL_Render.h"



class Rasterizer
{
public:
    
    virtual void configure(const SDL_Point& size, int32_t bit_depth) = 0;
    
    // Render 3D scene
    
    virtual void Begin(camera_settings_t* View) { view = View; }
    
	virtual void End() {}
	
    virtual void texture_horizontal_polygon(polygon_definition& textured_polygon) = 0;

    virtual void texture_vertical_polygon(polygon_definition& textured_polygon) = 0;

    virtual void texture_rectangle(rectangle_definition& textured_rectangle) = 0;
    
    camera_settings_t* get_view() { return view; }
    
protected:
    
    // Pointers to stuff used in scottish_textures:
    camera_settings_t *view; // TODO: const this
};


#endif
