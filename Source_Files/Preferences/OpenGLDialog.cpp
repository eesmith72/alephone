/*

	Copyright (C) 2006 and beyond by Bungie Studios, Inc.
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

#include "OpenGLDialog.h"
#include "preferences.hpp"
#include "binders.h"
#include "OGL_Setup.h"
#include "Screen.hpp"


class TexQualityPref : public Bindable<int>
{
public:
	TexQualityPref(int16& pref, int16 normal) : m_pref(pref), m_normal(normal) {}
	
	virtual int bind_export()
	{
		int result = 0;
		int temp = m_pref;
		while (temp >= m_normal) {
			temp = temp >> 1;
			++result;
		}
		return result;
	}
	
	virtual void bind_import(int value)
	{
		m_pref = (value == 0) ? 0 : m_normal << (value - 1);
	}
	
protected:
	int16& m_pref;
	int16 m_normal;
};


class ColourPref : public Bindable<rgb_color>
{
public:
	ColourPref(rgb_color& pref) : m_pref(pref) {}
	
	virtual rgb_color bind_export() { return m_pref; }
	virtual void bind_import(rgb_color value) { m_pref = value; }
	
protected:
	rgb_color& m_pref;
};

class FarFilterPref : public Bindable<int>
{
public:
	FarFilterPref(int16& pref) : m_pref(pref) { }

	int bind_export() {
		if (m_pref == 5)
		{
			return 3;
		} 
		else if (m_pref == 3)
		{
			return 2;
		}
		else
		{
			return m_pref;
		}
	}

	void bind_import(int value) {
		if (value == 2)
		{
			m_pref = 3;
		}
		else if (value == 3)
		{
			m_pref = 5;
		}
		else
		{
			m_pref = value;
		}
	}

protected:
	int16& m_pref;
};


class TimesTwoPref : public Bindable<int>
{
public:
	TimesTwoPref(int16& pref) : m_pref(pref) {}
	
	virtual int bind_export()
	{
		return (m_pref / 2);
	}
	
	virtual void bind_import(int value)
	{
		m_pref = value * 2;
	}

protected:
	int16& m_pref;
};


class AnisotropyPref : public Bindable<int>
{
public:
	AnisotropyPref(float& pref) : m_pref(pref) {}
	
	virtual int bind_export()
	{
		int result = 0;
		int temp = static_cast<int>(m_pref);
		while (temp >= 1) {
			temp = temp >> 1;
			++result;
		}
		return result;
	}
	
	virtual void bind_import(int value)
	{
		m_pref = (value == 0) ? 0.0 : 1 << (value - 1);
	}

protected:
	float& m_pref;
};


OpenGLDialog::OpenGLDialog() {}

OpenGLDialog::~OpenGLDialog()
{
	delete m_cancelWidget;
	delete m_okWidget;
	delete m_fogWidget;
	delete m_colourEffectsWidget;
	delete m_transparentLiquidsWidget;
	delete m_blurWidget;
	delete m_bumpWidget;
	delete m_anisotropicWidget;
	delete m_sRGBWidget;

}

void OpenGLDialog::OpenGLPrefsByRunning()
{
    // TODO: get rid of binders and migrate what's left of this code to use the same convention as other dialogs; the extra magic and different implementations only make the Prefs code hard to work on, especially when trying to consolidate and rework into a modern design (plus, if goal is to switch to imGui+Sol2, simplifying the current code needs done first so that porting becomes straightforward)
    
	m_cancelWidget->set_callback(std::bind(&OpenGLDialog::Stop, this, false));
	m_okWidget->set_callback(std::bind(&OpenGLDialog::Stop, this, true));
	
	BinderSet binders;
	
	BitPref fogPref(ogl_preferences.Flags, OGL_Flag_Fog);
	binders.insert<bool>(m_fogWidget, &fogPref);
	BitPref colourEffectsPref(ogl_preferences.Flags, OGL_Flag_Fader);
	binders.insert<bool>(m_colourEffectsWidget, &colourEffectsPref);
	BitPref transparentLiquidsPref(ogl_preferences.Flags, OGL_Flag_LiqSeeThru);
	binders.insert<bool>(m_transparentLiquidsWidget, &transparentLiquidsPref);
	BitPref blurPref(ogl_preferences.Flags, OGL_Flag_Bloom);
	binders.insert<bool>(m_blurWidget, &blurPref);
	BitPref bumpPref(ogl_preferences.Flags, OGL_Flag_BumpMap);
	binders.insert<bool>(m_bumpWidget, &bumpPref);
		
	AnisotropyPref anisotropyPref(ogl_preferences.AnisotropyLevel);
	binders.insert<int>(m_anisotropicWidget, &anisotropyPref);

	BoolPref sRGBPref(ogl_preferences.Use_sRGB);
	binders.insert<bool>(m_sRGBWidget, &sRGBPref);

	Int16Pref ephemeraQualityPref(graphics_preferences.ephemera_quality);
	binders.insert<int>(m_ephemeraQualityWidget, &ephemeraQualityPref);
	
	TexQualityPref modelQualityPref(ogl_preferences.ModelConfig.MaxSize, 256);
	binders.insert<int>(m_modelQualityWidget, &modelQualityPref);
	
	// Set initial values from prefs
	binders.migrate_all_second_to_first();
	
	bool result = Run();
	
	if (result)
    {
		// migrate prefs and save
		binders.migrate_all_first_to_second();
		write_preferences();
	}
}


static const strings_t far_filter_labels       = {"None", "Linear", "Bilinear", "Trilinear"};
static const strings_t near_filter_labels      = {"None", "Linear"};
static const strings_t ephemera_quality_labels = {"Off", "Low", "Medium", "High", "Ultra"};


class w_aniso_slider : public w_slider
{
public:
	w_aniso_slider(int num_items, int sel) : w_slider(num_items, sel) {
		init_formatted_value();
	}
	
	virtual std::string formatted_value(void) {
		std::ostringstream ss;
		ss << ((selection == 0) ? 0 : 1 << (selection - 1));
		return ss.str();
	}
};


class SdlOpenGLDialog : public OpenGLDialog
{
public:
	SdlOpenGLDialog()
	{

		vertical_placer *placer = new vertical_placer;
		placer->dual_add(new w_title("OPENGL OPTIONS"), m_dialog);
		placer->add(new w_spacer(), true);
		
		// horizontal_placer *tabs_placer = new horizontal_placer;
		// w_button *w_general_tab = new w_button("GENERAL");
		// w_general_tab->set_callback(choose_generic_tab, static_cast<void *>(this));
		// tabs_placer->dual_add(w_general_tab, m_dialog);
		// w_button *w_advanced_tab = new w_button("ADVANCED");
		// w_advanced_tab->set_callback(choose_advanced_tab, static_cast<void *>(this));
		// tabs_placer->dual_add(w_advanced_tab, m_dialog);
		// placer->add(tabs_placer, true);

		m_tabs = new tab_placer();

		std::vector<std::string> labels;
		labels.push_back("GENERAL");
		labels.push_back("ADVANCED");
		w_tab *tabs = new w_tab(labels, m_tabs);
		placer->dual_add(tabs, m_dialog);
		
		placer->add(new w_spacer(), true);

        // TODO: is there any reason these needs to be options?
        
		table_placer *general_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
		general_table->col_flags(0, placeable::kAlignRight);
		general_table->col_flags(1, placeable::kAlignLeft);
		
		w_toggle *fog_w = new w_toggle(false);
		general_table->dual_add(fog_w->adding_label("Fog"), m_dialog);
		general_table->dual_add(fog_w, m_dialog);

		w_toggle *fader_w = new w_toggle(false);
		general_table->dual_add(fader_w->adding_label("Color Effects"), m_dialog);
		general_table->dual_add(fader_w, m_dialog);

		w_toggle *liq_w = new w_toggle(false);
		general_table->dual_add(liq_w->adding_label("Transparent Liquids"), m_dialog);
		general_table->dual_add(liq_w, m_dialog);

		w_toggle *blur_w = new w_toggle(false);
		general_table->dual_add(blur_w->adding_label("Bloom Effects"), m_dialog);
		general_table->dual_add(blur_w, m_dialog);
		
		w_toggle *bump_w = new w_toggle(false);
		general_table->dual_add(bump_w->adding_label("Bump Mapping"), m_dialog);
		general_table->dual_add(bump_w, m_dialog);

		w_select_popup* ephemera_w = new w_select_popup();
		ephemera_w->set_labels(ephemera_quality_labels);
		general_table->dual_add(ephemera_w->adding_label("Scripted Effects Quality"), m_dialog);
		general_table->dual_add(ephemera_w, m_dialog);
		
		general_table->add_row(new w_spacer(), true);

		w_toggle *vsync_w = new w_toggle(false);
		general_table->dual_add(vsync_w->adding_label("VSync"), m_dialog);
		general_table->dual_add(vsync_w, m_dialog);

		w_aniso_slider* aniso_w = new w_aniso_slider(6, 1);
		general_table->dual_add(aniso_w->adding_label("Anisotropic Filtering"),m_dialog);
		general_table->dual_add(aniso_w, m_dialog);

		w_toggle *srgb_w = new w_toggle(false);
//		general_table->dual_add(srgb_w->adding_label("Gamma-corrected Blending"), m_dialog);
//		general_table->dual_add(srgb_w, m_dialog);


		general_table->add_row(new w_spacer(), true);

		general_table->dual_add_row(new w_static_text("Replacement Texture Quality"), m_dialog);
	
		w_select_popup *texture_quality_wa[OGL_NUMBER_OF_TEXTURE_TYPES];
		for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; i++) texture_quality_wa[i] = NULL;
		
		texture_quality_wa[OGL_Txtr_Wall] =  new w_select_popup();
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Wall]->adding_label("Walls"), m_dialog);
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Wall], m_dialog);
		
		texture_quality_wa[OGL_Txtr_Landscape] = new w_select_popup();
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Landscape]->adding_label("Landscapes"), m_dialog);
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Landscape], m_dialog);

		texture_quality_wa[OGL_Txtr_Inhabitant] = new w_select_popup();
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Inhabitant]->adding_label("Sprites"), m_dialog);
		general_table->dual_add(texture_quality_wa[OGL_Txtr_Inhabitant], m_dialog);

		texture_quality_wa[OGL_Txtr_WeaponsInHand] = new w_select_popup();
		general_table->dual_add(texture_quality_wa[OGL_Txtr_WeaponsInHand]->adding_label("Weapons in Hand"), m_dialog);
		general_table->dual_add(texture_quality_wa[OGL_Txtr_WeaponsInHand], m_dialog);

		texture_quality_wa[OGL_Txtr_HUD] = new w_select_popup();
		general_table->dual_add(texture_quality_wa[OGL_Txtr_HUD]->adding_label("HUD / Terminals"), m_dialog);
		general_table->dual_add(texture_quality_wa[OGL_Txtr_HUD], m_dialog);

		w_select_popup *model_quality_w = new w_select_popup();
		general_table->dual_add(model_quality_w->adding_label("3D Model Skins"), m_dialog);
		general_table->dual_add(model_quality_w, m_dialog);
	
        std::vector<string> tex_quality_strings;
		tex_quality_strings.push_back("Unlimited");
		tex_quality_strings.push_back("Normal");
		tex_quality_strings.push_back("High");
		tex_quality_strings.push_back("Higher");
		tex_quality_strings.push_back("Highest");
	
		for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; i++) {
			if (texture_quality_wa[i]) {
				texture_quality_wa[i]->set_labels(tex_quality_strings);
			}
		}
		model_quality_w->set_labels(tex_quality_strings);

		vertical_placer *advanced_placer = new vertical_placer;

		table_placer *advanced_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
		advanced_table->col_flags(0, placeable::kAlignRight);
	
		w_toggle *use_npot_w = new w_toggle(false);
		advanced_table->dual_add(use_npot_w->adding_label("Non-Power-of-Two Textures"), m_dialog);
		advanced_table->dual_add(use_npot_w, m_dialog);
		advanced_table->dual_add_row(new w_static_text("Non-power-of-two textures conserve memory,"), m_dialog);
		advanced_table->dual_add_row(new w_static_text("but cause problems on some machines."), m_dialog);

		advanced_table->add_row(new w_spacer(), true);
		advanced_table->dual_add_row(new w_static_text("Texture Filtering"), m_dialog);
		advanced_placer->add(advanced_table, true);

		w_select* near_filter_wa[OGL_NUMBER_OF_TEXTURE_TYPES];
		w_select* far_filter_wa[OGL_NUMBER_OF_TEXTURE_TYPES];
		for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; ++i)
		{
			near_filter_wa[i] = new w_select(0, near_filter_labels);
			if (i == OGL_Txtr_Wall || i == OGL_Txtr_Inhabitant)
				far_filter_wa[i] = new w_select(0, far_filter_labels);
			else
				far_filter_wa[i] = NULL;
		}
		
		w_label* near_filter_labels[OGL_NUMBER_OF_TEXTURE_TYPES];
		near_filter_labels[OGL_Txtr_Wall] = new w_label("Walls");
		near_filter_labels[OGL_Txtr_Inhabitant] = new w_label("Sprites");
		near_filter_labels[OGL_Txtr_Landscape] = new w_label("Landscapes");
		near_filter_labels[OGL_Txtr_WeaponsInHand] = new w_label("Weapons in Hand");
		near_filter_labels[OGL_Txtr_HUD] = new w_label("HUD / Terminals");
	
		table_placer *ftable = new table_placer(3, get_theme_space(ITEM_WIDGET));
		
		ftable->col_flags(0, placeable::kAlignRight);
		ftable->col_flags(1, placeable::kAlignLeft);
		ftable->col_flags(2, placeable::kAlignLeft);
		
		ftable->add(new w_spacer(), true);
		ftable->dual_add(new w_label("Near"), m_dialog);
		ftable->dual_add(new w_label("Distant"), m_dialog);
		
		for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; ++i)
		{
			ftable->dual_add(near_filter_labels[i], m_dialog);
			ftable->dual_add(near_filter_wa[i], m_dialog);
			near_filter_wa[i]->set_label(near_filter_labels[i]);

			if (far_filter_wa[i])
			{
				ftable->dual_add(far_filter_wa[i], m_dialog);
				far_filter_wa[i]->set_label(near_filter_labels[i]);
			}
			else
				ftable->add(new w_spacer(), true);
		}
		
		ftable->col_min_width(1, (ftable->col_width(0) - get_theme_space(ITEM_WIDGET)) / 2);
		ftable->col_min_width(2, (ftable->col_width(0) - get_theme_space(ITEM_WIDGET)) / 2);
		
		advanced_placer->add(ftable, true);

		advanced_placer->add(new w_spacer(), true);
		w_select_popup *texture_resolution_wa[OGL_NUMBER_OF_TEXTURE_TYPES];
		w_select_popup *texture_depth_wa[OGL_NUMBER_OF_TEXTURE_TYPES];
		for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; i++) 
		{
			texture_resolution_wa[i] = new w_select_popup();
			texture_depth_wa[i] = new w_select_popup();
		}

		w_label *texture_labels[OGL_NUMBER_OF_TEXTURE_TYPES];
		texture_labels[OGL_Txtr_Wall] = new w_label("Walls");
		texture_labels[OGL_Txtr_Landscape] = new w_label("Landscapes");
		texture_labels[OGL_Txtr_Inhabitant] = new w_label("Sprites");
		texture_labels[OGL_Txtr_WeaponsInHand] = new w_label("Weapons in Hand");
		texture_labels[OGL_Txtr_HUD] = new w_label("HUD / Terminals");

		m_tabs->add(general_table, true);
		m_tabs->add(advanced_placer, true);
		placer->add(m_tabs, false);
	
		placer->add(new w_spacer(), true);

		horizontal_placer *button_placer = new horizontal_placer;
		w_button* ok_w = new w_button("ACCEPT");
		button_placer->dual_add(ok_w, m_dialog);
		
		w_button* cancel_w = new w_button("CANCEL");
		button_placer->dual_add(cancel_w, m_dialog);
		placer->add(button_placer, true);

		m_dialog.set_widget_placer(placer);

		m_cancelWidget = new ButtonWidget(cancel_w);
		m_okWidget = new ButtonWidget(ok_w);
		
		m_fogWidget = new ToggleWidget(fog_w);
		m_colourEffectsWidget = new ToggleWidget(fader_w);
		m_transparentLiquidsWidget = new ToggleWidget(liq_w);
		m_blurWidget = new ToggleWidget(blur_w);
		m_bumpWidget = new ToggleWidget(bump_w);

		m_ephemeraQualityWidget = new PopupSelectorWidget(ephemera_w);

		m_anisotropicWidget = new SliderSelectorWidget(aniso_w);

		m_sRGBWidget = new ToggleWidget(srgb_w);

		//m_wallsFilterWidget = new SelectSelectorWidget(far_filter_wa[OGL_Txtr_Wall]);
		//m_spritesFilterWidget = new SelectSelectorWidget(far_filter_wa[OGL_Txtr_Inhabitant]);

        // TODO: near filter should be set per-collection in Shapes MML (or automatically if it can be inferred from bitmap dimensions and the size it's being rendered at); only HD sprites and wall textures should use this as its quality is abominable on low-res bitmaps
		//for (int i = 0; i < OGL_NUMBER_OF_TEXTURE_TYPES; ++i) {
		//	m_textureQualityWidget [i] = new PopupSelectorWidget(texture_quality_wa[i]);
		//	m_nearFiltersWidget[i] = new SelectSelectorWidget(near_filter_wa[i]);
		//}
		m_modelQualityWidget = new PopupSelectorWidget(model_quality_w); // needed? again, we should be able to infer a suitable setting automatically, or make it one of the values set by an Fx slider
	}

	~SdlOpenGLDialog()
    {
		delete m_tabs;
	}

	virtual bool Run()
	{	
		return (m_dialog.run() == 0);
	}

	virtual void Stop(bool result)
	{
		m_dialog.quit(result ? 0 : -1);
	}

	static void choose_generic_tab(void *arg);
	static void choose_advanced_tab(void *arg);

private:
	enum {
		TAB_WIDGET = 400,
		BASIC_TAB,
		ADVANCED_TAB
	};
	
	tab_placer* m_tabs;
	dialog m_dialog;
};


void SdlOpenGLDialog::choose_generic_tab(void *arg)
{
	SdlOpenGLDialog *d = static_cast<SdlOpenGLDialog *>(arg);
	d->m_tabs->choose_tab(0);
	d->m_dialog.draw_all_widgets();
}


void SdlOpenGLDialog::choose_advanced_tab(void *arg)
{
	SdlOpenGLDialog *d = static_cast<SdlOpenGLDialog *>(arg);
	d->m_tabs->choose_tab(1);
	d->m_dialog.draw_all_widgets();
}


std::unique_ptr<OpenGLDialog> OpenGLDialog::Create()
{
	return std::make_unique<SdlOpenGLDialog>();
}
