/*
 csstrings.hpp -- utility functions for modern UTF8 and legacy MacRoman
 
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

#ifndef __csstrings_hpp__
#define __csstrings_hpp__


#include "cstypes.h"
#include "cserr.hpp"


// -----------------------------------------------------------------------------------------
// Convert legacy (MacOS9-era) MacRoman strings (Map level and feature names, and computer terminal texts) to/from modern UTF8.

extern const std::array<std::string, 128> macroman_hi_chars;


// the top 4 bits in a uint8 indicate the number of bytes
#define utf8_4byte_bitmask (0xf0)
#define utf8_3byte_bitmask (0xe0)
#define utf8_2byte_bitmask (0xc0)
#define utf8_1byte_bitmask (0x80)

#define char_is_4byte_utf8(c) (((c) & utf8_4byte_bitmask) == utf8_4byte_bitmask)
#define char_is_3byte_utf8(c) (((c) & utf8_3byte_bitmask) == utf8_3byte_bitmask)
#define char_is_2byte_utf8(c) (((c) & utf8_2byte_bitmask) == utf8_2byte_bitmask)
#define char_is_1byte_utf8(c) (((c) & utf8_1byte_bitmask) == utf8_1byte_bitmask)

/*
 '....'
 
 '4...' = ok
 
 '.4..' = -3
 
 */

// Use this to copy a UTF8 std::string to a fixed-width C string buffer. Returns true if the entire string was copied;
// false if it was truncated to fit. The C string will be NUL-terminated after last complete codepoint.
inline bool copy_utf8_string_to_buffer(const std::string& str, char* buffer, size_t buffer_size) // max number of bytes, including NUL
{
    if (str.size() + 5 < buffer_size) // Unicode codepoints are 1-4 bytes in UTF8; if we know the last codepoint+NULL
    {
        strncpy(buffer, str.c_str(), buffer_size); // end of buffer is filled with NUL
        return true;
    }
    else // TODO: this will need checked
    {
        size_t trim; // allow for terminating NUL
        if      ((str[buffer_size - 3] & utf8_4byte_bitmask) >= utf8_4byte_bitmask)
            trim = 4; // the codepoint plus NUL needs 5 bytes to fit but there's only 4 left
        else if ((str[buffer_size - 2] & utf8_3byte_bitmask) >= utf8_3byte_bitmask) // this will also detect 4-byte
            trim = 3;
        else if ((str[buffer_size - 1] & utf8_2byte_bitmask) >= utf8_2byte_bitmask) // this will also detect 4-byte and 3-byte
            trim = 2;
        else
            trim = 1;
        memset(buffer + buffer_size - trim, 0, trim);
        strncpy(buffer, str.c_str(), buffer_size - trim);
        return false;
    }
}


// Caution: This discards non-printing ASCII characters except Space, Tab (replaced with 2 spaces),
// CR (replaced with LF), and LF. This is just some simple sanitation with shouldn't affect anything.
// It neither validates nor sanitizes non-ASCII content: that's much more complicated to do right so
// we'll assume new UTF8-encoded data files will be correctly prepared by their authors.
inline void append_macroman_char_to_utf8_string(uint8_t c, std::string& result)
{
    if (c < ' ')
    {
        switch (c)
        {
            case '\r':
            case '\n':
                result += '\n';
                break;
            case '\t':
                result += "  ";
                break;
        }
    }
    else if (c < 127)
    {
        result += c;
    }
    else if (c >= 128)
    {
        result += macroman_hi_chars[c - 128];
    }
}


// Note: MMLs *should* always be UTF8-encoded XML so, with luck, only level and landmark names will need converted from/to MacRoman when loading/saving Maps. (Migrating those to UTF8 can be done the next time MAP_VERSION increases.)

// Convert a legacy MacRoman string (e.g. level name) which may or may not be NUL-terminated to UTF8-encoded string.
// Stops reading when it finds a NUL or when max_bytes is reached. (The resulting std::string does not include the NUL as ::c_str adds NUL automatically.)
const std::string convert_macroman_cstr_to_utf8_string(const char* cstr, int32_t max_bytes = 255);

const std::string convert_utf8_string_to_macroman_string(const std::string utf8_string); // should only be needed when saving Map levels


// If the resulting MacRoman C string is shorter than buffer_size-1, the remaining buffer is filled with NUL.
// Otherwise, the string is copied up to buffer_size-1 and the final byte is filled with NUL. This behavior
// is slightly different to the original which I think accepted up to 63 bytes plus NUL, or 64 bytes without,
// but ensuring the returned C string is *always* NUL-terminated is safest.
// TODO: Storing arbitrary-length UTF8-encoded level names in a tagged WAD resource is for another day.
inline void convert_utf8_string_to_macroman_cstr(const std::string utf8_string, char* buffer, int32_t buffer_size)
{
    std::string tmp = convert_utf8_string_to_macroman_string(utf8_string);
    strncpy(buffer, tmp.data(), buffer_size - 1); // strncpy automatically fills the trailing byte(s) with NUL
}


inline uint32_t convert_utf8_string_to_four_char_code(const std::string& s) // WAD tags, e.g. 'term'
{
    // while I think all tags are representable as 4x printable ASCII chars, best to be safe and do it correctly
    std::string mr_string = convert_utf8_string_to_macroman_string(s);
    if (mr_string.size() != 4) return 0; // TODO: should probably throw here
    
     return FOUR_CHARS_TO_INT(mr_string[0], mr_string[1], mr_string[2], mr_string[3]);
}


const void obfuscate_string(std::string& str);


inline std::string pad_2digit_string(int32_t n)
{
    return ((n >= 0 && n < 10) ? "0" : "") + std::to_string(n);
}


// -----------------------------------------------------------------------------------------
// compatibility functions for Win32 dialog, env, fs APIs which use UCS2


#ifdef __WIN32__
std::wstring utf8_to_wide(const char* utf8);
std::wstring utf8_to_wide(const std::string& utf8);
std::string wide_to_utf8(const wchar_t* utf16);
std::string wide_to_utf8(const std::wstring& utf16);
#endif


#endif /* __csstrings_hpp__ */
