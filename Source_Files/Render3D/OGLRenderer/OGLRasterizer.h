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
	friend class OGLRenderer; // yuck

public:

	OGLRasterizer() = default;
	~OGLRasterizer() = default;
    
    virtual void configure(const SDL_Point& size, int32_t bit_depth) override;
    
	virtual void Begin(camera_settings_t* View) override;
	virtual void End() override;
    
    // drawing API from the SW renderer
    
    virtual void texture_horizontal_polygon(polygon_definition& textured_polygon) override
    {
        OGL_RenderWall(textured_polygon,false);
    }
    
    virtual void texture_vertical_polygon(polygon_definition& textured_polygon) override
    {
        OGL_RenderWall(textured_polygon,true);
    }
    
    virtual void texture_rectangle(billboard_t& textured_rectangle) override
    {
        OGL_RenderSprite(textured_rectangle);
    }
    
protected:
    std::unique_ptr<FBOSwapper> swapper; // EES: sure would be nice to have commented explanation of what this is used for
    short view_width;
    short view_height;
};


#endif
