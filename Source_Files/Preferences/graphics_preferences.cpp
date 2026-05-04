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

static const strings_t ephemera_quality_labels = {"Off", "Low", "Medium", "High", "Ultra"};


class w_fov_slider : public w_slider
{
public:
    w_fov_slider(int sel) : w_slider(101, sel)
    {
        init_formatted_value();
    }

    virtual std::string formatted_value(void) // numeric value appears to right of slider
    {
        std::ostringstream oss;
        oss << (selection + 30);
        return oss.str();
    }
};


class w_aniso_slider : public w_slider
{
public:
    w_aniso_slider(int num_items, int sel) : w_slider(num_items, sel)
    {
        init_formatted_value();
    }
    
    virtual std::string formatted_value(void)
    {
        std::ostringstream ss;
        ss << ((selection == 0) ? 0 : 1 << (selection - 1));
        return ss.str();
    }
};


extern float get_normal_FOV();

extern bool shapes_file_is_m1();
extern void ResetAllMMLValues();

static void ogl_graphics_dialog(void *arg);


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
    
// TODO: checkbox is bloody useless as unchecking it doesn't automatically move the slider back to its standard position. Ideally we get rid of both controls and set FOV automatically for each screen mode as appropriate. Failing that, move the slider onto the ENHANCED 3D OPTIONS dialog, modifying its implementation so there's a single 'tick' visible at its standard position and that this is slightly sticky (i.e. anything within ±4 snaps the slider to it), this being the appropriate default for the user's screen width (will need to check what 16:9 normally is/should be as it might be the same FOV as 4:3 or maybe slightly increased; for ultrawide the user gets the extra width, though netgames configuration can reduce to the nearest 16:9 while playing). Obviously FOV for Classic should be fixed at its original 1995 value (whatever that was; 74?).
    w_toggle *override_fov_w = new w_toggle(graphics_preferences.fov != 0);
    w_fov_slider *fov_slider_w = new w_fov_slider((graphics_preferences.fov == 0 ? static_cast<int>(get_normal_FOV()) : graphics_preferences.fov) - 30);
    fov_slider_w->set_enabled(graphics_preferences.fov != 0);
    override_fov_w->set_selection_changed_callback([&](w_select*) { fov_slider_w->set_enabled(override_fov_w->get_selection()); });
    
    // Wobble, how can you trust a man that wears both a belt and suspenders? Man can't even trust his own pants.
    table->dual_add(override_fov_w->adding_label("Override FOV*"), d);
    auto fov_placer = new horizontal_placer(get_theme_space(ITEM_WIDGET));
    fov_placer->dual_add(override_fov_w, d);
    fov_placer->dual_add(fov_slider_w, d);

    table->add(fov_placer);

    //table->dual_add_row(new w_static_text("*may interfere with third-party scenario effects"), d);

    table->add_row(new w_spacer(), true);
    
    // disabling camera bobbing may avoid motion sickness
    
    w_select *bobbing_type_w = new w_select(0, bobbing_view_labels);
    bobbing_type_w->set_selection(static_cast<int>(graphics_preferences.bobbing_type));

    table->dual_add(bobbing_type_w->adding_label("View Bobbing"), d);
    table->dual_add(bobbing_type_w, d);
    
    // TODO: FIX: modernize this: there should be one “HUD” menu with 4 options: Off, Small, Medium, Large
    
    table->add_row(new w_spacer(), true);
    table->dual_add_row(new w_static_text("Heads-Up Display"), d);
    
    // TODO: think HUD plugin selection should be in Environment prefs only
    /*
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
    */
    
    // TODO: in Classic mode, should sizes be restricted? Classic HUD will be limited to on or off anyway; its size is always bottom-third of virtual screen. Not sure if computer terminal should _always_ be full-size; I suspect adding an option to change it was a bodge to get around the shitty text quality when it's [badly] scaled to fit modern displays, not because a smaller reading area is useful. (Once the new HUD+terminal design is done, we may remove one or both of these controls; possibly leaving as a Console setting only, or just leave for a 3rd-party HUD plugin which can set the rects to anything.)
    
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
    map_w->set_enabled(graphics_preferences.screen_mode >= screen_mode_t::sd);
    size_w->set_selection_changed_callback([&](w_select*) { map_w->set_enabled(size_w->get_selection()); });
    
    table->dual_add(map_w->adding_label("Translucent Map"), d);
    table->dual_add(map_w, d);

    placer->add(table, true);

    placer->add(new w_spacer(), true);
    
    w_button *enhanced_w = new w_button("ENHANCED 3D OPTIONS", ogl_graphics_dialog, &d);
    enhanced_w->set_enabled(graphics_preferences.screen_mode >= screen_mode_t::sd);
    size_w->set_selection_changed_callback([&](w_select*) { enhanced_w->set_enabled(size_w->get_selection() >= (int32_t)screen_mode_t::sd); });

    placer->dual_add(enhanced_w, d);
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
        /*
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
        */
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


// from the poorly named `preference_dialogs.cpp`, which was basically an OpenGLDialog class that appeared when "Rendering Options" button was clicked, making code much more complicated than necessary

static void ogl_graphics_dialog(void *arg)
{
    dialog *parent = (dialog *)arg;

    // Create dialog
    dialog d;

    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("ENHANCED 3D OPTIONS"), d);
    placer->add(new w_spacer(), true);

    table_placer *general_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    general_table->col_flags(0, placeable::kAlignRight);
    general_table->col_flags(1, placeable::kAlignLeft);
    
    w_toggle *liq_w = new w_toggle(false);
    general_table->dual_add(liq_w->adding_label("Transparent Liquids"), d); // this one can affect gameplay
    general_table->dual_add(liq_w, d);
            
    w_toggle *fog_w = new w_toggle(false);
    general_table->dual_add(fog_w->adding_label("Fog"), d); // a very light fog on L'howon's outside levels might help create distance; similiarly, a light use in murky external spaces could help create a damp, oppressive atmosphere combined with suitable environment sounds
    general_table->dual_add(fog_w, d);

    w_toggle *fader_w = new w_toggle(false);
    general_table->dual_add(fader_w->adding_label("Color Effects"), d);
    general_table->dual_add(fader_w, d);

    w_toggle *bloom_w = new w_toggle(false);
    general_table->dual_add(bloom_w->adding_label("Bloom Effects"), d);
    general_table->dual_add(bloom_w, d);
    
    w_toggle *bump_w = new w_toggle(false);
    general_table->dual_add(bump_w->adding_label("Bump Mapping"), d);
    general_table->dual_add(bump_w, d);
    
    // improves the quality of angled textures further away from camera
    w_aniso_slider* aniso_w = new w_aniso_slider(6, 1);
    general_table->dual_add(aniso_w->adding_label("Anisotropic Filtering"),d);
    general_table->dual_add(aniso_w, d);
    
    general_table->add_row(new w_spacer(), true); // EES: bloody awful, undocumented API wasting my time: must use `add_row`, not `add` when `dual_add` is used to insert 2 columns (label and control)

    // EES: Lua plugins can query this setting so they know how much gibs/etc to create
    w_select* ephemera_w = new w_select(graphics_preferences.ephemera_quality, ephemera_quality_labels);
    general_table->dual_add(ephemera_w->adding_label("Scripted Effects Quality"), d);
    general_table->dual_add(ephemera_w, d);

    placer->add(general_table, true);
    
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    w_button* ok_w = new w_button("ACCEPT", dialog_ok, &d);
    button_placer->dual_add(ok_w, d);
    
    w_button* cancel_w = new w_button("CANCEL", dialog_cancel, &d);
    button_placer->dual_add(cancel_w, d);
    placer->add(button_placer, true);

    d.set_widget_placer(placer);

    
    clear_screen();
    
    // TODO: most/all of these settings should be applied immediately when user changes control
    if (d.run() == no_err)
    {
        bool changed = false;
        
        bool transparent_liquids = liq_w->get_selection() != 0;
        if (transparent_liquids != TEST_FLAG(ogl_preferences.Flags, OGL_Flag_LiqSeeThru))
        {
            SET_FLAG(ogl_preferences.Flags, OGL_Flag_LiqSeeThru, transparent_liquids);
            changed = true;
        }
        
        bool fog = fog_w->get_selection() != 0;
        if (fog != TEST_FLAG(ogl_preferences.Flags, OGL_Flag_Fog))
        {
            SET_FLAG(ogl_preferences.Flags, OGL_Flag_Fog, fog);
            changed = true;
        }
        
        bool color_effects = fader_w->get_selection() != 0;
        if (color_effects != TEST_FLAG(ogl_preferences.Flags, OGL_Flag_Fader))
        {
            SET_FLAG(ogl_preferences.Flags, OGL_Flag_Fog, color_effects);
            changed = true;
        }
        
        bool bloom = bloom_w->get_selection() != 0;
        if (bloom != TEST_FLAG(ogl_preferences.Flags, OGL_Flag_Bloom))
        {
            SET_FLAG(ogl_preferences.Flags, OGL_Flag_Fog, bloom);
            changed = true;
        }
        
        bool bump = bump_w->get_selection() != 0;
        if (bump != TEST_FLAG(ogl_preferences.Flags, OGL_Flag_Bloom))
        {
            SET_FLAG(ogl_preferences.Flags, OGL_Flag_Fog, bump);
            changed = true;
        }
        
        float aniso = (float)aniso_w->get_selection();
        if (aniso != ogl_preferences.AnisotropyLevel)
        {
            ogl_preferences.AnisotropyLevel = aniso;
            changed = true;
        }
        
        int16_t ephemera = (int16_t)ephemera_w->get_selection();
        if (ephemera != graphics_preferences.ephemera_quality)
        {
            graphics_preferences.ephemera_quality = ephemera;
            changed = true;
        }
        
        if (changed)
        {
            write_preferences();
        }
    }
}

