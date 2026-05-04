/*
 crc.h -- CRC Checksum generation for a file.
 
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

#ifndef __crc_h__
#define __crc_h__

#include "cseries.hpp"
#include "DataFile.hpp"

uint32 calculate_crc_for_file(const ao_path& path);

uint32 calculate_crc_for_opened_file(DataFile& file);

uint32 calculate_data_crc(unsigned char *buffer, int32 length);

uint16 calculate_data_crc_ccitt(unsigned char *buffer, int32 length);

#endif /* __crc_h__ */
