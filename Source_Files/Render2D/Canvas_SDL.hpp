// 2D drawing to SDL_Surface

// TODO: once OGL rendering is fully working in Canvas (based on the OGL map renderer's methods), we should be able to get rid of Canvas_SDL (we have to keep it for now as dialogs currently draw in SDL)

#ifndef Canvas_SDL_hpp
#define Canvas_SDL_hpp

#include "Canvas.hpp"



class Canvas_SDL : public Canvas
{
public:
    
    // important: this takes ownership of Surface and will Free it when destroyed
    Canvas_SDL(SDL_Surface* surface) : Canvas(surface->w, surface->h), m_surface(surface), m_blitter(nullptr) {}
    
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
    
    void draw_image(ImageBlitter* image, const SDL_Point& point) override;
    
    void draw_shape(Shape_Blitter* shape, const SDL_Point& point) override;
    
    void draw_surface(SDL_Surface* shape, const SDL_Rect& dst_rect) override;
    
    void draw_surface(SDL_Surface* shape, const SDL_Rect& dst_rect, const SDL_Rect& src_rect) override;
    
    
    void render_to_screen(const SDL_Rect* dst_rect = nullptr, const SDL_Rect* src_rect = nullptr) override;

    
protected:
    SDL_Surface* m_surface;
    std::shared_ptr<ImageBlitter> m_blitter;
};


#endif /* Canvas_SDL_hpp */
