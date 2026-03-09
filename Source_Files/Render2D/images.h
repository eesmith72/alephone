#ifndef __IMAGES_H
#define __IMAGES_H

/*
	images.h -- M2 Images file resource reader (also reads resources from other scenario files)
    (Life would've been much simpler had M2 moved everything into WADs as standard.)

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

Jul 31, 2002 (Loren Petrich)
	Added text-resource access in analogy with others' image- and sound-resource access;
	this is for supporting the M2-Win95 file format
*/

#include "DataFile.hpp"


void initialize_images_manager(void);

bool images_picture_exists(int base_resource);
bool scenario_picture_exists(int base_resource);


color_table* calculate_picture_clut(int pict_resource_number);
color_table* build_8bit_system_color_table();

void open_map_file_resources(const ao_path& File);
void close_map_file_resources();

void open_shapes_file_resources(const ao_path& File);

void open_m2_external_resources_file(const ao_path& File);

void open_sounds_file_resources(const ao_path& File);


void draw_full_screen_pict_resource_from_images(int pict_resource_number);
void draw_full_screen_pict_resource_from_scenario(int pict_resource_number);


void scroll_full_screen_pict_resource_from_scenario(int pict_resource_number, bool text_block);


bool get_picture_resource_from_images(int base_resource, LoadedResource& PictRsrc);
bool get_sound_resource_from_images(int resource_number, LoadedResource& PictRsrc);
bool get_picture_resource_from_scenario(int base_resource, LoadedResource& PictRsrc);

bool get_sound_resource_from_scenario(int resource_number, LoadedResource& SoundRsrc);
bool get_text_resource_from_scenario(int resource_number, LoadedResource& TextRsrc);


// Convert MacOS PICT resource to SDL surface
SDLSurfaceUniquePtr picture_to_surface(LoadedResource &rsrc);


// Rescale/tile surface
SDL_Surface *rescale_surface(SDL_Surface *s, int width, int height);
SDL_Surface *tile_surface(SDL_Surface *s, int width, int height);

SDLSurfaceUniquePtr find_m2_title_screen(const ao_path& file);
SDLSurfaceUniquePtr find_m1_title_screen(const ao_path& file);

#endif

