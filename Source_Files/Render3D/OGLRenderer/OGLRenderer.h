/*
 *  OGLRenderer.h -- this and OGLRasterizer render the 3D gameworld in OGL
 *  Created by Clemens Unterkofler on 1/20/09.
 *  for Aleph One
 *
 *  http://www.gnu.org/licenses/gpl.html
 */


#ifndef _RENDERRASTERIZE_SHADER__H
#define _RENDERRASTERIZE_SHADER__H

#include "cseries.hpp"
#include "map.h"
#include "Renderer.h"
#include "OGL_FBO.h"
#include "OGL_TextureManager.h"
#include "OGLRasterizer.h"


class Blur;


class OGLRenderer : public Renderer // EES: was RenderRasterize_Shader
{
public:
    
    OGLRenderer();
    ~OGLRenderer();
    
    virtual void initialize(const SDL_Point& size, int32_t bit_depth) override;
    
    virtual void shutdown() override;
    
    
    virtual void render_tree() override;
    
    bool renders_viewer_sprites_in_tree() override { return true; }
    
    std::unique_ptr<TextureManager> setupWallTexture(const shape_descriptor& Texture, short transferMode, float pulsate,
                                                     float wobble, float intensity, float offset, RenderStep renderStep);
    
    std::unique_ptr<TextureManager> setupSpriteTexture(const rectangle_definition& rect, short type,
                                                       float offset, RenderStep renderStep);
    
protected:
    
	virtual void render_node(sorted_node_data *node, bool SeeThruLiquids, RenderStep renderStep) override;
    
	virtual void store_endpoint(endpoint_data *endpoint, long_vector2d& p) override;
    
    
	virtual void render_node_floor_or_ceiling(clipping_window_data *window, polygon_data *polygon,
                                              horizontal_surface_data *surface, bool void_present, bool ceil, RenderStep renderStep) override;
    
	virtual void render_node_side(clipping_window_data *window,
                                  vertical_surface_data *surface, bool void_present, RenderStep renderStep) override;
    
	virtual void render_node_object(render_object_data *object, bool other_side_of_media, RenderStep renderStep) override;
	
    
	virtual void clip_to_window(clipping_window_data *win);
    
	virtual void _render_node_object_helper(render_object_data *object, RenderStep renderStep);
    
    
    void render_viewer_sprite_layer(RenderStep renderStep);
    
    void render_viewer_sprite(rectangle_definition& RenderRectangle, RenderStep renderStep);
	
    
private:
    
    OGLRasterizer ogl_rasterizer; // EES: see also RasPtr in Rasterizer base class, which is just a pointer to this; unknotting this crap will take time
    
    std::unique_ptr<Blur> blur;
    
    int objectCount;
    world_distance objectY;
    float weaponFlare;
    float selfLuminosity;
    
    long_vector2d leftmost_clip, rightmost_clip;
};

#endif
