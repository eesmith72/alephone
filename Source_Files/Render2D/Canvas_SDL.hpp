

#ifndef Canvas_SDL_hpp
#define Canvas_SDL_hpp

#include "Canvas.hpp"


// TODO: all/some SW rendering might be better done drawing directly to SDL_Renderer (see SDL_RenderDraw... functions)


class Canvas_SDL : public Canvas
{
public:
    Canvas_SDL(SDL_Surface* surface) : Canvas(), m_surface(surface), m_blitter(nullptr) {}
    
    void unload() override
    {
        SDL_FreeSurface(m_surface);
        m_surface = nullptr;
    }
    
    void start_draw() override; // TODO: not a huge fan of explicit start/end; OGL needs it
    void end_draw() override;
    
    void set_clip(const SDL_Rect& rect) override;
    SDL_Rect get_clip() override;
    void clear_clip() override;
    
    void clear(const SDL_Color& color = {0x00, 0x00, 0x00, 0xff}) override;
    
    // TODO: rects should be ptr args so null can be passed
    
    void draw_filled_rect(const SDL_Rect& rect, const SDL_Color& color) override;
    
    void draw_outlined_rect(const SDL_Rect& rect, const SDL_Color& color, int32_t line_weight = OUTLINE_THICKNESS) override;
    
    void draw_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect) override; // TODO: if text doesn't fit, return remaining string/index of first remaining character? or don't blit and return ao_err? probably want to return the remaining rect (this assumes single-line rendering); another option is to pass point plus max width
    
    void draw_image(Blitter* image, const SDL_Point& point) override;
    
    void draw_shape(Shape_Blitter* shape, const SDL_Point& point) override;
    
    void draw_surface(SDL_Surface* shape, const SDL_Rect& dst_rect) override;
    
    void draw_surface(SDL_Surface* shape, const SDL_Rect& dst_rect, const SDL_Rect& src_rect) override;
    
    
    void render_to_screen(const SDL_Rect* dst_rect = nullptr, const SDL_Rect* src_rect = nullptr) override;

    
protected:
    SDL_Surface* m_surface;
    std::shared_ptr<Blitter> m_blitter;
};


#endif /* Canvas_SDL_hpp */
