/*
 read_zip.cpp
 
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

#include "read_zip.hpp"


// -----------------------------------------------------------------------------------------


// utf8_zzip_io(): a zzip I/O handler set with a UTF-8-compatible 'open' handler
#ifdef HAVE_ZZIP
#ifdef __WIN32__
static int win_zzip_open(const char* f, int o, ...)
{
    return _wopen(utf8_to_wide(f).c_str(), o);
}

const zzip_plugin_io_handlers& utf8_zzip_io()
{
    static const zzip_plugin_io_handlers io = []
    {
        zzip_plugin_io_handlers io = {zzip_get_default_io()->fd};
        io.fd.open = &win_zzip_open;
        return io;
    }();
    return io;
}

#else
const zzip_plugin_io_handlers& utf8_zzip_io()
{
    return *zzip_get_default_io();
}
#endif
#endif // HAVE_ZZIP


// Read ZIP file contents; used by Plugins.cpp
ao_err read_zip_file_entries(const ao_path& path, std::vector<std::string>& result)
{
    result.clear();
    
#ifdef HAVE_ZZIP
    const auto zip = zzip_dir_open_ext_io(path.generic_u8string().c_str(), nullptr, nullptr, &utf8_zzip_io());
    if (!zip) { return errno; } // TODO: what error code?
    
    ZZIP_DIRENT entry;
    while (zzip_dir_read(zip, &entry)) { result.emplace_back(entry.d_name); }
    
    zzip_dir_close(zip);
    return no_err;
#else
    return ENOTSUP;
#endif
}


// -----------------------------------------------------------------------------------------



