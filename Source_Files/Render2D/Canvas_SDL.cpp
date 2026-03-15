

#include "Canvas_SDL.hpp"
#include "screen.h"



void Canvas_SDL::start_draw()
{
    assert_fail(m_surface, "");
    m_masking_mode = mask_mode::disabled;
    
    SDL_FillRect(m_surface, NULL, SDL_MapRGBA(m_surface->format, 0, 0, 0, 0));
    
    m_drawing = true;
    clear_mask();
}

void Canvas_SDL::end_draw()
{
    m_drawing = false;
}

void Canvas_SDL::set_clip(const SDL_Rect& rect)
{
    SDL_SetClipRect(m_surface, &rect);
}

SDL_Rect Canvas_SDL::get_clip()
{
    SDL_Rect rect;
    SDL_GetClipRect(m_surface, &rect);
    return rect;
}

void Canvas_SDL::clear_clip()
{
    SDL_SetClipRect(m_surface, nullptr);
}


void Canvas_SDL::clear(const SDL_Color& color)
{
    if (!m_drawing) return;
    
    SDL_FillRect(m_surface, nullptr, SDL_MapRGBA(m_surface->format, color.r, color.g, color.b, color.a));
}


void Canvas_SDL::draw_filled_rect(const SDL_Rect& rect, const SDL_Color& color)
{
    if (!m_drawing || rect.w == 0 || rect.h == 0) return;
    
    SDL_FillRect(m_surface, &rect, SDL_MapRGBA(m_surface->format, color.r, color.g, color.b, color.a));
}


void Canvas_SDL::draw_outlined_rect(const SDL_Rect& rect, const SDL_Color& color, int32_t line_weight)
{
    if (!m_drawing || rect.w == 0 || rect.h == 0) return;
    
    uint32_t pixel_color = SDL_MapRGBA(m_surface->format, color.r, color.g, color.b, color.a);
    SDL_Rect vertical_line = {rect.x, rect.y, line_weight, rect.h};
    SDL_FillRect(m_surface, &vertical_line, pixel_color);
    vertical_line.x = rect.x + rect.w - line_weight;
    SDL_FillRect(m_surface, &vertical_line, pixel_color);
    SDL_Rect horizontal_line = {rect.x + line_weight, rect.y, rect.w - line_weight * 2, line_weight};
    SDL_FillRect(m_surface, &horizontal_line, pixel_color);
    horizontal_line.y = rect.y + rect.h - line_weight;
    SDL_FillRect(m_surface, &horizontal_line, pixel_color);
}


void Canvas_SDL::draw_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect)
{
    if (!m_drawing || rect.w == 0 || rect.h == 0 || text.empty()) return;
    
    if (font->key.style & styleShadow)
    {
        draw_text(text, font->shadowed(), {0x00, 0x00, 0x00, 0xff}, {rect.x + 1, rect.y + 1}); // TODO: how to calculate shadow offset? (see also font_t::measure_width)
    }
    
    SDL_Surface* surface = TTF_RenderText_Blended(font->font, text.c_str(), color);
    if (surface->w > rect.w || surface->h > rect.h)
    {
        // TODO: how best to report/handle overflow?
    }
    SDL_Rect dest_rect;
    SDL_BlitSurface(surface, nullptr, m_surface, &dest_rect);
}


void Canvas_SDL::draw_image(Image_Blitter *image, const SDL_Point& point)
{
    if (!m_drawing) return;
  //  image->Draw(m_surface, point);
}


void Canvas_SDL::draw_shape(Shape_Blitter *shape, const SDL_Point& point)
{
    if (!m_drawing) return;
   // shape->SDL_Draw(m_surface, point);
}


void Canvas_SDL::draw_surface(SDL_Surface* shape, const SDL_Rect& rect)
{
    if (!m_drawing) return;
    
}

