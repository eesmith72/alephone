/*
 input_preferences.hpp
 
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

#ifndef input_preferences_hpp
#define input_preferences_hpp

#include "cseries.h"

#include "InfoTree.h"




#define NUMBER_OF_KEYS 21
#define NUMBER_UNUSED_KEYS 10

enum // input devices
{
    _keyboard_or_game_pad,
    _mouse_yaw_pitch
};


// LP addition: input-modifier flags
// run/walk and swim/sink
// LP addition: Josh Elsasser's dont-switch-weapons patch
enum {
    _inputmod_interchange_run_walk      = 0x0001,
    _inputmod_interchange_swim_sink     = 0x0002,
    _inputmod_dont_switch_to_new_weapon = 0x0004,
    _inputmod_invert_mouse              = 0x0008,
    _inputmod_use_button_sounds         = 0x0010,
    _inputmod_dont_auto_recenter        = 0x0020,   // ZZZ addition
    _inputmod_run_key_toggle            = 0x0040,
};


// shell keys
enum {
    _key_inventory_left,
    _key_inventory_right,
    _key_switch_view,
    _key_volume_up,
    _key_volume_down,
    _key_zoom_in,
    _key_zoom_out,
    _key_toggle_fps,
    _key_activate_console,
    _key_show_scores,
    NUMBER_OF_SHELL_KEYS
};

enum {
    _mouse_accel_none,
    _mouse_accel_classic,
    _mouse_accel_symmetric,
    NUMBER_OF_MOUSE_ACCEL_TYPES
};

static constexpr int NUMBER_OF_HOTKEYS = 12;

typedef std::map<int, std::set<SDL_Scancode>> key_binding_map;



struct input_preferences_data
{
    int16 input_device;

    uint16 modifiers;
    
    // Mouse-sensitivity parameters (LP: originally ZZZ)
    _fixed sens_horizontal;
    _fixed sens_vertical;
    int16 mouse_accel_type;
    float mouse_accel_scale;
    bool raw_mouse_input;
    bool extra_mouse_precision;
    bool classic_vertical_aim;
    
    // Limit absolute-mode {yaw, pitch} deltas per tick to +/- {32, 8} instead of {63, 15}
    bool classic_aim_speed_limits;
    
    bool controller_analog;
    bool controller_aim_inverted;
    _fixed controller_sensitivity_horizontal;
    _fixed controller_sensitivity_vertical;
    // if an axis reading is taken below this number in absolute
    // value, then we clip it to 0.  this lets people use
    // inaccurate zero points.
    int16 controller_deadzone_horizontal;
    int16 controller_deadzone_vertical;

    key_binding_map key_bindings;
    key_binding_map shell_key_bindings;
    key_binding_map hotkey_bindings;
    
    void reset();
    
    void read(InfoTree root, std::string version);
    
    InfoTree write();
};




extern input_preferences_data input_preferences;


void initialize_input_preferences();


void controls_dialog(void *arg);
void keyboard_dialog(void *arg);




#endif /* input_preferences_hpp */
