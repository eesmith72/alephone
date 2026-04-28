/*
 cscluts_sdl.cpp - CLUT handling, SDL implementation
 
 Written in 2000 by Christian Bauer
 
 Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
 and the "Aleph One" developers.
 
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
 */

#include "cseries.h"


const rgb_color rgb_black = {0x0000, 0x0000, 0x0000};
const rgb_color rgb_white = {0xffff, 0xffff, 0xffff};


// TODO: why aren't these static-allocated? are they variable length?

struct color_table* uncorrected_color_table = nullptr; // the pristine color environment of the game (can be 16bit)
struct color_table* world_color_table       = nullptr; // the gamma-corrected color environment of the game (can be 16bit)
struct color_table* interface_color_table   = nullptr; // always 8bit, for mixed-mode (i.e., valkyrie) fades
struct color_table* visible_color_table     = nullptr; // the color environment the player sees (can be 16bit)


// EES: saints preserve us... these do eventually get initialized right, way over in Screen::Initialize; also get used in images.cpp when finding best chapter screen pict to display
//short interface_bit_depth = 32;



// from screen.cpp

SDL_PixelFormat pixel_format_16, pixel_format_32;


void initialize_cluts()
{
    SDL_PixelFormat *pf = SDL_AllocFormat(SDL_PIXELFORMAT_RGB565);
    pixel_format_16 = *pf; // only used by SW world renderer (including shapes.cpp) in 16-bit mode
    SDL_FreeFormat(pf);
    pf = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    pixel_format_32 = *pf;
    SDL_FreeFormat(pf);
    
    uncorrected_color_table = (struct color_table *)malloc(sizeof(struct color_table));
    world_color_table = (struct color_table *)malloc(sizeof(struct color_table));
    visible_color_table = (struct color_table *)malloc(sizeof(struct color_table));
    interface_color_table = (struct color_table *)malloc(sizeof(struct color_table));
    assert(uncorrected_color_table && world_color_table && visible_color_table && interface_color_table);
    memset(uncorrected_color_table, 0, sizeof(struct color_table));
    memset(world_color_table, 0, sizeof(struct color_table));
    memset(visible_color_table, 0, sizeof(struct color_table));
    memset(interface_color_table, 0, sizeof(struct color_table));
}


void build_sdl_color_table(const color_table* color_table, SDL_Color* colors)
{
    const rgb_color *src = color_table->colors;
    SDL_Color *dst = colors;
    for (int32_t i = 0; i < color_table->color_count; i++)
    {
        dst->r = src->red >> 8;
        dst->g = src->green >> 8;
        dst->b = src->blue >> 8;
        dst->a = 0xff;
        src++; dst++;
    }
}








// TODO: using clut for 8-bit gamma is problematic as we want to draw terminal, automap, and HUD using OGL (only the world view is drawn via M2 renderer) so this almost certainly has to go
// Initial gamma table
bool default_gamma_inited = false;
uint16 default_gamma_r[256];
uint16 default_gamma_g[256];
uint16 default_gamma_b[256];
uint16 current_gamma_r[256];
uint16 current_gamma_g[256];
uint16 current_gamma_b[256];
//bool using_default_gamma = true;


void initialize_gamma()
{
    if (!default_gamma_inited) {
        default_gamma_inited = true;
        for (int i = 0; i < 256; ++i) {
            default_gamma_r[i] = default_gamma_g[i] = default_gamma_b[i] = i << 8;
        }
        memcpy(current_gamma_r, default_gamma_r, sizeof(current_gamma_r));
        memcpy(current_gamma_g, default_gamma_g, sizeof(current_gamma_g));
        memcpy(current_gamma_b, default_gamma_b, sizeof(current_gamma_b));
    }
}



// dump this here temporarily
void change_gamma_level(short gamma_level)
{
    /*
    game_preferences->gamma_level = gamma_level;
    gamma_correct_color_table(uncorrected_color_table, world_color_table, gamma_level);
    stop_fade();
    obj_copy(*visible_color_table, *world_color_table);
    assert_world_color_table(interface_color_table, world_color_table);
    change_screen_mode(&screen_mode, false);
    set_fade_effect(NONE);
     */
}





// would be nice to know what this actually *does*; just an awful function name in absence of explanatory comments
void assert_world_color_table(struct color_table *interface_color_table, struct color_table *world_color_table)
{
    /*
    if (main_screen.bit_depth() == 8)
    {
        SDL_Color colors[256];
        build_sdl_color_table(interface_color_table, colors);
        
        if (world_pixels) { SDL_SetPaletteColors(world_pixels->format->palette, colors, 0, 256); }
    }
    if (world_color_table) { animate_screen_clut(world_color_table, false); }
     */
}


/*
 
 static void apply_gamma(SDL_Surface *src, SDL_Surface *dst)
 {
     if (SDL_MUSTLOCK(dst)) {
         if (SDL_LockSurface(dst) < 0) return;
     }
     uint32 px, dst_px;
     uint8 src_r, src_g, src_b;
     uint8 dst_r, dst_g, dst_b;
     
     uint32 srm = src->format->Rmask, sgm = src->format->Gmask, sbm = src->format->Bmask;
     uint32 drm = dst->format->Rmask, dgm = dst->format->Gmask, dbm = dst->format->Bmask;
     uint32 srs = src->format->Rshift, sgs = src->format->Gshift, sbs = src->format->Bshift;
     uint32 drs = dst->format->Rshift, dgs = dst->format->Gshift, dbs = dst->format->Bshift;
     uint32 srl = src->format->Rloss, sgl = src->format->Gloss, sbl = src->format->Bloss;
     uint32 drl = dst->format->Rloss, dgl = dst->format->Gloss, dbl = dst->format->Bloss;
     
     int sbpp = src->format->BytesPerPixel;
     int dbpp = dst->format->BytesPerPixel;
     uint8 *sptr = static_cast<uint8*>(src->pixels);
     uint8 *dptr = static_cast<uint8*>(dst->pixels);
     size_t numpixels = src->w * src->h;
     for (size_t i = 0; i < numpixels; ++i) {
         switch (sbpp) {
             case 2:
                 px = reinterpret_cast<uint16*>(sptr)[i];
                 break;
             case 4:
                 px = reinterpret_cast<uint32*>(sptr)[i];
                 break;
             default:
                 return;
         }
     
         src_r = ((px & srm) >> srs) << srl;
         src_g = ((px & sgm) >> sgs) << sgl;
         src_b = ((px & sbm) >> sbs) << sbl;
         dst_r = current_gamma_r[src_r] >> 8;
         dst_g = current_gamma_g[src_g] >> 8;
         dst_b = current_gamma_b[src_b] >> 8;
         dst_px = (((dst_r >> drl) << drs) & drm) |
                  (((dst_g >> dgl) << dgs) & dgm) |
                  (((dst_b >> dbl) << dbs) & dbm);
             
         switch (dbpp) {
             case 2:
                 reinterpret_cast<uint16*>(dptr)[i] = dst_px;
                 break;
             case 4:
                 reinterpret_cast<uint32*>(dptr)[i] = dst_px;
                 break;
             default:
                 return;
         }
     }
     if (SDL_MUSTLOCK(dst))
         SDL_UnlockSurface(dst);
 }

 */
