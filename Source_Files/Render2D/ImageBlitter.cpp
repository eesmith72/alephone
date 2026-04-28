/*
IMAGE_BLITTER.CPP
 
    Copyright (C) 2009 by Jeremiah Morris and the Aleph One developers

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    This license is contained in the file "COPYING",
    which is included with this source code; it is available online at
    http://www.gnu.org/licenses/gpl.html

    Implements images for 2D UI
*/

#include "ImageBlitter.hpp"

#include "resource_manager.h"
#include "images.h"
#include "screen.hpp"

#include "OGL_Setup.h"
#include "OGL_Faders.cpp"
#include "OGL_Render.h"


// defined in OGL_Textures.cpp
inline int NextPowerOfTwo(int n);


// originally 256 but 2048 is a reasonable size nowadays
#define OGL_TEXTURE_SIZE  (2048)

// tracks all active ImageBlitter instances, ensuring a blitter's GPU textures are deleted when it is
static std::unordered_set<ImageBlitter*> m_blitter_registry;




void ImageBlitter::load(SDL_Surface* surface, bool own_it)
{
    unload();
    
    m_surface = surface;
    owns_surface = own_it;
    
    // when blitting surface, make sure we copy rather than blend the alpha // TODO: probably want to blend now
    SDL_SetSurfaceBlendMode(m_surface, SDL_BLENDMODE_NONE);
}


void ImageBlitter::take_surface(SDL_Surface* surface)
{
    load(surface, true);
}

void ImageBlitter::borrow_surface(SDL_Surface* surface)
{
    load(surface, false);
}


int32_t ImageBlitter::width()
{
    return m_surface ? m_surface->w : 0;
}


int32_t ImageBlitter::height()
{
	return m_surface ? m_surface->h : 0;
}


// this is the original Draw method, with tweaked API
void ImageBlitter::render_to_screen(const SDL_Rect* dst_rect, const SDL_Rect* src_rect)
{
    if (!m_surface)
    {
        log_error("Called ImageBlitter::render_to_screen but a Surface wasn't loaded. This is probably a bug.");
        return;
    }
    if (m_tiles.empty())
    {
        create_texture_tiles();
    }
    
    GLdouble dst_x, dst_y, dst_w, dst_h;
    if (dst_rect)
    {
        dst_x = dst_rect->x;
        dst_y = dst_rect->y;
        dst_w = dst_rect->w;
        dst_h = dst_rect->h;
    }
    else
    {
        int32_t w, h;
        main_screen.get_window_coordinates_size(w, h);
        dst_x = 0;
        dst_y = 0;
        dst_w = w;
        dst_h = h;
    }
    
    GLdouble src_x, src_y, src_w, src_h;
    if (src_rect)
    {
        src_x = src_rect->x;
        src_y = src_rect->y;
        src_w = src_rect->w;
        src_h = src_rect->h;
    }
    else
    {
        src_x = 0;
        src_y = 0;
        src_w = m_surface->w;
        src_h = m_surface->h;
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // disable everything but alpha blending and clipping
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
        
    bool rotating = (rotation > 0.1 || rotation < -0.1);
    if (rotating)
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef((dst_x + dst_w / 2.0), (dst_y + dst_h / 2.0), 0.0);
        glRotatef(rotation, 0.0, 0.0, 1.0);
        glTranslatef(-(dst_x + dst_w / 2.0), -(dst_y + dst_h / 2.0), 0.0);
    }
    
    glColor4f(tint_color_r, tint_color_g, tint_color_b, tint_color_a);
    
    for (const auto& tile : m_tiles)
    {
        // if src rect lies completely outside this texture tile, skip it...
        if (src_x > tile.rect.x + tile.rect.w || src_x + src_w < tile.rect.x ||
            src_y > tile.rect.y + tile.rect.h || src_y + src_h < tile.rect.y) continue;
        
        // ...otherwise, work out what part of the texture tile to render
        GLdouble tx = MAX(0,           src_x - tile.rect.x);
        GLdouble ty = MAX(0,           src_y - tile.rect.y);
        GLdouble tw = MIN(tile.rect.w, src_x + src_w - tile.rect.x) - tx;
        GLdouble th = MIN(tile.rect.h, src_y + src_h - tile.rect.y) - ty;
        
        GLdouble VMin = tx        / (GLdouble)m_tile_width;
        GLdouble VMax = (tx + tw) / (GLdouble)m_tile_width;
        GLdouble UMin = ty        / (GLdouble)m_tile_height;
        GLdouble UMax = (ty + th) / (GLdouble)m_tile_height;
        
        GLdouble tleft   = (tile.rect.x + tx) + (GLdouble)(dst_x - src_x);
        GLdouble tright  = tleft + tw;
        GLdouble ttop    = (tile.rect.y + ty) + (GLdouble)(dst_y - src_y);
        GLdouble tbottom = ttop + th;
        
        glBindTexture(GL_TEXTURE_2D, tile.ref);
        
        OGL_RenderTexturedRect(tleft, ttop, tright - tleft, tbottom - ttop, VMin, UMin, VMax, UMax);
    }
    
    if (rotating) glPopMatrix();
    glPopAttrib();
    printf("ImageBlitter::render_to_screen\n");
    main_screen.request_swap();
}




//-----------------------------------------------------------------------------
// GPU texture management
//
// On first call to `render_to_screen`, the Surface is lazily copied to one or more GPU textures.
// These textures remain in GPU memory until `unload` is called.


// Surfaces larger than OGL_TEXTURE_SIZE - MARGINS must be split into multiple Textures,
// which ImageBlitter::render_to_screen will tile back together when rendering to screen.
void ImageBlitter::create_texture_tiles()
{
    // EES: I'm guessing this is because Apple's OGL doesn't work if a GPU texture is too small? (ISTR textures smaller than 16px[?] not rendering on screen.)
    // This should NOT be a user setting though: AO needs to check on startup if the host system requires textures of a minimum size and set this flag automatically if it does.
    int32_t min_size = (graphics_preferences->OGL_Configure.Flags & OGL_Flag_TextureFix) ? 128 : 32;
    
    // this will be pretty wasteful if a Surface is just slightly larger than OGL_TEXTURE_SIZE, but it's more effort to make the rightmost/bottommost tiles narrower than the rest and, in practice, Surfaces larger than 2048px should be fairly rare outside of HD chapter screens
    m_tile_width  = std::clamp(NextPowerOfTwo(m_surface->w), min_size, OGL_TEXTURE_SIZE);
    m_tile_height = std::clamp(NextPowerOfTwo(m_surface->h), min_size, OGL_TEXTURE_SIZE);
    
    // calculate how many rects we need
    int32_t v_rects = ((m_surface->h + m_tile_height - 1) / m_tile_height);
    int32_t h_rects = ((m_surface->w + m_tile_width - 1) / m_tile_width);
    m_tiles.resize(v_rects * h_rects);
    
    glEnable(GL_TEXTURE_2D);
    
    
    // If the Surface fits a single tile and its dimensions are both power of 2, we don't need a tmp surface or edge smearing.
    // (We could also bypass tmp Surface when a texture is rendered non-repeating and doesn't require edge smearing,
    // but not going to support that right now as it requires ImageBlitter to know in advance how an image will be used.)
    if (m_surface->w == m_tile_width && m_surface->h == m_tile_height)
    {
        texture_tile_t& tile = m_tiles[0];
        glGenTextures(1, &tile.ref);
        glBindTexture(GL_TEXTURE_2D, tile.ref);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, near_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_tile_width, m_tile_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_surface->pixels);
    }
    else
    {
        // glTexImage2D needs contiguous bytes so if there's >1 tile we create a temporary Surface the same
        // size as the GPU texture, blit part of the original Surface to it, then modify pixels at its edges.
        SDL_Surface* tmp = CreateSDLSurface(m_tile_width, m_tile_height);
        
        uint32_t rgb_mask = ~(tmp->format->Amask);
        
        int32_t index = 0;
        for (int32_t y = 0; y < v_rects; y++)
        {
            texture_tile_t& tile = m_tiles[index];
            for (int32_t x = 0; x < h_rects; x++)
            {
                tile.rect.x = x * m_tile_width;
                tile.rect.y = y * m_tile_height;
                tile.rect.w = std::min(m_tile_width, static_cast<int>(m_surface->w - x * m_tile_width));
                tile.rect.h = std::min(m_tile_height, static_cast<int>(m_surface->h - y * m_tile_height));
                
                SDL_Rect sr = { tile.rect.x, tile.rect.y, tile.rect.w, tile.rect.h };
                SDL_BlitSurface(m_surface, &sr, tmp, NULL);
                
                // to avoid edge artifacts, smear edge pixels out to texture boundary
                for (int row = 0; row < tile.rect.h; ++row)
                {
                    uint32 *curRow = static_cast<uint32 *>(tmp->pixels) + (row * m_tile_width);
                    for (int col = tile.rect.w; col < m_tile_width; ++col)
                    {
                        curRow[col] = curRow[tile.rect.w - 1] & rgb_mask;
                    }
                }
                
                uint32 *lastRow = static_cast<uint32 *>(tmp->pixels) + ((tile.rect.h - 1) * m_tile_width);
                for (int row = tile.rect.h; row < m_tile_height; ++row)
                {
                    uint32 *curRow = static_cast<uint32 *>(tmp->pixels) + (row * m_tile_width);
                    for (int col = 0; col < m_tile_width; ++col)
                    {
                        curRow[col] = lastRow[col] & rgb_mask;
                    }
                }
                
                glGenTextures(1, &tile.ref);
                glBindTexture(GL_TEXTURE_2D, tile.ref);
                
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, near_filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_tile_width, m_tile_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp->pixels);
                
                index++;
            }
        }
        
        SDL_FreeSurface(tmp);
    }
    m_blitter_registry.insert(this); // ensure our GPU textures get cleaned up
}


void ImageBlitter::unload()
{
    if (!m_tiles.empty())
    {
        for (const auto& tile : m_tiles) { glDeleteTextures(1, &tile.ref); }
        m_tiles.clear();
        m_blitter_registry.erase(this);
    }
    if (owns_surface)
    {
        SDL_FreeSurface(m_surface);
        m_surface = nullptr;
    }
}


//

void ImageBlitter::unload_all() // class method // TODO: when should unload_all be called?
{
    log_note("Unloading all ImageBlitter textures.");
    while (!m_blitter_registry.empty()) { (*m_blitter_registry.begin())->unload(); }
}

