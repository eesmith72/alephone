/*
 text_renderer.hpp
 
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

#ifndef text_renderer_hpp
#define text_renderer_hpp

#include "terminal_support.hpp"


/*
 
 need to move M1 terminal parser call into 'term' resource reader, plus MacRoman-to-UTF8 conversion - the conversion means that we need to copy the original string anyway so we can just as easily allocate one string per group; OTOH, new terms will read the UTF8 string from disk so still convenient for them
 
 max bytesize of UTF8 = 3x MR bytesize, plus NUL terminator
 
 similarly, MR2UTF8 goes in M2 deserialization; when we reserialize, we should use our own format
 
 Q. should we bother to retain the strings, or should we render straight to Surface? straight to surface is probably easiest - it's resolution-dependent so if user changes resolution or terminal size (this will be rare!) then we just reload terms from file
 
 
 we want to create a HD SDL_Surface for each group, with known width and unknown length (but we'll tile 2048 max)
 
 
 probably easiest to implement separate readers for M1, M2, UTF8 that yield a single unichr
 
 
 if AO used SDL_gpu to draw 3D, we could use SDL_fox as-is; since we're stuck with OGL drawing, we can't use an SDL_Renderer so SDL_fox needs modified to use Surfaces, at least until a sensible 2D drawing API is created
 
 there is FontSpecifier::OGL_Render, but that doesn't play with SW rendering so would need to decide if translations work in HW rendering only; plus it's written by LP so will be an absolute warren
 
 drawing to Surface is CPU, so it's slow, but I doubt that will be noticeable so eat any inefficiencies for sake of implementing simplest thing that works; a portable API can always be extracted later

 */



#endif /* text_renderer_hpp */
