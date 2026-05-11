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

#include "cseries.hpp"

#include "OGL_Setup.h"
#include "render.h"

#include "ModelRenderer.h"


// TODO: Gradually getting OGL ready for replacement. Ideally, the 2D drawing functions will move into Canvas_OGL and the 3D drawing functions into the 3D OGL class[es]; what's left is presumably general setup and teardown that should be in its own file. Once there's a nice clean well-defined API separating the UI + game engine from renderers, work on replacing OGL with SDL_gpu can begin.



// these 3 have relocated to OGLRenderer.cpp
void SetupShaders();
extern ModelRenderer ModelRenderObject;
void PreloadTextures();


// Set OpenGL rendering bounds
void OGL_SetWindow(SDL_Rect& rect);

// Set view parameters; this is for proper perspective rendering
bool OGL_SetView(camera_settings_t &View);

// Sets the view to what's suitable for rendering foreground objects like weapons in hand
bool OGL_SetForeground();

// Sets whether a foreground object is horizontally reflected
bool OGL_SetForegroundView(bool HorizReflect);

// Start and end rendering of main view 
void OGL_StartMain(const camera_settings_t* view);
void OGL_EndMain();

// Stuff for doing OpenGL rendering of various objects
bool OGL_RenderWall(polygon_definition& RenderPolygon, bool IsVertical);
bool OGL_RenderSprite(rectangle_definition& RenderRectangle);

// TODO

bool OGL_RenderText(short BaseX, short BaseY, const std::string& Text, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff);
// Render cursor for Lua/chat console
bool OGL_RenderTextCursor(const SDL_Rect& rect, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff);


// TODO: define ao_rect, ao_point, ao_size; make them interchangeable with SDL_Rect and SDL_Point, SDL_FRect, SDL_FPoint, then rework the following APIs to use SDL_FRect (which is SDL_gpu-friendly)

// Render rectangles (set color beforehand)
void OGL_RenderRect(float x, float y, float w, float h);
void OGL_RenderRect(const SDL_Rect& rect);
void OGL_RenderTexturedRect(float x, float y, float w, float h, float tleft, float ttop, float tright, float tbottom);
void OGL_RenderFrame(float x, float y, float w, float h, float thickness);


// Render lines (for overhead map)
void OGL_RenderLines(const std::vector<world_point2d>& points, float thickness);

OGL_FogData* OGL_GetCurrFogData();

#endif
