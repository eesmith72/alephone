/*
 cseries.hpp -- umbrella header for basic support features
 
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

#ifndef cseries_hpp
#define cseries_hpp


#include "cstypes.hpp"
#include "cserr.hpp"

#include "csmacros.h"
#include "cscluts.h"
#include "csalerts.hpp"

#include "csrandom.hpp"
#include "cspaths.hpp"
#include "cstimes.hpp"

#include "csstrings.hpp"
#include "string_resources.hpp"
#include "string_resources_std.hpp"


#ifdef __MACOSX__
#include <CoreFoundation/CoreFoundation.h>
#endif



// slowly standardizing; if we're really worried about memory-allocating functions failing, wrap them like this:

// TODO: find and update remaining mallocs in code to use this as this eliminates individual null-checks for simpler code

inline uint8_t* ao_malloc(size_t size)
{
    uint8_t* bytes = (uint8_t*)malloc(size);
    if (!bytes) { exit(STRID(strDEBUG, db_out_of_memory)); }
    return bytes;
}


inline uint8_t* ao_calloc(size_t count, size_t size) // note: at least some calls to this are unnecessary as all bytes are subsequently written (e.g. by memcpy) but not going to figure out which zeroings are necessary and which are idiot makework right now
{
    uint8_t* bytes = (uint8_t*)calloc(count, size);
    if (!bytes) { exit(STRID(strDEBUG, db_out_of_memory)); }
    return bytes;
}


inline SDL_Surface* CreateSDLSurface(int32_t w, int32_t h)
{
    SDL_Surface* surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, SDLRGBSurfaceBitmask);
    if (!surface) { exit(STRID(strDEBUG, db_out_of_memory)); }
    return surface;
}


#endif /* cseries_hpp */
