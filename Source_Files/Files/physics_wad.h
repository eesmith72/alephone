/*
 physics_wad.h
 
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

#ifndef __physics_wad__
#define __physics_wad__

#include "cstypes.h"


#define BUNGIE_PHYSICS_DATA_VERSION   (0)
#define PHYSICS_DATA_VERSION          (1)
#define CURRENT_PHYSICS_DATA_VERSION  (PHYSICS_DATA_VERSION)

#define is_valid_physics_data(version)  ((version) >= BUNGIE_PHYSICS_DATA_VERSION && (version) <= CURRENT_PHYSICS_DATA_VERSION)


#define initialize_physics()  (load_default_physics())


void set_external_physics_file(const ao_path& path);

uint32_t get_external_physics_file_checksum(); // used in achievements.cpp; TODO: this only applies to external physics file, not network-supplied physics


// current physics can be any of the following

void load_default_physics();

void load_external_physics_file();

void load_physics_from_network_physics_buffer(void* data);

void *get_network_physics_buffer(int64_t* physics_length);



#endif /* __physics_wad__ */
