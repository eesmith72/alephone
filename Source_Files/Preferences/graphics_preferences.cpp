/*
 graphics_preferences.cpp
 
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

#include "graphics_preferences.hpp"

#include "preferences_support.hpp"


//*****************************************************************************
// PREFERENCES
//*****************************************************************************


graphics_preferences_data graphics_preferences;


void graphics_preferences_data::reset()
{
    gamma_level = DEFAULT_GAMMA_LEVEL;

    screen_mode = screen_mode_t::hd;
    fullscreen = true;
    
    hud_size = 2;
    terminal_size = 2;
    // TODO: where is map size?
    translucent_map = false;
    
    bobbing_type = BobbingType::camera_and_weapon;
    
    fov = 0; // use default
    horizontal_fov_is_constant = true;
    
    ephemera_quality = _ephemera_medium;
    ogl_preferences.reset();
    
    show_fps = false;
    in_game_fps_target = 30;

    movie_export_video_quality = 50;
    movie_export_audio_quality = 50;
    movie_export_video_bitrate = 0; // auto
}



int16 graphics_preferences_data::current_fps_target()
{
    return game_is_running() ? in_game_fps_target : FPS_DEFAULT;
}


void graphics_preferences_data::read(InfoTree root, std::string version)
{
    int32_t size;
    root.read_attr("screen_size", size);
    screen_mode = (screen_mode_t)size;
    root.read_attr("hud_size", hud_size);
    root.read_attr("terminal_size", terminal_size);
    root.read_attr("automap_is_translucent", translucent_map);
    root.read_attr("automap_size", automap_size);
    
    int bob;
    if (root.read_attr("scmode_camera_bob", bob)) { bobbing_type = static_cast<BobbingType>(bob); }
    
    root.read_attr("scmode_fullscreen", fullscreen);
    
    root.read_attr("scmode_fix_h_not_v", horizontal_fov_is_constant);
    root.read_attr("scmode_gamma", gamma_level);
    root.read_attr("scmode_fov", fov);
    root.read_attr("ogl_flags", ogl_preferences.Flags);
    root.read_attr("fps_target", in_game_fps_target);
    root.read_attr("anisotropy_level", ogl_preferences.AnisotropyLevel);
    root.read_attr("gamma_corrected_blending", ogl_preferences.Use_sRGB);
    root.read_attr_bounded<int16>("movie_export_video_quality", movie_export_video_quality, 0, 100);
    root.read_attr_bounded<int16>("movie_export_audio_quality", movie_export_audio_quality, 0, 100);
    root.read_attr("movie_export_video_bitrate", movie_export_video_bitrate);

    root.read_attr("scripted_effects_quality", ephemera_quality);
    
    for (const InfoTree &landscape : root.children_named("landscapes"))
    {
        for (const InfoTree &color : landscape.children_named("color"))
        {
            int16 index;
            if (color.read_indexed("index", index, 8))
                color.read_color(ogl_preferences.LscpColors[index / 2][index % 2]);
        }
    }
    
    for (const InfoTree &tex : root.children_named("texture"))
    {
        int16 index;
        if (tex.read_indexed("index", index, OGL_NUMBER_OF_TEXTURE_TYPES+1))
        {
            OGL_Texture_Configure& Config = (index == OGL_NUMBER_OF_TEXTURE_TYPES) ? ogl_preferences.ModelConfig : ogl_preferences.TxtrConfigList[index];
            tex.read_attr("near_filter", Config.NearFilter);
            tex.read_attr("max_size", Config.MaxSize);
        }
    }
    
    // Fix bool options
    horizontal_fov_is_constant = !!horizontal_fov_is_constant;
    
    if (gamma_level < 0 || gamma_level >= NUMBER_OF_GAMMA_LEVELS)
    {
        gamma_level = DEFAULT_GAMMA_LEVEL;
    }
    
    if (fov < 30 && fov != 0) { fov = 30; }
    if (fov > 130) { fov = 130; }
}


InfoTree graphics_preferences_data::write()
{
    InfoTree root;

    root.put_attr("screen_size", (int32_t)screen_mode);
    root.put_attr("hud_size", hud_size);
    root.put_attr("terminal_size", terminal_size);
    root.put_attr("automap_size", automap_size);
    root.put_attr("automap_is_translucent", translucent_map);
    
    root.put_attr("scmode_camera_bob", static_cast<int>(bobbing_type));
    root.put_attr("scmode_fov", fov);
    root.put_attr("scmode_fullscreen", fullscreen);
    root.put_attr("scmode_gamma", gamma_level);
    root.put_attr("scmode_fix_h_not_v", horizontal_fov_is_constant);
    
    root.put_attr("ogl_flags", ogl_preferences.Flags);
    root.put_attr("anisotropy_level", ogl_preferences.AnisotropyLevel);
    root.put_attr("gamma_corrected_blending", ogl_preferences.Use_sRGB);
    root.put_attr("movie_export_video_quality", movie_export_video_quality);
    root.put_attr("movie_export_video_bitrate", movie_export_video_bitrate);
    root.put_attr("movie_export_audio_quality", movie_export_audio_quality);
    root.put_attr("scripted_effects_quality", ephemera_quality);
    
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 2; ++j)
            root.add_color("landscapes.color", ogl_preferences.LscpColors[i][j], 2*i+j);

    for (int i = 0; i <= OGL_NUMBER_OF_TEXTURE_TYPES; ++i)
    {
        OGL_Texture_Configure& Config = (i == OGL_NUMBER_OF_TEXTURE_TYPES) ? ogl_preferences.ModelConfig : ogl_preferences.TxtrConfigList[i];
        
        InfoTree tex;
        tex.put_attr("index", i);
        tex.put_attr("near_filter", Config.NearFilter);
        tex.put_attr("max_size", Config.MaxSize);
        root.add_child("texture", tex);
    }
    return root;
}






//*****************************************************************************
// DIALOGS
//*****************************************************************************



static const strings_t fps_target_labels = {"30", "60 (interpolated)", "120 (interpolated)", "Unlimited (interpolated)"};

static const std::array<int16_t, 4> fps_target_values = {30, 60, 120, 0};

static const strings_t gamma_labels = {"Darkest", "Darker", "Dark", "Normal", "Light", "Really Light", "Even Lighter", "Lightest"};

static const strings_t bobbing_view_labels = {"None", "Default", "Weapon Only"};

static const strings_t hud_scale_labels = {"None", "Small", "Medium", "Large"};

static const strings_t term_scale_labels = {"Normal", "Double", "Largest"};





class w_fov_slider : public w_slider {
public:
    w_fov_slider(int sel) : w_slider(101, sel) {
        init_formatted_value();
    }

    virtual std::string formatted_value(void) {
        std::ostringstream oss;
        oss << (selection + 30);
        return oss.str();
    }
};

extern float get_normal_FOV();

extern bool shapes_file_is_m1();
extern void ResetAllMMLValues();

void graphics_dialog(void *arg)
{
    dialog *parent = (dialog *)arg;

    // Create dialog
    dialog d;

    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("GRAPHICS SETUP"), d);
    placer->add(new w_spacer(), true);

    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);
    table->col_flags(1, placeable::kAlignLeft);
    
    table->add_row(new w_spacer(), true);

    //w_select *size_w = new w_select((int32_t)graphics_preferences.screen_size, strScreenSize); // may be used for DEBUG (shows all sizes)
    w_select *size_w = new w_select((int32_t)graphics_preferences.screen_mode, main_screen.supported_modes());
    table->dual_add(size_w->adding_label("Screen Size"), d);
    table->dual_add(size_w, d);
        
    table->add_row(new w_spacer(), true);
    
    w_toggle *fullscreen_w = new w_toggle(!graphics_preferences.fullscreen);
    table->dual_add(fullscreen_w->adding_label("Windowed Mode"), d);
    table->dual_add(fullscreen_w, d);
    
    w_select_popup *gamma_w = new w_select_popup();
    gamma_w->set_labels(gamma_labels);
    gamma_w->set_selection(graphics_preferences.gamma_level);
    table->dual_add(gamma_w->adding_label("Brightness"), d);
    table->dual_add(gamma_w, d);

    w_select* fps_target_w = new w_select(0, fps_target_labels);
    for (auto i = 0; i < fps_target_labels.size(); ++i)
    {
        if (fps_target_values[i] == graphics_preferences.in_game_fps_target)
        {
            fps_target_w->set_selection(i);
        }
    }
    table->dual_add(fps_target_w->adding_label("Framerate Target"), d);
    table->dual_add(fps_target_w, d);

    table->add_row(new w_spacer(), true);
    
    // FOV options (caution: these can affect gameplay and/or provide unfair advantage to PvP users who have wider monitors)
    
    w_toggle *fixh_w = new w_toggle(!graphics_preferences.horizontal_fov_is_constant);
    table->dual_add(fixh_w->adding_label("Limit Vertical View"), d); // TODO: badly named (implies it limits the vertical look angle but I think it determines how FOV setting acts on displays with different aspects, i.e. how much you can see on a given axis for a given FOV value); probably best to get rid of it and the FOV slider, and set the FOV automatically for (fixed) 4:3, (fixed) 16:9, and (calculated) Ultrawide so that everyone sees the same degree of 'fisheye' regardless of screen shape
    table->dual_add(fixh_w, d);
    
    //
    
    w_toggle *override_fov_w = new w_toggle(graphics_preferences.fov != 0);
    w_fov_slider *fov_slider_w = new w_fov_slider((graphics_preferences.fov == 0 ? static_cast<int>(get_normal_FOV()) : graphics_preferences.fov) - 30);
    fov_slider_w->set_enabled(graphics_preferences.fov != 0);
    override_fov_w->set_selection_changed_callback([&](w_select*) { fov_slider_w->set_enabled(override_fov_w->get_selection()); });

    table->dual_add(override_fov_w->adding_label("Override FOV*"), d);
    auto fov_placer = new horizontal_placer(get_theme_space(ITEM_WIDGET));
    fov_placer->dual_add(override_fov_w, d);
    fov_placer->dual_add(fov_slider_w, d);

    table->add(fov_placer);

    table->dual_add_row(new w_static_text("*may interfere with third-party scenario effects"), d);

    table->add_row(new w_spacer(), true);
    
    // disabling camera bobbing may avoid motion sickness
    
    w_select *bobbing_type_w = new w_select(0, bobbing_view_labels);
    bobbing_type_w->set_selection(static_cast<int>(graphics_preferences.bobbing_type));

    table->dual_add(bobbing_type_w->adding_label("View Bobbing"), d);
    table->dual_add(bobbing_type_w, d);
    
    // TODO: FIX: modernize this: there should be one “HUD” menu with 4 options: Off, Small, Medium, Large
    
    table->add_row(new w_spacer(), true);
    table->dual_add_row(new w_static_text("Heads-Up Display"), d);

    std::vector<Plugin*> hud_plugins;
    auto hud_plugin_index = -1;
    for (auto& plugin : *Plugins::instance()) {
        if (!plugin.hud_lua.empty() && plugin.compatible() && plugin.allowed()) {
            hud_plugins.push_back(&plugin);
            if (plugin.enabled) {
                hud_plugin_index = (int32_t)hud_plugins.size() - 1;
            }
        }
    }
    
    // TODO: HUD is always Lua plugin now

    std::vector<std::string> hud_plugin_labels;
    if (!shapes_file_is_m1()) {
        ++hud_plugin_index;
        hud_plugin_labels.push_back("Classic"); // the Classic M2 HUD, now provided as Lua HUD plugin (TODO: this needs embedded); For Classic M1, use the Classic M2 HUD plugin (which should swap its bitmaps to M1-style automatically); if anyone wants to experience M1 in its 1994 postcard-view HUD, they should use a 3rd-party HUD plugin (we might build this into the Classic HUD plugin, e.g. for use on April 1, but Marathon 1994 deserves to look like its authors would've wanted it to look at the time)
    }

    for (auto hud_plugin : hud_plugins)
    {
        hud_plugin_labels.push_back(hud_plugin->name);
    }
    
    w_select_popup *hud_plugin_w = new w_select_popup();
    hud_plugin_w->set_labels(hud_plugin_labels);
    hud_plugin_w->set_selection(hud_plugin_index >= 0 ? hud_plugin_index : 0);

    table->dual_add(hud_plugin_w->adding_label("HUD Plugin"), d);
    table->dual_add(hud_plugin_w, d);
    
    w_select_popup *hud_scale_w = new w_select_popup();
    hud_scale_w->set_labels(hud_scale_labels);
    hud_scale_w->set_selection(graphics_preferences.hud_size);
    table->dual_add(hud_scale_w->adding_label("HUD Size"), d);
    table->dual_add(hud_scale_w, d);
    
    
    w_select_popup *term_scale_w = new w_select_popup();
    term_scale_w->set_labels(term_scale_labels);
    term_scale_w->set_selection(graphics_preferences.terminal_size);
    table->dual_add(term_scale_w->adding_label("Terminal Size"), d);
    table->dual_add(term_scale_w, d);
    
    w_toggle *map_w = new w_toggle(graphics_preferences.translucent_map);
    table->dual_add(map_w->adding_label("Overlay Map"), d);
    table->dual_add(map_w, d);

    placer->add(table, true);

    placer->add(new w_spacer(), true);
    placer->dual_add(new w_button("RENDERING OPTIONS", [](void* arg) {
        OpenGLDialog::Create()->OpenGLPrefsByRunning();
    }, &d), d);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

    placer->add(button_placer, true);
    
    d.set_widget_placer(placer);
    
    clear_screen();
    
    // TODO: most/all of these settings should be applied immediately when user changes control
    if (d.run() == no_err)
    {
        bool changed = false;
        
        screen_mode_t screen_size = (screen_mode_t)size_w->get_selection();
        if (screen_size != main_screen.mode())
        {
            main_screen.set_mode(screen_size);
            changed = true;
        }
        
        bool fullscreen = fullscreen_w->get_selection() == 0;
        if (fullscreen != graphics_preferences.fullscreen)
        {
            main_screen.set_fullscreen(fullscreen);
            changed = true;
        }
        
        short gamma = static_cast<short>(gamma_w->get_selection());
        if (gamma != graphics_preferences.gamma_level)
        {
            graphics_preferences.gamma_level = gamma; // TODO: main_screen.set_gamma
            changed = true;
        }

        auto fps_target = fps_target_values[fps_target_w->get_selection()];
        if (fps_target != graphics_preferences.in_game_fps_target)
        {
            graphics_preferences.in_game_fps_target = fps_target;
            changed = true;
        }
        
        bool horizontal_fov_is_constant = fixh_w->get_selection() == 0;
        if (horizontal_fov_is_constant != graphics_preferences.horizontal_fov_is_constant)
        {
            
            graphics_preferences.horizontal_fov_is_constant = horizontal_fov_is_constant;
            changed = true;
        }
        
        auto hud_plugin = static_cast<int>(hud_plugin_w->get_selection());
        if (hud_plugin != hud_plugin_index)
        {
            if (!shapes_file_is_m1())
            {
                --hud_plugin;
            }

            for (auto i = 0; i < hud_plugins.size(); ++i)
            {
                hud_plugins[i]->enabled = i == hud_plugin;
            }
            
            changed = true;
        }
        
        short hud_scale = static_cast<short>(hud_scale_w->get_selection());
        if (hud_scale != graphics_preferences.hud_size)
        {
            graphics_preferences.hud_size = hud_scale;
            changed = true;
            
            // L_Call_HUDResize(); TODO: changing hud_size here or on F-key needs to call `resize` trigger
        }
        
        short term_scale = static_cast<short>(term_scale_w->get_selection());
        if (term_scale != graphics_preferences.terminal_size)
        {
            graphics_preferences.terminal_size = term_scale;
            changed = true;
        }
        
        bool translucent_map = map_w->get_selection() != 0;
        if (translucent_map != graphics_preferences.translucent_map) {
            graphics_preferences.translucent_map = translucent_map;
            changed = true;
        }

        auto bobbing_type = static_cast<BobbingType>(bobbing_type_w->get_selection());
        if (bobbing_type != graphics_preferences.bobbing_type) {
            graphics_preferences.bobbing_type = bobbing_type;
            changed = true;
        }

        int fov = override_fov_w->get_selection() == 0 ? 0 : fov_slider_w->get_selection() + 30;
        if (fov != graphics_preferences.fov)
        {
            graphics_preferences.fov = fov;
            changed = true;
        }
        
        if (changed) {
            write_preferences();
            
            // TODO: FIX: unloading existing fonts in middle of dialogs (i.e. when graphics prefs change they reload MML, which resets fonts) causes a crash when next dialog tries to use its theme
/*
            Plugins::instance()->invalidate();
            ResetAllMMLValues();
            LoadBaseMMLScripts(true);
            Plugins::instance()->load_mml(true);
*/
            
            parent->layout();
            parent->draw_all_widgets();        // DirectX seems to need this
        }
    }
}
