
#include "Canvas_OGL.hpp"


#ifdef HAVE_OPENGL


/*
void Canvas_SDL::start_draw()
{
    alephone::Screen *scr = alephone::Screen::instance();
    scr->bound_screen();
    m_wr = scr->window_rect();
    m_opengl = (get_screen_mode()->acceleration);
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
    clear_mask();
}
 

void Canvas_SDL::end_draw(void)
{
    m_drawing = false;
    
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}
 

void Canvas_SDL::apply_clip(void)
{
    alephone::Screen *scr = alephone::Screen::instance();
    
    SDL_Rect r;
    r.x = m_wr.x + scr->lua_clip_rect.x;
    r.y = m_wr.y + scr->lua_clip_rect.y;
    r.w = MIN(scr->lua_clip_rect.w, m_wr.w - scr->lua_clip_rect.x);
    r.h = MIN(scr->lua_clip_rect.h, m_wr.h - scr->lua_clip_rect.y);

     glEnable(GL_SCISSOR_TEST);
    scr->scissor_screen_to_rect(r);
}

    
void Canvas_SDL::clear_mask(void)
{
    if (!m_drawing) return;
    
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
}
 

void Canvas_SDL::start_using_mask(void)
{
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

 
void Canvas_SDL::end_using_mask(void)
{
    glDisable(GL_STENCIL_TEST);
}

 
void Canvas_SDL::start_drawing_mask(bool erase)
{
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, erase ? 0 : 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5);
    
    glColorMask(false, false, false, false);
}

 
void Canvas_SDL::end_drawing_mask(void)
{
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glColorMask(true, true, true, true);
}

 
void Canvas_SDL::fill_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    if (!m_drawing) return;
    if (!w || !h) return;
    
    apply_clip();

    glColor4f(r, g, b, a);
    OGL_RenderRect(x, y, w, h);
}

 
void Canvas_SDL::draw_outlined_rect(float x, float y, float w, float h, float r, float g, float b, float a, float t)
{
    if (!m_drawing) return;
        
    apply_clip();
    glColor4f(r, g, b, a);
    OGL_RenderFrame(x, y, w, h, t);
}

 
void Canvas_SDL::draw_text(Font* font, const std::string& text, float x, float y, float r, float g, float b, float a, float scale)
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

 
void Canvas_SDL::draw_image(Image_Blitter *image, float x, float y)
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
 

void Canvas_SDL::draw_shape(Shape_Blitter *shape, float x, float y)
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


#endif /* HAVE_OPENGL */
