/*
 map.h
 
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

#ifndef __MAP_H
#define __MAP_H

#include "cseries.hpp"

#include "world.h"
#include "dynamic_limits.h"
#include "player.h" // get_number_of_players
#include "Console.h" // temporary (we hope)
#include "Packing.h"


/* ---------- constants */

#define MAP_INDEX_BUFFER_SIZE 8192
#define MINIMUM_SEPARATION_FROM_WALL (WORLD_ONE/4)
#define MINIMUM_SEPARATION_FROM_PROJECTILE ((3*WORLD_ONE)/4)

#define TELEPORTING_DURATION (2*TELEPORTING_MIDPOINT)
#define TELEPORTING_MIDPOINT (TICKS_PER_SECOND/2)

/* These arrays are the absolute limits, and are used only by the small memory allocating */
/*  arrays.  */
#define MAXIMUM_LEVELS_PER_MAP (128)

// #define LEVEL_NAME_LENGTH (64+2) // yeah, gonna go with 64 as the fixed length in the WAD data (std::string::c_str will add a NUL to that when needed)



// in devices.cpp, which should be named switches.cpp and also have a header file
struct InfoTree;
void parse_mml_control_panels(const InfoTree& root);
void reset_mml_control_panels();


/* ---------- shape descriptors */

#include "shapes.h"

/* ---------- damage */

enum /* damage types */
{
	_damage_explosion,
	_damage_electrical_staff,
	_damage_projectile,
	_damage_absorbed,
	_damage_flame,
	_damage_hound_claws,
	_damage_alien_projectile,
	_damage_hulk_slap,
	_damage_compiler_bolt,
	_damage_fusion_bolt,
	_damage_hunter_bolt,
	_damage_fist,
	_damage_teleporter,
	_damage_defender,
	_damage_yeti_claws,
	_damage_yeti_projectile,
	_damage_crushing,
	_damage_lava,
	_damage_suffocation,
	_damage_goo,
	_damage_energy_drain,
	_damage_oxygen_drain,
	_damage_hummer_bolt,
	_damage_shotgun_projectile,
	NUMBER_OF_DAMAGE_TYPES
};

enum /* damage flags */
{
	_alien_damage= 0x1 /* will be decreased at lower difficulty levels */
};

struct damage_definition
{
	int16 type, flags;
	
	int16 base, random;
	_fixed scale;
};
const int SIZEOF_damage_definition = 12;

/* ---------- saved objects (initial map locations, etc.) */

enum /* map object types */
{
	_saved_monster,	/* .index is monster type */
	_saved_object,	/* .index is scenery type */
	_saved_item,	/* .index is item type */
	_saved_player,	/* .index is team bitfield */
	_saved_goal,	/* .index is goal number */
	_saved_sound_source /* .index is source type, .facing is sound volume */
};

enum /* map object flags */
{
	_map_object_is_invisible= 0x0001, /* initially invisible */
	_map_object_is_platform_sound= 0x0001,
	_map_object_hanging_from_ceiling= 0x0002, /* used for calculating absolute .z coordinate */
	_map_object_is_blind= 0x0004, /* monster cannot activate by sight */
	_map_object_is_deaf= 0x0008, /* monster cannot activate by sound */
	_map_object_floats= 0x0010, /* used by sound sources caused by media */
	_map_object_is_network_only= 0x0020 /* for items only */
	
	// top four bits is activation bias for monsters
};

#define DECODE_ACTIVATION_BIAS(f) ((f)>>12)
#define ENCODE_ACTIVATION_BIAS(b) ((b)<<12)

struct map_object /* 16 bytes */
{
	int16 type; /* _saved_monster, _saved_object, _saved_item, ... */
	int16 index;
	int16 facing;
	int16 polygon_index;
	world_point3d location; // .z is a delta
	
	uint16 flags;
};
const int SIZEOF_map_object = 16;

// Due to misalignments, these have different sizes
typedef world_point2d saved_map_pt;
typedef struct line_data saved_line;
typedef struct polygon_data saved_poly;
typedef struct map_annotation saved_annotation;
typedef struct map_object saved_object;
typedef struct static_world_t saved_map_data;


/* ---------- map loading/new game structures */

enum  // entry point types - this is per map level (int32) // 'entry points' is a confusing name; 'game types' would make more sense; TODO: there seems to be a lot of functional overlap with game types enum further down; the obvious difference being that these flags are used in the directory_data struct whereas the game types flags are stored in the map's static_world struct; the sensible thing is to define them ONCE
{
	_single_player_entry_point              = 0x01,
	_multiplayer_cooperative_entry_point    = 0x02,
	_multiplayer_carnage_entry_point        = 0x04,
	_kill_the_man_with_the_ball_entry_point = 0x08, // was _capture_the_flag_entry_point, even though Bungie used it for KTMWTB
	_king_of_hill_entry_point               = 0x10,
	_defense_entry_point                    = 0x20,
	_rugby_entry_point                      = 0x40,
	_capture_the_flag_entry_point           = 0x80,
};

const int32 all_entry_points = _single_player_entry_point
                             | _multiplayer_cooperative_entry_point
                             | _multiplayer_carnage_entry_point
                             | _kill_the_man_with_the_ball_entry_point
                             | _king_of_hill_entry_point
                             | _defense_entry_point // EES: TODO: check this should be included (it wasn't before)
                             | _rugby_entry_point
                             | _capture_the_flag_entry_point;

/*
 enum // game types
 {
     _game_of_kill_monsters,        // single player & combative (EMFH?) use this
     _game_of_cooperative_play,    // multiple players (coop?), working together
     _game_of_capture_the_flag,    // A team game.
     _game_of_king_of_the_hill,
     _game_of_kill_man_with_ball,
     _game_of_defense,
     _game_of_rugby,
     _game_of_tag,
     _game_of_custom,
     NUMBER_OF_GAME_TYPES
 };
 */



// (the level name is apparently stored twice, in the WAD's directory data AND in the level's static_world, and both are MacRoman-encoded fixed-size 64-byte char arrays with NUL terminator)
#define MAX_LEVEL_NAME_LENGTH (64)


struct level_identity // used in dialogs; might eventually go away
{
	int16 level_number;
	std::string utf8_level_name; // with caveat that the M2 Map wad format isn't UTF8-aware, so pack/unpack functions must convert to/from MacRoman to store in the WAD's static_world entry (when we bump the map version number, we can store UTF8 with max length of 256 and, hopefully, simplify the structure)
};



struct directory_data
{
	int16_t mission_flags;
	int16_t environment_flags;
	int32_t entry_point_flags; // bitflags; a map level may support one or more game types
    std::string level_name; // originally `char level_name[MAX_LEVEL_NAME_LENGTH];`; now UTF8-encoded std::string
};
const int SIZEOF_directory_data = 74; // TODO: seems to be 66 bytes so not sure if that means it can have 64 chars and the extra 2 bytes are guaranteed to be NUL; gonna hedge bets and 

/* ---------- map annotations */

#define MAXIMUM_ANNOTATION_TEXT_LENGTH 64

struct map_annotation
{
	int16 type; /* turns into color, font, size, style, etc... */
	
	world_point2d location; /* where to draw this (lower left) */
	int16 polygon_index; /* only displayed if this polygon is in the automap */
	
	std::string text; // UTF8-encoded; hurrah!
};
const int SIZEOF_map_annotation = 72;

struct map_annotation *get_next_map_annotation(int16 *count);

/* ---------- ambient sound images */


// non-directional ambient component
struct ambient_sound_image_data // 16 bytes
{
	uint16 flags;
	
	int16 sound_index;
	int16 volume;

	int16 unused[5];
};
const int SIZEOF_ambient_sound_image_data = 16;

/* ---------- random sound images */

enum // sound image flags
{
	_sound_image_is_non_directional= 0x0001 // ignore direction
};

// possibly directional random sound effects
struct random_sound_image_data // 32 bytes
{
	uint16 flags;
	
	int16 sound_index;
	
	int16 volume, delta_volume;
	int16 period, delta_period;
	angle direction, delta_direction;
	_fixed pitch, delta_pitch;
	
	// only used at run-time; initialize to NONE
	int16 phase;
	
	int16 unused[3];
};
const int SIZEOF_random_sound_image_data = 32;


#define  get_objects_limit() (get_dynamic_limit(_dynamic_limit_objects))


/* SLOT_IS_USED(), SLOT_IS_FREE(), MARK_SLOT_AS_FREE(), MARK_SLOT_AS_USED() macros are also used
	for monsters, effects and projectiles */
#define SLOT_IS_USED(o) ((o)->flags&(uint16)0x8000)
#define SLOT_IS_FREE(o) (!SLOT_IS_USED(o))
#define MARK_SLOT_AS_FREE(o) ((o)->flags&=(uint16)~0xC000)
#define MARK_SLOT_AS_USED(o) ((o)->flags=((o)->flags|(uint16)0x8000)&(uint16)~0x4000)

#define OBJECT_WAS_RENDERED(o) ((o)->flags&(uint16)0x4000)
#define SET_OBJECT_RENDERED_FLAG(o) ((o)->flags|=(uint16)0x4000)
#define CLEAR_OBJECT_RENDERED_FLAG(o) ((o)->flags&=(uint16)~0x4000)

/* this field is only valid after transmogrify_object_shape is called; in terms of our pipeline, that
	means that it’s only valid if OBJECT_WAS_RENDERED returns true *and* was cleared before
	the last call to render_scene() ... this means that if OBJECT_WAS_RENDERED returns false,
	the monster and projectile managers will probably call transmogrif_object_shape themselves.
	for reasons beyond this scope of this comment to explain, the keyframe cannot be frame zero!
	also, when any of the flags below are set, the phase of the .sequence field can be examined
	to determine exactly how many ticks the last frame took to animate (that is, .sequence.phase
	is not reset until the next loop). */
#define OBJECT_WAS_ANIMATED(o) ((o)->flags&(uint16)_obj_animated)
#define GET_OBJECT_ANIMATION_FLAGS(o) ((o)->flags&(uint16)0x3c00)
#define SET_OBJECT_ANIMATION_FLAGS(o,n) { (o)->flags&= (uint16)~0x3c00; (o)->flags|= (n); }
enum /* object was animated flags */
{
	_obj_not_animated= 0x0000, /* nothing happened */
	_obj_animated= 0x2000, /* a new frame was reached */
	_obj_keyframe_started= 0x1000, /* the key-frame was reached */
	_obj_last_frame_animated= 0x0800, /* sequence complete, returning to first frame */
	_obj_transfer_mode_finished= 0x0400 /* transfer mode phase is about to loop */
};

#define GET_OBJECT_SCALE_FLAGS(o) (((o)->flags)&OBJECT_SCALE_FLAGS_MASK)
enum /* object scale flags */
{
	_object_is_enlarged= 0x0200,
	_object_is_tiny= 0x0100,

	OBJECT_SCALE_FLAGS_MASK= _object_is_enlarged|_object_is_tiny
};

#define OBJECT_IS_MEDIA_EFFECT(o) ((o)->flags&128)
#define SET_OBJECT_IS_MEDIA_EFFECT(o) ((o)->flags|= 128)

/* ignored by renderer if INVISIBLE */
#define OBJECT_IS_INVISIBLE(o) ((o)->flags&(uint16)32)
#define OBJECT_IS_VISIBLE(o) (!OBJECT_IS_INVISIBLE(o))
#define SET_OBJECT_INVISIBILITY(o,v) ((void)((v)?((o)->flags|=(uint16)32):((o)->flags&=(uint16)~32)))

/* call get_object_dimensions(object_index, &radius, &height) for SOLID objects to get their dimensions */
#define OBJECT_IS_SOLID(o) ((o)->flags&(uint16)16)
#define SET_OBJECT_SOLIDITY(o,v) ((void)((v)?((o)->flags|=(uint16)16):((o)->flags&=(uint16)~16)))

#define GET_OBJECT_STATUS(o) ((o)->flags&(uint16)8)
#define SET_OBJECT_STATUS(o,v) ((v)?((o)->flags|=(uint16)8):((o)->flags&=(uint16)~8))
#define TOGGLE_OBJECT_STATUS(o) ((o)->flags^=(uint16)8)

#define GET_OBJECT_OWNER(o) ((o)->flags&(uint16)7)
#define SET_OBJECT_OWNER(o,n) { /*assert_fail*/((n)>=0&&(n)<=7); (o)->flags&= (uint16)~7; (o)->flags|= (n); }
enum /* object owners (8) */
{
	_object_is_normal, /* normal */
	_object_is_scenery, /* impassable scenery */
	_object_is_monster, /* monster index in .permutation */
	_object_is_projectile, /* active projectile index in .permutation */
	_object_is_effect, /* explosion or something; index in .permutation */
	_object_is_item, /* .permutation is item type */
	_object_is_device, /* status given by bit in flags field, device type in .permutation */
	_object_is_garbage /* will be removed by garbage collection algorithms */
};

/* because of sign problems, we must rip out the values before we modify them; frame is
	in [0,16), phase is in [0,4096) ... this is for shape animations */
#define GET_SEQUENCE_FRAME(s) ((s)>>12)
#define GET_SEQUENCE_PHASE(s) ((s)&4095)
#define BUILD_SEQUENCE(f,p) (((f)<<12)|(p))

enum /* object transfer modes (high-level) */
{
	_xfer_normal,
	_xfer_fade_out_to_black, /* reduce ambient light until black, then tint-fade out */
	_xfer_invisibility,
	_xfer_subtle_invisibility,
	_xfer_pulsate, /* only valid for polygons */
	_xfer_wobble, /* only valid for polygons */
	_xfer_fast_wobble, /* only valid for polygons */
	_xfer_static,
	_xfer_50percent_static,
	_xfer_landscape,
	_xfer_smear, /* repeat pixel(0,0) of texture everywhere */
	_xfer_fade_out_static,
	_xfer_pulsating_static,
	_xfer_fold_in, /* appear */
	_xfer_fold_out, /* disappear */
	_xfer_horizontal_slide,
	_xfer_fast_horizontal_slide,
	_xfer_vertical_slide,
	_xfer_fast_vertical_slide,
	_xfer_wander,
	_xfer_fast_wander,
	_xfer_big_landscape,
	_xfer_reverse_horizontal_slide,
	_xfer_reverse_fast_horizontal_slide,
	_xfer_reverse_vertical_slide,
	_xfer_reverse_fast_vertical_slide,
	_xfer_2x,				  // scales texture by 2x
	_xfer_4x,				  // scales texture by 4x
	
	NUMBER_OF_TRANSFER_MODES
};

struct object_location
{
	struct world_point3d p;
	int16 polygon_index;
	
	angle yaw, pitch;
	
	uint16 flags;
};

struct object_data /* 32 bytes */
{
	/* these fields are in the order of a world_location3d structure, but are missing the pitch
		and velocity fields */
	world_point3d location;
	int16 polygon;

	angle facing;
	
	/* this is not really a shape descriptor: (and this is the only place in the game where you
		find this pseudo-shape_descriptor type) the collection is valid, as usual, but the
		shape index is an index into the animated shape array for that collection. */
	shape_descriptor shape;

	uint16 sequence; /* for shape animation */
	uint16 flags; /* [used_slot.1] [rendered.1] [animated.4] [unused.4] [invisible.1] [solid.1] [status.1] [owner.3] */
	int16 transfer_mode, transfer_period; /* if NONE take from shape data */
	int16 transfer_phase; /* for transfer mode animations */
	int16 permutation; /* usually index into owner array */
	
	int16 next_object; /* or NONE */
	int16 parasitic_object; /* or NONE */

	/* used when playing sounds */
	_fixed sound_pitch;
};
const int SIZEOF_object_data = 32;

/* ------------ endpoint definition */

#define ENDPOINT_IS_SOLID(e) ((e)->flags&1)
#define SET_ENDPOINT_SOLIDITY(e,s) ((s)?((e)->flags|=1):((e)->flags&=~(uint16)1))

#define ENDPOINT_IS_TRANSPARENT(e) ((e)->flags&4)
#define SET_ENDPOINT_TRANSPARENCY(e,s) ((s)?((e)->flags|=4):((e)->flags&=~(uint16)4))

/* false if all polygons sharing this endpoint have the same height */
#define ENDPOINT_IS_ELEVATION(e) ((e)->flags&2)
#define SET_ENDPOINT_ELEVATION(e,s) ((s)?((e)->flags|=2):((e)->flags&=~(uint16)2))

struct endpoint_data /* 16 bytes */
{
	uint16 flags;
	world_distance highest_adjacent_floor_height, lowest_adjacent_ceiling_height;
	
	world_point2d vertex;
	world_point2d transformed;
	
	int16 supporting_polygon_index;
};
const int SIZEOF_endpoint_data = 16;

// For loading plain points:
const int SIZEOF_world_point2d = 4;

/* ------------ line definition */

#define SOLID_LINE_BIT 0x4000
#define TRANSPARENT_LINE_BIT 0x2000
#define LANDSCAPE_LINE_BIT 0x1000
#define ELEVATION_LINE_BIT 0x800
#define VARIABLE_ELEVATION_LINE_BIT 0x400
#define LINE_HAS_TRANSPARENT_SIDE_BIT 0x200
#define LINE_IS_DECORATIVE_BIT 0x100

#define SET_LINE_SOLIDITY(l,v) ((v)?((l)->flags|=(uint16)SOLID_LINE_BIT):((l)->flags&=(uint16)~SOLID_LINE_BIT))
#define LINE_IS_SOLID(l) ((l)->flags&SOLID_LINE_BIT)

#define SET_LINE_TRANSPARENCY(l,v) ((v)?((l)->flags|=(uint16)TRANSPARENT_LINE_BIT):((l)->flags&=(uint16)~TRANSPARENT_LINE_BIT))
#define LINE_IS_TRANSPARENT(l) ((l)->flags&TRANSPARENT_LINE_BIT)

#define SET_LINE_LANDSCAPE_STATUS(l,v) ((v)?((l)->flags|=(uint16)LANDSCAPE_LINE_BIT):((l)->flags&=(uint16)~LANDSCAPE_LINE_BIT))
#define LINE_IS_LANDSCAPED(l) ((l)->flags&LANDSCAPE_LINE_BIT)

#define SET_LINE_ELEVATION(l,v) ((v)?((l)->flags|=(uint16)ELEVATION_LINE_BIT):((l)->flags&=(uint16)~ELEVATION_LINE_BIT))
#define LINE_IS_ELEVATION(l) ((l)->flags&ELEVATION_LINE_BIT)

#define SET_LINE_VARIABLE_ELEVATION(l,v) ((v)?((l)->flags|=(uint16)VARIABLE_ELEVATION_LINE_BIT):((l)->flags&=(uint16)~VARIABLE_ELEVATION_LINE_BIT))
#define LINE_IS_VARIABLE_ELEVATION(l) ((l)->flags&VARIABLE_ELEVATION_LINE_BIT)

#define SET_LINE_HAS_TRANSPARENT_SIDE(l,v) ((v)?((l)->flags|=(uint16)LINE_HAS_TRANSPARENT_SIDE_BIT):((l)->flags&=(uint16)~LINE_HAS_TRANSPARENT_SIDE_BIT))
#define LINE_HAS_TRANSPARENT_SIDE(l) ((l)->flags&LINE_HAS_TRANSPARENT_SIDE_BIT)

struct line_data /* 32 bytes */
{
	int16 endpoint_indexes[2];
	uint16 flags; /* no permutation field */

	world_distance length;
	world_distance highest_adjacent_floor, lowest_adjacent_ceiling;
	
	/* the side definition facing the clockwise polygon which references this side, and the side
		definition facing the counterclockwise polygon (can be NONE) */
	int16 clockwise_polygon_side_index, counterclockwise_polygon_side_index;
	
	/* a line can be owned by a clockwise polygon, a counterclockwise polygon, or both (but never
		two of the same) (can be NONE) */
	int16 clockwise_polygon_owner, counterclockwise_polygon_owner;
	
	int16 unused[6];

	// decorative lines always pass projectiles through their transparent sides
	bool is_decorative() const {
		return flags & LINE_IS_DECORATIVE_BIT;
	}

	void set_decorative(bool b) {
		if (b) flags |= LINE_IS_DECORATIVE_BIT;
		else flags &= ~LINE_IS_DECORATIVE_BIT;
	}
};
const int SIZEOF_line_data = 32;

/* --------------- side definition */

enum /* side flags */
{
	_control_panel_status= 0x0001,
	_side_is_control_panel= 0x0002,
	_side_is_repair_switch= 0x0004, // must be toggled to exit level
	_side_is_destructive_switch= 0x0008, // uses an item
	_side_is_lighted_switch= 0x0010, // switch must be lighted to use
	_side_switch_can_be_destroyed= 0x0020, // projectile hits toggle and destroy this switch
	_side_switch_can_only_be_hit_by_projectiles= 0x0040,
	_side_item_is_optional= 0x0080, // in Marathon, switches still work without items
	_side_is_m1_lighted_switch = 0x0100, // in Marathon, lighted switches must be above 50% (unlike M2, 75%)

	_editor_dirty_bit= 0x4000, // used by the editor...
	_reserved_side_flag = 0x8000 // some maps written by an old map editor
								 // (Pfhorte?) set lots of side flags; use this
								 // to detect and correct
};

enum /* control panel side types */
{
	_panel_is_oxygen_refuel,
	_panel_is_shield_refuel,
	_panel_is_double_shield_refuel,
	_panel_is_triple_shield_refuel,
	_panel_is_light_switch, // light index in .permutation
	_panel_is_platform_switch, // platform index in .permutation
	_panel_is_tag_switch, // tag in .permutation (NONE is tagless)
	_panel_is_pattern_buffer,
	_panel_is_computer_terminal,
	NUMBER_OF_CONTROL_PANELS
};

#define SIDE_IS_CONTROL_PANEL(s) ((s)->flags & _side_is_control_panel)
#define SET_SIDE_CONTROL_PANEL(s, t) ((void)((t) ? (s->flags |= (uint16) _side_is_control_panel) : (s->flags &= (uint16)~_side_is_control_panel)))

#define GET_CONTROL_PANEL_STATUS(s) (((s)->flags & _control_panel_status) != 0)
#define SET_CONTROL_PANEL_STATUS(s, t) ((t) ? (s->flags |= (uint16) _control_panel_status) : (s->flags &= (uint16)~_control_panel_status))
#define TOGGLE_CONTROL_PANEL_STATUS(s) ((s)->flags ^= _control_panel_status)

#define SIDE_IS_REPAIR_SWITCH(s) ((s)->flags & _side_is_repair_switch)
#define SET_SIDE_IS_REPAIR_SWITCH(s, t) ((t) ? (s->flags |= (uint16) _side_is_repair_switch) : (s->flags &= (uint16)~_side_is_repair_switch))

/* Flags used by Vulcan */
#define SIDE_IS_DIRTY(s) ((s)->flags&_editor_dirty_bit)
#define SET_SIDE_IS_DIRTY(s, t) ((t)?(s->flags|=(uint16)_editor_dirty_bit):(s->flags&=(uint16)~_editor_dirty_bit))

enum /* side types (largely redundant; most of this could be guessed for examining adjacent polygons) */
{
	_full_side, /* primary texture is mapped floor-to-ceiling */
	_high_side, /* primary texture is mapped on a panel coming down from the ceiling (implies 2 adjacent polygons) */
	_low_side, /* primary texture is mapped on a panel coming up from the floor (implies 2 adjacent polygons) */
	_composite_side, /* primary texture is mapped floor-to-ceiling, secondary texture is mapped into it (i.e., control panel) */
	_split_side /* primary texture is mapped onto a panel coming down from the ceiling, secondary
		texture is mapped on a panel coming up from the floor */
};

struct side_texture_definition
{
	world_distance x0, y0;
	shape_descriptor texture;
};

struct side_exclusion_zone
{
	world_point2d e0, e1, e2, e3;
};

struct side_data /* size platform-dependant */
{
	int16 type;
	uint16 flags;
	
	struct side_texture_definition primary_texture;
	struct side_texture_definition secondary_texture;
	struct side_texture_definition transparent_texture; /* not drawn if .texture==NONE */

	/* all sides have the potential of being impassable; the exclusion zone is the area near
		the side which cannot be walked through */
	struct side_exclusion_zone exclusion_zone;

	int16 control_panel_type; /* Only valid if side->flags & _side_is_control_panel */
	int16 control_panel_permutation; /* platform index, light source index, etc... */
	
	int16 primary_transfer_mode; /* These should be in the side_texture_definition.. */
	int16 secondary_transfer_mode;
	int16 transparent_transfer_mode;

	int16 polygon_index, line_index;

	int16 primary_lightsource_index;	
	int16 secondary_lightsource_index;
	int16 transparent_lightsource_index;

	int32 ambient_delta;

	int16 unused[1];
};
const int SIZEOF_side_data = 64;

/* ----------- polygon definition */

#define MAXIMUM_VERTICES_PER_POLYGON 8

// LP/AlexJLS change: added Marathon 1 polygon damage and glue stuff
enum /* polygon types */
{
	_polygon_is_normal,
	_polygon_is_item_impassable,
	_polygon_is_monster_impassable,
	_polygon_is_hill, /* for king-of-the-hill */
	_polygon_is_base, /* for capture the flag, rugby, etc. (team in .permutation) */
	_polygon_is_platform, /* platform index in .permutation */
	_polygon_is_light_on_trigger, /* lightsource index in .permutation */
	_polygon_is_platform_on_trigger, /* polygon index in .permutation */
	_polygon_is_light_off_trigger, /* lightsource index in .permutation */
	_polygon_is_platform_off_trigger, /* polygon index in .permutation */
	_polygon_is_teleporter, /* .permutation is polygon_index of destination */
	_polygon_is_zone_border,
	_polygon_is_goal,
	_polygon_is_visible_monster_trigger,
	_polygon_is_invisible_monster_trigger,
	_polygon_is_dual_monster_trigger,
	_polygon_is_item_trigger, /* activates all items in this zone */
	_polygon_must_be_explored,
	_polygon_is_automatic_exit, /* if success conditions are met, causes automatic transport too next level */
	_polygon_is_minor_ouch,
	_polygon_is_major_ouch,
	_polygon_is_glue,
	_polygon_is_glue_trigger,
	_polygon_is_superglue
};

#define POLYGON_IS_DETACHED_BIT 0x4000
#define POLYGON_IS_DETACHED(p) ((p)->flags&POLYGON_IS_DETACHED_BIT)
#define SET_POLYGON_DETACHED_STATE(p, v) ((v)?((p)->flags|=POLYGON_IS_DETACHED_BIT):((p)->flags&=~POLYGON_IS_DETACHED_BIT))

struct horizontal_surface_data /* should be in polygon structure */
{
	world_distance height;
	int16 lightsource_index;
	shape_descriptor texture;
	int16 transfer_mode, transfer_mode_data;
	
	world_point2d origin;
};

struct polygon_data /* 128 bytes */
{
	int16 type;
	uint16 flags;
	int16 permutation;

	uint16 vertex_count;
	int16 endpoint_indexes[MAXIMUM_VERTICES_PER_POLYGON]; /* clockwise */
	int16 line_indexes[MAXIMUM_VERTICES_PER_POLYGON];
	
	shape_descriptor floor_texture, ceiling_texture;
	world_distance floor_height, ceiling_height;
	int16 floor_lightsource_index, ceiling_lightsource_index;
	
	int32 area; /* in world_distance^2 units */
	
	int16 first_object;
	
	/* precalculated impassability information; each polygon has a list of lines and points
		that anything big (i.e., monsters but not projectiles) inside it must check against when
		ending a move inside it. */
	int16 first_exclusion_zone_index;
	int16 line_exclusion_zone_count;
	int16 point_exclusion_zone_count;

	int16 floor_transfer_mode;
	int16 ceiling_transfer_mode;
	
	int16 adjacent_polygon_indexes[MAXIMUM_VERTICES_PER_POLYGON];
	
	/* a list of polygons within WORLD_ONE of us */
	int16 first_neighbor_index;
	int16 neighbor_count;
	
	world_point2d center;
	
	int16 side_indexes[MAXIMUM_VERTICES_PER_POLYGON];
	
	world_point2d floor_origin, ceiling_origin;
	
	int16 media_index;
	int16 media_lightsource_index;
	
	/* NONE terminated list of _saved_sound_source indexes which must be checked while a
		listener is inside this polygon (can be none) */
	int16 sound_source_indexes;
	
	// either can be NONE
	int16 ambient_sound_image_index;
	int16 random_sound_image_index;
	
	int16 unused[1];
};
const int SIZEOF_polygon_data = 128;

/* ----------- static light definition */

struct saved_lighting_function_specification /* 7*2 == 14 bytes */
{
	int16 function;
	
	int16 period, delta_period;
	uint16 intensity_hi, intensity_lo, delta_intensity_hi, delta_intensity_lo;
};

struct saved_static_light_data /* 8*2 + 6*14 == 100 bytes */
{
	int16 type;
	uint16 flags;

	int16 phase; // initializer, so lights may start out-of-phase with each other
	
	struct saved_lighting_function_specification primary_active, secondary_active, becoming_active;
	struct saved_lighting_function_specification primary_inactive, secondary_inactive, becoming_inactive;
	
	int16 tag;
	
	int16 unused[4];
};
const int SIZEOF_saved_static_light_data = 100;

/* ---------- random placement data structures.. */

enum /* game difficulty levels */
{
	_wuss_level,
	_easy_level,
	_normal_level,
	_major_damage_level,
	_total_carnage_level,
	NUMBER_OF_GAME_DIFFICULTY_LEVELS
};


/* ---------- new object frequency structures. */


#define NUMBER_OF_OBJECT_FREQUENCY_DEFINITIONS (128)

#define MAXIMUM_OBJECT_TYPES (NUMBER_OF_OBJECT_FREQUENCY_DEFINITIONS / 2)


enum // flags for object_frequency_definition
{
	_reappears_in_random_location= 0x0001
};

struct object_frequency_definition
{
	uint16 flags;
	
	int16 initial_count;   // number that initially appear. can be greater than maximum_count
	int16 minimum_count;   // this number of objects will be maintained.
	int16 maximum_count;   // can’t exceed this, except at the beginning of the level.
	
	int16 random_count;    // maximum random occurences of the object
	uint16 random_chance;    // in (0, 65535]
};
const int SIZEOF_object_frequency_definition = 12;

// Placement frequencies for each type of object in map. (This is packed in WAD as 128-item array of 64 item slots followed by 64 monster slots.
// The actual number of slots in use is determined by the item definition and )
// Caution: it should be possible to increase the limit on monster types, but changing number of item types will break existing
// Lua scripts, as the Lua API treats both items and monsters as a single array of 'items'; see get_placement_info in lua_objects.cpp
extern std::array<object_frequency_definition, MAXIMUM_OBJECT_TYPES> item_placement_info;
extern std::array<object_frequency_definition, MAXIMUM_OBJECT_TYPES> monster_placement_info;


/* ---------- map */

enum /* mission flags */
{
	_mission_none= 0x0000,
	_mission_extermination= 0x0001,
	_mission_exploration= 0x0002,
	_mission_retrieval= 0x0004,
	_mission_repair= 0x0008,
	_mission_rescue= 0x0010,
	_mission_exploration_m1= 0x0020,
	_mission_rescue_m1= 0x0040,
	_mission_repair_m1= 0x0080
};

enum /* environment flags */
{
	_environment_normal= 0x0000,
	_environment_vacuum= 0x0001, // prevents certain weapons from working, player uses oxygen
	_environment_magnetic= 0x0002, // motion sensor works poorly
	_environment_rebellion= 0x0004, // makes clients fight pfhor
	_environment_low_gravity= 0x0008, // low gravity
	_environment_glue_m1= 0x0010, // handle glue polygons like Marathon 1
	_environment_ouch_m1= 0x0020, // the floor is lava
	_environment_rebellion_m1= 0x0040,  // use Marathon 1 rebellion (don't strip items/health)
	_environment_song_index_m1 = 0x0080, // play music
	_environment_terminals_stop_time = 0x0100, // solo only
	_environment_activation_ranges = 0x0200, // Marathon 1 monster activation limits
	_environment_m1_weapons = 0x0400,    // multiple weapon pickups on TC; low gravity grenades
        
	_environment_network= 0x2000,	// these two pseudo-environments are used to prevent items 
	_environment_single_player= 0x4000 // from arriving in the items.c code.
};

/* current map number is in player->map */
struct static_world_t
{
	int16 environment_code;
	
	int16 physics_model;
	int16 song_index;
	int16 mission_flags;
	int16 environment_flags;
	
	bool ball_in_play; // true if there's a ball in play // TODO: this smells awfully dynamic
    
	std::string level_name; // originally `char level_name[LEVEL_NAME_LENGTH];` (64-byte fixed-length C string with optional NUL)
	uint32 entry_point_flags; // game type[s], e.g. 
    
    // TODO: may want to adopt naming convention, e.g. unpack_stream_m2, to avoid confusion with future APIs
    
    void unpack_stream(uint8_t* S)
    {
        memset(this, 0, sizeof(static_world_t));
        
        StreamToValue(S, environment_code);            // 2-byte
        StreamToValue(S, physics_model);               // 2-byte
        StreamToValue(S, song_index);                  // 2-byte
        StreamToValue(S, mission_flags);               // 2-byte
        StreamToValue(S, environment_flags);           // 2-byte
        ball_in_play = false;
        S += 4*2;                                                            // 8-byte unused
        read_macroman_string(S, level_name, MAXIMUM_ANNOTATION_TEXT_LENGTH); // 64-byte
        S += 1*2;                                                            // 2-byte
        StreamToValue(S, entry_point_flags);                                 // 4-byte
    }
    
    void pack_stream(uint8_t* S)
    {
        ValueToStream(S, environment_code);
        ValueToStream(S, physics_model);
        ValueToStream(S, song_index);
        ValueToStream(S, mission_flags);
        ValueToStream(S, environment_flags);
        PadStream(8);
        write_macroman_string(S, level_name, MAXIMUM_ANNOTATION_TEXT_LENGTH);
        PadStream(2);
        ValueToStream(S, entry_point_flags);
    }
};
const unsigned int SIZEOF_static_data = 88;

enum /* game options.. */
{
	_multiplayer_game= 0x0001, /* multi or single? */
	_ammo_replenishes= 0x0002, /* Does or doesn't */
	_weapons_replenish= 0x0004, /* Weapons replenish? */
	_specials_replenish= 0x0008, /* Invisibility, Ammo? */
	_monsters_replenish= 0x0010, /* Monsters are lazarus.. */
	_motion_sensor_does_not_work= 0x00020, /* Motion sensor works */
	_overhead_map_is_omniscient=  0x0040, /* Only show teammates on overhead map */
	_burn_items_on_death= 0x0080, /* When you die, you lose everything but the initial crap.. */
	_live_network_stats= 0x0100,
	_game_has_kill_limit= 0x0200,  /* Game ends when the kill limit is reached. */
	_force_unique_teams= 0x0400, /* every player must have a unique team */
	_dying_is_penalized= 0x0800, /* time penalty for dying */
	_suicide_is_penalized= 0x1000, /* time penalty for killing yourselves */
	_overhead_map_shows_items= 0x2000,
	_overhead_map_shows_monsters= 0x4000,
	_overhead_map_shows_projectiles= 0x8000
};

enum /* cheat flags */
  {
    _allow_crosshair = 0x0001,
    _allow_tunnel_vision = 0x0002,
    _allow_behindview = 0x0004,
    _disable_carnage_messages = 0x0008,
    _disable_saving_level = 0x0010,
    _allow_overlay_map = 0x0020
  };

// TODO: not sure what to call it, but see setup_for_replay_from_file
#define default_cheat_flags  (_allow_crosshair | _allow_tunnel_vision | _allow_behindview | _allow_overlay_map)

enum // specifies how the user completed the level. saved in dynamic_data
{
	_level_unfinished, 
	_level_finished,
	_level_failed
};



enum // game types
{
	_game_of_kill_monsters,		// single player & combative (EMFH?) use this
	_game_of_cooperative_play,	// multiple players (coop?), working together
	_game_of_capture_the_flag,	// A team game.
	_game_of_king_of_the_hill,
	_game_of_kill_man_with_ball,
	_game_of_defense,
	_game_of_rugby,
	_game_of_tag,
	_game_of_custom,
	NUMBER_OF_GAME_TYPES
};

#define GET_GAME_TYPE() (dynamic_world.game_information.game_type)
#define GET_GAME_OPTIONS() (dynamic_world.game_information.game_options)
//#define GET_GAME_PARAMETER(x) (dynamic_world.game_information.parameters[(x)])

/*
 from network.h
 
 typedef struct game_info
 {
     int16 level_number;
 
     int16  net_game_type;
     int16  game_options;
     int16  kill_limit;
     uint16 initial_random_seed;
     int16  difficulty_level;

     int32  time_limit;
 
     int16 cheat_flags;
     
     std::string level_name; // should not be needed as long as we can look it up; possibly distributing it so it can be displayed, but getting rid of it simplifies our life wrt utf8
 
     uint32 original_map_file_checksum; // TODO: we need a more robust identifier for the Map file, and for all other dependencies
     
     // network parameters
     //int16  initial_updates_per_packet; //obsolete
     //int16  initial_update_latency; //obsolete
 } game_info;
 */

struct game_configuration_t // was `game_data`; setup for campaign or netmatch
{
    // this struct also gets embedded in dynamic_world_t when saving game; it'd be better if it had its own WAD tag so we can expand it in future
    
    
    // TODO: what about player identities for saved solo/coop game? right now the film header captures these; not sure about coop; solo always uses current player_preferences
    
    
    // when saving an edited level back into Map file, what data gets stored in its dynamic world and what is in dw's game_information?
    
    int16_t initial_level_number; // TODO: I added this to make consistent with game_info struct; this should prob be single source of truth, plus Map checksum
    
	int16 game_type; // One of previous enums; solo/coop is _game_of_kill_monsters; netgame can be any
	int16 game_options;
    int16 kill_limit;
    int16 initial_random_seed;
    int16 difficulty_level;
    
    // For a PvP network game, decrement each tick.
    // For a solo (+coop?) player game set to INT32_MAX, and decremented over time, so that you know how long it took you to solve the game.
    int32 game_time_remaining; // in game_info struct this is time_limit, but I think they're the same (it just implies this one is decremented)
    
    int16 cheat_flags;
    
    // these 2 are on the network-distributed version of this struct
    // level_name
    // original_map_file_checksum
	//int16 parameters[2]; // unused
    
    void clear()
    {
        memset(this, 0, sizeof(game_configuration_t));
    }
    
    
    void new_solo_game(int16_t level_number, int16_t difficulty)
    {
        initial_level_number   = level_number;
        game_time_remaining    = INT32_MAX;
        kill_limit             = 0;
        game_type              = _game_of_kill_monsters;
        game_options           = _burn_items_on_death | _ammo_replenishes | _weapons_replenish | _monsters_replenish;
        initial_random_seed    = machine_tick_count();
        difficulty_level       = difficulty;
        cheat_flags            = default_cheat_flags; // EES: added this; TODO: why wasn't this set here before? where is it/should it be set?
    }
    
    void read_stream(uint8* &S) 
    {
        StreamToValue(S, game_time_remaining);
        StreamToValue(S, game_type);
        StreamToValue(S, game_options);
        StreamToValue(S, kill_limit);
        StreamToValue(S, initial_random_seed);
        StreamToValue(S, difficulty_level);
        S += 2; // parameters is unused
    }

    void write_stream(uint8* &S)
    {
        ValueToStream(S, game_time_remaining);
        ValueToStream(S, game_type);
        ValueToStream(S, game_options);
        ValueToStream(S, kill_limit);
        ValueToStream(S, initial_random_seed);
        ValueToStream(S, difficulty_level);
        int16_t parameters[2] = {0, 0};
        ListToStream(S,parameters,2);
    }

};




struct dynamic_world_t
{
    // these members persist when teleporting to new level (ideally they'd be stored separately to map data)
    int16_t player_count; // when loading a saved game, first check number of players to determine if it's a solo or a co-op game // TODO: should be superseded by get_number_of_players (the number of Player objects in `players` vector) and get_number_of_players_from_wad (calculates number of players from length of WAD's 'plyr' entry)
    int32_t tick_count; // ticks since the beginning of the game
    int16_t total_civilian_count,  total_civilian_causalties; // presumably appears in end-of-campaign stats
    uint16_t random_seed; // the RNG's state at time game was saved; call set_random_seed(dynamic_world.random_seed) // TODO: what does this mean?
    
    game_configuration_t game_information; // persists across saves
    
    
    // these members are reset when teleporting to new level

    int16 ball_player_index; // was `game_player_index`; in pvp, this is the player currently tagged "it" or holding the ball
    int16 civilians_killed_by_players; // number of civilians killed by players; periodically decremented in move_monsters
    
    // level state
    
    int16 current_level_number; // current_level_number

    // used by item + monster placement
    int16 random_items_left[MAXIMUM_OBJECT_TYPES];
    int16 current_item_count[MAXIMUM_OBJECT_TYPES];
    int16 random_monsters_left[MAXIMUM_OBJECT_TYPES];
    int16 current_monster_count[MAXIMUM_OBJECT_TYPES];
    
    // used by new_monster() to adjust for different difficulty levels
    int16 new_monster_mangler_cookie, new_monster_vanishing_cookie;
    
    // used by move_monsters() to decide who gets to generate paths, etc.
    int16 last_monster_index_to_get_time, last_monster_index_to_build_path;
    
    // used by register_dead_monster so it knows when to start pruning corpses
    int16 dead_monster_count;
    
    int16 current_civilian_causalties, current_civilian_count;
    
    world_point2d game_beacon; // KOTH/defense // TODO: this is calculated by averaging center of _polygon_is_hill polys so don't think it needs stored in WAD data, but need to confirm (see initialize_net_game and pvp)
    
    
    // this must be called after level is loaded
    void initialize_for_new_game(game_configuration_t game_configuration)
    {
        memset(this, 0, sizeof(dynamic_world_t)); // TODO: not great; where is random_seed, etc set

        this->game_information = game_configuration;
        
        tick_count = 0;
        current_level_number = game_configuration.initial_level_number;
        random_seed = game_configuration.initial_random_seed;
        
        
        //player_count = 0;
        // initialize our globals to be the same thing on all machines
        //civilians_killed_by_players         =  0;
        last_monster_index_to_get_time      = -1;
        last_monster_index_to_build_path    = -1;
        new_monster_mangler_cookie          = global_random();
        new_monster_vanishing_cookie        = global_random();
    }
    
    
    // this must be called after level is loaded
    void initialize_for_new_level()
    {
        total_civilian_count      += current_civilian_count;
        total_civilian_causalties += current_civilian_causalties;
        
        // TODO: this should set everything after game_information to 0; unstanking it is for later
        memset(&current_level_number, 0, sizeof(dynamic_world_t) - ((uint64_t)&current_level_number - (uint64_t)this));
    }
    
    
    // TODO: use modified BStream with explicit [un]packSIZE methods so we can decouple in-memory storage from serialized format
    
    void unpack_stream(uint8_t* S)
    {
        StreamToValue(S, tick_count);
        StreamToValue(S, random_seed);
        game_information.read_stream(S);
        StreamToValue(S, player_count);
        
        S += 38; // map counts, mostly
        
        StreamToValue(S, dead_monster_count);
        StreamToValue(S, last_monster_index_to_get_time);
        StreamToValue(S, last_monster_index_to_build_path);
        StreamToValue(S, new_monster_mangler_cookie);
        StreamToValue(S, new_monster_vanishing_cookie);
        
        StreamToValue(S, civilians_killed_by_players);
        
        StreamToList(S, random_monsters_left,  MAXIMUM_OBJECT_TYPES);
        StreamToList(S, current_monster_count, MAXIMUM_OBJECT_TYPES);
        StreamToList(S, random_items_left,     MAXIMUM_OBJECT_TYPES);
        StreamToList(S, current_item_count,    MAXIMUM_OBJECT_TYPES);

        StreamToValue(S, current_level_number);
        
        StreamToValue(S, current_civilian_causalties);
        StreamToValue(S, current_civilian_count);
        StreamToValue(S, total_civilian_causalties);
        StreamToValue(S, total_civilian_count);
        
        StreamToValue(S, game_beacon.x);
        StreamToValue(S, game_beacon.y);
        StreamToValue(S, ball_player_index);
    }


    void pack_stream(uint8_t* S)
    {
        ValueToStream(S, tick_count);
        ValueToStream(S, random_seed);
        game_information.write_stream(S);
        ValueToStream(S, get_number_of_players()); // redundant since the 'plyr' chunk determines actual number, but for now it's easiest to get it from dynamic world data; TODO: once there's a nice efficient friendly WAD[File] class, we can just ask that for number of entries in 'plyr'
        
        // not sure how checksum is calculated, so let's pad with zeroes for now
        
        PadStream(38); // static map counts, mostly
        
        // solo/coop player state[s] are stored under a different WAD tag; TODO: what about player identities?
        ValueToStream(S, dead_monster_count);
        ValueToStream(S, last_monster_index_to_get_time);
        ValueToStream(S, last_monster_index_to_build_path);
        ValueToStream(S, new_monster_mangler_cookie);
        ValueToStream(S, new_monster_vanishing_cookie);
        
        ValueToStream(S, civilians_killed_by_players);
        
        ListToStream(S, random_monsters_left,  MAXIMUM_OBJECT_TYPES);
        ListToStream(S, current_monster_count, MAXIMUM_OBJECT_TYPES);
        ListToStream(S, random_items_left,     MAXIMUM_OBJECT_TYPES);
        ListToStream(S, current_item_count,    MAXIMUM_OBJECT_TYPES);

        ValueToStream(S, current_level_number);
        
        ValueToStream(S, current_civilian_causalties);
        ValueToStream(S, current_civilian_count);
        ValueToStream(S, total_civilian_causalties);
        ValueToStream(S, total_civilian_count);
        
        ValueToStream(S, game_beacon.x);
        ValueToStream(S, game_beacon.y);
        ValueToStream(S, ball_player_index);
    }

};
const unsigned int SIZEOF_dynamic_data = 604;


// the currently loaded level's geometry, object placement, automap visibility; see also lights.cpp, platforms.cpp, etc.

extern static_world_t static_world;
extern dynamic_world_t dynamic_world;

extern std::vector<object_data> ObjectList;

extern std::vector<endpoint_data> EndpointList;
extern std::vector<line_data> LineList;
extern std::vector<side_data> SideList;
extern std::vector<polygon_data> PolygonList;

extern std::vector<ambient_sound_image_data> AmbientSoundImageList;
extern std::vector<random_sound_image_data> RandomSoundImageList;


// TODO: move these to automap.hpp?
extern std::vector<int16_t> MapIndexList;
extern std::vector<map_annotation> MapAnnotationList;


extern std::vector<map_object> SavedObjectList;


extern bool game_is_networked(); /* true if this is a network game */

// Whether or not Marathon 2/oo landscapes had been loaded (switch off for Marathon 1 compatibility)
extern bool LandscapesLoaded;

// The index number of the first texture loaded (should be the main wall texture);
// needed for infravision fog when landscapes are switched off
extern short LoadedWallTexture;



// TODO: move these declarations to marathon2.h

void initialize_marathon();

void enter_gameworld(bool is_restoring_saved_game); // when restoring a saved game, there may be saved script state (but why isn't that determined automatically by looking for it in the damn wad?)

void exit_gameworld();

bool update_world(int32_t& elapsed_time); // returns needs_redraw

// ZZZ: these really don't go here, but they live in marathon2.cpp where update_world() lives.....
void reset_intermediate_action_queues();
void set_prediction_wanted(bool inPrediction);

/* Called to activate lights, platforms, etc. (original polygon may be NONE) */
void changed_polygon(short original_polygon_index, short new_polygon_index, short player_index);

short calculate_damage(struct damage_definition *damage);
void cause_polygon_damage(short polygon_index, short monster_index);

short calculate_level_completion_state();
short calculate_classic_level_completion_state(void);


/* ---------- prototypes/MAP.C */

void mark_environment_collections(short environment_code, bool loading);
void mark_map_collections(bool loading);
bool collection_in_environment(short collection_code, short environment_code);

bool valid_point2d(world_point2d *p);
bool valid_point3d(world_point3d *p);

void reconnect_map_object_list(void);
short new_map_object2d(world_point2d *location, short polygon_index, shape_descriptor shape, angle facing);
short new_map_object3d(world_point3d *location, short polygon_index, shape_descriptor shape, angle facing);
short new_map_object(struct object_location *location, shape_descriptor shape);
short attach_parasitic_object(short host_index, shape_descriptor shape, angle facing);
void remove_parasitic_object(short host_index);
bool translate_map_object(short object_index, world_point3d *new_location, short new_polygon_index);
short find_new_object_polygon(world_point2d *parent_location, world_point2d *child_location, short parent_polygon_index);
void remove_map_object(short index);


// ZZZ additions in support of prediction:

void add_object_to_polygon_object_list(short object_index, short polygon_index);
void add_object_to_polygon_object_list(short object_index); // infers polygon_index from the object's "polygon" member field

// Schedules object at object_index for later insertion into a polygon object list. It'll be inserted before the
// object with index index_to_precede (which had better be in the list or be scheduled for insertion by the time
// perform_deferred_polygon_object_list_manipulations() is called, else A1 will assert).
void deferred_add_object_to_polygon_object_list(short object_index, short index_to_precede);

// Actually does the insertions scheduled by deferred_add_object_to_polygon_object_list(). Uses the polygon
// index each scheduled object has _when this function is called_, not whatever polygon index it had when
// deferred_add_object_to_polygon_object_list() was called!
void perform_deferred_polygon_object_list_manipulations();

// Removes the object at object_index from the polygon with index in object's 'polygon' field.
void remove_object_from_polygon_object_list(short object_index, short polygon_index);
void remove_object_from_polygon_object_list(short object_index);



struct shape_and_transfer_mode
{
	/* extended shape descriptor */
	short collection_code, low_level_shape_index;
	
	short transfer_mode;
	_fixed transfer_phase; /* [0,FIXED_ONE] */
	
	// Needed for animated models: which frame in an individual sequence (0, 1, 2, ...)
	short Frame, NextFrame;
	
	// Needed for animated models: which tick in a frame, and total ticks per frame
	short Phase, Ticks;
};

void get_object_shape_and_transfer_mode(world_point3d *camera_location, short object_index, struct shape_and_transfer_mode *data);
void get_object_shape_and_transfer_mode(world_point3d *camera_location, object_data* object, shape_and_transfer_mode *data);
void set_object_shape_and_transfer_mode(short object_index, shape_descriptor shape, short transfer_mode);
void animate_object(short object_index); /* assumes ∂t==1 tick */
void animate_object(object_data* data, int16_t object_index);
bool randomize_object_sequence(short object_index, shape_descriptor shape);

world_location3d* get_object_sound_location(short object_index);
void play_object_sound(short object_index, short sound_code, bool local_sound = false);
void play_polygon_sound(short polygon_index, short sound_code);
void play_side_sound(short side_index, short sound_code, _fixed pitch, bool soft_rewind = false);
void play_world_sound(short polygon_index, world_point3d *origin, short sound_code);

void handle_random_sound_image(void);

void initialize_map_for_new_player(void);
void generate_map(short level);

short world_point_to_polygon_index(world_point2d *location);
short clockwise_endpoint_in_line(short polygon_index, short line_index, short index);

short find_adjacent_polygon(short polygon_index, short line_index);
short find_flooding_polygon(short polygon_index);
short find_adjacent_side(short polygon_index, short line_index);
short find_shared_line(short polygon_index1, short polygon_index2);
bool line_is_landscaped(short polygon_index, short line_index, world_distance z);
short find_line_crossed_leaving_polygon(short polygon_index, world_point2d *p0, world_point2d *p1);
bool point_in_polygon(short polygon_index, world_point2d *p);
void find_center_of_polygon(short polygon_index, world_point2d *center);

int32 point_to_line_segment_distance_squared(world_point2d *p, world_point2d *a, world_point2d *b);
int32 point_to_line_distance_squared(world_point2d *p, world_point2d *a, world_point2d *b);

_fixed closest_point_on_line(world_point2d *e0, world_point2d *e1, world_point2d *p, world_point2d *closest_point);
void closest_point_on_circle(world_point2d *c, world_distance radius, world_point2d *p, world_point2d *closest_point);

_fixed find_line_intersection(world_point2d *e0, world_point2d *e1, world_point3d *p0,
	world_point3d *p1, world_point3d *intersection);
_fixed find_floor_or_ceiling_intersection(world_distance h, world_point3d *p0, world_point3d *p1, world_point3d *intersection);

void ray_to_line_segment(world_point2d *p0, world_point2d *p1, angle theta, world_distance d);

void push_out_line(world_point2d *e0, world_point2d *e1, world_distance d, world_distance line_length);
bool keep_line_segment_out_of_walls(short polygon_index, world_point3d *p0,
	world_point3d *p1, world_distance maximum_delta_height, world_distance height, world_distance *adjusted_floor_height,
	world_distance *adjusted_ceiling_height, short *supporting_polygon_index);

_fixed get_object_light_intensity(short object_index);

bool line_has_variable_height(short line_index);

void recalculate_map_counts(void);

bool change_polygon_height(short polygon_index, world_distance new_floor_height,
	world_distance new_ceiling_height, struct damage_definition *damage);

bool line_is_obstructed(short polygon_index1, world_point2d* p1, short polygon_index2, world_point2d* p2, bool for_sounds = false);
bool point_is_player_visible(short max_players, short polygon_index, world_point2d *p, int32 *distance);
bool point_is_monster_visible(short polygon_index, world_point2d *p, int32 *distance);

void register_dead_monster(short garbage_object_index);

void random_point_on_circle(world_point3d *center, short center_polygon_index,
	world_distance radius, world_point3d *random_point, short *random_polygon_index);

void calculate_line_midpoint(short line_index, world_point3d *midpoint);

void *get_map_structure_chunk(long chunk_size);
void reallocate_map_structure_memory(long size);

/* ---------- prototypes/MAP_ACCESSORS.C */

// LP changed: previously inline; now de-inlined for less code bulk
// When the index is out of range,
// the geometry ones make failed asserts,
// while the sound ones return null pointers.

object_data *get_object_data(
	const short object_index);

polygon_data *get_polygon_data(
	const short polygon_index);

line_data *get_line_data(
	const short line_index);

side_data *get_side_data(
	const short side_index);

endpoint_data *get_endpoint_data(
	const short endpoint_index);

short *get_map_indexes(
	const short index,
	const short count);

ambient_sound_image_data *get_ambient_sound_image_data(
	const short ambient_sound_image_index);

random_sound_image_data *get_random_sound_image_data(
	const short random_sound_image_index);

/* ---------- prototypes/MAP_CONSTRUCTORS.C */

short new_map_endpoint(world_point2d *where);
short duplicate_map_endpoint(short old_endpoint_index);
short new_map_line(short a, short b, short poly_a, short poly_b, short side_a, short side_b);
short duplicate_map_line(short old_line_index);
short new_map_polygon(short *line_indexes, short line_count, short floor_height,
	short ceiling_height, short floor_texture, short ceiling_texture, short lightsource_index);
void recalculate_side_type(short side_index);
short new_side(short polygon_index, short line_index);

void precalculate_map_indexes(void);

void touch_polygon(short polygon_index);
void recalculate_redundant_polygon_data(short polygon_index);
void recalculate_redundant_endpoint_data(short endpoint_index);
void recalculate_redundant_line_data(short line_index);
void recalculate_redundant_side_data(short side_index, short line_index);

void calculate_endpoint_polygon_owners(short endpoint_index, short *first_index, short *index_count);
void calculate_endpoint_line_owners(short endpoint_index, short *first_index, short *index_count);

void guess_side_lightsource_indexes(short side_index);

void set_map_index_buffer_size(long length);


// EES: not entirely clear on distinction between 'point' and 'endpoint' but clarify another time
void unpack_point_data(uint8 *S, size_t count);

uint8 *unpack_endpoint_data(uint8 *Stream, size_t Count);
uint8 *pack_endpoint_data(uint8 *Stream, endpoint_data* Objects, size_t Count);
uint8 *unpack_line_data(uint8 *Stream, size_t Count);
uint8 *pack_line_data(uint8 *Stream, line_data* Objects, size_t Count);
uint8 *unpack_side_data(uint8 *Stream, size_t Count, int16_t version);
uint8 *pack_side_data(uint8 *Stream, side_data* Objects, size_t Count);
uint8 *unpack_polygon_data(uint8 *Stream, size_t Count, int16_t version);
uint8 *pack_polygon_data(uint8 *Stream, polygon_data* Objects, size_t Count);

void unpack_automap_line_data(uint8 *Stream, size_t count);
void unpack_automap_polygon_data(uint8 *Stream, size_t count);

void unpack_map_index_data(uint8 *Stream, size_t count);

uint8 *unpack_map_annotations(uint8 *Stream, size_t Count);
uint8 *pack_map_annotation(uint8 *Stream, map_annotation* Objects, size_t Count);
uint8 *unpack_map_objects(uint8 *Stream, size_t Count, int version);
uint8 *pack_map_object(uint8 *Stream, map_object* Objects, size_t Count);
uint8 *unpack_object_frequency_definition(uint8 *Stream, object_frequency_definition* Objects, size_t Count);
uint8 *pack_object_frequency_definition(uint8 *Stream, object_frequency_definition* Objects, size_t Count);
uint8 *unpack_ambient_sound_image_data(uint8 *Stream, size_t Count);
uint8 *pack_ambient_sound_image_data(uint8 *Stream, ambient_sound_image_data* Objects, size_t Count);
uint8 *unpack_random_sound_image_data(uint8 *Stream, size_t Count);
uint8 *pack_random_sound_image_data(uint8 *Stream, random_sound_image_data* Objects, size_t Count);

uint8 *unpack_object_data(uint8 *Stream, size_t Count);
uint8 *pack_object_data(uint8 *Stream, object_data* Objects, size_t Count);

uint8 *unpack_damage_definition(uint8 *Stream, damage_definition* Objects, size_t Count);
uint8 *pack_damage_definition(uint8 *Stream, damage_definition* Objects, size_t Count);


/* ---------- prototypes/PLACEMENT.C */

void unpack_placement_data(uint8* Stream, size_t Count);


void initialize_items_and_monsters(void);
void recreate_objects(void);
void object_was_just_added(short object_class, short object_type);
void object_was_just_destroyed(short object_class, short object_type);
short get_random_player_starting_location_and_facing(short max_player_index, short team, struct object_location *location);

void mark_all_monster_collections(bool loading);
void load_all_monster_sounds(void);

/* ---------- prototypes/GAME_DIALOGS.C */

/* --------- prototypes/LIGHTSOURCE.C */

void update_lightsources(void);
short new_lightsource_from_old(short old_source);
void entered_polygon(short index);
void left_polygon(short index);
/* Only send _light_turning_on, _light_turning_off, _light_toggle */
void change_light_state(size_t lightsource_index, short state);

/* ---------- prototypes/DEVICES.C */

void mark_control_panel_shapes(bool load);
void initialize_control_panels(void); 
void update_control_panels(void);

bool control_panel_in_environment(short control_panel_type, short environment_code);

void change_device_state(short device_index, bool active);
short new_device(world_point2d *location, short initial_polygon_index, 
	short type, short extra_data, bool active);
void update_action_key(short player_index, bool triggered);

bool untoggled_repair_switches_on_level(bool only_last_switch = false);

void assume_correct_switch_position(short switch_type, short permutation, bool new_state);

void try_and_toggle_control_panel(short polygon_index, short line_index, short projectile_index);

bool line_side_has_control_panel(short line_index, short polygon_index, short *side_index_with_panel);

/* ---------- prototypes/GAME_WAD.C */


// index = NONE means use any starting location
short get_player_starting_location_and_facing(short team, short index, object_location& location);
short get_number_of_players_starting_location_and_facing(short team, short index);


// find levels which support the specified game type[s]; on success, populates level_identity and updates
// start_at_index for use in the next get_next_level_ call
ao_err get_next_level_for_game_types(int32_t game_type_flags, int16_t& start_at_index, level_identity& level_info); // defined in map_wad.cpp

bool get_all_levels_for_game_types(std::vector<level_identity> &result, int32_t game_type_flags); // TODO: update to return ao_err



class InfoTree;
void parse_mml_texture_loading(const InfoTree& root);
void reset_mml_texture_loading();

#endif
