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


#ifdef HAVE_UNISTD_H
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif


#include <algorithm> // e.g. std::min
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
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



// SDL is used in several cs*.cpp files as well as most everywhere else
#include <SDL2/SDL.h>
#include <SDL2/SDL_endian.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_types.h>

// TODO: is there any reason (e.g. licensing) why SDL_Image wouldn't always be included now? if not, lose the HAVE_SDL_IMAGE macro; ditto the HAVE_PNG macro
#ifdef HAVE_SDL_IMAGE
#include <SDL2/SDL_image.h>
#if defined(__WIN32__)
#include "alephone32.xpm"
#elif !defined(__MACOSX__)
#include "alephone.xpm"
#endif
#endif


#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
//#include <boost/format.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>


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
 for (size_t i = 0; i < count; i++) { ptr[i] = SDL_SwapBE32(ptr[i]); }
}

#define SDLRGBSurfaceBitmask  0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000

#else /* SDL_BYTEORDER == SDL_BIG_ENDIAN (big-endian; who still uses that?!) */

#undef ALEPHONE_LITTLE_ENDIAN

#define PlatformIsLittleEndian()  (false)

#define swap_array_BE16(ptr, count)  ((void)0)
#define swap_array_BE32(ptr, count)  ((void)0)


#define SDLRGBSurfaceBitmask  0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#endif



// I am not convinced these add value over C ptrs and clear ownership
typedef std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> SDLWindowUniquePtr;
typedef std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> SDLSurfaceUniquePtr;



// IR note: consts in headers are slow and eat TOC space.
//const int NONE = -1;
enum {
    NONE = -1,
    UNONE = 65535
};



typedef std::filesystem::path ao_path;



// TODO: as part of the grand code cleanup, get rid of all these aliases and standardize on stdint.h (uint8_t, etc) and time_t types throughout; also replace `short`, `int`, `long`, etc
// Integer types with specific bit size
typedef Uint8 uint8;
typedef Sint8 int8;
typedef Uint16 uint16;
typedef Sint16 int16;
typedef Uint32 uint32;
typedef Sint32 int32;
typedef time_t TimeType;


// EES: I know `_t` suffixes are technically reserved for the "Official Standards" but they are just too damn useful in practice: e.g. `wad_header wad_header` is lousy legibility whereas `wad_header_t wad_header` instantly distinguishes type from var name. As long as our `NAME_t` typedefs are for AO-specific NAMEs that aren't likely to be Officially Used, we should be okay using them (e.g. `wad_header_t` and `wad_data_t` are safe but `fixed_t` is not). For structs which may in future be 'upgraded' to CPP classes (e.g. for inheritance and `public/protected/private:` access levels), convert their names to TitleCase now.

// Hmmm, this should be removed one day...; yup, I think `uint8_t*` is pretty obvious in meaning; the only reason to typedef it would be to clarify what kind of data it is and better protect against accidental mutation, e.g. `typedef uint8_t const* wad_data_t`, `typedef uint8_t* wad_data_mutable_t`
typedef uint8 byte;


// Fixed point (16.16) type
// LP: changed to _fixed to get around MSVC namespace conflict
typedef int32 _fixed; // TODO: see if stdfix.h defines a NAME_t for 16.16 Fixed; if it doesn't, decide a good name that isn't likely to conflict (btw, a leading underscore commonly indicates names reserved for compiler use; `fixed_` would've been better)


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


// Make it compile on systems without OpenGL
#ifndef HAVE_OPENGL
#define GLfloat float
#endif



// belongs to csalerts.hpp, but defined here to avoid a circular #include between csalerts and string_resources

enum class alert_level_t : int32_t
{
    // 'debug' and 'bug' levels might also be nice, but that's a job for another time
    info,
    error, // recoverable // TODO: Mac/Win dialogs title this "WARNING", which might be confusing
    fatal, // "Frog-blast the vent-core!"
};



constexpr bool operator==(SDL_Color c1, SDL_Color c2) { return (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b, c1.a == c2.a); }
constexpr bool operator!=(SDL_Color c1, SDL_Color c2) { return !(c1 == c2); }




// TODO: migrate to SDL_Rect
struct screen_rectangle
{
    short top, left, bottom, right;
};


#endif
