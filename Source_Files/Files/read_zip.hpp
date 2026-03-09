/*
 read_zip.hpp
 
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

#ifndef read_zip_hpp
#define read_zip_hpp

#include "cseries.h"

#ifdef HAVE_ZZIP
#include "SDL_rwops_zzip.h"
#endif


const zzip_plugin_io_handlers& utf8_zzip_io(); // used here and in DataFile.cpp

ao_err read_zip_file_entries(const ao_path& path, std::vector<std::string>& result); // used in Plugin.cpp


#endif /* read_zip_hpp */
