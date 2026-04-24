/*
 DataFile.cpp
 
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


#include "DataFile.hpp"

#include "resource_manager.h"


#include "shell.h"
#include "interface.h"
#include "screen.h"
#include "tags.h"


#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "SoundManager.h" // !

#include "preferences.h"

#include "read_zip.hpp"


// -----------------------------------------------------------------------------------------
// DataFile


ao_err DataFile::open(const ao_path& path, const char* mode) // TODO: review when done as previously this returned false for error and now returns 0 for no error
{
    close();
    
    if (!std::filesystem::is_regular_file(path)) { return missingFile; } // TODO: what error?
    
    //log_note_f("DataFile %p opening: '%s'", this, path.c_str());

    current_path = path;
    current_mode = mode;
    
    is_writable = strchr(mode, 'w');
    is_binary = strchr(mode, 'b');
    
#ifdef HAVE_ZZIP
    if (is_writable)
    {
        fh = SDL_RWFromFile(path.generic_u8string().c_str(), mode);
    }
    else
    {
        fh = SDL_RWFromZZIP(path.generic_u8string().c_str(), &utf8_zzip_io()); // seems a bit odd; what if file's not zip-compressed? I'm guessing it automatically opens it using RWFromFile, but it'd be nice to see it stated
    }
#else
    fh = SDL_RWFromFile(path.generic_u8string().c_str(), mode);
#endif
    if (!fh) { return STRID(strERRORS, cantReadFile); }
    
    if (is_binary && !is_writable)
    {
        // EES: no idea if we still encounter these in the wild, so leaving
        // Transparently handle AppleSingle and MacBinary files on reading
        int32_t offset, data_length, rsrc_length;
        if (is_applesingle(fh, false, offset, data_length))
        {
            is_forked = true; 
            fork_offset = offset;
            fork_length = data_length;
            SDL_RWseek(fh, fork_offset, SEEK_SET);
        }
        else if (is_macbinary(fh, data_length, rsrc_length))
        {
            is_forked = true;
            fork_offset = 128;
            fork_length = data_length;
            SDL_RWseek(fh, fork_offset, SEEK_SET);
        }
    }
    //log_note_f("Opened DataFile %p: '%s'", this, current_path.c_str());
    
    set_position(0);
    SDL_ClearError();
    length = SDL_RWseek(fh, 0, RW_SEEK_END);
    if (length < 0) { log_error_f("Can't get length of DataFile '%s': %s", current_path.c_str(), SDL_GetError()); } // should probably return error code
    set_position(0);
    return no_err;
}


void DataFile::close()
{
	if (fh)
    {
        //log_note_f("Closed DataFile %p: '%s'", this, current_path.c_str());
		SDL_RWclose(fh);
        fh = nullptr;
	}
	is_forked = false;
	fork_offset = 0;
	fork_length = 0;
}


#define throw_if_not_open() \
    if (!fh) { throw_datafile_exception("Can't access '%s'", fileIsNotOpen, current_path.c_str()); }

#define throw_datafile_exception(format, err, ...)  throw_ao_exception_f(format, STRID(strERRORS, (err)), __VA_ARGS__)

int64_t DataFile::get_length() const
{
    throw_if_not_open();
    if (is_forked)
    {
        return fork_length;
    }
    else if (is_writable)
    {
        SDL_ClearError();
        int64_t result = SDL_RWsize(fh); // -1 if unknown OR error; TODO: so throwing is problematic; also, not getting a value here is
        if (result < 0) { log_warning_f("Can't get length of DataFile '%s': %s", current_path.c_str(), SDL_GetError()); }
        return result;
    }
    else
    {
        return length; // TODO: kludge: when trying to slurp the entire file into a dynamically allocated buffer, if SDL_RWsize returns -1 here it's a big problem
    }
}


int64_t DataFile::get_position() const
{
    throw_if_not_open();
    SDL_ClearError();
	int64_t result = SDL_RWtell(fh) - fork_offset; // treating -1 as error is problematic
    if (result < 0) { log_warning_f("Can't get position of DataFile '%s': %s", current_path.c_str(), SDL_GetError()); }
    return result;
}


void DataFile::set_position(int64_t position)
{
    throw_if_not_open();
    int64_t result = SDL_RWseek(fh, position + fork_offset, SEEK_SET);
    if (result < 0) { throw_datafile_exception("Can't set position of DataFile '%s' to %lld: %s", cantReadFile, current_path.c_str(), position, SDL_GetError()); }
}


// TODO: buffer then count, same as other APIs? yes, change these
void DataFile::read(int64_t count, void* buffer)
{
    throw_if_not_open();
    int64_t result = SDL_RWread(fh, buffer, 1, count);
    if (result != count) { throw_datafile_exception("Can't read DataFile '%s': %s", cantReadFile, current_path.c_str(), SDL_GetError()); }
}

void DataFile::write(int64_t count, const void* buffer)
{
    throw_if_not_open();
	int64_t result = SDL_RWwrite(fh, buffer, 1, count);
    if (result != count) { throw_datafile_exception("Can't write DataFile '%s': %s", cantWriteFile, current_path.c_str(), SDL_GetError()); }
}





OpenedFileDevice::OpenedFileDevice(DataFile& file) : file(file) { }

std::streamsize OpenedFileDevice::read(char* s, std::streamsize n)
{
	return SDL_RWread(file.borrow_rwops(), s, 1, n);
}


std::streamsize OpenedFileDevice::write(const char* s, std::streamsize n)
{
	return SDL_RWwrite(file.borrow_rwops(), s, 1, n);
}


std::streampos OpenedFileDevice::seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way)
{
	std::streampos pos;

	switch (way)
	{
	case std::ios_base::beg:
		pos = SDL_RWseek(file.borrow_rwops(), off + file.fork_offset, SEEK_SET);
		break;
	case std::ios_base::end:
		pos = SDL_RWseek(file.borrow_rwops(), off, SEEK_END);
		break;
	case std::ios_base::cur:
		pos = SDL_RWseek(file.borrow_rwops(), off, SEEK_CUR);
		break;
	default:
		break;
	}

	return pos - static_cast<std::streampos>(file.fork_offset);
}



