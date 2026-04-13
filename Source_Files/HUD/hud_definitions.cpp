/*
 HUDRenderer.cpp - Lua HUD rendering
 
 Written in 2001 by Christian Bauer
 
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


#include "hud_definitions.hpp"

#include "items.h" // _i_knife, etc
#include "InfoTree.h"



static const std::array<weapon_interface_data, 10> default_weapon_interface_definitions = {
    (weapon_interface_data)
    /* Mac, the knife.. */
    {
        _i_knife,
        UNONE,
        433, 432,
        NONE, NONE,
        0, 0,
        false,
        {(weapon_interface_ammo_data)
            { _uses_none, 0, 0, 0, 0, 0, 0, UNONE, UNONE, true},
            { _uses_none, 0, 0, 0, 0, 0, 0, UNONE, UNONE, true}
        },
        UNONE, UNONE,
        0, 0
    },
    
    /* Harry, the .44 */
    {
        _i_magnum,
        BUILD_DESCRIPTOR(_collection_interface, _magnum_panel),
        432, 444,
        420, NONE,
        366, 517,
        true,
        {(weapon_interface_ammo_data)
            { _uses_bullets, 517, 412, 8, 1, 5, 14, BUILD_DESCRIPTOR(_collection_interface, _magnum_bullet), BUILD_DESCRIPTOR(_collection_interface, _magnum_casing), false},
            { _uses_bullets, 452, 412, 8, 1, 5, 14, BUILD_DESCRIPTOR(_collection_interface, _magnum_bullet), BUILD_DESCRIPTOR(_collection_interface, _magnum_casing), true}
        },
        BUILD_DESCRIPTOR(_collection_interface, _left_magnum),
        BUILD_DESCRIPTOR(_collection_interface, _left_magnum_unusable),
        -97, 0
    },

    /* Ripley, the plasma pistol. */
    {
        _i_plasma_pistol,
        BUILD_DESCRIPTOR(_collection_interface, _zeus_panel),
        431, 443,
        401, NONE,
        366, 475,
        false,
        {(weapon_interface_ammo_data)
            { _uses_energy, 414, 366, 20, 0, 38, 57, _energy_weapon_full_color, _energy_weapon_empty_color, true},
            { _uses_none, 450, 410, 50, 0, 62, 7, _energy_weapon_full_color, _energy_weapon_empty_color, true}
        },
        UNONE, UNONE,
        0, 0
    },
    
    /* Arnold, the assault rifle */
    {
        _i_assault_rifle,
        BUILD_DESCRIPTOR(_collection_interface, _assault_panel),
        430, 452,
        439, NONE, //••
        366, 460,
        false,
        {(weapon_interface_ammo_data)
            { _uses_bullets, 391, 368, 13, 4, 4, 10, BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_bullet), BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_casing), true},
            { _uses_bullets, 390, 413, 7, 1, 8, 12, BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_grenade), BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_grenade_casing), true},
        },
        UNONE, UNONE,
        0, 0
    },
        
    /* John R., the missile launcher */
    {
        _i_missile_launcher,
        BUILD_DESCRIPTOR(_collection_interface, _missile_panel),
        433, 445,
        426, NONE,
        365, 419,
        false,
        {(weapon_interface_ammo_data)
            { _uses_bullets, 385, 376, 2, 1, 16, 49, BUILD_DESCRIPTOR(_collection_interface, _missile), BUILD_DESCRIPTOR(_collection_interface, _missile_casing), true},
            { _uses_none, 0, 0, 0, 0, 0, 0, UNONE, UNONE, true }
        },
        UNONE, UNONE,
        0, 0
    },

    /* ???, the flame thrower */
    {
        _i_flamethrower,
        BUILD_DESCRIPTOR(_collection_interface, _flamethrower_panel),
        433, 445,
        398, NONE,
        363, 475,
        false,
        {(weapon_interface_ammo_data)
            /* This weapon has 7 seconds of flamethrower carnage.. */
            { _uses_energy, 427, 369, 7*TICKS_PER_SECOND, 0, 38, 57, _energy_weapon_full_color, _energy_weapon_empty_color, true},
            { _uses_none, 450, 410, 50, 0, 62, 7, _energy_weapon_full_color, _energy_weapon_empty_color, true}
        },
        UNONE, UNONE,
        0, 0
    },

    /* Predator, the alien shotgun */
    {
        _i_alien_shotgun,
        BUILD_DESCRIPTOR(_collection_interface, _alien_weapon_panel),
        418, 445,
        395, 575,
        359, 400,
        false,
        {(weapon_interface_ammo_data)
            { _uses_none, 425, 411, 50, 0, 96, 7, _energy_weapon_full_color, _energy_weapon_empty_color, true},
            { _uses_none, 450, 410, 50, 0, 62, 7, _energy_weapon_full_color, _energy_weapon_empty_color, true}
        },
        UNONE, UNONE,
        0, 0
    },

    /* Shotgun */
    {
        _i_shotgun,
        BUILD_DESCRIPTOR(_collection_interface, _single_shotgun),
        432, 444,
        420, NONE,
        373, 451,
        true,
        {(weapon_interface_ammo_data)
            { _uses_bullets, 483, 411, 2, 1, 12, 16, BUILD_DESCRIPTOR(_collection_interface, _shotgun_bullet), BUILD_DESCRIPTOR(_collection_interface, _shotgun_casing), true},
            { _uses_bullets, 451, 411, 2, 1, 12, 16, BUILD_DESCRIPTOR(_collection_interface, _shotgun_bullet), BUILD_DESCRIPTOR(_collection_interface, _shotgun_casing), true}
        },
        BUILD_DESCRIPTOR(_collection_interface, _double_shotgun),
        UNONE,
        0, -12
    },

    /* Ball */
    {
        _i_red_ball, // statistically unlikely to be valid (really should be SKULL)
        BUILD_DESCRIPTOR(_collection_interface, _skull),
        432, 444,
        402, NONE,
        366, 465,
        false,
        {(weapon_interface_ammo_data)
            { _uses_none, 451, 411, 2, 1, 12, 16, BUILD_DESCRIPTOR(_collection_interface, _shotgun_bullet), BUILD_DESCRIPTOR(_collection_interface, _shotgun_casing), true},
            { _uses_none, 483, 411, 2, 1, 12, 16, BUILD_DESCRIPTOR(_collection_interface, _shotgun_bullet), BUILD_DESCRIPTOR(_collection_interface, _shotgun_casing), true}
        },
        UNONE, UNONE,
        0, 0
    },
    
    /* LP addition: SMG (clone of assault rifle) */
    {
        _i_smg,
        BUILD_DESCRIPTOR(_collection_interface, _smg),
        430, 452,
        439, NONE, //••
        366, 460,
        false,
        {(weapon_interface_ammo_data)
            { _uses_bullets, 405, 382, 8, 4, 5, 10, BUILD_DESCRIPTOR(_collection_interface, _smg_bullet), BUILD_DESCRIPTOR(_collection_interface, _smg_casing), true},
            { _uses_none, 390, 413, 7, 1, 8, 12, BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_grenade), BUILD_DESCRIPTOR(_collection_interface, _assault_rifle_grenade_casing), true},
        },
        UNONE, UNONE,
        0, 0
    },
};


std::array<weapon_interface_data, 10> weapon_interface_definitions;



// MML


void reset_mml_hud_definitions()
{
    weapon_interface_definitions = default_weapon_interface_definitions;
}


void parse_mml_hud_definitions(const InfoTree& root)
{
    for (const InfoTree &weapon : root.children_named("weapon"))
    {
        int16 index;
        if (!weapon.read_indexed("index", index, weapon_interface_definitions.size())) continue;
        weapon_interface_data& def = weapon_interface_definitions[index];
        
        weapon.read_attr("shape", def.weapon_panel_shape);
        weapon.read_attr("start_y", def.weapon_name_start_y);
        weapon.read_attr("end_y", def.weapon_name_end_y);
        weapon.read_attr("start_x", def.weapon_name_start_x);
        weapon.read_attr("end_x", def.weapon_name_end_x);
        weapon.read_attr("top", def.standard_weapon_panel_top);
        weapon.read_attr("left", def.standard_weapon_panel_left);
        weapon.read_attr("multiple", def.multi_weapon);
        weapon.read_attr("multiple_shape", def.multiple_shape);
        weapon.read_attr("multiple_unusable_shape", def.multiple_unusable_shape);
        weapon.read_attr("multiple_delta_x", def.multiple_delta_x);
        weapon.read_attr("multiple_delta_y", def.multiple_delta_y);
        
        for (const InfoTree &ammo : weapon.children_named("ammo")) // primary and secondary ammo types
        {
            int16 index;
            if (!ammo.read_indexed("index", index, (int32_t)def.ammo_data.size())) continue;
            weapon_interface_ammo_data& adef = def.ammo_data[index];
            
            ammo.read_attr("type", adef.type);
            ammo.read_attr("left", adef.screen_left);
            ammo.read_attr("top", adef.screen_top);
            ammo.read_attr("across", adef.ammo_across);
            ammo.read_attr("down", adef.ammo_down);
            ammo.read_attr("delta_x", adef.delta_x);
            ammo.read_attr("delta_y", adef.delta_y);
            ammo.read_attr("bullet_shape", adef.bullet);
            ammo.read_attr("empty_shape", adef.empty_bullet);
            ammo.read_attr("right_to_left", adef.right_to_left);
        }
    }
}
