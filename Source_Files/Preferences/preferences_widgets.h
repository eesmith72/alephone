/*
 preferences_widgets.h - Preferences widgets
 
 Written in 2000 by Christian Bauer
 
 Ripped out of preferences_sdl.* into a new file Mar 1, 2002 by Woody Zenfell.
 
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

#ifndef PREFERENCES_WIDGETS_SDL_H
#define PREFERENCES_WIDGETS_SDL_H

#include "cseries.h"
#include "find_files.hpp"
#include "Canvas.hpp"
#include "collection_definition.h"
#include "sdl_widgets.h"
#include "fonts.hpp"
#include "Screen.hpp"
#include "screen_drawing.h"
#include "interface.h"
#include "Plugins.h"

// From shell_sdl.cpp
extern std::vector<ao_path> scenario_data_search_paths;


// Environment item
class env_item
{
public:
	env_item() : indent(0), selectable(false)
	{
		name[0] = 0;
	}

    env_item(const ao_path& fs, int i, bool sel) : spec(fs), indent(i), selectable(sel)
	{
        name = spec.filename();
	}

    ao_path spec;       // Specifier of associated file
	std::string name;   // Last part of file name
	int indent;         // Indentation level
	bool selectable;    // Flag: item refers to selectable file (otherwise to directory name)
};


// Environment file list widget
class w_env_list : public w_list<env_item>
{
public:
	w_env_list(const std::vector<env_item> &items, const std::string& selection, dialog *d) : w_list<env_item>(items, 400, 15, 0), parent(d)
	{
		int32_t num = 0;
		for (const auto& it : items)
        {
			if (it.spec == selection)
            {
				set_selection(num);
				break;
			}
            num++;
		}
	}
    
    int32_t count() const { return (int32_t)items.size(); }

	bool is_item_selectable(size_t i)
	{
		return items.at(i).selectable;
	}

	void item_selected(void)
	{
		parent->quit(0);
	}

	void draw_item(std::vector<env_item>::const_iterator i, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected)
	{
		y += get_font()->ascent;

		SDL_Color color;
		if (i->selectable)
        {
			color = selected ? get_theme_color(ITEM_WIDGET, ACTIVE_STATE) : get_theme_color(ITEM_WIDGET, DEFAULT_STATE);
		}
        else
        {
            color = get_theme_color(LABEL_WIDGET, DEFAULT_STATE);
        }
        canvas->set_clip({x, 0, width, canvas->h});
        canvas->draw_text(hide_ao_filename_extension(i->name), get_font(), color, {x + i->indent * 8, y});
        canvas->clear_clip();
	}

private:
	dialog *parent;
};


// Environment selection button
// ZZZ: added callback stuff - callback is made if user clicks on an entry in the selection dialog.
class w_env_select;
using selection_made_callback_t = std::function<void(w_env_select*)>;


class w_env_select : public w_select_button
{
public:
    w_env_select(const std::string& path, const std::string& m, filetype_t t, dialog *d)
        : w_select_button("", select_item_callback, NULL), // TODO: this previously passed item_name
    	  parent(d), menu_title(m), type(t), mCallback(NULL), prefer_net{false}
	{
		set_arg(this);
		set_path(path);
	}
	~w_env_select() {}

    void set_selection_made_callback(selection_made_callback_t inCallback) {
        mCallback = inCallback;
    }

	void set_path(const ao_path& path)
	{
		item = path;
		
		if (!path.empty())
		{
            std::string name = hide_ao_filename_extension(item.filename());
			item_name = std::filesystem::exists(item) ? name : "[?" + name + "]";
			set_selection(item_name);
		}
		else
		{
			set_selection(sFileChooserInvalidFileString);
		}
	}

	const ao_path& get_path(void) const
    {
		return item;
	}

	void set_prefer_net(bool prefer_net)
	{
		this->prefer_net = prefer_net;
	}

private:
	void select_item(dialog *parent);
	static void select_item_callback(void *arg);

    dialog *parent;
	const std::string menu_title;	// Selection menu title

	ao_path item;
	filetype_t type;
	std::string item_name;

    selection_made_callback_t mCallback;

	bool prefer_net;
};


class EnvSelectWidget : public SDLWidgetWidget, public Bindable<ao_path>
{
public:
	EnvSelectWidget(w_env_select* env_select) : SDLWidgetWidget(env_select), m_env_select(env_select) {}

	void set_callback(ControlHitCallback callback)
    {
        m_env_select->set_selection_made_callback([=](w_env_select*){ callback(); });
    }
    
	void set_file(const ao_path& path) { m_env_select->set_path(path); }
	const ao_path get_file() { return m_env_select->get_path(); }

    virtual void bind_import(ao_path f) { set_file(f); }
	virtual ao_path bind_export() { return get_file(); }

	void set_prefer_net(bool prefer_net) { m_env_select->set_prefer_net(prefer_net); }

private:
	w_env_select* m_env_select;
};



class w_plugins : public w_list_base {
public:
	w_plugins(std::vector<Plugin>& plugins, int width, int numRows) : w_list_base(width, numRows), m_plugins(plugins)
	{
		saved_min_height = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
		trough_rect.h = saved_min_height - get_theme_space(LIST_WIDGET, TROUGH_T_SPACE) - get_theme_space(LIST_WIDGET, TROUGH_B_SPACE);
		new_items();
	}

	uint16 item_height() { return 2 * get_font()->line_height + get_font()->line_height / 2 + 2; }
    
    int32_t count() const { return (int32_t)m_plugins.size(); }

protected:
	void draw_items(Canvas* canvas);
	void item_selected();

private:
	std::vector<Plugin>& m_plugins;
	void draw_item(Plugins::iterator i, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected);
};

#endif
