/*
 csstrings.cpp
 
 Copyright (C) 1991-2001 and beyond by Bo Lindbergh and the "Aleph One" developers.
 
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

#include "csstrings.hpp"

#include "cspaths.hpp"


// -----------------------------------------------------------------------------------------
// MacRoman-to-UTF8 converter


const std::array<std::string, 128> macroman_hi_chars = {
    "\xC3\x84",
    "\xC3\x85",
    "\xC3\x87",
    "\xC3\x89",
    "\xC3\x91",
    "\xC3\x96",
    "\xC3\x9C",
    "\xC3\xA1",
    "\xC3\xA0",
    "\xC3\xA2",
    "\xC3\xA4",
    "\xC3\xA3",
    "\xC3\xA5",
    "\xC3\xA7",
    "\xC3\xA9",
    "\xC3\xA8",
    
    "\xC3\xAA",
    "\xC3\xAB",
    "\xC3\xAD",
    "\xC3\xAC",
    "\xC3\xAE",
    "\xC3\xAF",
    "\xC3\xB1",
    "\xC3\xB3",
    "\xC3\xB2",
    "\xC3\xB4",
    "\xC3\xB6",
    "\xC3\xB5",
    "\xC3\xBA",
    "\xC3\xB9",
    "\xC3\xBB",
    "\xC3\xBC",
    
    "\xE2\x80\xA0",
    "\xC2\xB0",
    "\xC2\xA2",
    "\xC2\xA3",
    "\xC2\xA7",
    "\xE2\x80\xA2",
    "\xC2\xB6",
    "\xC3\x9F",
    "\xC2\xAE",
    "\xC2\xA9",
    "\xE2\x84\xA2",
    "\xC2\xB4",
    "\xC2\xA8",
    "\xE2\x89\xA0",
    "\xC3\x86",
    "\xC3\x98",
    
    "\xE2\x88\x9E",
    "\xC2\xB1",
    "\xE2\x89\xA4",
    "\xE2\x89\xA5",
    "\xC2\xA5",
    "\xC2\xB5",
    "\xE2\x88\x82",
    "\xE2\x88\x91",
    "\xE2\x88\x8F",
    "\xCF\x80",
    "\xE2\x88\xAB",
    "\xC2\xAA",
    "\xC2\xBA",
    "\xCE\xA9",
    "\xC3\xA6",
    "\xC3\xB8",
    
    "\xC2\xBF",
    "\xC2\xA1",
    "\xC2\xAC",
    "\xE2\x88\x9A",
    "\xC6\x92",
    "\xE2\x89\x88",
    "\xE2\x88\x86",
    "\xC2\xAB",
    "\xC2\xBB",
    "\xE2\x80\xA6",
    "\xC2\xA0",
    "\xC3\x80",
    "\xC3\x83",
    "\xC3\x95",
    "\xC5\x92",
    "\xC5\x93",
    
    "\xE2\x80\x93",
    "\xE2\x80\x94",
    "\xE2\x80\x9C",
    "\xE2\x80\x9D",
    "\xE2\x80\x98",
    "\xE2\x80\x99",
    "\xC3\xB7",
    "\xE2\x97\x8A",
    "\xC3\xBF",
    "\xC5\xB8",
    "\xE2\x81\x84",
    "\xE2\x82\xAC",
    "\xE2\x80\xB9",
    "\xE2\x80\xBA",
    "\xEF\xAC\x81",
    "\xEF\xAC\x82",
    
    "\xE2\x80\xA1",
    "\xC2\xB7",
    "\xE2\x80\x9A",
    "\xE2\x80\x9E",
    "\xE2\x80\xB0",
    "\xC3\x82",
    "\xC3\x8A",
    "\xC3\x81",
    "\xC3\x8B",
    "\xC3\x88",
    "\xC3\x8D",
    "\xC3\x8E",
    "\xC3\x8F",
    "\xC3\x8C",
    "\xC3\x93",
    "\xC3\x94",
    
    "?", // "\xEF\xA3\xBF" is private-area Apple logo, so just substitute it
    "\xC3\x92",
    "\xC3\x9A",
    "\xC3\x9B",
    "\xC3\x99",
    "\xC4\xB1",
    "\xCB\x86",
    "\xCB\x9C",
    "\xC2\xAF",
    "\xCB\x98",
    "\xCB\x99",
    "\xCB\x9A",
    "\xC2\xB8",
    "\xCB\x9D",
    "\xCB\x9B",
    "\xCB\x87",
};


// -----------------------------------------------------------------------------------------
// MacRoman-encoded strings are used in M1/M2 Maps' level names and computer terminal ('term') resources


const std::string convert_macroman_cstr_to_utf8_string(const char* cstr, int32_t max_bytes)
{
    std::string result;
    for (int32_t i = 0; i < max_bytes && *cstr; i++)
    {
        append_macroman_char_to_utf8_string(*cstr, result);
        cstr++;
    }
    return result;
}


inline char convert_utf8_char_to_macroman_hi_char(std::string& key)
{
    for (char i = 0; i < macroman_hi_chars.size(); i++)
    {
        if (key.compare(macroman_hi_chars[i]) == 0) { return i + 128; }
    }
    return 0; // no match
}


// Caution: this does not guard against malformed UTF8 data.
// UTF8 characters not in MacRoman will be substituted with '?'.
const std::string convert_utf8_string_to_macroman_string(const std::string utf8_string)
{
    std::string result;
    result.reserve(utf8_string.size());
    int32_t i = 0;
    while (i < utf8_string.size())
    {
        char c = utf8_string[i];
        if (c < utf8_1byte_bitmask) // 1-byte
        {
            result += c;
            i += 1;
        }
        else if (c >= utf8_4byte_bitmask) // top 4 bits = 4-byte
        {
            result += '?';
            i += 4;
        }
        else // top 3 bits (>=0xe0) = 3-byte, top 2 bits (>=0xc0) = 2-byte (<0xc0 = invalid)
        {
            int32_t size = c >= utf8_3byte_bitmask ? 3 : 2;
            std::string key = utf8_string.substr(i, size);
            char mrc = convert_utf8_char_to_macroman_hi_char(key);
            result += mrc ? mrc : '?';
            i += size;
        }
    }
    return result;
}


const void obfuscate_string(std::string& str)
{
    // pretty simple, e.g. an additional XOR of str[i]^str[i+1] would make it a bit more effort to decode, but since anyone can look at AO code to see how it's done it's entirely academic unless we have the user enter a string we don't have access to. Not gonna get bogged in this (authentication should be handled by the Metaserver; the user just needs it to supply UUIDs as tokens so the only place their password data exists is in the DB)
    static const std::array<char, 7> salt = {35, -14, 51, 7, -105, 127, -14};
    for (int32_t i = 0; i < str.size(); i++) { str[i] ^= salt[i % salt.size()]; }
}



// -----------------------------------------------------------------------------------------
// compatibility functions for Win32 dialog, env, fs APIs which use UCS2


#ifdef __WIN32__
static std::wstring utf8_to_wide(const char* utf8, int in_length)
{
	const int out_length = MultiByteToWideChar(CP_UTF8, 0, utf8, in_length, nullptr, 0); // >= 0
	std::wstring out(out_length, char{});
	MultiByteToWideChar(CP_UTF8, 0, utf8, in_length, &out[0], out_length);
	return out;
}

static std::string wide_to_utf8(const wchar_t* utf16, int in_length)
{
	const int out_length = WideCharToMultiByte(CP_UTF8, 0, utf16, in_length, nullptr, 0, nullptr, nullptr); // >= 0
	std::string out(out_length, char{});
	WideCharToMultiByte(CP_UTF8, 0, utf16, in_length, &out[0], out_length, nullptr, nullptr);
	return out;
}

std::wstring utf8_to_wide(const char* utf8)         { return utf8_to_wide(utf8, strlen(utf8)); }
std::wstring utf8_to_wide(const std::string& utf8)  { return utf8_to_wide(utf8.c_str(), utf8.size()); }
std::string wide_to_utf8(const wchar_t* utf16)      { return wide_to_utf8(utf16, wcslen(utf16)); }
std::string wide_to_utf8(const std::wstring& utf16) { return wide_to_utf8(utf16.c_str(), utf16.size()); }
#endif // __WIN32__

