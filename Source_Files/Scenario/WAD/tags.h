/*
 TAGS.H -- all of the tags used by code that uses the wad file format
 
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

/*
 One tag, KEY_TAG, has special meaning, and KEY_TAG_SIZE must be set to the
 size of an index entry.  Each wad can only have one index entry.  You can get the
 index entry from a wad, or from all of the wads in the file easily.
 
 Marathon uses the KEY_TAG as the name of the level.
 */

#ifndef __TAGS_H
#define __TAGS_H

#include "cstypes.hpp"


#define MAXIMUM_LEVEL_NAME_SIZE  (64)



/* Other tags-  */
#define POINT_TAG FOUR_CHARS_TO_INT('P','N','T','S')
#define LINE_TAG FOUR_CHARS_TO_INT('L','I','N','S')
#define SIDE_TAG FOUR_CHARS_TO_INT('S','I','D','S')
#define POLYGON_TAG FOUR_CHARS_TO_INT('P','O','L','Y')
#define LIGHTSOURCE_TAG FOUR_CHARS_TO_INT('L','I','T','E')
#define ANNOTATION_TAG FOUR_CHARS_TO_INT('N','O','T','E')
#define OBJECT_TAG FOUR_CHARS_TO_INT('O','B','J','S')
#define GUARDPATH_TAG FOUR_CHARS_TO_INT('p','\x8c','t','h')
#define MAP_INFO_TAG FOUR_CHARS_TO_INT('M','i','n','f')
#define OBJECT_PLACEMENT_STRUCTURE_TAG FOUR_CHARS_TO_INT('p','l','a','c')
#define DOOR_EXTRA_DATA_TAG FOUR_CHARS_TO_INT('d','o','o','r')
#define PLATFORM_STATIC_DATA_TAG FOUR_CHARS_TO_INT('p','l','a','t')
#define ENDPOINT_DATA_TAG FOUR_CHARS_TO_INT('E','P','N','T')
#define MEDIA_TAG FOUR_CHARS_TO_INT('m','e','d','i')
#define AMBIENT_SOUND_TAG FOUR_CHARS_TO_INT('a','m','b','i')
#define RANDOM_SOUND_TAG FOUR_CHARS_TO_INT('b','o','n','k')
#define TERMINAL_DATA_TAG FOUR_CHARS_TO_INT('t','e','r','m')

/* Save/Load game tags. */
#define PLAYER_STRUCTURE_TAG FOUR_CHARS_TO_INT('p','l','y','r')
#define DYNAMIC_STRUCTURE_TAG FOUR_CHARS_TO_INT('d','w','o','l')
#define OBJECT_STRUCTURE_TAG FOUR_CHARS_TO_INT('m','o','b','j')
#define DOOR_STRUCTURE_TAG FOUR_CHARS_TO_INT('d','o','o','r')
#define MAP_INDEXES_TAG FOUR_CHARS_TO_INT('i','i','d','x')
#define AUTOMAP_LINES FOUR_CHARS_TO_INT('a','l','i','n')
#define AUTOMAP_POLYGONS FOUR_CHARS_TO_INT('a','p','o','l')
#define MONSTERS_STRUCTURE_TAG FOUR_CHARS_TO_INT('m','O','n','s')
#define EFFECTS_STRUCTURE_TAG FOUR_CHARS_TO_INT('f','x',' ',' ')
#define PROJECTILES_STRUCTURE_TAG FOUR_CHARS_TO_INT('b','a','n','g')
#define PLATFORM_STRUCTURE_TAG FOUR_CHARS_TO_INT('P','L','A','T')
#define WEAPON_STATE_TAG FOUR_CHARS_TO_INT('w','e','a','p')
#define TERMINAL_STATE_TAG FOUR_CHARS_TO_INT('c','i','n','t')
#define LUA_STATE_TAG FOUR_CHARS_TO_INT('s','l','u','a')

/* Save metadata tags */
#define SAVE_META_TAG FOUR_CHARS_TO_INT('S', 'M', 'E', 'T')
#define SAVE_IMG_TAG FOUR_CHARS_TO_INT('S', 'I', 'M', 'G')

/* Physix model tags */
#define MONSTER_PHYSICS_TAG FOUR_CHARS_TO_INT('M','N','p','x')
#define EFFECTS_PHYSICS_TAG FOUR_CHARS_TO_INT('F','X','p','x')
#define PROJECTILE_PHYSICS_TAG FOUR_CHARS_TO_INT('P','R','p','x')
#define PHYSICS_PHYSICS_TAG FOUR_CHARS_TO_INT('P','X','p','x')
#define WEAPONS_PHYSICS_TAG FOUR_CHARS_TO_INT('W','P','p','x')

#define M1_MONSTER_PHYSICS_TAG FOUR_CHARS_TO_INT('m','o','n','s')
#define M1_EFFECTS_PHYSICS_TAG FOUR_CHARS_TO_INT('e','f','f','e')
#define M1_PROJECTILE_PHYSICS_TAG FOUR_CHARS_TO_INT('p','r','o','j')
#define M1_PHYSICS_PHYSICS_TAG FOUR_CHARS_TO_INT('p','h','y','s')
#define M1_WEAPONS_PHYSICS_TAG FOUR_CHARS_TO_INT('w','e','a','p')

/* Embedded shapes */
#define SHAPE_PATCH_TAG FOUR_CHARS_TO_INT('S','h','P','a')

/* Embedded sounds */
#define SOUND_PATCH_TAG FOUR_CHARS_TO_INT('S','n','P','a')

/* Embedded scripts */
#define MMLS_TAG FOUR_CHARS_TO_INT('M','M','L','S')
#define LUAS_TAG FOUR_CHARS_TO_INT('L','U','A','S')

/* Preferences Tags.. */
#define prefGRAPHICS_TAG FOUR_CHARS_TO_INT('g','r','a','f')
#define prefSERIAL_TAG FOUR_CHARS_TO_INT('s','e','r','l')
#define prefNETWORK_TAG FOUR_CHARS_TO_INT('n','e','t','w')
#define prefPLAYER_TAG FOUR_CHARS_TO_INT('p','l','y','r')
#define prefINPUT_TAG FOUR_CHARS_TO_INT('i','n','p','u')
#define prefSOUND_TAG FOUR_CHARS_TO_INT('s','n','d',' ')
#define prefENVIRONMENT_TAG FOUR_CHARS_TO_INT('e','n','v','r')

#endif
