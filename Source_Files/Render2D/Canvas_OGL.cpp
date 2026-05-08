/*
 Canvas_OGL.cpp
 
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


#include "Canvas_OGL.hpp"

#include "map.h"
#include "Screen.hpp"

#include "OGL_Render.h"
#include "OGL_TextureManager.h"


// ao_rgb straight to OpenGL
static inline void SetColor(const ao_rgb& Color)
{
	if (automap_is_translucent())
		glColor4us(Color.r, Color.g, Color.b, 32767);
	else
		glColor3usv((unsigned short *)(&Color));
}


// Need to test this so as to find out when the color changes
static inline bool ColorsEqual(const ao_rgb& Color1, const ao_rgb& Color2)
{
	return Color1.r == Color2.r && Color1.g == Color2.g && Color1.b == Color2.b;
}


static world_point2d& GetVertex(short index) { return get_endpoint_data(index)->transformed; }

static world_point2d* GetFirstVertex() { return &GetVertex(0); }

static int GetVertexStride() { return sizeof(endpoint_data); }




// For marking out the area to be blanked out when starting rendering; these are defined in OGL_Render.cpp // TODO: smh
extern short ViewWidth, ViewHeight;



void Canvas_OGL::begin_overall()
{
	// Blank out the screen
	// Do that by painting a black polygon
	if (!automap_is_translucent())
	{
		glColor3f(0,0,0);
		OGL_RenderRect(0, 0, ViewWidth, ViewHeight);
	}
	
/*
	glEnable(GL_SCISSOR_TEST);	// Don't erase the HUD
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
*/
	
	// Here's for the overhead map
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	if (automap_is_translucent())
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_FOG);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}


void Canvas_OGL::end_overall()
{
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}


void Canvas_OGL::begin_polygons()
{
	// Polygons are rendered before lines, and use the endpoint array,
	// so both of them will have it set here. Using the compiled-vertex extension,
	// however, makes everything the same color :-P
	glVertexPointer(2,GL_SHORT,GetVertexStride(),GetFirstVertex());

	// Reset color defaults
	SavedColor.r = SavedColor.g = SavedColor.b = 0;
	SetColor(SavedColor);
	
	// Reset cache to zero length
	PolygonCache.clear();
}


void Canvas_OGL::draw_polygon(short vertex_count, const short* vertices, const ao_rgb& color)
{
	// Test whether the polygon parameters have changed
	bool AreColorsEqual = ColorsEqual(color,SavedColor);
	
	// If any change, then draw the cached lines with the *old* parameters,
	// Set the new parameters
	if (!AreColorsEqual)
	{
		DrawCachedPolygons();
		SavedColor = color;
		SetColor(SavedColor);
	}
	
	// Implement the polygons as triangle fans
	for (int k=2; k<vertex_count; k++)
	{
		PolygonCache.push_back(vertices[0]);
		PolygonCache.push_back(vertices[k-1]);
		PolygonCache.push_back(vertices[k]);
	}
	
	// glDrawElements(GL_POLYGON,vertex_count,GL_UNSIGNED_SHORT,vertices);
}


void Canvas_OGL::end_polygons()
{
	DrawCachedPolygons();
}


void Canvas_OGL::DrawCachedPolygons()
{
    glDrawElements(GL_TRIANGLES, (GLsizei)PolygonCache.size(), GL_UNSIGNED_SHORT, PolygonCache.data());
	PolygonCache.clear();
}


void Canvas_OGL::begin_lines()
{
	// Reset color and pen size to defaults
	SetColor(SavedColor);
	SavedPenSize = 1;
	
	// Reset cache to zero length
	LineCache.clear();
}


void Canvas_OGL::draw_line(const short* vertices, const ao_rgb& color, short pen_size)
{
	// Test whether the line parameters have changed
	bool AreColorsEqual = ColorsEqual(color,SavedColor);
	bool AreLinesEquallyWide = (pen_size == SavedPenSize);
	
	// If any change, then draw the cached lines with the *old* parameters
	if (!AreColorsEqual || !AreLinesEquallyWide) DrawCachedLines();
	
	// Set the new parameters
	if (!AreColorsEqual)
	{
		SavedColor = color;
		SetColor(SavedColor);
	}
	
	if (!AreLinesEquallyWide)
	{
		SavedPenSize = pen_size;
	}
	
	// Add the line's points to the cached line
	LineCache.push_back(GetVertex(vertices[0]));
	LineCache.push_back(GetVertex(vertices[1]));
}


void Canvas_OGL::end_lines()
{
	DrawCachedLines();
}


void Canvas_OGL::DrawCachedLines()
{
	OGL_RenderLines(LineCache, SavedPenSize);
	LineCache.clear();
}



void Canvas_OGL::draw_square(const world_point2d& center, const ao_rgb& color, short radius)
{
    SetColor(color);
    
    // Let OpenGL do the transformation work
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(center.x,center.y,0);
    glScalef(radius, radius, 1);

    OGL_RenderRect(-0.75f, -0.75f, 1.5f, 1.5f);
    
    glPopMatrix();
}



void Canvas_OGL::draw_circle(const world_point2d& center, const ao_rgb& color, short radius)
{
	SetColor(color);
	
	// Let OpenGL do the transformation work
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(center.x,center.y,0);
	glScalef(radius, radius, 1);


    GLfloat ft = 0.1f;
    GLfloat ht = ft * 0.5f;
    GLfloat vertices[36] = {
        -0.30f - ht, -0.75f,
        -0.30f + ht, -0.75f + ft,
        +0.30f + ht, -0.75f,
        +0.30f - ht, -0.75f + ft,
        +0.75f - ft, -0.30f + ht,
        +0.75f,      -0.30f - ht,
        +0.75f - ft, +0.30f - ht,
        +0.75f,      +0.30f + ht,
        +0.30f - ht, +0.75f - ft,
        +0.30f + ht, +0.75f,
        -0.30f + ht, +0.75f - ft,
        -0.30f - ht, +0.75f,
        -0.75f + ft, +0.30f - ht,
        -0.75f,      +0.30f + ht,
        -0.75f + ft, -0.30f + ht,
        -0.75f,      -0.30f - ht,
        -0.30f - ht, -0.75f,
        -0.30f + ht, -0.75f + ft
    };
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 36);

    glPopMatrix();
}


void Canvas_OGL::draw_triangle(const world_point2d& center, angle facing, const ao_rgb& color,
                               short shrink, short front, short rear, short rear_theta)
{
	SetColor(color);
	
	// The player is a simple triangle
	GLfloat PlayerShape[3][2];
	
	double rear_theta_rads = rear_theta*(8*atan(1.0)/FULL_CIRCLE);
	float rear_x = (float)(rear*cos(rear_theta_rads));
	float rear_y = (float)(rear*sin(rear_theta_rads));
	PlayerShape[0][0] = front;
	PlayerShape[0][1] = 0;
	PlayerShape[1][0] = rear_x;
	PlayerShape[1][1] = rear_y;
	PlayerShape[2][0] = rear_x;
	PlayerShape[2][1] = - rear_y;
	
	// Let OpenGL do the transformation work
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(center.x,center.y,0);
	glRotatef(facing*(360.0F/FULL_CIRCLE),0,0,1);
	float scale = 1/float(1 << shrink);
	glScalef(scale,scale,1);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2,GL_FLOAT,0,PlayerShape[0]);
	glDrawArrays(GL_POLYGON,0,3);

	glPopMatrix();
}

	

void Canvas_OGL::draw_text(const world_point2d& location, const ao_rgb& color,
                           const std::string& text, const font_t* FontData, short justify)
{
    /*
	// Find the left-side location
	world_point2d left_location = location;
	switch(justify)
    {
        case _justify_left:
            break;
        case _justify_center:
            left_location.x -= FontData->TextWidth(text.c_str()) >> 1;
            break;
        default:
            return;
    }
	
	// Set color and location	
	SetColor(color);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(left_location.x,left_location.y,0);
	FontData.NearFilter = TxtrTypeInfoList[OGL_Txtr_HUD].NearFilter;
    FontData.OGL_Render(text.c_str());
	glPopMatrix();
     */
}

	
void Canvas_OGL::begin_path(const ao_rgb& color)
{
	SetColor(color);
}


void Canvas_OGL::draw_path_point(short step, const world_point2d& location)
{
	// At first step, reset the length
	if (step <= 0) PathPoints.clear();
	
	// Duplicate points to form lines at each step
	if (PathPoints.size() > 1)
		PathPoints.push_back(PathPoints.back());

	// Add the point
	PathPoints.push_back(location);
}


void Canvas_OGL::end_path()
{
	OGL_RenderLines(PathPoints, 1);
	PathPoints.clear();
}





// TODO: the following was pulled out of HUD_Lua_Class (which is what the new Canvas class was initially derived from); unifying everything in a single OGL-based drawing class is WIP

/*
void Canvas::start_draw()
{
    main_screen.set_vscreen_for_game(); // sus
    m_wr = main_screen.window_rect();
    m_opengl = (modern_renderer_is_active()); // TODO: it's always OGL now
    m_masking_mode = _mask_disabled;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_FOG);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(m_wr.x, m_wr.y, 0.0);
    
    m_surface = NULL;
    
    m_drawing = true;
    clear_clip();
}
 

void Canvas::end_draw(void)
{
    m_drawing = false;
    
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}
 

void Canvas::apply_clip(void)
{
 // lua_clip_rect is no more; Lua should call set_virtual_drawing_rect
    SDL_Rect r;
    r.x = m_wr.x + main_screen.lua_clip_rect.x;
    r.y = m_wr.y + main_screen.lua_clip_rect.y;
    r.w = MIN(main_screen.lua_clip_rect.w, m_wr.w - main_screen.lua_clip_rect.x);
    r.h = MIN(main_screen.lua_clip_rect.h, m_wr.h - main_screen.lua_clip_rect.y);

     glEnable(GL_SCISSOR_TEST);
     main_screen.set_clipping_rect(r);
}

    
void Canvas::clear_clip(void)
{
    if (!m_drawing) return;
    
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
}
 

void Canvas::start_using_mask(void)
{
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

 
void Canvas::end_using_mask(void)
{
    glDisable(GL_STENCIL_TEST);
}

 
void Canvas::start_drawing_mask(bool erase)
{
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, erase ? 0 : 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5);
    
    glColorMask(false, false, false, false);
}

 
void Canvas::end_drawing_mask(void)
{
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glColorMask(true, true, true, true);
}

 
void Canvas::fill_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    if (!m_drawing) return;
    if (!w || !h) return;
    
    apply_clip();

    glColor4f(r, g, b, a);
    OGL_RenderRect(x, y, w, h);
}

 
void Canvas::draw_outlined_rect(float x, float y, float w, float h, float r, float g, float b, float a, float t)
{
    if (!m_drawing) return;
        
    apply_clip();
    glColor4f(r, g, b, a);
    OGL_RenderFrame(x, y, w, h, t);
}

 
void Canvas::draw_text(Font* font, const std::string& text, float x, float y, float r, float g, float b, float a, float scale)
{
    if (!m_drawing || text.empty()) return;
    
    apply_clip();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(x, y + (font->Height * scale), 0);
    glScalef(scale, scale, 1.0);
    glColor4f(r, g, b, a);
    font->OGL_Render(text.c_str());
    glColor4f(1, 1, 1, 1);
    glPopMatrix();
}

 
void Canvas::draw_image(ImageBlitter* image, float x, float y)
{
    if (!m_drawing) return;
    
    Image_Rect r{ x, y, image->crop_rect.w, image->crop_rect.h };
    
    if (!r.w || !r.h) return;

    apply_clip();
    if (m_surface)
    {
        r.x += m_wr.x;
        r.y += m_wr.y;
    }
    image->Draw(MainScreenSurface(), r);
}
 

void Canvas::draw_shape(ShapeBlitter *shape, float x, float y)
{
    if (!m_drawing) return;
    
    Image_Rect r;
    r.x = x;
    r.y = y;
    r.w = shape->crop_rect.w;
    r.h = shape->crop_rect.h;
    
    if (!r.w || !r.h) return;
    
    apply_clip();

    shape->OGL_Draw(r);
}

*/

