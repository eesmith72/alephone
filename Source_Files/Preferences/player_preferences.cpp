/*
 player_preferences.cpp
 
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

#include "player_preferences.hpp"

#include "preferences_support.hpp"


//*****************************************************************************
// PREFERENCES
//*****************************************************************************


player_preferences_data player_preferences;



// ZZZ: override player-behavior modifiers
static bool sStandardizeModifiers = false; // EES: seriously, is the ONLY 'custom behavior' disabling auto-switching of depleted weapon? (obviously it's not - there are custom keys, zoom, and other shit - but it seems this is the only one that's tested); TODO: simplest solution: all custom flags and shit goes in its own file, and one of that file's jobs is generating a description of customizations that can be shared across network, stored in prefs and saved game+film files


void set_custom_behaviors_enabled(bool can_customize)
{
    sStandardizeModifiers = !can_customize;
}


bool is_player_behavior_standard()
{
    return !dont_switch_to_new_weapon(); // TODO: FIX: yeesh; presumably if player sets other customizations, this returns incorrect result
}


// LP addition: modification of Josh Elsasser's dont-switch-weapons patch so as to access preferences stuff here
bool dont_switch_to_new_weapon()
{
    // ZZZ: let game require standard modifiers for a while
    return !sStandardizeModifiers ? TEST_FLAG(input_preferences.modifiers,_inputmod_dont_switch_to_new_weapon) : false;
}


bool dont_auto_recenter()
{
    return TEST_FLAG(input_preferences.modifiers, _inputmod_dont_auto_recenter);
}


void player_preferences_data::read(InfoTree root, std::string version)
{
    root.read_attr("name", name);
    root.read_attr("color", color);
    root.read_attr("team", team);
    root.read_attr("last_time_ran", last_time_ran);
    root.read_attr("difficulty", difficulty_level);
    root.read_attr("bkgd_music", background_music_on);
    root.read_attr("crosshairs_active", crosshairs_active);
    
    for (const InfoTree &child : root.children_named("chase_cam"))
    {
        child.read_attr("behind", ChaseCam.Behind);
        child.read_attr("upward", ChaseCam.Upward);
        child.read_attr("rightward", ChaseCam.Rightward);
        child.read_attr("flags", ChaseCam.Flags);
        child.read_attr("damping", ChaseCam.Damping);
        child.read_attr("spring", ChaseCam.Spring);
        child.read_attr("opacity", ChaseCam.Opacity);
    }
    
    if (Scenario::instance()->AllowsClassicGameplay())
    {
        root.read_attr("solo_profile", solo_profile);
    }
    
    // Fix bool options
    background_music_on = !!background_music_on;
}


InfoTree player_preferences_data::write()
{
    InfoTree root;
    
    root.put_attr_cstr("name", name);
    root.put_attr("color", color);
    root.put_attr("team", team);
    root.put_attr("last_time_ran", last_time_ran);
    root.put_attr("difficulty", difficulty_level);
    root.put_attr("bkgd_music", background_music_on);
    root.put_attr("crosshairs_active", crosshairs_active);
    
    InfoTree cam;
    cam.put_attr("behind", ChaseCam.Behind);
    cam.put_attr("upward", ChaseCam.Upward);
    cam.put_attr("rightward", ChaseCam.Rightward);
    cam.put_attr("flags", ChaseCam.Flags);
    cam.put_attr("damping", ChaseCam.Damping);
    cam.put_attr("spring", ChaseCam.Spring);
    cam.put_attr("opacity", ChaseCam.Opacity);
    root.put_child("chase_cam", cam);
    
    root.put_attr("solo_profile", solo_profile);

    return root;
}



//*****************************************************************************
// DIALOGS
//*****************************************************************************


static const strings_t solo_profile_labels = {
    "Aleph One Fixes",
    "Classic Marathon 2",
    "Classic Marathon Infinity",
};


void player_dialog(void *arg)
{
    // Create dialog
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("PLAYER SETTINGS"), d);
    placer->add(new w_spacer());

    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);
    table->col_flags(1, placeable::kAlignLeft);

    w_select *level_w = new w_select(player_preferences.difficulty_level, kDifficultyLevelsStringSetID);
    table->dual_add(level_w->adding_label("Difficulty"), d);
    table->dual_add(level_w, d);
    
    // TODO: this implementation is flawed: it only appears when the current scenario says it supports it. (BTW, it'd really help if these gameplay differences were nicely documented, or at least summarized, for users.) My inclination is to get rid of this checkbox (most users won't care, or even notice the difference) and always use the scenario's flag (this flag needs set on the original M1-3 Classic scenarios but not their tarted Modern versions) and if any 3rd-party scenario sets the flag then always use classic gameplay for those too as presumably they have a good reason for it (e.g. an old scenario which doesn't behave correctly with current AO fixes). If we MUST keep this checkbox (for Reasons) then it needs to be in the Scenario chooser dialog and only disables, not hides, when a selected scenario doesn't support it.
    w_select* solo_profile_w = nullptr;
    if (Scenario::instance()->AllowsClassicGameplay())
    {
        table->add_row(new w_spacer(), true);

        auto profile = player_preferences.solo_profile;
        if (profile >= 1) --profile;
        
        solo_profile_w = new w_select(profile, solo_profile_labels);
        table->dual_add(solo_profile_w->adding_label("Solo Gameplay"), d);
        table->dual_add(solo_profile_w, d);

        table->dual_add_row(new w_static_text("Note: net games always use Aleph One fixes"), d);
    }
    
    table->add_row(new w_spacer(), true);

    table->dual_add_row(new w_static_text("Appearance"), d);

    w_text_entry *name_w = new w_text_entry(MAXIMUM_PLAYER_NAME_LENGTH, player_preferences.name);
    name_w->set_identifier(0); // NAME_W
    name_w->set_enter_pressed_callback(dialog_try_ok);
    name_w->set_value_changed_callback(dialog_disable_ok_if_empty);
    table->dual_add(name_w->adding_label("Name"), d);
    table->dual_add(name_w, d);

    w_select* pcolor_w = new w_select(player_preferences.color, kTeamColorsStringSetID);
    table->dual_add(pcolor_w->adding_label("Color"), d);
    table->dual_add(pcolor_w, d);

    w_select* tcolor_w = new w_select(player_preferences.team, kTeamColorsStringSetID);
    table->dual_add(tcolor_w->adding_label("Team"), d);
    table->dual_add(tcolor_w, d);

    table->add_row(new w_spacer(), true);

    w_toggle *crosshairs_active_w = new w_toggle(player_preferences.crosshairs_active);
    table->dual_add(crosshairs_active_w->adding_label("Show crosshairs"), d);
    table->dual_add(crosshairs_active_w, d);
    placer->add(table, true);
    // TODO: crosshairs to be provided by a LuaHUD plugin now

    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    
    w_button* ok_button = new w_button("ACCEPT", dialog_ok, &d);
    ok_button->set_identifier(iOK);
    button_placer->dual_add(ok_button, d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

    placer->add(button_placer, true);

    d.set_widget_placer(placer);

    main_screen.clear();

    if (d.run() == 0)
    {
        bool changed = false;

        const std::string name = name_w->get_text();
        if (name == player_preferences.name)
        {
            player_preferences.name = name;
            changed = true;
        }

        int16 level = static_cast<int16>(level_w->get_selection());
        assert_fail(level >= 0, "");
        if (level != player_preferences.difficulty_level)
        {
            player_preferences.difficulty_level = level;
            changed = true;
        }

        if (Scenario::instance()->AllowsClassicGameplay()) // TODO: see above TODO regarding this control
        {
            auto profile = solo_profile_w->get_selection();
            if (profile >= 1) ++profile;

            if (profile != player_preferences.solo_profile)
            {
                player_preferences.solo_profile = profile;
                changed = true;
            }
        }

        int16 player_color = static_cast<int16>(pcolor_w->get_selection());
        assert_fail(player_color >= 0, "");
        if (player_color != player_preferences.color)
        {
            player_preferences.color = player_color;
            changed = true;
        }

        int16 team_color = static_cast<int16>(tcolor_w->get_selection());
        assert_fail(team_color >= 0, "");
        if (team_color != player_preferences.team)
        {
            player_preferences.team = team_color;
            changed = true;
        }
        
        bool show_crosshairs = crosshairs_active_w->get_selection();
        if (show_crosshairs != player_preferences.crosshairs_active)
        {
            player_preferences.crosshairs_active = show_crosshairs;
            changed = true;
        }

        if (changed) { write_preferences(); }
    }
}


