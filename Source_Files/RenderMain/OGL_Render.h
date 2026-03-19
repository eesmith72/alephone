/*
 OGL_Render.h -- interface OpenGL 3D-rendering code with the rest of the Marathon source code
 by Loren Petrich, March 12, 2000
 
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

#ifndef _OGL_RENDER_
#define _OGL_RENDER_

#include "cseries.h"

#include "OGL_Setup.h"
#include "render.h"


// Is AO currently using OpenGL to render UI/gameworld screen?
bool ogl_is_active();


// set entire screen black
void OGL_ClearScreen();


// Start an OpenGL run (creates a rendering context)
bool OGL_StartRun();

// Stop an OpenGL run (destroys a rendering context)
bool OGL_StopRun();


// Sets the infravision tinting color for a shapes collection, and whether to use such tinting;
// the color values are from 0 to 1.
bool OGL_SetInfravisionTint(short Collection, bool IsTinted, float Red, float Green, float Blue);

// Set OpenGL rendering-window bounds; these are calculated using the following boundary Rects:
// The screen (gotten from its portRect)
// The view (here, the main rendering view)
// Whether to allocate a back buffer
bool OGL_SetWindow(screen_rectangle &ScreenBounds, screen_rectangle &ViewBounds, bool UseBackBuffer);


// Set view parameters; this is for proper perspective rendering
bool OGL_SetView(view_data &View);

// Sets the view to what's suitable for rendering foreground objects like weapons in hand
bool OGL_SetForeground();

// Sets whether a foreground object is horizontally reflected
bool OGL_SetForegroundView(bool HorizReflect);

// Start and end rendering of main view 
bool OGL_StartMain();
bool OGL_EndMain();

// Stuff for doing OpenGL rendering of various objects
bool OGL_RenderWall(polygon_definition& RenderPolygon, bool IsVertical);
bool OGL_RenderSprite(rectangle_definition& RenderRectangle);


bool OGL_RenderCrosshairs();


bool OGL_RenderText(short BaseX, short BaseY, const std::string& Text, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff);
// Render cursor for Lua/chat console
bool OGL_RenderTextCursor(const SDL_Rect& rect, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff);


// Render rectangles (set color beforehand)
void OGL_RenderRect(float x, float y, float w, float h);
void OGL_RenderRect(const SDL_Rect& rect);
void OGL_RenderTexturedRect(float x, float y, float w, float h, float tleft, float ttop, float tright, float tbottom);
void OGL_RenderFrame(float x, float y, float w, float h, float thickness);


// Render lines (for overhead map)
void OGL_RenderLines(const std::vector<world_point2d>& points, float thickness);

OGL_FogData* OGL_GetCurrFogData();

#endif
