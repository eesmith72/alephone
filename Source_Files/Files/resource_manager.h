/*
 resource_manager.h - MacOS resource handling for non-Mac platforms
 
 Written in 2000 by Christian Bauer
 
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

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "cseries.h"


class FileSpecifier;
class LoadedResource;

void initialize_resources(void);

SDL_RWops *open_file_resource(FileSpecifier &file);

void close_file_resource(SDL_RWops *file);

SDL_RWops *get_current_resource_file(); // hmmm

void use_file_resource(SDL_RWops *file);

// th
size_t count_1_resources(uint32 type);
size_t count_resources(uint32 type);

void get_1_resource_id_list(uint32 type, std::vector<int> &ids);
void get_resource_id_list(uint32 type, std::vector<int> &ids);

bool get_1_resource(uint32 type, int id, LoadedResource &rsrc);
bool get_resource(uint32 type, int id, LoadedResource &rsrc);

bool get_1_ind_resource(uint32 type, int index, LoadedResource &rsrc);
bool get_ind_resource(uint32 type, int index, LoadedResource &rsrc);

bool has_1_resource(uint32 type, int id);
bool has_resource(uint32 type, int id);

void set_external_resources_file(FileSpecifier&);
void close_external_resources();

#endif
