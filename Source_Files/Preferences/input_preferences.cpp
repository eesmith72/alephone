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

#include "input_preferences.hpp"

#include "preferences_support.hpp"


// TODO: active game key bindings should be a single std::map of SDL_SCANCODE_/JOYSTICK/CONTROLLER inputs to actions; this should be populated from a single table that defines all keys and their default actions, with flags for dialog groupings and whether or not a key is user-definable (F-keys are always fixed) and other flags (e.g. stateful/latching); once this is cleaned up, we can expand vbl's `action` type from uint32 to uint64 (or possibly larger) so ALL keys map to that - this is first step towards fully supporting customizations in film recordings and netgames


//*****************************************************************************
// KEY BINDINGS
//*****************************************************************************


struct key_binding_t
{
    SDL_Scancode keyboard;
    SDL_Scancode controller;
    // mouse?
    
    // action - this is an enum so it can be serialized in Prefs; we need a separate map/switch to convert enums to actions
    
    // would be nice to specify where it appears in Preferences' CONTROL tabs
    
    bool can_rebind; // we could put in-game F-keys in this table and specify they can't rebind (while the UI supports some common F-keys, it won't support them all, e.g. HUD size is only applicable in-game)
};


const int NUM_KEYS = 21;

static key_binding_map default_key_bindings = {
    { 0, { SDL_SCANCODE_W,
           static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_NEGATIVE + SDL_CONTROLLER_AXIS_LEFTY)
    } },
    { 1, { SDL_SCANCODE_S,
            static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_LEFTY)
    } },
    { 2, { SDL_SCANCODE_LEFT,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_NEGATIVE + SDL_CONTROLLER_AXIS_RIGHTX)
    } },
    { 3, { SDL_SCANCODE_RIGHT,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_RIGHTX)
    } },
    { 4, { SDL_SCANCODE_A,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_NEGATIVE + SDL_CONTROLLER_AXIS_LEFTX)
    } },
    { 5, { SDL_SCANCODE_D,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_LEFTX)
    } },
    { 6, { SDL_SCANCODE_Q,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_LEFT)
    } },
    { 7, { SDL_SCANCODE_E,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
    } },
    { 8, { SDL_SCANCODE_UP,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_NEGATIVE + SDL_CONTROLLER_AXIS_RIGHTY)
    } },
    { 9, { SDL_SCANCODE_DOWN,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_RIGHTY)
    } },
    { 10, { SDL_SCANCODE_V,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_RIGHTSTICK)
    } },
    { 11, { SDL_SCANCODE_F,
        static_cast<SDL_Scancode>(AO_SCANCODE_MOUSESCROLL_UP),
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
    } },
    { 12, { SDL_SCANCODE_R,
        static_cast<SDL_Scancode>(AO_SCANCODE_MOUSESCROLL_DOWN),
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
    } },
    { 13, { SDL_SCANCODE_SPACE,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_MOUSE_BUTTON + SDL_BUTTON_LEFT - 1),
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
    } },
    { 14, { SDL_SCANCODE_LSHIFT,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_MOUSE_BUTTON + SDL_BUTTON_RIGHT - 1),
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + SDL_CONTROLLER_AXIS_TRIGGERLEFT)
    } },
    { 15, { SDL_SCANCODE_LALT
    } },
    { 16, { SDL_SCANCODE_LCTRL,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_LEFTSTICK)
    } },
    { 17, { SDL_SCANCODE_LGUI
    } },
    { 18, { SDL_SCANCODE_TAB,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_A)
    } },
    { 19, { SDL_SCANCODE_M,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_X)
    } },
    { 20, { SDL_SCANCODE_GRAVE,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_Y)
    } },
};



static key_binding_map default_shell_key_bindings = {
    { 0, { SDL_SCANCODE_LEFTBRACKET
    } },
    { 1, { SDL_SCANCODE_RIGHTBRACKET
    } },
    { 2, { SDL_SCANCODE_BACKSPACE
    } },
    { 3, { SDL_SCANCODE_PERIOD
    } },
    { 4, { SDL_SCANCODE_COMMA
    } },
    { 5, { SDL_SCANCODE_EQUALS,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_UP)
    } },
    { 6, { SDL_SCANCODE_MINUS,
        static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_DOWN)
    } },
    { 7, { SDL_SCANCODE_SLASH
    } },
    { 8, { SDL_SCANCODE_BACKSLASH
    } },
    { 9, { SDL_SCANCODE_N
    } },
};


static key_binding_map default_hotkey_bindings = {
    { 0, { SDL_SCANCODE_1 }},
    { 1, { SDL_SCANCODE_2 }},
    { 2, { SDL_SCANCODE_3 }},
    { 3, { SDL_SCANCODE_4 }},
    { 4, { SDL_SCANCODE_5 }},
    { 5, { SDL_SCANCODE_6 }},
    { 6, { SDL_SCANCODE_7 }},
    { 7, { SDL_SCANCODE_8 }},
    { 8, { SDL_SCANCODE_9 }},
    { 9, { SDL_SCANCODE_T }},
    { 10, { SDL_SCANCODE_G }},
    { 11, { SDL_SCANCODE_B }}
};




static const char *action_name[NUM_KEYS] = {
    "Move Forward", "Move Backward", "Turn Left", "Turn Right", "Sidestep Left", "Sidestep Right",
    "Glance Left", "Glance Right", "Look Up", "Look Down", "Recenter View",
    "Previous Weapon", "Next Weapon", "Trigger", "2nd Trigger",
    "Turn -> Sidestep", "Run/Swim", "Move -> Look",
    "Action", "Auto Map", "Aux Trigger"
};

static const char *shell_action_name[NUMBER_OF_SHELL_KEYS] = {
    "Inventory Left", "Inventory Right", "Switch Player View", "Volume Up", "Volume Down", "Zoom Map In", "Zoom Map Out", "Toggle FPS", "Chat/Console", "Network Stats"
};

static const char* hotkey_action_name[NUMBER_OF_HOTKEYS] = {
    "Hotkey 1",
    "Hotkey 2",
    "Hotkey 3",
    "Hotkey 4",
    "Hotkey 5",
    "Hotkey 6",
    "Hotkey 7",
    "Hotkey 8",
    "Hotkey 9",
    "Hotkey 10",
    "Hotkey 11",
    "Hotkey 12",
};

// symbolic names for key and button bindings
static const strings_t binding_action_name = { // NUM_KEYS
    "forward", "back", "look-left", "look-right", "strafe-left",
    "strafe-right", "glance-left", "glance-right", "look-up", "look-down",
    "look-ahead", "prev-weapon", "next-weapon", "trigger-1", "trigger-2",
    "strafe", "run", "look", "action", "map",
    "microphone"
};

static const strings_t binding_shell_action_name = { // NUMBER_OF_SHELL_KEYS
    "inventory-left", "inventory-right", "switch-player-view", "volume-up", "volume-down",
    "map-zoom-in", "map-zoom-out", "fps", "chat", "net-stats"
};

static const strings_t binding_hotkey_action_name = { // NUMBER_OF_HOTKEYS
    "hotkey-1", "hotkey-2", "hotkey-3", "hotkey-4", "hotkey-5", "hotkey-6", "hotkey-7", "hotkey-8", "hotkey-9", "hotkey-10", "hotkey-11", "hotkey-12"
};

static const strings_t binding_mouse_button_name = { // NUM_SDL_MOUSE_BUTTONS
    "mouse-left", "mouse-middle", "mouse-right", "mouse-x1", "mouse-x2",
    "mouse-scroll-up", "mouse-scroll-down"
};

static const strings_t binding_scancode_name = { // static const int binding_num_scancodes = 285;
    "unknown", "unknown-1", "unknown-2", "unknown-3", "a",
    "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k",
    "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u",
    "v", "w", "x", "y", "z",
    "1", "2", "3", "4", "5",
    "6", "7", "8", "9", "unknown-39",
    "return", "escape", "backspace", "tab", "space",
    "minus", "equals", "leftbracket", "rightbracket", "backslash",
    "nonushash", "semicolon", "apostrophe", "grave", "comma",
    "period", "slash", "capslock", "f1", "f2",
    "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11", "f12",
    "printscreen", "scrolllock", "pause", "insert", "home",
    "pageup", "delete", "end", "pagedown", "right",
    "left", "down", "up", "numlockclear", "kp-divide",
    "kp-multiply", "kp-minus", "kp-plus", "kp-enter", "kp-1",
    "kp-2", "kp-3", "kp-4", "kp-5", "kp-6",
    "kp-7", "kp-8", "kp-9", "kp-0", "kp-period",
    "nonusbackslash", "application", "power", "kp-equals", "f13",
    "f14", "f15", "f16", "f17", "f18",
    "f19", "f20", "f21", "f22", "f23",
    "f24", "execute", "help", "menu", "select",
    "stop", "again", "undo", "cut", "copy",
    "paste", "find", "mute", "volumeup", "volumedown",
    "unknown-130", "unknown-131", "unknown-132", "kp-comma", "kp-equalsas400",
    "international1", "international2", "international3", "international4", "international5",
    "international6", "international7", "international8", "international9", "lang1",
    "lang2", "lang3", "lang4", "lang5", "lang6",
    "lang7", "lang8", "lang9", "alterase", "sysreq",
    "cancel", "clear", "prior", "return2", "separator",
    "out", "oper", "clearagain", "crsel", "exsel",
    "unknown-165", "unknown-166", "unknown-167", "unknown-168", "unknown-169",
    "unknown-170", "unknown-171", "unknown-172", "unknown-173", "unknown-174",
    "unknown-175", "kp-00", "kp-000", "thousandsseparator", "decimalseparator",
    "currencyunit", "currencysubunit", "kp-leftparen", "kp-rightparen", "kp-leftbrace",
    "kp-rightbrace", "kp-tab", "kp-backspace", "kp-a", "kp-b",
    "kp-c", "kp-d", "kp-e", "kp-f", "kp-xor",
    "kp-power", "kp-percent", "kp-less", "kp-greater", "kp-ampersand",
    "kp-dblampersand", "kp-verticalbar", "kp-dblverticalbar", "kp-colon", "kp-hash",
    "kp-space", "kp-at", "kp-exclam", "kp-memstore", "kp-memrecall",
    "kp-memclear", "kp-memadd", "kp-memsubtract", "kp-memmultiply", "kp-memdivide",
    "kp-plusminus", "kp-clear", "kp-clearentry", "kp-binary", "kp-octal",
    "kp-decimal", "kp-hexadecimal", "unknown-222", "unknown-223", "lctrl",
    "lshift", "lalt", "lgui", "rctrl", "rshift",
    "ralt", "rgui", "unknown-232", "unknown-233", "unknown-234",
    "unknown-235", "unknown-236", "unknown-237", "unknown-238", "unknown-239",
    "unknown-240", "unknown-241", "unknown-242", "unknown-243", "unknown-244",
    "unknown-245", "unknown-246", "unknown-247", "unknown-248", "unknown-249",
    "unknown-250", "unknown-251", "unknown-252", "unknown-253", "unknown-254",
    "unknown-255", "unknown-256", "mode", "audionext", "audioprev",
    "audiostop", "audioplay", "audiomute", "mediaselect", "www",
    "mail", "calculator", "computer", "ac-search", "ac-home",
    "ac-back", "ac-forward", "ac-stop", "ac-refresh", "ac-bookmarks",
    "brightnessdown", "brightnessup", "displayswitch", "kbdillumtoggle", "kbdillumdown",
    "kbdillumup", "eject", "sleep", "app1", "app2"
};

static const strings_t joystick_button_names = {
    "controller-a", "controller-b", "controller-x", "controller-y",
    "controller-back", "controller-guide", "controller-start",
    "controller-ls", "controller-rs", "controller-lb", "controller-rb",
    "controller-up", "controller-down", "controller-left",
    "controller-right",
    // new in SDL 2.0.14
    "controller-misc1", "controller-paddle1", "controller-paddle2",
    "controller-paddle3", "controller-paddle4",
    "controller-touchpad-button",
};

static const strings_t joystick_axes_names = {
    "controller-ls-right", "controller-ls-down", "controller-rs-right",
    "controller-rs-down", "controller-lt", "controller-rt",
    "controller-ls-left", "controller-ls-up", "controller-rs-left",
    "controller-rs-up", "controller-lt-neg", "controller-rt-neg"
};


static const std::string get_binding_joystick_button_name(int offset)
{
    assert_fail(SDL_CONTROLLER_BUTTON_MAX <= 21 && SDL_CONTROLLER_AXIS_MAX <= 12, "SDL changed the number of buttons/axes again!");

    if (offset < SDL_CONTROLLER_BUTTON_MAX)
    {
        return joystick_button_names[offset];
    }
    else
    {
        return joystick_axes_names[offset - SDL_CONTROLLER_BUTTON_MAX];
    }
}


static const std::string binding_name_for_code(SDL_Scancode code)
{
    int i = static_cast<int>(code);
    if (i >= 0 && i < binding_scancode_name.size())
        return binding_scancode_name[i];
    else if (i >= AO_SCANCODE_BASE_MOUSE_BUTTON && i < (AO_SCANCODE_BASE_MOUSE_BUTTON + NUM_SDL_MOUSE_BUTTONS))
        return binding_mouse_button_name[i - AO_SCANCODE_BASE_MOUSE_BUTTON];
    else if (i >= AO_SCANCODE_BASE_JOYSTICK_BUTTON && i < (AO_SCANCODE_BASE_JOYSTICK_BUTTON + NUM_SDL_JOYSTICK_BUTTONS))
        return get_binding_joystick_button_name(i - AO_SCANCODE_BASE_JOYSTICK_BUTTON);

    return "unknown";
}


static SDL_Scancode code_for_binding_name(std::string name)
{
    for (int i = 0; i < binding_scancode_name.size(); ++i)
    {
        if (name == binding_scancode_name[i])
            return static_cast<SDL_Scancode>(i);
    }
    for (int i = 0; i < NUM_SDL_MOUSE_BUTTONS; ++i)
    {
        if (name == binding_mouse_button_name[i])
            return static_cast<SDL_Scancode>(i + AO_SCANCODE_BASE_MOUSE_BUTTON);
    }
    for (int i = 0; i < NUM_SDL_JOYSTICK_BUTTONS; ++i)
    {
        if (name == get_binding_joystick_button_name(i))
            return static_cast<SDL_Scancode>(i + AO_SCANCODE_BASE_JOYSTICK_BUTTON);
    }
    return SDL_SCANCODE_UNKNOWN;
}


enum class BindingType
{
    in_game,
    shell,
    hotkey
};


static int index_for_action_name(std::string name, BindingType& binding_type)
{
    for (int i = 0; i < NUM_KEYS; ++i)
    {
        if (name == binding_action_name[i])
        {
            binding_type = BindingType::in_game;
            return i;
        }
    }
    for (int i = 0; i < NUMBER_OF_SHELL_KEYS; ++i)
    {
        if (name == binding_shell_action_name[i])
        {
            binding_type = BindingType::shell;
            return i;
        }
    }
    for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
    {
        if (name == binding_hotkey_action_name[i])
        {
            binding_type = BindingType::hotkey;
            return i;
        }
    }
    return -1;
}


static void unset_scancode(SDL_Scancode code)
{
    for (int i = 0; i < NUM_KEYS; ++i)
        input_preferences.key_bindings[i].erase(code);
    for (int i = 0; i < NUMBER_OF_SHELL_KEYS; ++i)
        input_preferences.shell_key_bindings[i].erase(code);
    for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
    {
        input_preferences.hotkey_bindings[i].erase(code);
    }
}


extern const char* GetSDLKeyName(SDL_Scancode); // yuck, defined in sdl_widgets.cpp


const char* get_hotkey_binding(int hotkey, int type)
{
    auto bindings = input_preferences.hotkey_bindings[hotkey - 1];
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
    {
        if (w_key::event_type_for_key(*it) == type)
        {
            return GetSDLKeyName(*it);
        }
        else
        {
            continue;
        }
    }

    return "";
}



//*****************************************************************************
// PREFERENCES
//*****************************************************************************


input_preferences_data input_preferences;


void input_preferences_data::reset()
{
    input_device                      = _mouse_yaw_pitch;
    key_bindings                      = default_key_bindings;
    shell_key_bindings                = default_shell_key_bindings;
    hotkey_bindings                   = default_hotkey_bindings;
    
    modifiers                         = 0;

    sens_horizontal                   = FIXED_ONE / 4;
    sens_vertical                     = FIXED_ONE / 4;
    mouse_accel_type                  = _mouse_accel_none;
    mouse_accel_scale                 = 1.f;
    raw_mouse_input                   = true;
    extra_mouse_precision             = true;
    classic_vertical_aim              = false;
    classic_aim_speed_limits          = false;

    controller_aim_inverted           = false;
    controller_analog                 = true;
    controller_sensitivity_horizontal = FIXED_ONE;
    controller_deadzone_horizontal    = 3276;
    controller_sensitivity_vertical   = FIXED_ONE;
    controller_deadzone_vertical      = 3276;
}


void input_preferences_data::read(InfoTree root, std::string version)
{
    root.read_attr("device", input_device);
    root.read_attr("modifiers", modifiers);

    // old prefs may have combined sensitivity
    root.read_attr("sensitivity", sens_horizontal);
    root.read_attr("sensitivity", sens_vertical);
    root.read_attr("sens_horizontal", sens_horizontal);
    root.read_attr("sens_vertical", sens_vertical);
    
    if (!version.length() || version < "20181113")
        classic_vertical_aim = true;
    else if (version < "20190317")
        classic_vertical_aim = false;
    root.read_attr("classic_vertical_aim", classic_vertical_aim);
    
    if (!root.read_attr("classic_aim_speed_limits", classic_aim_speed_limits))
    {
        // Assume users with older prefs with "mouse_max_speed" above its default value don't want classic limits
        float mouse_max_speed;
        if (root.read_attr("mouse_max_speed", mouse_max_speed) && mouse_max_speed > 0.25)
            classic_aim_speed_limits = false;
    }

    // old prefs may have boolean acceleration flag
    bool accel = false;
    if (root.read_attr("mouse_acceleration", accel))
    {
        mouse_accel_type = _mouse_accel_classic;
        mouse_accel_scale = 1.f;
    }
    root.read_attr_bounded<int16>("mouse_accel_type",
                                  mouse_accel_type,
                                  0, NUMBER_OF_MOUSE_ACCEL_TYPES - 1);
    root.read_attr("mouse_accel_scale", mouse_accel_scale);
    
    // old prefs mixed "classic vertical aim" with acceleration type
    if (version >= "20181113" && version < "20190317")
    {
        if (mouse_accel_type == _mouse_accel_symmetric)
        {
            classic_vertical_aim = false;
            mouse_accel_type = _mouse_accel_classic;
        }
        else if (mouse_accel_type == _mouse_accel_classic)
        {
            classic_vertical_aim = true;
            sens_vertical *= 4.f;
        }
    }

    if (!version.length() || version < "20170821")
        raw_mouse_input = false;
    root.read_attr("raw_mouse_input", raw_mouse_input);
    if (!version.length() || version < "20181208")
        extra_mouse_precision = false;
    root.read_attr("extra_mouse_precision", extra_mouse_precision);
    root.read_attr("controller_analog", controller_analog);
    root.read_attr("controller_aim_inverted", controller_aim_inverted);

    _fixed old_controller_sensitivity_pref;
    if (root.read_attr("controller_sensitivity", old_controller_sensitivity_pref))
    {
        controller_sensitivity_vertical =
            controller_sensitivity_horizontal = old_controller_sensitivity_pref;
    }

    short old_controller_deadzone_pref;
    if (root.read_attr("controller_deadzone", old_controller_deadzone_pref))
    {
        controller_deadzone_vertical =
            controller_deadzone_horizontal = old_controller_deadzone_pref;
    }

    root.read_attr("controller_sensitivity_horizontal", controller_sensitivity_horizontal);
    root.read_attr("controller_deadzone_horizontal", controller_deadzone_horizontal);
    root.read_attr("controller_sensitivity_vertical", controller_sensitivity_vertical);
    root.read_attr("controller_deadzone_vertical", controller_deadzone_vertical);

    // remove default key bindings the first time we see one from these prefs
    std::set<std::pair<BindingType, int>> seen_key;
    
    
    for (const InfoTree &key : root.children_named("binding"))
    {
        std::string action_name, pressed_name;
        if (key.read_attr("action", action_name) &&
            key.read_attr("pressed", pressed_name))
        {
            BindingType binding_type;
            int index = index_for_action_name(action_name, binding_type);
            if (index < 0)
                continue;
            SDL_Scancode code = code_for_binding_name(pressed_name);
            key_binding_map* map;
            
            switch (binding_type)
            {
            case BindingType::in_game:
                map = &key_bindings;
                break;
            case BindingType::shell:
                map = &shell_key_bindings;
                break;
            case BindingType::hotkey:
                map = &hotkey_bindings;
                break;
            }

            auto k = std::make_pair(binding_type, index);
            if (seen_key.count(k) == 0)
            {
                (*map)[index].clear();
                seen_key.insert(k);
            }

            unset_scancode(code);
            (*map)[index].insert(code);
        }
    }
}

InfoTree input_preferences_data::write()
{
    InfoTree root;
    
    root.put_attr("device", input_device);
    root.put_attr("modifiers", modifiers);
    root.put_attr("sens_horizontal", sens_horizontal);
    root.put_attr("sens_vertical", sens_vertical);
    root.put_attr("classic_vertical_aim", classic_vertical_aim);
    root.put_attr("classic_aim_speed_limits", classic_aim_speed_limits);
    root.put_attr("mouse_accel_type", mouse_accel_type);
    root.put_attr("mouse_accel_scale", mouse_accel_scale);
    root.put_attr("raw_mouse_input", raw_mouse_input);
    root.put_attr("extra_mouse_precision", extra_mouse_precision);
    
    root.put_attr("controller_analog", controller_analog);
    root.put_attr("controller_aim_inverted", controller_aim_inverted);
    root.put_attr("controller_sensitivity_horizontal", controller_sensitivity_horizontal);
    root.put_attr("controller_deadzone_horizontal", controller_deadzone_horizontal);
    root.put_attr("controller_sensitivity_vertical", controller_sensitivity_vertical);
    root.put_attr("controller_deadzone_vertical", controller_deadzone_vertical);
    
    for (int i = 0; i < (NUMBER_OF_KEYS + NUMBER_OF_SHELL_KEYS); ++i)
    {
        std::set<SDL_Scancode> codeset;
        std::string name;
        if (i < NUMBER_OF_KEYS) {
            codeset = key_bindings[i];
            name = binding_action_name[i];
        } else {
            codeset = shell_key_bindings[i - NUMBER_OF_KEYS];
            name = binding_shell_action_name[i - NUMBER_OF_KEYS];
        }
        
        for (const SDL_Scancode &code : codeset)
        {
            if (code == SDL_SCANCODE_UNKNOWN)
                continue;
            InfoTree key;
            key.put_attr("action", name);
            key.put_attr("pressed", binding_name_for_code(code));
            root.add_child("binding", key);
        }
    }

    for (auto i = 0;i < NUMBER_OF_HOTKEYS; ++i)
    {
        for (auto code : hotkey_bindings[i])
        {
            if (code == SDL_SCANCODE_UNKNOWN)
            {
                continue;
            }

            InfoTree key;
            key.put_attr("action", binding_hotkey_action_name[i]);
            key.put_attr("pressed", binding_name_for_code(code));
            root.add_child("binding", key);
        }
    }
    
    return root;
}





//*****************************************************************************
// DIALOGS
//*****************************************************************************



const float kMinSensitivityLog = -7.0f;
const float kMaxSensitivityLog = 3.0f;
const float kSensitivityLogRange = kMaxSensitivityLog - kMinSensitivityLog;

class w_sens_slider : public w_slider
{
public:
    w_sens_slider(int num_items, int sel) : w_slider(num_items, sel)
    {
        init_formatted_value();
    }
    
    virtual std::string formatted_value()
    {
        std::ostringstream ss;
        float val = std::exp(selection * kSensitivityLogRange / 1000.0f + kMinSensitivityLog);
        if (val >= 10.f)
            ss.precision(2);
        else
            ss.precision(3);

        ss << std::fixed << std::showpoint << val;
        return ss.str();
    }
};


class w_deadzone_slider : public w_slider
{
public:
    w_deadzone_slider(int num_items, int sel) : w_slider(num_items, sel)
    {
        init_formatted_value();
    }
    
    virtual std::string formatted_value()
    {
        std::ostringstream ss;
        ss << selection << "%";
        return ss.str();
    }
};




class w_prefs_key;

typedef std::multimap<int, w_prefs_key*> prefsKeyMap;
typedef std::pair<int, w_prefs_key*> prefsKeyMapPair;
static prefsKeyMap key_w;
static prefsKeyMap shell_key_w;
static prefsKeyMap hotkey_w;


class w_prefs_key : public w_key
{
public:
    w_prefs_key(SDL_Scancode key, w_key::Type event_type) : w_key(key, event_type) {}

    void set_key(SDL_Scancode new_key)
    {
        // Key used for in-game function?
        int error = NONE;
        switch (new_key) {
        case SDL_SCANCODE_F1:
        case SDL_SCANCODE_F2:
        case SDL_SCANCODE_F3:
        case SDL_SCANCODE_F4:
        case SDL_SCANCODE_F5:
        case SDL_SCANCODE_F6:
        case SDL_SCANCODE_F7:
        case SDL_SCANCODE_F8:
        case SDL_SCANCODE_F9:
        case SDL_SCANCODE_F10:
        case SDL_SCANCODE_F11:
        case SDL_SCANCODE_F12:
        case SDL_SCANCODE_ESCAPE: // (ZZZ: for quitting)
        case AO_SCANCODE_JOYSTICK_ESCAPE:
            error = keyIsUsedAlready;
            break;
            
        default:
            break;
        }
        if (error != NONE) {
            notify_user(STRID(strERRORS, error));
            return;
        }

        w_key::set_key(new_key);
        dirty = true;
        if (new_key == SDL_SCANCODE_UNKNOWN)
            return;

        // Remove binding to this key from all other widgets
        for (auto it = key_w.begin(); it != key_w.end(); ++it) {
            if (it->second != this && it->second->get_key() == new_key) {
                it->second->set_key(SDL_SCANCODE_UNKNOWN);
                it->second->dirty = true;
            }
        }
        for (auto it = shell_key_w.begin(); it != shell_key_w.end(); ++it) {
            if (it->second != this && it->second->get_key() == new_key) {
                it->second->set_key(SDL_SCANCODE_UNKNOWN);
                it->second->dirty = true;
            }
        }
        
        for (auto it = hotkey_w.begin(); it != hotkey_w.end(); ++it)
        {
            if (it->second != this && it->second->get_key() == new_key)
            {
                it->second->set_key(SDL_SCANCODE_UNKNOWN);
                it->second->dirty = true;
            }
        }
    }
};


enum {
    KEYBOARD_TABS,
    TAB_KEYS,
    TAB_MORE_KEYS
};



static void load_default_keys(void *arg)
{
    for (int i = 0; i < NUM_KEYS; i++) {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (SDL_Scancode code : default_key_bindings[i])
        {
            if (code == SDL_SCANCODE_UNKNOWN)
                continue;
            switch (w_key::event_type_for_key(code))
            {
                case w_key::MouseButton:
                    mcode = code;
                    break;
                case w_key::JoystickButton:
                    jcode = code;
                    break;
                case w_key::KeyboardKey:
                default:
                    kcode = code;
                    break;
            }
        }
        auto range = key_w.equal_range(i);
        for (auto ik = range.first; ik != range.second; ++ik) {
            w_prefs_key *pk = ik->second;
            switch (pk->event_type) {
                case w_key::MouseButton:
                    pk->set_key(mcode);
                    break;
                case w_key::JoystickButton:
                    pk->set_key(jcode);
                    break;
                case w_key::KeyboardKey:
                default:
                    pk->set_key(kcode);
                    break;
            }
        }
    }

    for (int i = 0; i < NUMBER_OF_SHELL_KEYS; i++) {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (auto it = default_shell_key_bindings[i].begin(); it != default_shell_key_bindings[i].end(); ++it) {
            SDL_Scancode code = *it;
            if (code == SDL_SCANCODE_UNKNOWN)
                continue;
            switch (w_key::event_type_for_key(code)) {
                case w_key::MouseButton:
                    mcode = code;
                    break;
                case w_key::JoystickButton:
                    jcode = code;
                    break;
                case w_key::KeyboardKey:
                default:
                    kcode = code;
                    break;
            }
        }
        auto range = shell_key_w.equal_range(i);
        for (auto ik = range.first; ik != range.second; ++ik) {
            w_prefs_key *pk = ik->second;
            switch (pk->event_type) {
                case w_key::MouseButton:
                    pk->set_key(mcode);
                    break;
                case w_key::JoystickButton:
                    pk->set_key(jcode);
                    break;
                case w_key::KeyboardKey:
                default:
                    pk->set_key(kcode);
                    break;
            }
        }
    }

    for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
    {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (auto it = default_hotkey_bindings[i].begin(); it != default_hotkey_bindings[i].end(); ++it) {
            SDL_Scancode code = *it;
            if (code == SDL_SCANCODE_UNKNOWN)
                continue;
            switch (w_key::event_type_for_key(code)) {
                case w_key::MouseButton:
                    mcode = code;
                    break;
                case w_key::JoystickButton:
                    jcode = code;
                    break;
                case w_key::KeyboardKey:
                default:
                    kcode = code;
                    break;
            }
        }
        auto range = hotkey_w.equal_range(i);
        for (auto ik = range.first; ik != range.second; ++ik) {
            w_prefs_key *pk = ik->second;
            switch (pk->event_type) {
                case w_key::MouseButton:
                    pk->set_key(mcode);
                    break;
                case w_key::JoystickButton:
                    pk->set_key(jcode);
                    break;
                case w_key::KeyboardKey:
                default:
                    pk->set_key(kcode);
                    break;
            }
        }
    }

    dialog *d = (dialog *)arg;
    d->draw_all_widgets();
}



const std::vector<std::string> mouse_feel_labels = { "Classic", "Modern", "(custom)" };


static w_select_popup *mouse_feel_w;
static w_select_popup *mouse_feel_details_w;
static w_toggle *mouse_raw_w;
static w_toggle *mouse_vertical_w;
static w_toggle *mouse_accel_w;
static w_toggle *mouse_precision_w;
static w_toggle *mouse_speed_limit_w;
static bool inside_callback = false;  // prevent circular changes


static void mouse_feel_details_changed(void *arg)
{
    if (inside_callback)
        return;
    inside_callback = true;
    switch (mouse_feel_details_w->get_selection())
    {
        case 0:
            mouse_raw_w->set_selection(1);
            mouse_accel_w->set_selection(1);
            mouse_vertical_w->set_selection(1);
            mouse_precision_w->set_selection(1);
            mouse_speed_limit_w->set_selection(1);
            break;
        case 1:
            mouse_raw_w->set_selection(1);
            mouse_accel_w->set_selection(0);
            mouse_vertical_w->set_selection(0);
            mouse_precision_w->set_selection(0);
            mouse_speed_limit_w->set_selection(0);
            break;
        default:
            break;
    }
    inside_callback = false;
}


static void update_mouse_feel_details(void *arg)
{
    if (inside_callback)
        return;
    inside_callback = true;
    if (mouse_raw_w->get_selection() == 1 &&
        mouse_accel_w->get_selection() == 1 &&
        mouse_vertical_w->get_selection() == 1 &&
        mouse_precision_w->get_selection() == 1 &&
        mouse_speed_limit_w->get_selection() == 1)
    {
        mouse_feel_details_w->set_selection(0);
    }
    else if (mouse_raw_w->get_selection() == 1 &&
             mouse_accel_w->get_selection() == 0 &&
             mouse_vertical_w->get_selection() == 0 &&
             mouse_precision_w->get_selection() == 0 &&
             mouse_speed_limit_w->get_selection() == 0)
    {
        mouse_feel_details_w->set_selection(1);
    }
    else
    {
        mouse_feel_details_w->set_selection(2);
    }
    inside_callback = false;
}


static void update_mouse_feel(void *arg)
{
    if (input_preferences.raw_mouse_input == true &&
        input_preferences.mouse_accel_type == _mouse_accel_classic &&
        input_preferences.classic_vertical_aim == true &&
        input_preferences.extra_mouse_precision == false &&
        input_preferences.classic_aim_speed_limits)
    {
        mouse_feel_w->set_selection(0);
    }
    else if (input_preferences.raw_mouse_input == true &&
             input_preferences.mouse_accel_type == _mouse_accel_none &&
             input_preferences.classic_vertical_aim == false &&
             input_preferences.extra_mouse_precision == true &&
             !input_preferences.classic_aim_speed_limits)
    {
        mouse_feel_w->set_selection(1);
    }
    else
    {
        mouse_feel_w->set_selection(2);
    }
}


static bool apply_mouse_feel(int selection)
{
    bool changed = false;
    switch (selection)
    {
        case 0:
            if (true != input_preferences.raw_mouse_input) {
                input_preferences.raw_mouse_input = true;
                changed = true;
            }
            if (_mouse_accel_classic != input_preferences.mouse_accel_type) {
                input_preferences.mouse_accel_type = _mouse_accel_classic;
                changed = true;
            }
            if (true != input_preferences.classic_vertical_aim) {
                input_preferences.classic_vertical_aim = true;
                changed = true;
            }
            if (false != input_preferences.extra_mouse_precision) {
                input_preferences.extra_mouse_precision = false;
                changed = true;
            }
            if (!input_preferences.classic_aim_speed_limits)
            {
                input_preferences.classic_aim_speed_limits = true;
                changed = true;
            }
            break;
        case 1:
            if (true != input_preferences.raw_mouse_input) {
                input_preferences.raw_mouse_input = true;
                changed = true;
            }
            if (_mouse_accel_none != input_preferences.mouse_accel_type) {
                input_preferences.mouse_accel_type = _mouse_accel_none;
                changed = true;
            }
            if (false != input_preferences.classic_vertical_aim) {
                input_preferences.classic_vertical_aim = false;
                changed = true;
            }
            if (true != input_preferences.extra_mouse_precision) {
                input_preferences.extra_mouse_precision = true;
                changed = true;
            }
            if (input_preferences.classic_aim_speed_limits) {
                input_preferences.classic_aim_speed_limits = false;
                changed = true;
            }
            break;
        default:
            break;
    }
    return changed;
}








static void mouse_custom_dialog(void *arg)
{
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("MOUSE ADVANCED"), d);
    placer->add(new w_spacer());
    
    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);
    
    float hSensitivity = ((float) input_preferences.sens_horizontal) / FIXED_ONE;
    if (hSensitivity <= 0.0f) hSensitivity = 1.0f;
    float hSensitivityLog = std::log(hSensitivity);
    int hSliderPosition =
        (int) ((hSensitivityLog - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);
    w_sens_slider *mouse_h_sens_w = new w_sens_slider(1000, hSliderPosition);
    table->dual_add(mouse_h_sens_w->adding_label("Horizontal Sensitivity"), d);
    table->dual_add(mouse_h_sens_w, d);

    float vSensitivity = ((float) input_preferences.sens_vertical) / FIXED_ONE;
    if (vSensitivity <= 0.0f) vSensitivity = 1.0f;
    float vSensitivityLog = std::log(vSensitivity);
    int vSliderPosition =
        (int) ((vSensitivityLog - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);
    w_sens_slider *mouse_v_sens_w = new w_sens_slider(1000, vSliderPosition);
    table->dual_add(mouse_v_sens_w->adding_label("Vertical Sensitivity"), d);
    table->dual_add(mouse_v_sens_w, d);

    w_toggle *mouse_v_invert_w = new w_toggle(input_preferences.modifiers & _inputmod_invert_mouse);
    mouse_v_invert_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_v_invert_w->adding_label("Invert Vertical Aim"), d);
    table->dual_add(mouse_v_invert_w, d);

    table->add_row(new w_spacer(), true);

    mouse_feel_details_w = new w_select_popup();
    mouse_feel_details_w->set_labels(mouse_feel_labels);
    mouse_feel_details_w->set_selection(mouse_feel_w->get_selection());
    mouse_feel_details_w->set_popup_callback(mouse_feel_details_changed, NULL);
    table->dual_add(mouse_feel_details_w->adding_label("Mouse Feel"), d);
    table->dual_add(mouse_feel_details_w, d);
    
    mouse_raw_w = new w_toggle(input_preferences.raw_mouse_input);
    mouse_raw_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_raw_w->adding_label("Raw Input Mode"), d);
    table->dual_add(mouse_raw_w, d);
    
    mouse_accel_w = new w_toggle(input_preferences.mouse_accel_type == _mouse_accel_classic);
    mouse_accel_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_accel_w->adding_label("Acceleration"), d);
    table->dual_add(mouse_accel_w, d);
    
    mouse_vertical_w = new w_toggle(input_preferences.classic_vertical_aim);
    mouse_vertical_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_vertical_w->adding_label("Adjust Vertical Speed"), d);
    table->dual_add(mouse_vertical_w, d);
    
    mouse_precision_w = new w_toggle(!input_preferences.extra_mouse_precision);
    mouse_precision_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_precision_w->adding_label("Snap View to Weapon Aim"), d);
    table->dual_add(mouse_precision_w, d);

    mouse_speed_limit_w = new w_toggle(input_preferences.classic_aim_speed_limits);
    mouse_speed_limit_w->set_selection_changed_callback(update_mouse_feel_details);
    table->dual_add(mouse_speed_limit_w->adding_label("Classic Aim Speed Limit"), d);
    table->dual_add(mouse_speed_limit_w, d);
    
    placer->add(table);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
    placer->add(button_placer, true);

    d.set_widget_placer(placer);
    mouse_feel_details_changed(NULL);
    
    // Run dialog
    if (d.run() == 0) {    // Accepted
        bool changed = false;
        
        int hPos = mouse_h_sens_w->get_selection();
        float hLog = kMinSensitivityLog + ((float) hPos) * (kSensitivityLogRange / 1000.0f);
        _fixed hNorm = _fixed(std::exp(hLog) * FIXED_ONE);
        if (hNorm != input_preferences.sens_horizontal) {
            input_preferences.sens_horizontal = hNorm;
            changed = true;
        }
        
        int vPos = mouse_v_sens_w->get_selection();
        float vLog = kMinSensitivityLog + ((float) vPos) * (kSensitivityLogRange / 1000.0f);
        _fixed vNorm = _fixed(std::exp(vLog) * FIXED_ONE);
        if (vNorm != input_preferences.sens_vertical) {
            input_preferences.sens_vertical = vNorm;
            changed = true;
        }

        uint16 flags = input_preferences.modifiers;
        if (mouse_v_invert_w->get_selection()) {
            flags |= _inputmod_invert_mouse;
        } else {
            flags &= ~_inputmod_invert_mouse;
        }
        if (flags != input_preferences.modifiers) {
            input_preferences.modifiers = flags;
            changed = true;
        }
        
        if (mouse_raw_w->get_selection() != input_preferences.raw_mouse_input) {
            input_preferences.raw_mouse_input = mouse_raw_w->get_selection();
            changed = true;
        }
        
        if (mouse_accel_w->get_selection() != input_preferences.mouse_accel_type) {
            input_preferences.mouse_accel_type = mouse_accel_w->get_selection();
            changed = true;
        }
        
        bool vert = mouse_vertical_w->get_selection();
        if (vert != input_preferences.classic_vertical_aim) {
            input_preferences.classic_vertical_aim = vert;
            changed = true;
        }
        
        bool precision = (mouse_precision_w->get_selection() == 0);
        if (precision != input_preferences.extra_mouse_precision) {
            input_preferences.extra_mouse_precision = precision;
            changed = true;
        }

        auto speed_limit = mouse_speed_limit_w->get_selection();
        if (speed_limit != input_preferences.classic_aim_speed_limits) {
            input_preferences.classic_aim_speed_limits = speed_limit;
            changed = true;
        }

        if (changed) {
            write_preferences();
        }
        update_mouse_feel(NULL);
    }
}


static void controller_details_dialog(void *arg)
{
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("CONTROLLER ADVANCED"), d);
    placer->add(new w_spacer());
    
    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);
    
    float joySensitivityX = ((float) input_preferences.controller_sensitivity_horizontal) / FIXED_ONE;
    if (joySensitivityX <= 0.0f) joySensitivityX = 1.0f;
    float joySensitivityLogX = std::log(joySensitivityX);
    int joySliderPositionX = (int) ((joySensitivityLogX - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);

    w_sens_slider* sens_joy_w_x = new w_sens_slider(1000, joySliderPositionX);
    table->dual_add(sens_joy_w_x->adding_label("Aiming Horizontal Sensitivity"), d);
    table->dual_add(sens_joy_w_x, d);
    
    int joyDeadzoneX = (int)((input_preferences.controller_deadzone_horizontal / 655.36f) + 0.5f);
    w_deadzone_slider* dead_joy_w_x = new w_deadzone_slider(11, joyDeadzoneX);
    table->dual_add(dead_joy_w_x->adding_label("Analog Horizontal Dead Zone"), d);
    table->dual_add(dead_joy_w_x, d);

    float joySensitivityY = ((float)input_preferences.controller_sensitivity_vertical) / FIXED_ONE;
    if (joySensitivityY <= 0.0f) joySensitivityY = 1.0f;
    float joySensitivityLogY = std::log(joySensitivityY);
    int joySliderPositionY = (int)((joySensitivityLogY - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);

    w_sens_slider* sens_joy_w_y = new w_sens_slider(1000, joySliderPositionY);
    table->dual_add(sens_joy_w_y->adding_label("Aiming Vertical Sensitivity"), d);
    table->dual_add(sens_joy_w_y, d);

    int joyDeadzoneY = (int)((input_preferences.controller_deadzone_vertical / 655.36f) + 0.5f);
    w_deadzone_slider* dead_joy_w_y = new w_deadzone_slider(11, joyDeadzoneY);
    table->dual_add(dead_joy_w_y->adding_label("Analog Vertical Dead Zone"), d);
    table->dual_add(dead_joy_w_y, d);

    w_toggle* controller_inverted = new w_toggle(input_preferences.controller_aim_inverted);
    table->dual_add(controller_inverted->adding_label("Invert Vertical Aim"), d);
    table->dual_add(controller_inverted, d);
    
    table->add_row(new w_spacer(), true);
    placer->add(table, true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
    placer->add(button_placer, true);
    
    d.set_widget_placer(placer);
    
    // Run dialog
    if (d.run() == 0) {    // Accepted
        bool changed = false;

        int sensPosX = sens_joy_w_x->get_selection();
        float sensLogX = kMinSensitivityLog + ((float)sensPosX) * (kSensitivityLogRange / 1000.0f);
        _fixed sensNormX = _fixed(std::exp(sensLogX) * FIXED_ONE);
        if (sensNormX != input_preferences.controller_sensitivity_horizontal) {
            input_preferences.controller_sensitivity_horizontal = sensNormX;
            changed = true;
        }
        
        int deadPosX = dead_joy_w_x->get_selection();
        int deadNormX = deadPosX * 655.36f;
        if (deadNormX != input_preferences.controller_deadzone_horizontal) {
            input_preferences.controller_deadzone_horizontal = deadNormX;
            changed = true;
        }

        int sensPosY = sens_joy_w_y->get_selection();
        float sensLogY = kMinSensitivityLog + ((float)sensPosY) * (kSensitivityLogRange / 1000.0f);
        _fixed sensNormY = _fixed(std::exp(sensLogY) * FIXED_ONE);
        if (sensNormY != input_preferences.controller_sensitivity_vertical) {
            input_preferences.controller_sensitivity_vertical = sensNormY;
            changed = true;
        }

        int deadPosY = dead_joy_w_y->get_selection();
        int deadNormY = deadPosY * 655.36f;
        if (deadNormY != input_preferences.controller_deadzone_vertical) {
            input_preferences.controller_deadzone_vertical = deadNormY;
            changed = true;
        }

        bool inverted_controls = controller_inverted->get_selection();
        if (input_preferences.controller_aim_inverted != inverted_controls) {
            input_preferences.controller_aim_inverted = inverted_controls;
            changed = true;
        }

        if (changed) {
            write_preferences();
        }
    }
}



static const strings_t run_option_labels = {"Hold to Run", "Always Run", "Toggle",};

static const strings_t swim_option_labels = {"Hold to Swim", "Always Swim"};

static const strings_t swim_toggle_labels = {"", "", "Hold to Swim"}; // hack, otherwise width changes


void controls_dialog(void *arg)
{
    // Clear array of key widgets (because w_prefs_key::set_key() scans it)
    key_w.clear();
    shell_key_w.clear();
    hotkey_w.clear();

    // Create dialog
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("CONTROLS"), d);
    placer->add(new w_spacer());
    
    // create all key widgets
    for (int i = 0; i < NUM_KEYS; i++) {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (std::set<SDL_Scancode>::const_iterator bit = input_preferences.key_bindings[i].begin(); bit != input_preferences.key_bindings[i].end(); ++bit) {
            SDL_Scancode code = *bit;
            if (code >= AO_SCANCODE_BASE_JOYSTICK_BUTTON && code < (AO_SCANCODE_BASE_JOYSTICK_BUTTON + NUM_SDL_JOYSTICK_BUTTONS)) {
                jcode = code;
            } else if (code >= AO_SCANCODE_BASE_MOUSE_BUTTON && code < (AO_SCANCODE_BASE_MOUSE_BUTTON + NUM_SDL_MOUSE_BUTTONS)) {
                mcode = code;
            } else {
                kcode = code;
            }
        }
        key_w.insert(prefsKeyMapPair(i, new w_prefs_key(kcode, w_key::KeyboardKey)));
        key_w.insert(prefsKeyMapPair(i, new w_prefs_key(mcode, w_key::MouseButton)));
        key_w.insert(prefsKeyMapPair(i, new w_prefs_key(jcode, w_key::JoystickButton)));
    }
    for (int i = 0; i < NUMBER_OF_SHELL_KEYS; i++) {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (std::set<SDL_Scancode>::const_iterator bit = input_preferences.shell_key_bindings[i].begin(); bit != input_preferences.shell_key_bindings[i].end(); ++bit) {
            SDL_Scancode code = *bit;
            if (code >= AO_SCANCODE_BASE_JOYSTICK_BUTTON && code < (AO_SCANCODE_BASE_JOYSTICK_BUTTON + NUM_SDL_JOYSTICK_BUTTONS)) {
                jcode = code;
            } else if (code >= AO_SCANCODE_BASE_MOUSE_BUTTON && code < (AO_SCANCODE_BASE_MOUSE_BUTTON + NUM_SDL_MOUSE_BUTTONS)) {
                mcode = code;
            } else {
                kcode = code;
            }
        }
        shell_key_w.insert(prefsKeyMapPair(i, new w_prefs_key(kcode, w_key::KeyboardKey)));
        shell_key_w.insert(prefsKeyMapPair(i, new w_prefs_key(mcode, w_key::MouseButton)));
        shell_key_w.insert(prefsKeyMapPair(i, new w_prefs_key(jcode, w_key::JoystickButton)));
    }
    
    for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
    {
        SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
        for (std::set<SDL_Scancode>::const_iterator bit = input_preferences.hotkey_bindings[i].begin(); bit != input_preferences.hotkey_bindings[i].end(); ++bit) {
            SDL_Scancode code = *bit;
            if (code >= AO_SCANCODE_BASE_JOYSTICK_BUTTON && code < (AO_SCANCODE_BASE_JOYSTICK_BUTTON + NUM_SDL_JOYSTICK_BUTTONS)) {
                jcode = code;
            } else if (code >= AO_SCANCODE_BASE_MOUSE_BUTTON && code < (AO_SCANCODE_BASE_MOUSE_BUTTON + NUM_SDL_MOUSE_BUTTONS)) {
                mcode = code;
            } else {
                kcode = code;
            }
        }
        hotkey_w.insert(prefsKeyMapPair(i, new w_prefs_key(kcode, w_key::KeyboardKey)));
        hotkey_w.insert(prefsKeyMapPair(i, new w_prefs_key(mcode, w_key::MouseButton)));
        hotkey_w.insert(prefsKeyMapPair(i, new w_prefs_key(jcode, w_key::JoystickButton)));
    }
    
    tab_placer* tabs = new tab_placer();
    
    std::vector<std::string> labels = { "AIM", "MOVE", "ACTIONS", "HOTKEYS", "INTERFACE", "OTHER" };
    w_tab *tab_w = new w_tab(labels, tabs);
    
    placer->dual_add(tab_w, d);
    placer->add(new w_spacer(), true);
    
    vertical_placer *move = new vertical_placer();
    table_placer *move_table = new table_placer(4, get_theme_space(ITEM_WIDGET), true);
    move_table->col_flags(0, placeable::kAlignRight);
    move_table->col_flags(1, placeable::kAlignLeft);
    move_table->col_flags(2, placeable::kAlignLeft);
    move_table->col_flags(3, placeable::kAlignLeft);
    move_table->add(new w_spacer(), true);
    move_table->dual_add(new w_label("Keyboard"), d);
    move_table->dual_add(new w_label("Mouse"), d);
    move_table->dual_add(new w_label("Controller"), d);
    
    std::vector<int> move_keys = { 0, 1, 4, 5, -1, 16, 15, 17 };
    for (auto it = move_keys.begin(); it != move_keys.end(); ++it) {
        if (*it < 0) {
            move_table->add_row(new w_spacer(), true);
        } else if (*it >= 100) {
            int i = *it - 100;
            move_table->dual_add(new w_label(shell_action_name[i]), d);
            auto range = shell_key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                move_table->dual_add(ik->second, d);
            }
        } else {
            int i = *it;
            move_table->dual_add(new w_label(action_name[i]), d);
            auto range = key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                move_table->dual_add(ik->second, d);
            }
        }
    }
    move->add(move_table, true);
    move->add(new w_spacer(), true);

    table_placer* move_options = new table_placer(3, get_theme_space(ITEM_WIDGET));
    move_options->col_flags(0, placeable::kAlignRight);

    w_select *run_w = new w_select(input_preferences.modifiers & _inputmod_run_key_toggle ? 2 : input_preferences.modifiers & _inputmod_interchange_run_walk ? 1 : 0, run_option_labels);
    move_options->dual_add(run_w->adding_label("Run/Swim Behavior"), d);
    move_options->dual_add(run_w, d);

    w_select *swim_w = new w_select(input_preferences.modifiers & _inputmod_interchange_swim_sink ? 1 : 0, swim_option_labels);
    move_options->dual_add(swim_w, d);

    const auto update_swim_w = [&](w_select*) {
        if (run_w->get_selection() == 2)
        {
            swim_w->set_labels(swim_toggle_labels);
            swim_w->set_enabled(false);
        }
        else
        {
            swim_w->set_labels(swim_option_labels);
            swim_w->set_enabled(true);
        }
    };

    update_swim_w(swim_w);
    run_w->set_selection_changed_callback(update_swim_w);

    move->add(move_options, true);
    move->add(new w_spacer(), true);
    move->dual_add(new w_static_text("Double-tap Run/Walk to activate control panels and doors"), d);
    move->dual_add(new w_static_text("Double-tap Move -> Look to center vertical view"), d);

    vertical_placer *look = new vertical_placer();
    table_placer *look_table = new table_placer(4, get_theme_space(ITEM_WIDGET), true);
    look_table->col_flags(0, placeable::kAlignRight);
    look_table->col_flags(1, placeable::kAlignLeft);
    look_table->col_flags(2, placeable::kAlignLeft);
    look_table->col_flags(3, placeable::kAlignLeft);
    look_table->add(new w_spacer(), true);
    look_table->dual_add(new w_label("Keyboard"), d);
    look_table->dual_add(new w_label("Mouse"), d);
    look_table->dual_add(new w_label("Controller"), d);
    
    std::vector<int> look_keys = { 8, 9, 2, 3, -1, 6, 7, 10 };
    for (auto it = look_keys.begin(); it != look_keys.end(); ++it) {
        if (*it < 0) {
            look_table->add_row(new w_spacer(), true);
        } else if (*it >= 100) {
            int i = *it - 100;
            look_table->dual_add(new w_label(shell_action_name[i]), d);
            auto range = shell_key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                look_table->dual_add(ik->second, d);
            }
        } else {
            int i = *it;
            look_table->dual_add(new w_label(action_name[i]), d);
            auto range = key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                if (ik->second->event_type == w_key::MouseButton) {
                    w_text_entry* txt = NULL;
                    switch (i) {
                        case 8:
                            txt = new w_text_entry(12, "move up");
                            break;
                        case 9:
                            txt = new w_text_entry(12, "move down");
                            break;
                        case 2:
                            txt = new w_text_entry(12, "move left");
                            break;
                        case 3:
                            txt = new w_text_entry(12, "move right");
                            break;
                        default:
                            break;
                    }
                    if (txt) {
                        txt->set_enabled(false);
                        txt->set_min_width(50);
                        look_table->dual_add(txt, d);
                        continue;
                    }
                }
                look_table->dual_add(ik->second, d);
            }
        }
    }
    look->add(look_table, true);
    look->add(new w_spacer(), true);
    
    table_placer *look_options = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    look_options->col_flags(0, placeable::kAlignRight);
    
    w_toggle* auto_recenter_w = new w_toggle(!(input_preferences.modifiers & _inputmod_dont_auto_recenter));
    look_options->dual_add(auto_recenter_w->adding_label("Auto-Recenter View"), d);
    look_options->dual_add(auto_recenter_w, d);
    
    look_options->add_row(new w_spacer(), true);
    
    table_placer *mouse_options = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    mouse_options->col_flags(0, placeable::kAlignRight);
    mouse_options->col_flags(1, placeable::kAlignLeft);

    w_toggle *enable_mouse_w = new w_toggle(input_preferences.input_device == _mouse_yaw_pitch);
    mouse_options->dual_add(enable_mouse_w->adding_label("Mouse Aiming"), d);
    mouse_options->dual_add(enable_mouse_w, d);
    
    mouse_feel_w = new w_select_popup();
    mouse_feel_w->set_labels(mouse_feel_labels);
    update_mouse_feel(NULL);
    mouse_options->dual_add(mouse_feel_w->adding_label("Mouse Feel"), d);
    mouse_options->dual_add(mouse_feel_w, d);
    
    mouse_options->add_row(new w_spacer(), true);
    mouse_options->dual_add_row(new w_button("MOUSE ADVANCED", mouse_custom_dialog, &d), d);
    
    look_options->add(mouse_options, true);
    
    table_placer *controller_options = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    controller_options->col_flags(0, placeable::kAlignRight);
    controller_options->col_flags(1, placeable::kAlignLeft);
    
    controller_options->dual_add_row(new w_label(""), d);
    std::vector<std::string> joystick_aiming_labels = { "Treat as Analog Stick", "Treat as D-Pad" };
    w_select_popup *joystick_aiming_w = new w_select_popup();
    joystick_aiming_w->set_labels(joystick_aiming_labels);
    joystick_aiming_w->set_selection(input_preferences.controller_analog ? 0 : 1);
    controller_options->dual_add(joystick_aiming_w->adding_label("Controller Feel"), d);
    controller_options->dual_add(joystick_aiming_w, d);
    
    controller_options->add_row(new w_spacer(), true);
    controller_options->dual_add_row(new w_button("CONTROLLER ADVANCED", controller_details_dialog, &d), d);
    
    look_options->add(controller_options, true);

    look->add(look_options, true);
    
    vertical_placer *actions = new vertical_placer();
    table_placer *actions_table = new table_placer(4, get_theme_space(ITEM_WIDGET), true);
    actions_table->col_flags(0, placeable::kAlignRight);
    actions_table->col_flags(1, placeable::kAlignLeft);
    actions_table->col_flags(2, placeable::kAlignLeft);
    actions_table->col_flags(3, placeable::kAlignLeft);
    actions_table->add(new w_spacer(), true);
    actions_table->dual_add(new w_label("Keyboard"), d);
    actions_table->dual_add(new w_label("Mouse"), d);
    actions_table->dual_add(new w_label("Controller"), d);
    
    std::vector<int> actions_keys = { 13, 14, 11, 12, -1, 18, -1, 20, 108 };
    for (auto it = actions_keys.begin(); it != actions_keys.end(); ++it) {
        if (*it < 0) {
            actions_table->add_row(new w_spacer(), true);
        } else if (*it >= 100) {
            int i = *it - 100;
            actions_table->dual_add(new w_label(shell_action_name[i]), d);
            auto range = shell_key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                actions_table->dual_add(ik->second, d);
            }
        } else {
            int i = *it;
            actions_table->dual_add(new w_label(action_name[i]), d);
            auto range = key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                actions_table->dual_add(ik->second, d);
            }
        }
    }
    actions->add(actions_table, true);
    actions->add(new w_spacer(), true);
    
    table_placer *actions_options = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    actions_options->col_flags(0, placeable::kAlignRight);

    w_toggle *weapon_w = new w_toggle(!(input_preferences.modifiers & _inputmod_dont_switch_to_new_weapon));
    actions_options->dual_add(weapon_w->adding_label("Auto-Switch Weapons"), d);
    actions_options->dual_add(weapon_w, d);
    
    actions->add(actions_options, true);
    
    actions->add(new w_spacer(), true);
    actions->dual_add(new w_static_text("Warning: Auto-Switch Weapons is always ON in"), d);
    actions->dual_add(new w_static_text("network play.  Turning it OFF will also disable"), d);
    actions->dual_add(new w_static_text("film recording for single-player games."), d);

    vertical_placer* hotkeys = new vertical_placer();
    table_placer* hotkey_table = new table_placer(4, get_theme_space(ITEM_WIDGET), true);
    hotkey_table->col_flags(0, placeable::kAlignRight);
    hotkey_table->col_flags(1, placeable::kAlignLeft);
    hotkey_table->col_flags(2, placeable::kAlignLeft);
    hotkey_table->col_flags(3, placeable::kAlignLeft);
    hotkey_table->add(new w_spacer(), true);
    hotkey_table->dual_add(new w_label("Keyboard"), d);
    hotkey_table->dual_add(new w_label("Mouse"), d);
    hotkey_table->dual_add(new w_label("Controller"), d);

    for (auto i = 0; i < NUMBER_OF_HOTKEYS; ++i)
    {
        if (i == 9)
        {
            hotkey_table->add_row(new w_spacer(), true);
        }
        
        hotkey_table->dual_add(new w_label(hotkey_action_name[i]), d);
        auto range = hotkey_w.equal_range(i);
        for (auto ik = range.first; ik != range.second; ++ik)
        {
            hotkey_table->dual_add(ik->second, d);
        }
    }
    hotkeys->add(hotkey_table, true);

    hotkeys->add(new w_spacer(), true);
    hotkeys->dual_add(new w_static_text("Hotkeys 1-9 are used to switch weapons, but can be overridden by Lua scripts"), d);
    hotkeys->dual_add(new w_static_text("Hotkeys 10-12 are reserved for Lua scripts"), d);

    vertical_placer *iface = new vertical_placer();
    table_placer *interface_table = new table_placer(4, get_theme_space(ITEM_WIDGET), true);
    interface_table->col_flags(0, placeable::kAlignRight);
    interface_table->col_flags(1, placeable::kAlignLeft);
    interface_table->col_flags(2, placeable::kAlignLeft);
    interface_table->col_flags(3, placeable::kAlignLeft);
    interface_table->add(new w_spacer(), true);
    interface_table->dual_add(new w_label("Keyboard"), d);
    interface_table->dual_add(new w_label("Mouse"), d);
    interface_table->dual_add(new w_label("Controller"), d);
    
    std::vector<int> interface_keys = { 19, 105, 106, -1, 103, 104, -1, 100, 101, -1, 102, 107, 109, -1, -2 };
    for (auto it = interface_keys.begin(); it != interface_keys.end(); ++it) {
        if (*it == -2) {
            interface_table->dual_add(new w_label("Exit Game"), d);
            w_prefs_key *kb = new w_prefs_key(SDL_SCANCODE_ESCAPE, w_key::KeyboardKey);
            kb->set_enabled(false);
            interface_table->dual_add(kb, d);
            interface_table->dual_add(new w_label(""), d);
            w_prefs_key *cn = new w_prefs_key(AO_SCANCODE_JOYSTICK_ESCAPE, w_key::JoystickButton);
            cn->set_enabled(false);
            interface_table->dual_add(cn, d);
        } else if (*it < 0) {
            interface_table->add_row(new w_spacer(), true);
        } else if (*it >= 100) {
            int i = *it - 100;
            interface_table->dual_add(new w_label(shell_action_name[i]), d);
            auto range = shell_key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                interface_table->dual_add(ik->second, d);
            }
        } else {
            int i = *it;
            interface_table->dual_add(new w_label(action_name[i]), d);
            auto range = key_w.equal_range(i);
            for (auto ik = range.first; ik != range.second; ++ik) {
                interface_table->dual_add(ik->second, d);
            }
        }
    }
    iface->add(interface_table, true);

    vertical_placer *other = new vertical_placer();
    other->dual_add(new w_static_text("These keyboard shortcuts cannot be changed."), d);
    other->add(new w_spacer());

    table_placer *other_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    other_table->dual_add(new w_label("Main Menu"), d);
    other_table->dual_add(new w_label("In Game"), d);

    table_placer *other_menu = new table_placer(2, get_theme_space(ITEM_WIDGET), false);
    other_menu->col_flags(0, placeable::kAlignRight);
    other_menu->col_flags(1, placeable::kAlignLeft);
    std::vector<std::string> menu_shortcuts = {
        "N", "Begin new game",
#ifdef __MACOSX__
        "Cmd-Option-N", "Level select",
#else
        "Ctrl+Shift+N", "Level select",
#endif
        "O", "Continue saved game",
        "G", "Gather network game",
        "J", "Join network game",
        "R", "Replay saved film",
        "P", "Preferences",
        "Q", "Quit",
        "C", "Scenario credits",
        "A", "About Aleph One",
#ifdef __MACOSX__
        "Cmd-Return", "Toggle fullscreen",
#else
        "Alt+Enter", "Toggle fullscreen",
#endif
    };
    for (auto it = menu_shortcuts.begin(); it != menu_shortcuts.end(); ++it) {
        other_menu->dual_add(new w_label(it->c_str()), d);
    }
    other_table->add(other_menu, true);

    table_placer *other_game = new table_placer(2, get_theme_space(ITEM_WIDGET), false);
    other_game->col_flags(0, placeable::kAlignRight);
    other_game->col_flags(1, placeable::kAlignLeft);
    std::vector<std::string> game_shortcuts = {
        "F1", "Decrease resolution",
        "F2", "Increase resolution",
        "F8", "Crosshairs",
        "F9", "Screenshot",
        "F10", "Debug info",
#ifdef HAVE_STEAM
        "Shift+F11", "Decrease brightness",
        "Shift+F12", "Increase brightness",
#else
        "F11", "Decrease brightness",
        "F12", "Increase brightness",
#endif
#ifdef __MACOSX__
        "Cmd-Return", "Toggle fullscreen",
#else
        "Alt+Enter", "Toggle fullscreen",
#endif
        "Escape", "Exit game"
    };
    for (auto it = game_shortcuts.begin(); it != game_shortcuts.end(); ++it) {
        other_game->dual_add(new w_label(it->c_str()), d);
    }
    other_table->add(other_game, true);

    other->add(other_table, true);

    tabs->add(look, true);
    tabs->add(move, true);
    tabs->add(actions, true);
    tabs->add(hotkeys, true);
    tabs->add(iface, true);
    tabs->add(other, true);
    placer->add(tabs, true);
    
    placer->add(new w_spacer(), true);
    placer->dual_add(new w_button("RESET ALL KEYS TO DEFAULTS", load_default_keys, &d), d);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
    placer->add(button_placer, true);

    d.set_widget_placer(placer);

    // Clear screen
    main_screen.clear();

    enter_joystick();

    // Run dialog
    if (d.run() == 0) {    // Accepted
        bool changed = false;
        
        uint16 flags = input_preferences.modifiers & _inputmod_invert_mouse;

        if (run_w->get_selection() == 2)
        {
            flags |= _inputmod_run_key_toggle;
        }
        else
        {
            if (run_w->get_selection()) flags |= _inputmod_interchange_run_walk;
            if (swim_w->get_selection()) flags |= _inputmod_interchange_swim_sink;
        }

        if (!(weapon_w->get_selection())) flags |= _inputmod_dont_switch_to_new_weapon;
        if (!(auto_recenter_w->get_selection())) flags |= _inputmod_dont_auto_recenter;
        
        if (flags != input_preferences.modifiers) {
            input_preferences.modifiers = flags;
            changed = true;
        }

        for (int i = 0; i < NUM_KEYS; i++) {
            input_preferences.key_bindings[i].clear();
        }
        for (int i = 0; i < NUMBER_OF_SHELL_KEYS; i++) {
            input_preferences.shell_key_bindings[i].clear();
        }

        for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
        {
            input_preferences.hotkey_bindings[i].clear();
        }

        for (auto it = key_w.begin(); it != key_w.end(); ++it) {
            int i = it->first;
            SDL_Scancode key = it->second->get_key();
            if (key != SDL_SCANCODE_UNKNOWN) {
                unset_scancode(key);
                input_preferences.key_bindings[i].insert(key);
                changed = true;
            }
        }

        for (auto it = shell_key_w.begin(); it != shell_key_w.end(); ++it) {
            int i = it->first;
            SDL_Scancode key = it->second->get_key();
            if (key != SDL_SCANCODE_UNKNOWN) {
                unset_scancode(key);
                input_preferences.shell_key_bindings[i].insert(key);
                changed = true;
            }
        }

        for (auto it = hotkey_w.begin(); it != hotkey_w.end(); ++it)
        {
            int i = it->first;
            auto key = it->second->get_key();
            if (key != SDL_SCANCODE_UNKNOWN)
            {
                unset_scancode(key);
                input_preferences.hotkey_bindings[i].insert(key);
                changed = true;
            }
        }
        
        int16 device = enable_mouse_w->get_selection() ? _mouse_yaw_pitch : _keyboard_or_game_pad;
        if (input_preferences.input_device != device) {
            input_preferences.input_device = device;
            changed = true;
        }
        
        bool jaim = (joystick_aiming_w->get_selection() == 0);
        if (input_preferences.controller_analog != jaim) {
            input_preferences.controller_analog = jaim;
            changed = true;
        }
        
        if (apply_mouse_feel(mouse_feel_w->get_selection())) {
            changed = true;
        }
    
        if (changed)
            write_preferences();
    }

    exit_joystick();
}
