/*
 environment_preferences.cpp
 
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

#include "environment_preferences.hpp"

#include "preferences_support.hpp"


// TODO: move everything into a single dialog (package manager)


//*****************************************************************************
// PREFERENCES
//*****************************************************************************


environment_preferences_data environment_preferences;

static std::vector<boost::filesystem::path> orphan_disabled_plugins;
static std::vector<boost::filesystem::path> orphan_enabled_plugins;



#ifdef HAVE_STEAM
static const strings_t max_saves_labels = {"20", "100", "500"};

static const std::array<uint32_t, 3> max_saves_values = {20, 100, 500};
#else
static const strings_t max_saves_labels = {"20", "100", "500", "Unlimited"};

static const std::array<uint32_t, 4> max_saves_values = {20, 100, 500, 0};
#endif



void environment_preferences_data::reset()
{
    memset(this, 0, sizeof(environment_preferences_data));
        
    set_map_file(get_scenario_map_path());
    set_physics_file(get_scenario_physics_path());
    set_shapes_file(get_scenario_shapes_path());
    set_sounds_file(get_scenario_sounds_path());
    
    // TODO: look for Images[.img2] first? get_scenario_images_path()
    set_resources_file(get_scenario_m1_resources_path());

    solo_lua_file.clear();
    use_solo_lua = false;
    use_replay_net_lua = false;
    hide_extensions = true;
    
    group_by_directory = true;
    reduce_singletons = false;
    smooth_text = true;
    
    film_profile = FILM_PROFILE_DEFAULT;
#ifdef HAVE_STEAM
    maximum_quick_saves = 500;
#else
    maximum_quick_saves = 0;
#endif
#ifdef HAVE_NFD
    use_native_file_dialogs = false;
#endif
    auto_play_demos = true;
}


void environment_preferences_data::read(InfoTree root, std::string version)
{
    root.read_path("map_file", map_file);
    root.read_path("physics_file", physics_file);
    root.read_path("shapes_file", shapes_file);
    root.read_path("sounds_file", sounds_file);
    root.read_path("resources_file", resources_file);
    root.read_attr("map_checksum", map_checksum);
    root.read_attr("physics_checksum", physics_checksum);
    
    // TODO: FIX: as above
    //root.read_attr("shapes_mod_date", shapes_mod_date);
    //root.read_attr("sounds_mod_date", sounds_mod_date);
    
    root.read_attr("group_by_directory", group_by_directory);
    root.read_attr("reduce_singletons", reduce_singletons);
    root.read_attr("smooth_text", smooth_text);
    root.read_path("solo_lua_file", solo_lua_file);
    root.read_attr("use_solo_lua", use_solo_lua);
    root.read_attr("use_replay_net_lua", use_replay_net_lua);
    root.read_attr("hide_alephone_extensions", hide_extensions);
    
    uint32 profile = FILM_PROFILE_DEFAULT + 1;
    root.read_attr("film_profile", profile);
    if (profile <= FILM_PROFILE_DEFAULT)
        film_profile = static_cast<FilmProfileType>(profile);
    
    root.read_attr("maximum_quick_saves", maximum_quick_saves);
#ifdef HAVE_NFD
    root.read_attr("use_native_file_dialogs", use_native_file_dialogs);
#endif
    root.read_attr("auto_play_demos", auto_play_demos);
    
    orphan_disabled_plugins.clear();
    for (const InfoTree &plugin : root.children_named("disable_plugin"))
    {
        std::string tmp;
        if (plugin.read_path("path", tmp))
        {
            if (!Plugins::instance()->disable(tmp)) { orphan_disabled_plugins.push_back(tmp); }
        }
    }

    orphan_enabled_plugins.clear();
    for (const InfoTree& plugin : root.children_named("enable_plugin"))
    {
        std::string tmp;
        if (plugin.read_path("path", tmp))
        {
            if (!Plugins::instance()->enable(tmp)) { orphan_enabled_plugins.push_back(tmp); }
        }
    }
}


InfoTree environment_preferences_data::write()
{
    InfoTree root;

    root.put_attr_path("map_file", map_file);
    root.put_attr_path("physics_file", physics_file);
    root.put_attr_path("shapes_file", shapes_file);
    root.put_attr_path("sounds_file", sounds_file);
    root.put_attr_path("resources_file", resources_file);
    root.put_attr("map_checksum", map_checksum);
    root.put_attr("physics_checksum", physics_checksum);
    
    // TODO: FIX: _mod_date is std::filesystem::file_time_type now; that said, this seems more of a checksum thing
//    root.put_attr("shapes_mod_date", static_cast<uint32>(shapes_mod_date));
//    root.put_attr("sounds_mod_date", static_cast<uint32>(sounds_mod_date));
    
    root.put_attr("group_by_directory", group_by_directory);
    root.put_attr("reduce_singletons", reduce_singletons);
    root.put_attr("smooth_text", smooth_text);
    root.put_attr_path("solo_lua_file", solo_lua_file);
    root.put_attr("use_solo_lua", use_solo_lua);
    root.put_attr("use_replay_net_lua", use_replay_net_lua);
    root.put_attr("hide_alephone_extensions", hide_extensions);
    root.put_attr("film_profile", static_cast<uint32>(film_profile));
    root.put_attr("maximum_quick_saves", maximum_quick_saves);
#ifdef HAVE_NFD
    root.put_attr("use_native_file_dialogs", use_native_file_dialogs);
#endif
    root.put_attr("auto_play_demos", auto_play_demos);

    for (Plugins::iterator it = Plugins::instance()->begin(); it != Plugins::instance()->end(); ++it)
    {
        if (it->compatible())
        {
            if (it->auto_enable && !it->enabled)
            {
                InfoTree disable;
                disable.put_attr_path("path", it->directory);
                root.add_child("disable_plugin", disable);
            }
            else if (!it->auto_enable && it->enabled)
            {
                InfoTree enable;
                enable.put_attr_path("path", it->directory);
                root.add_child("enable_plugin", enable);
            }
        }
    }

    for (const auto& plugin : orphan_disabled_plugins) {
        InfoTree disable;
        disable.put_attr_path("path", plugin.string());
        root.add_child("disable_plugin", disable);
    }

    for (const auto& plugin : orphan_enabled_plugins) {
        InfoTree enable;
        enable.put_attr_path("path", plugin.string());
        root.add_child("enable_plugin", enable);
    }
    
    return root;
}


//*****************************************************************************



void load_scenario_from_environment_preferences()
{
   // EES: it goes without saying: ugh. Environment prefs are subtly different from what's actually set in map_wad, shapes, etc: the built-in scenario is represented by the default filenames (built-in or, in practice, MML-specified since they need to include filename extensions), though since the dialog only ever shows filenames it is difficult to tell which of multiple "Maps" files the current selection is! This is partly a UI/UX problem, partly a scenario file management problem

   // MAP
   ao_path map_path = environment_preferences.map_file;
   if (!std::filesystem::is_regular_file(map_path)) { map_path.clear(); }
   if (map_path.empty()) // try to find the (installed) Map file by its checksum
   {
       map_path = find_scenario_file({match_file_type(_typecode_map), match_checksum(environment_preferences.map_checksum)});
   }
   if (map_path.empty())
   {
       map_path = get_scenario_map_path();
   }
   set_current_map_path(map_path);
   
   // PHYSICS
   ao_path physics_path = environment_preferences.physics_file;
   if (!std::filesystem::is_regular_file(physics_path)) { physics_path.clear(); }
   if (physics_path.empty())
   {
       physics_path = find_scenario_file({match_file_type(_typecode_physics), match_checksum(environment_preferences.physics_checksum)});
   }
   if (physics_path.empty())
   {
       physics_path = get_scenario_physics_path();
   }
   set_external_physics_file(physics_path);
   //load_external_physics_file(); // this is redundant as any external Physics file will be loaded when Map level is unpacked
   
   // SHAPES
   ao_path shapes_path = environment_preferences.shapes_file;
   if (!std::filesystem::is_regular_file(shapes_path)) { shapes_path.clear(); }
   if (shapes_path.empty())
   {
       shapes_path = find_scenario_file({match_file_type(_typecode_shapes), match_modification_date(environment_preferences.shapes_mod_date)});
   }
   if (shapes_path.empty())
   {
       shapes_path = get_scenario_shapes_path();
   }
   open_shapes_file(shapes_path);
   
   // SOUNDS
   ao_path sounds_path = environment_preferences.sounds_file;
   if (!std::filesystem::is_regular_file(sounds_path)) { sounds_path.clear(); }
   if (sounds_path.empty())
   {
       sounds_path = find_scenario_file({match_file_type(_typecode_sounds), match_modification_date(environment_preferences.sounds_mod_date)});
   }
   if (sounds_path.empty())
   {
       sounds_path = get_scenario_sounds_path();
   }
   open_sounds_file(sounds_path);
   
   // RESOURCES // TODO: this is smelly; it'd have been so much simpler if M1 had called the exported App's resource form Images.imgs
   ao_path resources_path = environment_preferences.resources_file;
   if (!std::filesystem::is_regular_file(resources_path)) { resources_path.clear(); }
   if (resources_path.empty())
   {
       resources_path = get_scenario_m1_resources_path();
   }
   if (resources_path.empty())
   {
       resources_path = get_scenario_images_path();
   }
   // TODO: straighten out; probably easiest to go by filename extension
   open_m1_external_resources_file(resources_path);
   open_m2_external_resources_file(resources_path);
}


//*****************************************************************************
// DIALOGS
//*****************************************************************************


void plugins_dialog(void* arg)
{
    dialog* parent = (dialog*)arg;

    dialog d;
    vertical_placer *placer = new vertical_placer;
    w_title *w_header = new w_title("PLUGINS");
    placer->dual_add(w_header, d);
    placer->add(new w_spacer, true);

    std::vector<Plugin> plugins(Plugins::instance()->begin(), Plugins::instance()->end());
    w_plugins* plugins_w = new w_plugins(plugins, 400, 7);
    placer->dual_add(plugins_w, d);
    
    placer->add(new w_spacer, true);

    horizontal_placer* button_placer = new horizontal_placer;
    w_button* accept_w = new w_button("ACCEPT", dialog_ok, &d);
    button_placer->dual_add(accept_w, d);
    w_button* cancel_w = new w_button("CANCEL", dialog_cancel, &d);
    button_placer->dual_add(cancel_w, d);

    placer->add(button_placer, true);

    d.set_widget_placer(placer);
    d.activate_widget(plugins_w);

    ao_path old_theme;
    const Plugin* theme_plugin = Plugins::instance()->find_theme();
    if (theme_plugin)
    {
        old_theme = theme_plugin->directory / theme_plugin->theme;
    }

    if (d.run() == 0) {
        bool changed = false;
        Plugins::iterator plugin = Plugins::instance()->begin();
        for (Plugins::iterator it = plugins.begin(); it != plugins.end(); ++it, ++plugin) {
            changed |= (plugin->enabled != it->enabled);
            plugin->enabled = it->enabled;
        }

        if (changed) {
            Plugins::instance()->invalidate();
            write_preferences();

            ResetAllMMLValues();
            LoadBaseMMLScripts(true);
            Plugins::instance()->load_mml(true);

            Plugins::instance()->set_map_checksum(get_current_map_checksum());
            read_scripts_from_current_map();

            ao_path new_theme;
            theme_plugin = Plugins::instance()->find_theme();
            if (theme_plugin)
            {
                new_theme = theme_plugin->directory / theme_plugin->theme;
            }

            // Redraw parent dialog
            if (new_theme != old_theme)
            {
                load_widget_themes();
                parent->quit(0); // Quit the parent dialog so it won't draw in the old theme
            }
        }
    }
}



static const strings_t film_profile_labels = {"Aleph One 1.0", "Marathon 2", "Marathon Infinity"};


void environment_dialog(void *arg)
{
    // Create dialog
    dialog d;
    vertical_placer *placer = new vertical_placer;
    w_title *w_header = new w_title("ENVIRONMENT SETTINGS");
    placer->dual_add(w_header, d);
    placer->add(new w_spacer, true);

    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);
    
#ifndef MAC_APP_STORE
    w_env_select *map_w = new w_env_select(environment_preferences.map_file, "AVAILABLE MAPS", _typecode_map, &d);
    table->dual_add(map_w->adding_label("Map"), d);
    table->dual_add(map_w, d);
    
    w_env_select *physics_w = new w_env_select(environment_preferences.physics_file, "AVAILABLE PHYSICS MODELS", _typecode_physics, &d);
    table->dual_add(physics_w->adding_label("Physics"), d);
    table->dual_add(physics_w, d);

    w_env_select *shapes_w = new w_env_select(environment_preferences.shapes_file, "AVAILABLE SHAPES", _typecode_shapes, &d);
    table->dual_add(shapes_w->adding_label("Shapes"), d);
    table->dual_add(shapes_w, d);

    w_env_select *sounds_w = new w_env_select(environment_preferences.sounds_file, "AVAILABLE SOUNDS", _typecode_sounds, &d);
    table->dual_add(sounds_w->adding_label("Sounds"), d);
    table->dual_add(sounds_w, d);

    // TODO: this should support both M1 .appl files and M2+ .imgA, etc (the file containing M1's exported app resource fork should've been named Images.imgA, but AO never does simple and interchangeable when baroquely convoluted, consistently inconsistent, and frustratingly non-interchangeable is achievable, which it always is)
    w_env_select* resources_w = new w_env_select(environment_preferences.resources_file, "AVAILABLE FILES", _typecode_m1_resources, &d);
    table->dual_add(resources_w->adding_label("External Resources"), d);
    table->dual_add(resources_w, d);
    
    table->add_row(new w_spacer, true);
    table->dual_add_row(new w_static_text("Solo Script"), d);
    w_enabling_toggle* use_solo_lua_w = new w_enabling_toggle(environment_preferences.use_solo_lua);
    table->dual_add(use_solo_lua_w->adding_label("Use Solo Script"), d);
    table->dual_add(use_solo_lua_w, d);

    w_env_select *solo_lua_w = new w_env_select(environment_preferences.solo_lua_file, "AVAILABLE SOLO SCRIPTS", _typecode_netscript, &d);
    table->dual_add(solo_lua_w->adding_label("Script File"), d);
    table->dual_add(solo_lua_w, d);
    use_solo_lua_w->add_dependent_widget(solo_lua_w);
#endif

    table->add_row(new w_spacer, true);
    table->dual_add_row(new w_static_text("Film Playback"), d);

    w_select* film_profile_w = new w_select(environment_preferences.film_profile, film_profile_labels);
    table->dual_add(film_profile_w->adding_label("Unversioned Film Profile"), d);
    table->dual_add(film_profile_w, d);
    
#ifndef MAC_APP_STORE
    w_enabling_toggle* use_replay_net_lua_w = new w_enabling_toggle(environment_preferences.use_replay_net_lua);
    table->dual_add(use_replay_net_lua_w->adding_label("Use Netscript in Films"), d);
    table->dual_add(use_replay_net_lua_w, d);
    
    w_env_select *replay_net_lua_w = new w_env_select(network_preferences.netscript_file, "AVAILABLE NETSCRIPTS", _typecode_netscript, &d);
    replay_net_lua_w->set_prefer_net(true);
    table->dual_add(replay_net_lua_w->adding_label("Netscript File"), d);
    table->dual_add(replay_net_lua_w, d);
    use_replay_net_lua_w->add_dependent_widget(replay_net_lua_w);
#endif

    w_toggle* auto_play_demos_w = new w_toggle(environment_preferences.auto_play_demos);
    table->dual_add(auto_play_demos_w->adding_label("Play Demos When Idle"), d);
    table->dual_add(auto_play_demos_w, d);
    
    table->add_row(new w_spacer, true);
    table->dual_add_row(new w_static_text("Options"), d);

#ifndef MAC_APP_STORE
    w_toggle *hide_extensions_w = new w_toggle(environment_preferences.hide_extensions);
    table->dual_add(hide_extensions_w->adding_label("Hide File Extensions"), d);
    table->dual_add(hide_extensions_w, d);
#endif

#ifdef HAVE_NFD
    w_toggle *use_native_file_dialogs_w = new w_toggle(environment_preferences.use_native_file_dialogs);
    table->dual_add(use_native_file_dialogs_w->adding_label("Use Native File Dialogs"), d);
    table->dual_add(use_native_file_dialogs_w, d);
#endif

    w_select *max_saves_w = new w_select(2, max_saves_labels);
    for (int i = 0; i < max_saves_labels.size(); ++i) {
        if (max_saves_values[i] == environment_preferences.maximum_quick_saves)
            max_saves_w->set_selection(i);
    }
    table->dual_add(max_saves_w->adding_label("Unnamed Saves to Keep"), d);
    table->dual_add(max_saves_w, d);

    placer->add(table, true);

    placer->add(new w_spacer, true);

    horizontal_placer *button_placer = new horizontal_placer;
    w_button *w_accept = new w_button("ACCEPT", dialog_ok, &d);
    button_placer->dual_add(w_accept, d);
    w_button *w_cancel = new w_button("CANCEL", dialog_cancel, &d);
    button_placer->dual_add(w_cancel, d);
    placer->add(button_placer, true);

    d.set_widget_placer(placer);

    // Clear screen
    main_screen.clear();

    // Run dialog

    if (d.run() == 0) {    // Accepted
        bool changed = false;

#ifndef MAC_APP_STORE
        std::string path = map_w->get_path();
        if (path != environment_preferences.map_file)
        {
            environment_preferences.set_map_file(path);
            changed = true;
        }

        path = physics_w->get_path();
        if (path != environment_preferences.physics_file)
        {
            environment_preferences.set_physics_file(path);
            changed = true;
        }

        path = shapes_w->get_path();
        if (path != environment_preferences.shapes_file)
        {
            environment_preferences.set_shapes_file(path);
            changed = true;
        }

        path = sounds_w->get_path();
        if (path != environment_preferences.sounds_file)
        {
            environment_preferences.set_sounds_file(path);
            changed = true;
        }
        
        path = resources_w->get_path();
        if (path != environment_preferences.resources_file)
        {
            environment_preferences.set_resources_file(path);
            changed = true;
        }
        
        
        bool use_solo_lua = use_solo_lua_w->get_selection() != 0;
        if (use_solo_lua != environment_preferences.use_solo_lua)
        {
            environment_preferences.use_solo_lua = use_solo_lua;
            changed = true;
        }
        
        path = solo_lua_w->get_path();
        if (path != environment_preferences.solo_lua_file)
        {
            environment_preferences.solo_lua_file = path;
            changed = true;
        }

        bool use_replay_net_lua = use_replay_net_lua_w->get_selection() != 0;
        if (use_replay_net_lua != environment_preferences.use_replay_net_lua)
        {
            environment_preferences.use_replay_net_lua = use_replay_net_lua;
            changed = true;
        }
        
        path = replay_net_lua_w->get_path();
        if (path != network_preferences.netscript_file)
        {
            network_preferences.netscript_file = path;
            changed = true;
        }
#endif

#ifndef MAC_APP_STORE
        bool hide_extensions = hide_extensions_w->get_selection() != 0;
        if (hide_extensions != environment_preferences.hide_extensions)
        {
            environment_preferences.hide_extensions = hide_extensions;
            changed = true;
        }
#endif

        if (film_profile_w->get_selection() != environment_preferences.film_profile)
        {
            environment_preferences.film_profile = static_cast<FilmProfileType>(film_profile_w->get_selection());
        
            changed = true;
        }

        bool saves_changed = false;
        int saves = max_saves_values[max_saves_w->get_selection()];
        if (saves != environment_preferences.maximum_quick_saves) {
            environment_preferences.maximum_quick_saves = saves;
            saves_changed = true;
        }

#ifdef HAVE_NFD
        auto use_native_file_dialogs = use_native_file_dialogs_w->get_selection() != 0;
        if (use_native_file_dialogs != environment_preferences.use_native_file_dialogs)
        {
            environment_preferences.use_native_file_dialogs = use_native_file_dialogs;
            changed = true;
        }
#endif

        auto auto_play_demos = auto_play_demos_w->get_selection() != 0;
        if (auto_play_demos != environment_preferences.auto_play_demos)
        {
            environment_preferences.auto_play_demos = auto_play_demos;
            changed = true;
        }
        
        if (changed)
            load_scenario_from_environment_preferences();

        if (changed || saves_changed)
            write_preferences();
    }
}
