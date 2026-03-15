/*
 cseries.h

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

#ifndef __cseries_h__
#define __cseries_h__


#include "cstypes.h"
#include "cserr.hpp"

#include "csmacros.h"
#include "cscluts.h"
#include "cspixels.h"
#include "csalerts.hpp"

#include "byte_swapping.h"


#include "cspaths.hpp"
#include "csmisc.h"

#include "csstrings.hpp"
#include "string_resources.hpp"
#include "string_resources_std.hpp"


#ifdef __MACOSX__
// if we're on the right platform, we can use the real thing (and get headers for functions we might want to use)
#include <CoreFoundation/CoreFoundation.h>
#else

struct Rect
{
	int16 top, left;
	int16 bottom, right;
};

#endif


constexpr Rect MakeRect(int16 top, int16 left, int16 bottom, int16 right)
{
    return {top, left, bottom, right};
}


constexpr Rect MakeRect(SDL_Rect r)
{
    return {int16(r.y), int16(r.x), int16(r.y + r.h), int16(r.x + r.w)};
}


struct RGBColor
{
	uint16 red, green, blue;
};


#endif /* __cseries_h__ */
