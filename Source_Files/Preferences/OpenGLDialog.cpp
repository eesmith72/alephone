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

	Int16Pref ephemeraQualityPref(graphics_preferences.ephemera_quality);
	binders.insert<int>(m_ephemeraQualityWidget, &ephemeraQualityPref);
	
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
    SdlOpenGLDialog(){}
	

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

private:
	enum {
		TAB_WIDGET = 400,
		BASIC_TAB,
		ADVANCED_TAB
	};
	
	tab_placer* m_tabs;
	dialog m_dialog;
};




std::unique_ptr<OpenGLDialog> OpenGLDialog::Create()
{
	return std::make_unique<SdlOpenGLDialog>();
}
