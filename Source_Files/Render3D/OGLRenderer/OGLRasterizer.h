/*
 *  OGLRasterizer.h -- this and OGLRenderer render the 3D gameworld in OGL
 *  Created by Clemens Unterkofler on 1/20/09.
 *  for Aleph One
 *
 *  http://www.gnu.org/licenses/gpl.html
 */


#ifndef _RASTERIZER_SHADER__H
#define _RASTERIZER_SHADER__H

#include "cseries.hpp"

#include "map.h"
#include "Rasterizer.h"


class FBOSwapper;


class OGLRasterizer : public Rasterizer
{
	friend class OGLRenderer;
	
protected:
	std::unique_ptr<FBOSwapper> swapper; // EES: sure would be nice to have commented explanation of what this is used for
	short view_width;
	short view_height;

    void configure_for_view(camera_settings_t* view);

public:

	OGLRasterizer() = default;
	~OGLRasterizer() = default;
    
	virtual void configure();
    
	virtual void Begin(camera_settings_t* View);
	virtual void End();
    
    // Sets the rasterizer so that it will start rendering foreground objects like weapons in hand
    virtual void SetForeground()
    {
        OGL_SetForeground();
    }
    
    // Sets the view of a foreground object; parameter is whether it is horizontally reflected
    virtual void SetForegroundView(bool HorizReflect)
    {
        OGL_SetForegroundView(HorizReflect);
    }
    
    // drawing API from the SW renderer
    
    void texture_horizontal_polygon(polygon_definition& textured_polygon)
    {
        OGL_RenderWall(textured_polygon,false);
    }
    
    void texture_vertical_polygon(polygon_definition& textured_polygon)
    {
        OGL_RenderWall(textured_polygon,true);
    }
    
    void texture_rectangle(rectangle_definition& textured_rectangle)
    {
        OGL_RenderSprite(textured_rectangle);
    }
};


#endif
