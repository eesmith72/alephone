/*
 DataFile.hpp
 
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

#ifndef __DataFile_h__
#define __DataFile_h__


#include "cseries.hpp"

#include <boost/iostreams/categories.hpp> // used in OpenedFileDevice
#include <boost/iostreams/positioning.hpp>


// -----------------------------------------------------------------------------------------


class OpenedFileDevice;

// A data file opened for reading (and optionally writing); a convenience OO wrapper around SDL_RWops C APIs
class DataFile
{
	friend class OpenedFileDevice;
	
public:
    
    inline static const char* mode_binary_read  = "rb";
    inline static const char* mode_binary_write = "wb+";
    inline static const char* mode_text_read    = "r";
    inline static const char* mode_text_write   = "w+";
    
    // TODO: copying shouldn't happen, given that fh is single owner
    DataFile(const DataFile& df) : fh(df.fh), current_path(df.current_path), current_mode(df.current_mode), is_forked(df.is_forked), fork_offset(df.fork_offset), fork_length(df.fork_length) {
        log_note_f("%p copied DataFile '%s'", this, current_path.c_str());
    }
    
    DataFile() : fh(nullptr), current_path(""), current_mode(""), is_forked(false), fork_offset(0), fork_length(0) {}
    ~DataFile() { close(); }
    
    // since constructors don't return values, declare first (global/stack), then call open() and check for no_err or error
    ao_err open(const ao_path& path, const char* mode = mode_binary_read);
    
	void close();

    bool is_open() { return fh != nullptr; } // the illogic of an 'DataFile' class that can be closed...
    
    ao_err reopen() { return open(current_path, current_mode); }
    
    // these throw exceptions on failure (EES: let's see how these compare to returning error codes, which gets tedious when errors are not expected)
    int64_t get_length() const;
    int64_t get_position() const;
    void set_position(int64_t Position);
	
    void read(int64_t count, void* buffer);
    void write(int64_t count, const void* buffer);
    
    // these are big-endian for reading legacy M2/AO files
    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    
    int8_t  read_i8();
    int16_t read_i16();
    int32_t read_i32();
    int64_t read_i64();
    
    void skip(int64_t number_of_bytes); // seek relative to current position
    
    
    // not ideal, but going to live with it
	SDL_RWops* borrow_rwops()  // used by OpenFileDevice and others
    {
        assert_fail_f(fh != nullptr, "%p can't borrow nullptr", this);
        return fh;
    }
    int64_t get_fork_offset() { return fork_offset; } // used by OpenFileDevice
    
	SDL_RWops* take_rwops() // Hand over SDL_RWops; in `SndfileDecoder::Open` and `load_ttf_font`
    {
        SDL_RWops* ptr = fh;
        fh = nullptr;
        close();
        return ptr;
    }
    
    ao_path get_path() { return current_path; }
    
    uint32_t get_map_checksum() // sticking this here for now
    {
        ao_err err = no_err;
        int64_t tmp = get_position();
        set_position(0x44);
        uint32_t checksum = SDL_ReadBE32(fh);
        set_position(tmp);
        return checksum;
    }

    
protected:
    SDL_RWops* fh;
    int64_t length;
    bool is_writable, is_binary;

    
private:
    ao_path current_path;
    const char* current_mode;
    
	bool is_forked; // bit mystified on this naming - it's set when reading an applesingle/macbinary file
    int64_t fork_offset, fork_length;
};


//-----------------------------------------------------------------------------
// OpenedFileDevice

// TODO: the Boost dependency could probably be replaced with std:: streams
// used in network_star_hub.cpp, SoundFile.cpp, SoundsPatch.cpp, InfoTree.cpp as:
//
//    boost::iostreams::stream<OpenedFileDevice>
//    boost::iostreams::stream_buffer<OpenedFileDevice>
//
class OpenedFileDevice
{
public:
    typedef char char_type;
    typedef boost::iostreams::seekable_device_tag category;
    
    
    OpenedFileDevice(DataFile& f);
    
    std::streamsize read(char* s, std::streamsize n);
    
    std::streamsize write(const char* s, std::streamsize n);
    
    std::streampos seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way);

private:
    DataFile& file;
};


#endif /* __DataFile_h__ */

