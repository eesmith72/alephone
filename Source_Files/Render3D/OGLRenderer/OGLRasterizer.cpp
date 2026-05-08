/*
 *  OGLRasterizer.cpp
 *  Created by Clemens Unterkofler on 1/20/09.
 *  for Aleph One
 *
 *  http://www.gnu.org/licenses/gpl.html
 */

#include "OGLRasterizer.h"

#include "lightsource.h"
#include "media.h"
#include "player.h"
#include "weapons.h"
#include "AnimatedTextures.h"
#include "ChaseCam.h"
#include "preferences.hpp"
#include "visual_effects.hpp"
#include "Screen.hpp"

#include "visual_effects.hpp"
#include "OGL_FBO.h"
#include "OGL_TextureManager.h"
#include "OGL_Shader.h"

#define MAXIMUM_VERTICES_PER_WORLD_POLYGON (MAXIMUM_VERTICES_PER_POLYGON+4)

const float FixedAngleToDegrees = 360.0/(float(FIXED_ONE)*float(FULL_CIRCLE));

const GLdouble kViewBaseMatrix[16] = {
	0,	0,	-1,	0,
	1,	0,	0,	0,
	0,	1,	0,	0,
	0,	0,	0,	1
};

const GLdouble kViewBaseMatrixInverse[16] = {
	0,	1,	0,	0,
	0,	0,	1,	0,
	-1,	0,	0,	0,
	0,	0,	0,	1
};


void OGLRasterizer::configure(const SDL_Point& size, int32_t bit_depth)
{
    view_width = 0;
    view_height = 0;
    swapper.reset();
}



void OGLRasterizer::Begin(camera_settings_t* View)
{
    Rasterizer::Begin(View);
    
    OGL_SetView(*view);
    
    // TODO: Screen::did_change should be responsible for telling the current Renderer (and its Rasterizer) when they need to update
    if (view->screen_width != view_width || view->screen_height != view_height || !swapper)
    {
        view_width = view->screen_width; // TODO: so it
        view_height = view->screen_height;
        swapper.reset();
        swapper.reset(new FBOSwapper(view_width / main_screen.pixel_to_virtual_scale(),
                                     view_height / main_screen.pixel_to_virtual_scale(), false));
    }
    
    float aspect = view->screen_width / float(view->screen_height);
    float deg2rad = 8.0 * atan(1.0) / 360.0;
    float xtan, ytan;
    if (graphics_preferences.horizontal_fov_is_constant)
    {
        xtan = tan(view->current_field_of_view * deg2rad / 2.0);
        ytan = xtan / aspect;
    }
    else
    {
        ytan = tan(view->current_field_of_view * deg2rad / 2.0) / 2.0;
        xtan = ytan * aspect;
    }
    
    // Adjust for view distortion during teleport effect
    ytan *= view->real_world_to_screen_y / double(view->world_to_screen_y);
    xtan *= view->real_world_to_screen_x / double(view->world_to_screen_x);

    double yaw = view->virtual_yaw * FixedAngleToDegrees;
    double pitch = view->virtual_pitch * FixedAngleToDegrees;
    pitch = (pitch > 180.0 ? pitch - 360.0 : pitch);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearVal = 64.0;
    float farVal = 128.0 * 1024.0;
    float x = xtan * nearVal;
    float y = ytan * nearVal;
    float yoff = tan(pitch * deg2rad) * nearVal;
    glFrustum(-x, x, -y + yoff, y + yoff, nearVal, farVal);

    glMatrixMode(GL_MODELVIEW);

    // setup a rotation matrix for the landscape texture shader
    // this aligns the landscapes to the center of the screen for standard
    // pitch ranges, so that they don't need to be stretched

    glLoadIdentity();
    glTranslated(view->origin.x, view->origin.y, view->origin.z);
    glRotated(yaw, 0.0, 0.0, 1.0);
    glRotated(-pitch, 0.0, 1.0, 0.0);
    glMultMatrixd(kViewBaseMatrixInverse);

    GLfloat landscapeInverseMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, landscapeInverseMatrix);

    Shader *shader = Shader::get(Shader::S_Landscape);
    shader->enable();
    shader->setMatrix4(Shader::U_LandscapeInverseMatrix, landscapeInverseMatrix);

    shader = Shader::get(Shader::S_LandscapeBloom);
    shader->enable();
    shader->setMatrix4(Shader::U_LandscapeInverseMatrix, landscapeInverseMatrix);

    Shader::disable();

    // setup the normal view matrix

    glLoadMatrixd(kViewBaseMatrix);
    glRotated(pitch, 0.0, 1.0, 0.0);
//    apperently 'roll' is not what i think it is
//    rubicon sets it to some strange value
//    double roll = view->roll * 360.0 / float(NUMBER_OF_ANGLES);
//    glRotated(roll, 1.0, 0.0, 0.0);
    glRotated(-yaw, 0.0, 0.0, 1.0);
    glTranslated(-view->origin.x, -view->origin.y, -view->origin.z);

    OGL_StartMain();
    assert_fail(swapper, "must not be nullptr");
	swapper->activate();
    swapper->current_contents().draw_full(); // Modern renderer does not "smear the void" if a wall is untextured
}


void OGLRasterizer::End()
{
    // apply gamma to gameworld (EES: not sure why this invokes FBO swapper but not going to figure it out)
    
	swapper->deactivate();
	swapper->swap();
	
    if (graphics_preferences.gamma_level != DEFAULT_GAMMA_LEVEL)
    {
		Shader *shader = Shader::get(Shader::S_Gamma);
		shader->enable();
		shader->setFloat(Shader::U_GammaAdjust, graphics_preferences.gamma_adjustment());
	}
	swapper->draw();
	Shader::disable();
	
	SetForeground();
	glColor3f(0, 0, 0);
	OGL_RenderFrame(0, 0, view_width, view_height, 1); // draws 1px black keyline around worldview
	
    OGL_EndMain();
}


