

#include "Canvas_SDL.hpp"

#include "screen.h"
#include "ImageBlitter.hpp"
#include "Shape_Blitter.h"


// one argument in favor of an ao_rect struct: it could manage coordinate transforms, e.g. from classical 640x480 grid to logical 1920x1080 display to true screen resolution 3840x2160, absolute to relative, and scaling


void Canvas_SDL::start_draw()
{
    assert_fail(m_surface, "");
    m_masking_mode = mask_mode::disabled;
    m_drawing = true;
    clear_clip();
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
        draw_text(text, font->shadowed(), {0x00, 0x00, 0x00, 0xff}, {rect.x + 1, rect.y + 1, rect.w - 1, rect.h - 1}); // TODO: how to calculate shadow offset? (see also font_t::measure_width; once dialogs use native screen resolution we have to account for scaling, though it's TBD if this is done here or if Canvas uses a shim to scale rects and other sizes)
    }
    
    // TODO: text quality is crap when drawn at 640x480 and scaled to screen resolution
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font->font, text.c_str(), color);
    if (surface->w > rect.w || surface->h > rect.h)
    {
        // TODO: how best to deal with text overflowing? e.g. crop as-is? shorten till it fits and return remaining string? return errTextDoesNotFit
    }
    SDL_Rect dest_rect = rect;
    SDL_BlitSurface(surface, nullptr, m_surface, &dest_rect);
}


void Canvas_SDL::draw_image(ImageBlitter* image, const SDL_Point& point)
{
    if (!m_drawing) return;
  //  image->Draw(m_surface, point); // TODO: FIX
}


void Canvas_SDL::draw_shape(Shape_Blitter *shape, const SDL_Point& point)
{
    if (!m_drawing) return;
   // shape->SDL_Draw(m_surface, point); // TODO: FIX
}


void Canvas_SDL::draw_surface(SDL_Surface* surface, const SDL_Rect& rect) // TODO: this doesn't bother to check if scaling/centering is needed
{
    if (!m_drawing) return;
    SDL_BlitSurface(surface, nullptr, m_surface, (SDL_Rect*)&rect);
}


void Canvas_SDL::draw_surface(SDL_Surface* surface, const SDL_Rect& dst_rect, const SDL_Rect& src_rect) // TODO: ditto
{
    if (!m_drawing) return;
    SDL_BlitSurface(surface, &src_rect, m_surface, (SDL_Rect*)&dst_rect);
}

#include "images.h"

void Canvas_SDL::render_to_screen(const SDL_Rect* dst_rect, const SDL_Rect* src_rect)
{
    if (!m_surface) return;
    if (!m_blitter) { m_blitter.reset(new ImageBlitter()); }
    m_blitter->borrow_surface(m_surface);
    m_blitter->render_to_screen(dst_rect, src_rect);
}
