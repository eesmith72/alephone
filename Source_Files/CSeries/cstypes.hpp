/*
 cstypes.hpp
 
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


// TODO: is HAVE_CONFIG_H ever not used?
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
#include <cstring>
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
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
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



// from interface colors enum (there are 8 team/player colors, 6-14); leaving it here for now
#define PLAYER_COLOR_BASE_INDEX  (6)



// Endianess (MacOS9 ran on big-endian 68K and PPC, so M2 data files are all BE-based)

#if SDL_BYTEORDER == SDL_LIL_ENDIAN

#define ALEPHONE_LITTLE_ENDIAN 1

// TODO: get rid of this stupid thing. (The compiler will optimize out `if (PlatformIsLittleEndian()) {...} else {...}` statements, but it should be `#ifdef ALEPHONE_LITTLE_ENDIAN ... #else ... #endif` to make its intentions obvious. Using it like a runtime function in conditionals just makes modern compilers emit annoying 'code will never be executed' warnings.)
#define PlatformIsLittleEndian()  (true)


// swap an array of big-endian integers in-place, if needed; replaces byte_swapping.h

// comment from byte_swapping.cpp: count is int because it can become negative in the code
// EES: byte_swapping.cpp did nothing if count<=0 so we've replicated that behavior here
inline void swap_array_BE16(uint16_t* ptr, int32_t count)
{
    for (int32_t i = 0; i < count; i++) { ptr[i] = SDL_SwapBE16(ptr[i]); }
}

inline void swap_array_BE32(uint32_t* ptr, int32_t count)
{
    for (int32_t i = 0; i < count; i++) { ptr[i] = SDL_SwapBE32(ptr[i]); }
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



// belongs to csalerts.hpp, but defined here to avoid a circular #include between csalerts and string_resources

enum class alert_level_t : int32_t
{
    // 'debug' and 'bug' levels might also be nice, but that's a job for another time
    info,
    error, // recoverable // TODO: Mac/Win dialogs title this "WARNING", which might be confusing
    fatal, // "Frog-blast the vent-core!"
};



constexpr bool operator==(SDL_Color c1, SDL_Color c2) { return (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a); }
constexpr bool operator!=(SDL_Color c1, SDL_Color c2) { return !(c1 == c2); }




// TODO: migrate to SDL_Rect
struct screen_rectangle
{
    short top, left, bottom, right;
};


//-----------------------------------------------------------------------------
// from cspixels.h


typedef uint8 pixel8;
typedef uint16 pixel16;
typedef uint32 pixel32;

#define PIXEL8_MAXIMUM_COLORS 256
#define PIXEL16_MAXIMUM_COMPONENT 31
#define PIXEL32_MAXIMUM_COMPONENT 255
#define NUMBER_OF_COLOR_COMPONENTS 3

/*
    note that the combiner macros expect input values in the range
        0x0000 through 0xFFFF
    while the extractor macros return output values in the ranges
        0x00 through 0x1F (in the 16-bit case)
        0x00 through 0xFF (in the 32-bit case)
 */

#define RGBCOLOR_TO_PIXEL16(r,g,b) (((r)>>1&0x7C00) | ((g)>>6&0x03E0) | ((b)>>11&0x001F))
#define RED16(p) ((p)>>10&0x1F)
#define GREEN16(p) ((p)>>5&0x1F)
#define BLUE16(p) ((p)&0x1F)

#define RGBCOLOR_TO_PIXEL32(r,g,b) (((r)<<8&0x00FF0000) | ((g)&0x00000FF00) | ((b)>>8&0x000000FF))
#define RED32(p) ((p)>>16&0xFF)
#define GREEN32(p) ((p)>>8&0xFF)
#define BLUE32(p) ((p)&0xFF)


//-----------------------------------------------------------------------------
// these types were originally defined in world.h and used all over; moved them here to simplify #includes


typedef int16 angle;
typedef _fixed fixed_angle; // angle with _fixed precision
typedef int16 world_distance;


/* ---------- int32 (long_...) and int16 (world_...) vectors and points */

// Conversions:
//     world_ to long_:  implicit
//     long_ to world_:  use to_world() (truncates)
//     3D to 2D:         use .ij() or .xy()
//     2D to 3D:         no shortcut currently
//     vector to point:  write long_pointNd{} + vec
//     point to vector:  write pt - long_pointNd{}
// Math ops:
//                 -vector  ->  long_vector
//     vector {+,-} vector  ->  long_vector
//         scalar * vector  ->  long_vector
//           point - point  ->  long_vector
//      point {+,-} vector  ->  long_point
//          vector + point  ->  unsupported (other way around is clearer)
//     long_ types support compound assignment
//     no guards against int32 overflow


struct long_vector2d
{
    int32 i, j;
    constexpr auto& operator+=(long_vector2d b) { i += b.i; j += b.j; return *this; }
    constexpr auto& operator-=(long_vector2d b) { i -= b.i; j -= b.j; return *this; }
    template <class S> constexpr auto& operator*=(S s) { return (*this = {int32(s*i), int32(s*j)}); }
};


struct long_vector3d
{
    int32 i, j, k;
    constexpr auto& operator+=(long_vector3d b) { i += b.i; j += b.j; k += b.k; return *this; }
    constexpr auto& operator-=(long_vector3d b) { i -= b.i; j -= b.j; k -= b.k; return *this; }
    template <class S> constexpr auto& operator*=(S s) { return (*this = {int32(s*i), int32(s*j), int32(s*k)}); }
    constexpr auto ij() const { return long_vector2d{i, j}; }
};


struct long_point2d
{
    int32 x, y;
    constexpr auto& operator+=(long_vector2d v) { x += v.i; y += v.j; return *this; }
    constexpr auto& operator-=(long_vector2d v) { x -= v.i; y -= v.j; return *this; }
};


struct long_point3d
{
    int32 x, y, z;
    constexpr auto& operator+=(long_vector3d v) { x += v.i; y += v.j; z += v.k; return *this; }
    constexpr auto& operator-=(long_vector3d v) { x -= v.i; y -= v.j; z -= v.k; return *this; }
    constexpr auto xy() const { return long_point2d{x, y}; }
};


struct world_vector2d
{
    world_distance i, j;
    /*implicit*/ constexpr operator long_vector2d() const { return {i, j}; }
};


struct world_vector3d
{
    world_distance i, j, k;
    /*implicit*/ constexpr operator long_vector3d() const { return {i, j, k}; }
    constexpr auto ij() const { return world_vector2d{i, j}; }
};


struct world_point2d
{
    world_distance x, y;
    /*implicit*/ constexpr operator long_point2d() const { return {x, y}; }
};


struct world_point3d
{
    world_distance x, y, z;
    /*implicit*/ constexpr operator long_point3d() const { return {x, y, z}; }
    constexpr auto xy() const { return world_point2d{x, y}; }
};


// world_ operands promote
constexpr bool operator==(long_vector2d a, long_vector2d b) { return a.i == b.i && a.j == b.j; }
constexpr bool operator==(long_vector3d a, long_vector3d b) { return a.i == b.i && a.j == b.j && a.k == b.k; }
constexpr bool operator==(long_point2d a, long_point2d b) { return a.x == b.x && a.y == b.y; }
constexpr bool operator==(long_point3d a, long_point3d b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
constexpr bool operator!=(long_vector2d a, long_vector2d b) { return !(a == b); }
constexpr bool operator!=(long_vector3d a, long_vector3d b) { return !(a == b); }
constexpr bool operator!=(long_point2d a, long_point2d b) { return !(a == b); }
constexpr bool operator!=(long_point3d a, long_point3d b) { return !(a == b); }
constexpr auto operator+(long_vector2d a, long_vector2d b) { return a += b; }
constexpr auto operator+(long_vector3d a, long_vector3d b) { return a += b; }
constexpr auto operator-(long_vector2d a, long_vector2d b) { return a -= b; }
constexpr auto operator-(long_vector3d a, long_vector3d b) { return a -= b; }
constexpr auto operator-(long_vector2d v) { return long_vector2d{} - v; }
constexpr auto operator-(long_vector3d v) { return long_vector3d{} - v; }
template <class S> constexpr auto operator*(S s, long_vector2d v) { return v *= s; }
template <class S> constexpr auto operator*(S s, long_vector3d v) { return v *= s; }
constexpr auto operator-(long_point2d a, long_point2d b) { return long_vector2d{a.x - b.x, a.y - b.y}; }
constexpr auto operator-(long_point3d a, long_point3d b) { return long_vector3d{a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr auto operator+(long_point2d p, long_vector2d v) { return p += v; }
constexpr auto operator+(long_point3d p, long_vector3d v) { return p += v; }
constexpr auto operator-(long_point2d p, long_vector2d v) { return p -= v; }
constexpr auto operator-(long_point3d p, long_vector3d v) { return p -= v; }

constexpr auto to_world(long_vector2d v) { return world_vector2d{int16(v.i), int16(v.j)}; }
constexpr auto to_world(long_vector3d v) { return world_vector3d{int16(v.i), int16(v.j), int16(v.k)}; }
constexpr auto to_world(long_point2d p) { return world_point2d{int16(p.x), int16(p.y)}; }
constexpr auto to_world(long_point3d p) { return world_point3d{int16(p.x), int16(p.y), int16(p.z)}; }


/* ---------- fixed-point vectors and points */

struct fixed_vector3d
{
    _fixed i, j, k;
};


struct fixed_point3d
{
    _fixed x, y, z;
};


/* ---------- angle structures */

// A relative or (possibly non-normalized) absolute direction
struct fixed_yaw_pitch { fixed_angle yaw, pitch; };


/* ---------- locations */

struct world_location3d
{
    world_point3d point;
    short polygon_index;
    
    angle yaw, pitch;

    world_vector3d velocity;

    bool operator==(const world_location3d& other) const {
        return std::tie(pitch, yaw, polygon_index, point, velocity) == std::tie(other.pitch, other.yaw, other.polygon_index, other.point, other.velocity);
    }

    bool operator!=(const world_location3d& other) const {
        return !(*(this) == other);
    }
};
typedef struct world_location3d world_location3d;


#endif
