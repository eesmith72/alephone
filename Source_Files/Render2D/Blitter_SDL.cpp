

#include "Blitter_SDL.hpp"

#include "screen.h" // sw_render_texture_to_screen


// TODO: when drawing in SW mode, Canvas_SDL should ideally get its drawing Surface from SDL_LockTextureToSurface

// TODO: if resolution/bit-depth changes, AO discards old SW renderer and creates new one so we'll need to register Textures (Blitter_SDL instances) to have their m_textures destroyed and reset to nullptr



void Blitter_SDL::unload()
{
    if (m_texture)
    {
        // Blitters always own their textures
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    Blitter::unload();
}

// TODO: it is possible to add to an existing texture, though we probably don't want to support it here
//SDL_UpdateTexture(m_texture, nullptr, m_surface->pixels, m_surface->pitch);


void Blitter_SDL::render_to_screen(const SDL_Rect* dst_rect, const SDL_Rect* src_rect)
{
    if (!m_surface)
    {
        log_error("Called Blitter_SDL::render_to_screen but a Surface wasn't loaded. This is probably a bug.");
        return;
    }
    if (!m_texture)
    {
        m_texture = SDL_CreateTextureFromSurface(get_sw_renderer(), m_surface);
        SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeBest); // or Linear, if we want decent quality a bit faster
    }
    
    // TODO: this skips gamma adjustment, for now
    //printf("Blitter_SDL::render_to_screen\n");
    sw_render_texture_to_screen(m_texture, dst_rect, src_rect);
}

