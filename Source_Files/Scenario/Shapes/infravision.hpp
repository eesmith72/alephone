/*
 infravision.hpp -- builds a collection's nightvision tint color table
                    This is an alternate color table where every ramp is
                    a single color (e.g. monsters are red, walls are blue).
 
 
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

#ifndef infravision_hpp
#define infravision_hpp

#include "ShapesCollection.hpp"


void build_collection_tinting_tables(ShapesCollection* collection, shapes_colors_t& colors);



struct InfoTree;

void reset_mml_infravision();

void parse_mml_infravision(const InfoTree& root);


#endif /* infravision_hpp */
