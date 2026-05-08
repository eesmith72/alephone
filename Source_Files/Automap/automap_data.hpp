/*
 automap_data.hpp -- structs containing visibility and appearance data
                       for use in AutomapRenderer
 
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

#ifndef automap_data_hpp
#define automap_data_hpp


#include "cseries.hpp"

#include "fonts.hpp"

#include "monsters.h" // NUMBER_OF_MONSTER_TYPES
//#include "shapes.h" // NUMBER_OF_COLLECTIONS
#include "world.h"


#define OVERHEAD_MAP_MINIMUM_SCALE 1
#define OVERHEAD_MAP_MAXIMUM_SCALE 4
#define DEFAULT_OVERHEAD_MAP_SCALE 3


// TODO: Modern automap really needs a 'Mission: Exploration objectives 0/5' banner so user knows when they've found everything, possibly also highlighting the found polys on automap.



// TODO: pulled from camera.cpp's badly named `reset_screen`; where to put it?
// graphics_preferences.automap_size = DEFAULT_OVERHEAD_MAP_SCALE;



// keep this state separate to renderer so player automap and quicksave thumbnail maps can share a common instance (what player has seen)
struct automap_visibility_t
{
    enum class mode_t : int32_t
    {
        normal, // everything the player has seen
        camera, // what's currently visible to the player
        global, // the entire map (e.g. cheat mode, 2D map editor)
    };
    
    mode_t mode;
    
    std::vector<uint8_t> lines, polygons;
    
    size_t line_count = 0, polygon_count;
    
    // important: constructor/configure must be called with the current Map's line and polygon counts before use
    
#define calculate_automap_list_size(count)  ((count) / 8 + (((count) % 8) ? 1 : 0))

    void configure(size_t line_count = 0, size_t polygon_count = 0)
    {
        lines.resize(calculate_automap_list_size(line_count));
        polygons.resize(calculate_automap_list_size(polygon_count));
        
        // if this instance is allocated once and reused over multiple levels, clear the old state
        if (this->line_count != 0)
        {
            if (mode == mode_t::global)
                reveal_all();
            else
                clear_all();
        }
        
        this->line_count = line_count;
        this->polygon_count = polygon_count;
    }

#undef calculate_automap_list_size

    
    automap_visibility_t(size_t line_count = 0, size_t polygon_count = 0, mode_t mode_ = mode_t::normal)
    {
        configure(line_count, polygon_count);
        mode = mode_t::normal;
    }
    
    ~automap_visibility_t() = default;
    
    
    void clear_all()
    {
        std::fill(lines.begin(), lines.end(), 0x00);
        std::fill(polygons.begin(), polygons.end(), 0x00);
    }

    void reveal_all()
    {
        std::fill(lines.begin(), lines.end(), 0xff);
        std::fill(polygons.begin(), polygons.end(), 0xff);
    }
    
    // EES: nice!
#define get_bit(index) ((uint8_t)1 << (index & 0x07))
    
    void add_line(int32_t index)    { lines[index >> 3] |=  get_bit(index); }
    void remove_line(int32_t index) { lines[index >> 3] &= ~get_bit(index); }
    bool has_line(int32_t index)    { return lines[index >> 3] & get_bit(index); }
    
    void add_polygon(int32_t index)    { polygons[index >> 3] |=  get_bit(index); }
    void remove_polygon(int32_t index) { polygons[index >> 3] &= ~get_bit(index); }
    bool has_polygon(int32_t index)    { return polygons[index >> 3] & get_bit(index); }

#undef get_bit
    
    void fill_polygon(short polygon_index);
    
    void flood_fill_polygons(short polygon_index);
    
    
    void refresh()
    {
        switch (mode)
        {
            case mode_t::normal: // Do nothing here (mapping is cumulative)
                break;
            case mode_t::camera: // Discard previous visibility
                clear_all();
                break;
            case mode_t::global: // Make everything visible
                reveal_all();
                break;
        };
    }
};


// the in-game automap; configured by map_wad and updated by RenderVisTree
extern automap_visibility_t player_automap_visibility;


// modifies rendering according to need
enum class automap_type_t : int32_t
{
	saved_game_preview,
	terminal_checkpoint,
	player_in_game,
    // TODO: floorplan_editor,
};



enum /* polygon colors */
{
    _polygon_color,
    _polygon_platform_color,
    _polygon_water_color,
    _polygon_lava_color,
    _polygon_sewage_color,
    _polygon_jjaro_color,     // LP addition
    _polygon_goo_color,        // LP: PfhorSlime moved down here
    _polygon_hill_color,
    _polygon_minor_ouch_color,    // LP, AlexJS: these two added for M1 compatibility
    _polygon_major_ouch_color,
    _polygon_teleporter_color,
    NUMBER_OF_POLYGON_COLORS,
    // For backwards compatibility: all those before the "ouch" colors
    NUMBER_OF_OLD_POLYGON_COLORS = _polygon_hill_color + 1
};



const int NUMBER_OF_ZOOM_LEVELS = OVERHEAD_MAP_MAXIMUM_SCALE - OVERHEAD_MAP_MINIMUM_SCALE + 1;

// Data constituents

// Note: all the colors were changed from RGBColor to ao_rgb,
// which has the same members (3 unsigned shorts), but which is intended to be more portable.

enum /* line colors */
{
    _solid_line_color,
    _elevation_line_color,
    _control_panel_line_color,
    NUMBER_OF_LINE_DEFINITIONS
};

struct automap_line_style_t
{
    ao_rgb color;
    short pen_sizes[NUMBER_OF_ZOOM_LEVELS];
};



enum /* thing colors */
{
    _civilian_thing,
    _monster_thing,
    _item_thing,
    _projectile_thing,
    _checkpoint_thing,
    NUMBER_OF_THINGS
};

enum
{
    _rectangle_thing,
    _circle_thing
};

struct automap_shape_style_t
{
    ao_rgb color;
    short shape;
    short radii[NUMBER_OF_ZOOM_LEVELS];
};


struct automap_player_style_t
{
    short front, rear, rear_theta;
};


struct automap_label_style_t // annotations
{
    ao_rgb color;
    font_key_t Fonts[NUMBER_OF_ZOOM_LEVELS];
};

// For some reason, only one annotation color was ever implemented
const int NUMBER_OF_ANNOTATION_DEFINITIONS = 1;


struct automap_title_style_t // map name at top of screen
{
    ao_rgb color;
    font_key_t key;
    short offset_down; // from top of screen
};




struct automap_appearance_t
{
    // this order of members is unfriendly but not rearranging automap_style_std table right now
    ao_rgb polygon_colors[NUMBER_OF_POLYGON_COLORS];
    automap_line_style_t line_definitions[NUMBER_OF_LINE_DEFINITIONS];
    automap_shape_style_t thing_definitions[NUMBER_OF_THINGS];
    short monster_displays[NUMBER_OF_MONSTER_TYPES];
    short dead_monster_displays[NUMBER_OF_COLLECTIONS];
    // Note: there is only one definition of the player-entity shape; TODO: what if we want to show map of PvP game
    automap_player_style_t player_entity;
    automap_label_style_t annotation_definitions[NUMBER_OF_ANNOTATION_DEFINITIONS]; // currently only one of these, but might increase for 2D map editor
    automap_title_style_t map_name_data;
    ao_rgb path_color;
    
    // Which of these to show // TODO: move these to automap_visibility_t
    bool ShowAliens;
    bool ShowItems;
    bool ShowProjectiles;
    bool ShowPaths;
    
    
    void InitMapFonts() // where should this go?
    {
        /*
        // Init the fonts the first time through
        if (!MapFontsInited)
        {
            for (int i = 0; i < NUMBER_OF_ANNOTATION_DEFINITIONS; i++)
            {
                automap_label_style_t& NoteDef = automap_settings_std.annotation_definitions[i];
                for (int j = 0; j < NUMBER_OF_ZOOM_LEVELS; j++) {
                    if (!NoteDef.Fonts[j].Info)
                        NoteDef.Fonts[j].Init();
                }
            }

            if (!automap_settings_std.map_name_data.Font.Info)
                automap_settings_std.map_name_data.Font.Init();

            MapFontsInited = true;
        }
         */
    }
    
};


extern automap_appearance_t player_automap_appearance;



// TODO: this needs updated

class InfoTree;
void parse_mml_overhead_map(const InfoTree& root);
void reset_mml_overhead_map();


#endif /* automap_data_hpp */
