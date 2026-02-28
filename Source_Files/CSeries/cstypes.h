/*
 cstypes.h
 
 Copyright (C) 1991-2001 and beyond by Bo Lindbergh
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

#ifndef _CSERIES_TYPES_
#define _CSERIES_TYPES_


// pick up HAVE_OPENGL
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define VERSION "unknown version"
#endif




// IR note: consts in headers are slow and eat TOC space.
//const int NONE = -1;
enum {
	NONE = -1,
	UNONE = 65535
};



#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    // for time_t


#include <algorithm> // e.g. std::min
#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory> // unique_ptr
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>


// SDL is used in several cs*.cpp files so put it in cstypes which is available to everything
#include <SDL2/SDL.h>
#include <SDL2/SDL_endian.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_types.h>



#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
//#include <boost/format.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/unordered_map.hpp>


#if defined(__WIN32__)
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>

#else
#include <sys/wait.h>
#endif




// Endianess (MacOS9 ran on big-endian 68K and PPC, so M2 data files are all BE-based)

#if SDL_BYTEORDER == SDL_LIL_ENDIAN

// TODO: think it's safe to use __ORDER_LITTLE_ENDIAN__ nowadays
#define ALEPHONE_LITTLE_ENDIAN 1

// TODO: the compiler will optimize out all the dumb `if (PlatformIsLittleEndian()) {...} else {...}`, but it should've been a macro to begin with to make its intentions obvious. The only thinb making it look like a runtime test does is encourage modern compilers to emit aggravating 'code will never be executed' warnings. Almost all of it's just swapping bitmasks. It may be simplest to write (e.g) `SDL_Swap32BE(0x00ffff)` to flip a single bitmask, and define our own SDL_CreateRGBSurface_BE(...) macro for the 80% of times it's swapping the RGBA masks for that.
// absolute piece of crap used all over the place
#define PlatformIsLittleEndian()  (true)


// TODO: these swap_array_ functions *should* be sufficient to replace byte_swap_memory in byte_swapping.h/.cpp; in practice there's a comment in byte_swapping.cpp which suggests `count` may be negative(!) so that nonsense needs straightened out first (a simple std::max(0,n) before calling swap_ should do it, though it'd be better to put a proper bounds-check on it as a negative index strongly suggests nasty buffer overrun potential; obviously using modern size_t for all array indexing won't happen any time soon given half of AO is still rooted in M2's antiquated [u]int16s...)

inline void swap_array_BE16(uint16_t* ptr, size_t count)
{
 for (size_t i = 0; i < count; i++) { ptr[i] = SDL_SwapBE16(ptr[i]); }
}

inline void swap_array_BE32(uint32_t* ptr, size_t count)
{
 for (size_t i = 0; i < count; i++) { ptr[i] = SDL_SwapBE16(ptr[i]); }
}

#else /* big-endian; who still uses that?! */

#undef ALEPHONE_LITTLE_ENDIAN

#define PlatformIsLittleEndian()  (false)

#define swap_array_BE16(ptr, count)  ((void)0)
#define swap_array_BE32(ptr, count)  ((void)0)



#endif







enum class alert_level_t : int32_t
{
    // 'debug' and 'bug' levels might also be nice, but that's a job for another time
    info,
    error, // recoverable // TODO: Mac/Win dialogs title this "WARNING", which might be confusing
    fatal, // "Frog-blast the vent-core!"
};






// Integer types with specific bit size
typedef Uint8 uint8;
typedef Sint8 int8;
typedef Uint16 uint16;
typedef Sint16 int16;
typedef Uint32 uint32;
typedef Sint32 int32;
typedef time_t TimeType;


// Minimum and maximum values for these types
#ifndef INT16_MAX
#define INT16_MAX 32767
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif
#ifndef INT16_MIN
#define INT16_MIN (-INT16_MAX-1)
#endif
#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif
#ifndef INT32_MIN
#define INT32_MIN (-INT32_MAX-1)
#endif

// Fixed point (16.16) type
// LP: changed to _fixed to get around MSVC namespace conflict
typedef int32 _fixed;

#define FIXED_FRACTIONAL_BITS 16
#define INTEGER_TO_FIXED(i) ((_fixed)(i)<<FIXED_FRACTIONAL_BITS)
#define FIXED_INTEGERAL_PART(f) ((f)>>FIXED_FRACTIONAL_BITS)

#define FIXED_ONE		(1L<<FIXED_FRACTIONAL_BITS)
#define FIXED_ONE_HALF	(1L<<(FIXED_FRACTIONAL_BITS-1))

// Binary powers
const int MEG = 0x100000;
const int KILO = 0x400L;

// Construct four-character-code
#define FOUR_CHARS_TO_INT(a,b,c,d) (((uint32)(a) << 24) | ((uint32)(b) << 16) | ((uint32)(c) << 8) | (uint32)(d))

// Hmmm, this should be removed one day...
typedef uint8 byte;

// Make it compile on systems without OpenGL
#ifndef HAVE_OPENGL
#define GLfloat float
#endif

#endif
