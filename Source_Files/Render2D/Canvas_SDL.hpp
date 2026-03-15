

#ifndef Canvas_SDL_hpp
#define Canvas_SDL_hpp

#include "Canvas.hpp"


class Canvas_SDL : public Canvas
{
public:
    Canvas_SDL(SDL_Surface* surface) : Canvas(), m_surface(surface) {}
    ~Canvas_SDL() {}
    
    void start_draw() override;
    void end_draw() override;
    
    void set_clip(const SDL_Rect& rect) override;
    SDL_Rect get_clip() override;
    void clear_clip() override;
    
    void clear(const SDL_Color& color = {0x00, 0x00, 0x00, 0xff}) override;
    
    void draw_filled_rect(const SDL_Rect& rect, const SDL_Color& color) override;
    
    void draw_outlined_rect(const SDL_Rect& rect, const SDL_Color& color, int32_t line_weight = OUTLINE_THICKNESS) override;
    
    void draw_text(const std::string& text, const font_t* font, const SDL_Color& color, const SDL_Rect& rect) override; // TODO: if text doesn't fit, return remaining string/index of first remaining character? or don't blit and return ao_err? probably want to return the remaining rect (this assumes single-line rendering); another option is to pass point plus max width
    
    void draw_image(Image_Blitter* image, const SDL_Point& point) override;
    
    void draw_shape(Shape_Blitter* shape, const SDL_Point& point) override;
    
    void draw_surface(SDL_Surface* shape, const SDL_Rect& rect) override;
    
protected:
    SDL_Surface* m_surface;

    
    void apply_clip() override;
    void clear_mask() override {};

    void start_using_mask() {}
    void end_using_mask() {}
    void start_drawing_mask(bool erase) {}
    void end_drawing_mask() {}
    
};


#endif /* Canvas_SDL_hpp */
