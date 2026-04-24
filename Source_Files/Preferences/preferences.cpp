/*
 preferences.cpp
 
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

#include "map.h"
#include "shell.h" /* For the screen_mode structure */
#include "interface.h"
#include "SoundManager.h"

#include "preferences.h"
#include "wad.h"
#include "network.h" // for _ethernet, etc.
#include "find_files.hpp"
#include "map_wad.h" // for set_current_map_path
#include "screen.h"
#include "fades.h"
#include "physics_wad.h"
#include "Console.h"
#include "Plugins.h"

#include "InfoTree.h"
#include "StarGameProtocol.h"

#include "tags.h"

#include "sdl_dialogs.h"
#include "fonts.hpp"
#include "sdl_widgets.h"
#include "images.h"
#include "preference_dialogs.h"
#include "preferences_widgets.h"
#include "mouse.h"
#include "joystick.h"

#include "Music.h"
#include "HTTP.h"
#include "alephversion.h"

#include <boost/algorithm/hex.hpp>

#include "shell_options.h"
#include "OpenALManager.h"
#include "resource_manager.h"
#include "XML_LevelScript.h"



// 8-bit support is still here if you undefine this, but you'll need to fix it
// #define TRUE_COLOR_ONLY 1

using namespace alephone;

static const char sPasswordMask[] = "reverof nohtaram";

static const strings_t sNetworkGameProtocolNames =
{	// These should match up with _network_game_protocol_star, etc.
	"star"
};

static const size_t NUMBER_OF_NETWORK_GAME_PROTOCOL_NAMES = sizeof(sNetworkGameProtocolNames) / sizeof(sNetworkGameProtocolNames[0]);


// Have the prefs been inited?
static bool PrefsInited = false;

static std::vector<boost::filesystem::path> orphan_disabled_plugins;
static std::vector<boost::filesystem::path> orphan_enabled_plugins;

// Global preferences data
struct graphics_preferences_data *graphics_preferences = NULL;
struct network_preferences_data *network_preferences = NULL;
struct player_preferences_data *player_preferences = NULL;
struct input_preferences_data *input_preferences = NULL;
SoundManager::Parameters *sound_preferences = NULL;


environment_preferences_data environment_preferences;


static std::string get_name_from_system(void);


// LP: getting rid of the (void *) mechanism as inelegant and non-type-safe
static void default_graphics_preferences(graphics_preferences_data *preferences);
static bool validate_graphics_preferences(graphics_preferences_data *preferences);
static void default_network_preferences(network_preferences_data *preferences);
static bool validate_network_preferences(network_preferences_data *preferences);
static void default_player_preferences(player_preferences_data *preferences);
static void default_input_preferences(input_preferences_data *preferences);
static void default_environment_preferences(environment_preferences_data *preferences);


void parse_graphics_preferences(InfoTree root, std::string version);
void parse_player_preferences(InfoTree root, std::string version);
void parse_input_preferences(InfoTree root, std::string version);
void parse_sound_preferences(InfoTree root, std::string version);
void parse_network_preferences(InfoTree root, std::string version);
void parse_environment_preferences(InfoTree root, std::string version);

// Prototypes
static void player_dialog(void *arg);
static void online_dialog(void *arg);
static void graphics_dialog(void *arg);
static void sound_dialog(void *arg);
static void controls_dialog(void *arg);
static void environment_dialog(void *arg);
static void plugins_dialog(void *arg);
static void keyboard_dialog(void *arg);
//static void texture_options_dialog(void *arg);

/*
 *  Get user name
 */

static std::string get_name_from_system()
{
#if defined(unix) || defined (__MACOSX__) || defined(__NetBSD__) || defined(__OpenBSD__)

	const char *login_name = getlogin();
	std::string login = (login_name ? login_name : "");
	if (login.length())
		return login;

#elif defined(__WIN32__)

	wchar_t wname[UNLEN + 1];
	DWORD wname_n = UNLEN + 1;
	if (GetUserNameW(wname, &wname_n))
		return wide_to_utf8(wname);

#else
//#error get_name_from_system() not implemented for this platform
#endif

	return "Bob User";
}


/*
 *  Main preferences dialog
 */

void display_main_preferences_dialog(void)
{
	// Save the existing preferences, in case we have to reload them
	write_preferences();

	// Create top-level dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
	w_title *w_header = new w_title("PREFERENCES");
	d.add(w_header);
	w_button *w_player = new w_button("PLAYER", player_dialog, &d);
	d.add(w_player);
	w_button *w_online = new w_button("INTERNET", online_dialog, &d);
	d.add(w_online);
	w_button *w_graphics = new w_button("GRAPHICS", graphics_dialog, &d);
	d.add(w_graphics);
	w_button *w_sound = new w_button("SOUND", sound_dialog, &d);
	d.add(w_sound);
	w_button *w_controls = new w_button("CONTROLS", controls_dialog, &d);
	d.add(w_controls);
	w_button *w_environment = new w_button("ENVIRONMENT", environment_dialog, &d);
	d.add(w_environment);
	w_button *w_plugins = new w_button("PLUGINS", plugins_dialog, &d);
	d.add(w_plugins);

	w_button *w_return = new w_button("RETURN", dialog_cancel, &d);
	d.add(w_return);

	placer->add(w_header);
	placer->add(new w_spacer, true);
	placer->add(w_player);
	placer->add(w_online);
	placer->add(w_graphics);
	placer->add(w_sound);
	placer->add(w_controls);
	placer->add(w_environment);
	placer->add(w_plugins);
	placer->add(new w_spacer, true);
	placer->add(w_return);

	d.set_widget_placer(placer);

	// Clear menu screen
	clear_screen();

	// Run dialog
	d.run();

    // if bit depth or screen resolution changes, update screen.cpp
    screen_mode_data mode = graphics_preferences->screen_mode;
    if (mode.bit_depth != graphics_preferences->screen_mode.bit_depth)
    {
        clear_screen();
        Screen::instance()->Initialize(&graphics_preferences->screen_mode);

    }
    else if (memcmp(&mode, &graphics_preferences->screen_mode, sizeof(struct screen_mode_data)))
    {
        change_screen_mode(&graphics_preferences->screen_mode, false);
    }
}

class CrosshairPref : public Bindable<int>
{
public:
	CrosshairPref(short& pref) : m_pref(pref) { }

	virtual int bind_export() {
		return (m_pref - 1);
	}

	virtual void bind_import(int value) {
		m_pref = value + 1;
	}

protected:
	short& m_pref;
};


class ColorComponentPref : public Bindable<int>
{
public:
	ColorComponentPref(uint16& pref) : m_pref(pref) { }
	
	virtual int bind_export() {
		return (m_pref >> 12);
	}

	virtual void bind_import(int value) {
		m_pref = value << 12;
	}

protected:
	uint16& m_pref;
};


class OpacityPref : public Bindable<int>
{
public:
	OpacityPref(float& pref) : m_pref(pref) { }
	
	virtual int bind_export() {
		return (static_cast<int>(floor(m_pref * 16)));
	}

	virtual void bind_import(int value) {
		m_pref = ((float) value / 16.0);
	}
protected:
	float& m_pref;
};


static const strings_t shape_labels = {
	"Cross",
    "Octagon",
};

enum { kCrosshairWidget };

static std::unique_ptr<BinderSet> crosshair_binders;

struct update_crosshair_display
{
	void operator()(dialog *d) {
		crosshair_binders->migrate_all_first_to_second();
	}
};

class w_crosshair_slider : public w_slider {
public:
	w_crosshair_slider(int num_items, int sel) : w_slider(num_items, sel) {
		init_formatted_value();
	}
	
	virtual std::string formatted_value(void) {
		std::ostringstream ss;
		ss << (selection + 1);
		return ss.str();
	}
};

static void crosshair_dialog(void *arg)
{
	CrosshairData OldCrosshairs = player_preferences->Crosshairs;
	crosshair_binders.reset(new BinderSet);

	dialog *parent = (dialog *) arg;
	(void)parent;

	dialog d;
	vertical_placer *placer = new vertical_placer;
	w_title *w_header = new w_title("CROSSHAIR SETTINGS");
	placer->dual_add(w_header, d);
	placer->add(new w_spacer, true);

	placer->dual_add(new w_static_text("HUD plugins may override these settings."), d);
	placer->add(new w_spacer, true);

	w_crosshair_display *crosshair_w = new w_crosshair_display();
	placer->dual_add(crosshair_w, d);

	placer->add(new w_spacer, true);

	table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET));
	table->col_flags(0, placeable::kAlignRight);

	// Shape
	w_select *shape_w = new w_select(0, shape_labels);
	SelectSelectorWidget shapeWidget(shape_w);
	Int16Pref shapePref(player_preferences->Crosshairs.Shape);
	crosshair_binders->insert<int> (&shapeWidget, &shapePref);
	table->dual_add(shape_w->adding_label("Shape"), d);
	table->dual_add(shape_w, d);

	table->add_row(new w_spacer(), true);

	// Thickness
	w_slider* thickness_w = new w_crosshair_slider(7, 0);
	SliderSelectorWidget thicknessWidget(thickness_w);
	CrosshairPref thicknessPref(player_preferences->Crosshairs.Thickness);
	crosshair_binders->insert<int> (&thicknessWidget, &thicknessPref);
	table->dual_add(thickness_w->adding_label("Width"), d);
	table->dual_add(thickness_w, d);

	// From Center
	w_slider *from_center_w = new w_slider(15, 0);
	SliderSelectorWidget fromCenterWidget(from_center_w);
	Int16Pref fromCenterPref(player_preferences->Crosshairs.FromCenter);
	crosshair_binders->insert<int> (&fromCenterWidget, &fromCenterPref);
	table->dual_add(from_center_w->adding_label("Gap"), d);
	table->dual_add(from_center_w, d);

	// Length
	w_slider *length_w = new w_crosshair_slider(15, 0);
	SliderSelectorWidget lengthWidget(length_w);
	CrosshairPref lengthPref(player_preferences->Crosshairs.Length);
	crosshair_binders->insert<int> (&lengthWidget, &lengthPref);
	table->dual_add(length_w->adding_label("Size"), d);
	table->dual_add(length_w, d);

	table->add_row(new w_spacer(), true);
	table->dual_add_row(new w_static_text("Color"), d);

	// Color
	w_slider *red_w = new w_percentage_slider(16, 0);
	SliderSelectorWidget redWidget(red_w);
	ColorComponentPref redPref(player_preferences->Crosshairs.Color.red);
	crosshair_binders->insert<int> (&redWidget, &redPref);
	table->dual_add(red_w->adding_label("Red"), d);
	table->dual_add(red_w, d);

	w_slider *green_w = new w_percentage_slider(16, 0);
	SliderSelectorWidget greenWidget(green_w);
	ColorComponentPref greenPref(player_preferences->Crosshairs.Color.green);
	crosshair_binders->insert<int> (&greenWidget, &greenPref);
	table->dual_add(green_w->adding_label("Green"), d);
	table->dual_add(green_w, d);

	w_slider *blue_w = new w_percentage_slider(16, 0);
	SliderSelectorWidget blueWidget(blue_w);
	ColorComponentPref bluePref(player_preferences->Crosshairs.Color.blue);
	crosshair_binders->insert<int> (&blueWidget, &bluePref);
	table->dual_add(blue_w->adding_label("Blue"), d);
	table->dual_add(blue_w, d);

	table->add_row(new w_spacer(), true);
	table->dual_add_row(new w_static_text("OpenGL Only (no preview)"), d);

	w_slider *opacity_w = new w_percentage_slider(16, 0);
	SliderSelectorWidget opacityWidget(opacity_w);
	OpacityPref opacityPref(player_preferences->Crosshairs.Opacity);
	crosshair_binders->insert<int> (&opacityWidget, &opacityPref);
	table->dual_add(opacity_w->adding_label("Opacity"), d);
	table->dual_add(opacity_w, d);

	placer->add(table, true);
	placer->add(new w_spacer, true);

	horizontal_placer *button_placer = new horizontal_placer;
	w_button *w_accept = new w_button("ACCEPT", dialog_ok, &d);
	button_placer->dual_add(w_accept, d);
	w_button *w_cancel = new w_button("CANCEL", dialog_cancel, &d);
	button_placer->dual_add(w_cancel, d);
	placer->add(button_placer, true);

	d.set_widget_placer(placer);
	d.set_processing_function(update_crosshair_display());

	crosshair_binders->migrate_all_second_to_first();

	clear_screen();

	if (d.run() == 0) // Accepted
	{
		crosshair_binders->migrate_all_first_to_second();
		player_preferences->Crosshairs.PreCalced = false;
		write_preferences();
	}
	else
	{
		player_preferences->Crosshairs = OldCrosshairs;
	}

	crosshair_binders.reset(0);
}

/*
 *  Player dialog
 */

enum {
	NAME_W
};


static const strings_t solo_profile_labels = {
	"Aleph One Fixes",
	"Classic Marathon 2",
	"Classic Marathon Infinity",
};

static void player_dialog(void *arg)
{
	// Create dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add(new w_title("PLAYER SETTINGS"), d);
	placer->add(new w_spacer());

	table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	table->col_flags(0, placeable::kAlignRight);
	table->col_flags(1, placeable::kAlignLeft);

	w_select *level_w = new w_select(player_preferences->difficulty_level, get_strings_for_resource(kDifficultyLevelsStringSetID));
	table->dual_add(level_w->adding_label("Difficulty"), d);
	table->dual_add(level_w, d);

	w_select* solo_profile_w;
	if (Scenario::instance()->AllowsClassicGameplay())
	{
		table->add_row(new w_spacer(), true);

		auto profile = player_preferences->solo_profile;
		if (profile >= 1) --profile;
		
		solo_profile_w = new w_select(profile, solo_profile_labels);
		table->dual_add(solo_profile_w->adding_label("Solo Gameplay"), d);
		table->dual_add(solo_profile_w, d);

		table->dual_add_row(new w_static_text("Note: net games always use Aleph One fixes"), d);
	}
	
	table->add_row(new w_spacer(), true);

	table->dual_add_row(new w_static_text("Appearance"), d);

	w_text_entry *name_w = new w_text_entry(PREFERENCES_NAME_LENGTH, player_preferences->name);
	name_w->set_identifier(NAME_W);
	name_w->set_enter_pressed_callback(dialog_try_ok);
	name_w->set_value_changed_callback(dialog_disable_ok_if_empty);
	table->dual_add(name_w->adding_label("Name"), d);
	table->dual_add(name_w, d);

	w_select* pcolor_w = new w_select(player_preferences->color, get_strings_for_resource(kTeamColorsStringSetID));
	table->dual_add(pcolor_w->adding_label("Color"), d);
	table->dual_add(pcolor_w, d);

	w_select* tcolor_w = new w_select(player_preferences->team, get_strings_for_resource(kTeamColorsStringSetID));
	table->dual_add(tcolor_w->adding_label("Team"), d);
	table->dual_add(tcolor_w, d);

	table->add_row(new w_spacer(), true);

	w_toggle *crosshairs_active_w = new w_toggle(player_preferences->crosshairs_active);
	table->dual_add(crosshairs_active_w->adding_label("Show crosshairs"), d);
	table->dual_add(crosshairs_active_w, d);

	placer->add(table, true);

	placer->add(new w_spacer(), true);

	w_button *crosshair_button = new w_button("CROSSHAIR SETTINGS", crosshair_dialog, &d);
	placer->dual_add(crosshair_button, d);

	placer->add(new w_spacer(), true);

	horizontal_placer *button_placer = new horizontal_placer;
	
	w_button* ok_button = new w_button("ACCEPT", dialog_ok, &d);
	ok_button->set_identifier(iOK);
	button_placer->dual_add(ok_button, d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

	placer->add(button_placer, true);

	d.set_widget_placer(placer);

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		const std::string name = name_w->get_text();
		if (name == player_preferences->name)
        {
			player_preferences->name = name;
			changed = true;
		}

		int16 level = static_cast<int16>(level_w->get_selection());
		assert_fail(level >= 0, "");
		if (level != player_preferences->difficulty_level) {
			player_preferences->difficulty_level = level;
			changed = true;
		}

		if (Scenario::instance()->AllowsClassicGameplay())
		{
			auto profile = solo_profile_w->get_selection();
			if (profile >= 1) ++profile;

			if (profile != player_preferences->solo_profile)
			{
				player_preferences->solo_profile = profile;
				changed = true;
			}
		}

		int16 color = static_cast<int16>(pcolor_w->get_selection());
		assert_fail(color >= 0, "");
		if (color != player_preferences->color) {
			player_preferences->color = color;
			changed = true;
		}

		int16 team = static_cast<int16>(tcolor_w->get_selection());
		assert_fail(team >= 0, "");
		if (team != player_preferences->team) {
			player_preferences->team = team;
			changed = true;
		}
		
		bool crosshair = crosshairs_active_w->get_selection();
		if (crosshair != player_preferences->crosshairs_active) {
			player_preferences->crosshairs_active = crosshair;
			changed = true;
		}

		if (changed)
			write_preferences();
	}
}

/*
 *  Online (lhowon.org) dialog
 */

const int iONLINE_USERNAME_W = 10;
const int iONLINE_PASSWORD_W = 11;
const int iSIGNUP_EMAIL_W = 20;
const int iSIGNUP_USERNAME_W = 21;
const int iSIGNUP_PASSWORD_W = 22;

static void proc_account_link(void *arg)
{
	dialog *d = static_cast<dialog *>(arg);
	
	HTTPClient conn;
	HTTPClient::parameter_map params;
	w_text_entry *username_w = static_cast<w_text_entry *>(d->get_widget_by_id(iONLINE_USERNAME_W));
	w_text_entry *password_w = static_cast<w_text_entry *>(d->get_widget_by_id(iONLINE_PASSWORD_W));
	
	params["username"] = username_w->get_text();
	params["password"] = password_w->get_text();
	params["salt"] = "";
	
	std::string url = A1_METASERVER_SETTINGS_URL;
	if (conn.Post(A1_METASERVER_LOGIN_URL, params))
	{
		std::string token = boost::algorithm::hex(conn.Response());
		url += "?token=" + token;
	}
	
	set_full_screen_enabled(false);
	open_url_in_browser(url);
	d->draw_all_widgets();
}

static void signup_dialog_ok(void *arg)
{
	dialog *d = static_cast<dialog *>(arg);
	w_text_entry *email_w = static_cast<w_text_entry *>(d->get_widget_by_id(iSIGNUP_EMAIL_W));
	w_text_entry *login_w = static_cast<w_text_entry *>(d->get_widget_by_id(iSIGNUP_USERNAME_W));
	w_password_entry *password_w = static_cast<w_password_entry *>(d->get_widget_by_id(iSIGNUP_PASSWORD_W));
	
	// check that fields are filled out
    if (email_w->get_text().empty())
	{
		notify_user(0, "Please enter your email address.");
	}
    else if (login_w->get_text().empty())
	{
		notify_user(0, "Please enter a username.");
	}
	else if (password_w->get_text().empty())
	{
		notify_user(0, "Please enter a password.");
	}
	else
	{
		// send parameters to server
		HTTPClient conn;
		HTTPClient::parameter_map params;
		params["email"] = email_w->get_text();
		params["username"] = login_w->get_text();
		params["password"] = password_w->get_text();
		
		if (conn.Post(A1_METASERVER_SIGNUP_URL, params))
		{
			if (conn.Response() == "OK")
			{
				// account was created successfully, save username and password
				network_preferences->metaserver_login = login_w->get_text();
                network_preferences->metaserver_password = password_w->get_text();
				write_preferences();
				d->quit(0);
			}
			else
			{
				notify_user(0, conn.Response());
			}
		}
		else
		{
			notify_user(0, "There was a problem contacting the server.");
		}
	}
}

static void signup_dialog(void *arg)
{
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add(new w_title("ACCOUNT SIGN UP"), d);
	placer->add(new w_spacer());
	
	table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	table->col_flags(0, placeable::kAlignRight);
	table->col_flags(1, placeable::kAlignLeft);
	
	w_text_entry *email_w = new w_text_entry(256, "");
	email_w->set_identifier(iSIGNUP_EMAIL_W);
	table->dual_add(email_w->adding_label("Email Address"), d);
	table->dual_add(email_w, d);
	
	w_text_entry *login_w = new w_text_entry(network_preferences_data::kMetaserverLoginLength, network_preferences->metaserver_login);
	login_w->set_identifier(iSIGNUP_USERNAME_W);
	table->dual_add(login_w->adding_label("Username"), d);
	table->dual_add(login_w, d);
	
	w_password_entry *password_w = new w_password_entry(network_preferences_data::kMetaserverLoginLength, network_preferences->metaserver_password);
	password_w->set_identifier(iSIGNUP_PASSWORD_W);
	table->dual_add(password_w->adding_label("Password"), d);
	table->dual_add(password_w, d);
	
	table->add_row(new w_spacer(), true);
	placer->add(table, true);
	
	horizontal_placer *button_placer = new horizontal_placer;
	
	w_button* ok_button = new w_button("SIGN UP", signup_dialog_ok, &d);
	ok_button->set_identifier(iOK);
	button_placer->dual_add(ok_button, d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
	
	placer->add(button_placer, true);
	
	d.set_widget_placer(placer);
	
	clear_screen();
	
	if (d.run() == 0)
	{
		// account was successfully created, update parent fields with new account info
		dialog *parent = static_cast<dialog *>(arg);
		w_text_entry *login_w = static_cast<w_text_entry *>(parent->get_widget_by_id(iONLINE_USERNAME_W));
		login_w->set_text(network_preferences->metaserver_login);
		w_password_entry *password_w = static_cast<w_password_entry *>(parent->get_widget_by_id(iONLINE_PASSWORD_W));
		password_w->set_text(network_preferences->metaserver_password);
	}
}

static void online_dialog(void *arg)
{
	// Create dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add(new w_title("INTERNET GAME SETUP"), d);
	placer->add(new w_spacer());
	
	tab_placer* tabs = new tab_placer();
	
	std::vector<std::string> labels;
	labels.push_back("ACCOUNT");
	labels.push_back("PREGAME LOBBY");
	labels.push_back("STATS");
	w_tab *tab_w = new w_tab(labels, tabs);
	
	placer->dual_add(tab_w, d);
	placer->add(new w_spacer(), true);
	
	vertical_placer *account = new vertical_placer();
	table_placer *account_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	account_table->col_flags(0, placeable::kAlignRight);
	account_table->col_flags(1, placeable::kAlignLeft);
	
	w_text_entry *login_w = new w_text_entry(network_preferences_data::kMetaserverLoginLength, network_preferences->metaserver_login);
	login_w->set_identifier(iONLINE_USERNAME_W);
	account_table->dual_add(login_w->adding_label("Username"), d);
	account_table->dual_add(login_w, d);
	
	w_password_entry *password_w = new w_password_entry(network_preferences_data::kMetaserverLoginLength, network_preferences->metaserver_password);
	password_w->set_identifier(iONLINE_PASSWORD_W);
	account_table->dual_add(password_w->adding_label("Password"), d);
	account_table->dual_add(password_w, d);
	
	w_hyperlink *account_link_w = new w_hyperlink("", "Visit my lhowon.org account page");
	account_link_w->set_callback(proc_account_link, &d);
	account_table->dual_add_row(account_link_w, d);
	
	account_table->add_row(new w_spacer(), true);
	
	w_button *signup_button = new w_button("SIGN UP", signup_dialog, &d);
	account_table->dual_add_row(signup_button, d);
	
	account_table->add_row(new w_spacer(), true);
	
	account->add(account_table, true);
	
	vertical_placer *lobby = new vertical_placer();
	table_placer *lobby_table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	lobby_table->col_flags(0, placeable::kAlignRight);
	lobby_table->col_flags(1, placeable::kAlignLeft);
	
	w_text_entry *name_w = new w_text_entry(PREFERENCES_NAME_LENGTH, player_preferences->name);
	name_w->set_identifier(NAME_W);
	name_w->set_enter_pressed_callback(dialog_try_ok);
	name_w->set_value_changed_callback(dialog_disable_ok_if_empty);
	lobby_table->dual_add(name_w->adding_label("Name"), d);
	lobby_table->dual_add(name_w, d);
	
	w_enabling_toggle *custom_colors_w = new w_enabling_toggle(network_preferences->use_custom_metaserver_colors);
	lobby_table->dual_add(custom_colors_w->adding_label("Custom Chat Colors"), d);
	lobby_table->dual_add(custom_colors_w, d);
	
	w_color_picker *primary_w = new w_color_picker(network_preferences->metaserver_colors[0]);
	lobby_table->dual_add(primary_w->adding_label("Primary"), d);
	lobby_table->dual_add(primary_w, d);
	
	w_color_picker *secondary_w = new w_color_picker(network_preferences->metaserver_colors[1]);
	lobby_table->dual_add(secondary_w->adding_label("Secondary"), d);
	lobby_table->dual_add(secondary_w, d);
	
	custom_colors_w->add_dependent_widget(primary_w);
	custom_colors_w->add_dependent_widget(secondary_w);

	w_toggle *mute_guests_w = new w_toggle(network_preferences->mute_metaserver_guests);
	lobby_table->dual_add(mute_guests_w->adding_label("Mute All Guest Chat"), d);
	lobby_table->dual_add(mute_guests_w, d);

	lobby_table->add_row(new w_spacer(), true);
	
	w_toggle *join_meta_w = new w_toggle(network_preferences->join_metaserver_by_default);
	lobby_table->dual_add(join_meta_w->adding_label("Join Pregame Lobby by Default"), d);
	lobby_table->dual_add(join_meta_w, d);
	
	lobby_table->add_row(new w_spacer(), true);
	
	lobby->add(lobby_table, true);
	
	vertical_placer *stats = new vertical_placer();
	stats->dual_add(new w_hyperlink(A1_LEADERBOARD_URL, "Visit the leaderboards"), d);
	stats->add(new w_spacer(), true);
	
	horizontal_placer *stats_box = new horizontal_placer();
	
	w_toggle *allow_stats_w = new w_toggle(network_preferences->allow_stats);
	stats_box->dual_add(allow_stats_w, d);
	stats_box->dual_add(allow_stats_w->adding_label("Send Stats to Lhowon.org"), d);
	
	stats->add(stats_box, true);
	stats->add(new w_spacer(), true);
	
	stats->dual_add(new w_static_text("To compete on the leaderboards,"), d);
	stats->dual_add(new w_static_text("you need an online account, and a"), d);
	stats->dual_add(new w_static_text("Stats plugin installed and enabled."), d);
	
	stats->add(new w_spacer(), true);
	stats->dual_add(new w_button("PLUGINS", plugins_dialog, &d), d);
	
	stats->add(new w_spacer(), true);
	
	tabs->add(account, true);
	tabs->add(lobby, true);
	tabs->add(stats, true);
	
	placer->add(tabs, true);
	placer->add(new w_spacer(), true);

	horizontal_placer *button_placer = new horizontal_placer;
	
	w_button* ok_button = new w_button("ACCEPT", dialog_ok, &d);
	ok_button->set_identifier(iOK);
	button_placer->dual_add(ok_button, d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
	
	placer->add(button_placer, true);
	
	d.set_widget_placer(placer);
	
	// Clear screen
	clear_screen();
	
	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;
		
		const std::string name = name_w->get_text();
		if (name != player_preferences->name)
        {
			player_preferences->name = name;
			changed = true;
		}
		
		const std::string metaserver_login = login_w->get_text();
		if (metaserver_login != network_preferences->metaserver_login)
        {
			network_preferences->metaserver_login = metaserver_login;
			changed = true;
		}
		
		// clear password if login has been cleared
        if (metaserver_login.empty() && !network_preferences->metaserver_password.empty())
        {
            network_preferences->metaserver_password.clear();
            changed = true;
		}
        else
        {
			const std::string metaserver_password = password_w->get_text();
			if (metaserver_password != network_preferences->metaserver_password)
            {
                network_preferences->metaserver_password = metaserver_password;
				changed = true;
			}
		}
		
		bool use_custom_metaserver_colors = custom_colors_w->get_selection();
		if (use_custom_metaserver_colors != network_preferences->use_custom_metaserver_colors)
		{
			network_preferences->use_custom_metaserver_colors = use_custom_metaserver_colors;
			changed = true;
		}
		
		if (use_custom_metaserver_colors)
		{
            SDL_Color primary_color = primary_w->get_selection();
			if (primary_color != network_preferences->metaserver_colors[0])
			{
				network_preferences->metaserver_colors[0] = primary_color;
				changed = true;
			}
			
            SDL_Color secondary_color = secondary_w->get_selection();
			if (secondary_color != network_preferences->metaserver_colors[1])
            {
				network_preferences->metaserver_colors[1] = secondary_color;
				changed = true;
			}
			
		}
		
		bool mute_metaserver_guests = mute_guests_w->get_selection() == 1;
		if (mute_metaserver_guests != network_preferences->mute_metaserver_guests)
		{
			network_preferences->mute_metaserver_guests = mute_metaserver_guests;
			changed = true;
		}
		
		bool join_meta = join_meta_w->get_selection() == 1;
		if (join_meta != network_preferences->join_metaserver_by_default)
		{
			network_preferences->join_metaserver_by_default = join_meta;
			changed = true;
		}
		
		bool allow_stats = allow_stats_w->get_selection() == 1;
		if (allow_stats != network_preferences->allow_stats)
		{
			network_preferences->allow_stats = allow_stats;
			Plugins::instance()->invalidate();
			changed = true;
		}
		
		
		if (changed)
			write_preferences();
	}
}

/*
 *  Handle graphics dialog
 */

#ifdef TRUE_COLOR_ONLY
static const strings_t depth_labels = {"16 Bit", "32 Bit"};
#else
static const strings_t depth_labels = {"8 Bit", "16 Bit", "32 Bit"};
#endif

static const strings_t resolution_labels = {"Low", "High"};

static const strings_t sw_alpha_blending_labels = {"Off", "Fast", "Nice"};

static const strings_t sw_sdl_driver_labels = {"Default", "None", "Direct3D", "OpenGL"};

static const strings_t ephemera_quality_labels = {"Off", "Low", "Medium", "High", "Ultra"};

static const strings_t fps_target_labels = {"30", "60 (interpolated)", "120 (interpolated)", "Unlimited (interpolated)"};

static const std::array<int16_t, 4> fps_target_values = {30, 60, 120, 0};

static const strings_t gamma_labels = {"Darkest", "Darker", "Dark", "Normal", "Light", "Really Light", "Even Lighter", "Lightest"};

static const strings_t renderer_labels = {"Software", "OpenGL"};

static const strings_t bobbing_view_labels = {"None", "Default", "Weapon Only"};

static const strings_t hud_scale_labels = {"Normal", "Double", "Largest"};

static const strings_t term_scale_labels = {"Normal", "Double", "Largest"};

#ifdef HAVE_STEAM
static const strings_t max_saves_labels = {"20", "100", "500"};

static const std::array<uint32_t, 3> max_saves_values = {20, 100, 500};
#else
static const strings_t max_saves_labels = {"20", "100", "500", "Unlimited"};

static const std::array<uint32_t, 4> max_saves_values = {20, 100, 500, 0};
#endif


static const std::unordered_map<ChannelType, int> mapping_channel_index = {
	{ChannelType::_mono, 0},
	{ChannelType::_stereo, 1},
	{ChannelType::_quad, 2},
	{ChannelType::_5_1, 3},
	{ChannelType::_6_1, 4},
	{ChannelType::_7_1, 5}
};

static const std::unordered_map<int, ChannelType> mapping_index_channel = {
	{0, ChannelType::_mono},
	{1, ChannelType::_stereo},
	{2, ChannelType::_quad},
	{3, ChannelType::_5_1},
	{4, ChannelType::_6_1},
	{5, ChannelType::_7_1}
};


enum {
    iRENDERING_SYSTEM = 1000
};


static void software_rendering_options_dialog(void* arg)
{
	// Create dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add(new w_title("SOFTWARE RENDERING OPTIONS"), d);
	placer->add(new w_spacer(), true);

	table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	table->col_flags(0, placeable::kAlignRight);

#ifdef TRUE_COLOR_ONLY
	w_select *depth_w = new w_select(graphics_preferences->screen_mode.bit_depth == 16 ? 0 : 1, depth_labels);
#else
	w_select *depth_w = new w_select(graphics_preferences->screen_mode.bit_depth == 8 ? 0 : graphics_preferences->screen_mode.bit_depth == 16 ? 1 : 2, depth_labels);
#endif
	table->dual_add(depth_w->adding_label("Color Depth"), d);
	table->dual_add(depth_w, d);

	w_toggle *resolution_w = new w_toggle(graphics_preferences->screen_mode.high_resolution, resolution_labels);
	table->dual_add(resolution_w->adding_label("Resolution"), d);
	table->dual_add(resolution_w, d);

	table->add_row(new w_spacer(), true);

	w_select *sw_alpha_blending_w = new w_select(graphics_preferences->software_alpha_blending, sw_alpha_blending_labels);
	table->dual_add(sw_alpha_blending_w->adding_label("Transparent Liquids"), d);
	table->dual_add(sw_alpha_blending_w, d);

	w_select* ephemera_quality_w = new w_select(graphics_preferences->ephemera_quality, ephemera_quality_labels);
	table->dual_add(ephemera_quality_w->adding_label("Scripted Effects Quality"), d);
	table->dual_add(ephemera_quality_w, d);

	w_select *sw_driver_w = new w_select(graphics_preferences->software_sdl_driver, sw_sdl_driver_labels);
	table->dual_add(sw_driver_w->adding_label("Acceleration"), d);
	table->dual_add(sw_driver_w, d);

	placer->add(table, true);

	placer->add(new w_spacer(), true);
	horizontal_placer *button_placer = new horizontal_placer;
	button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
	placer->add(button_placer, true);

	d.set_widget_placer(placer);
	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

#ifdef TRUE_COLOR_ONLY
		int depth = (depth_w->get_selection() == 0 ? 16 : 32);
#else
		int depth = (depth_w->get_selection() == 0 ? 8 : depth_w->get_selection() == 1 ? 16 : 32);
#endif
		if (depth != graphics_preferences->screen_mode.bit_depth) {
			graphics_preferences->screen_mode.bit_depth = depth;
			changed = true;
			// don't change mode now; it will be changed when the game starts
		}

		bool hi_res = resolution_w->get_selection() != 0;
		if (hi_res != graphics_preferences->screen_mode.high_resolution) {
			graphics_preferences->screen_mode.high_resolution = hi_res;
			changed = true;
		}

		if (sw_alpha_blending_w->get_selection() != graphics_preferences->software_alpha_blending)
		{
			graphics_preferences->software_alpha_blending = sw_alpha_blending_w->get_selection();
			changed = true;
		}

		if (sw_driver_w->get_selection() != graphics_preferences->software_sdl_driver)
		{
			graphics_preferences->software_sdl_driver = sw_driver_w->get_selection();
			changed = true;
		}

		if (ephemera_quality_w->get_selection() != graphics_preferences->ephemera_quality)
		{
			graphics_preferences->ephemera_quality = ephemera_quality_w->get_selection();
			changed = true;
		}
		
		if (changed)
			write_preferences();
	}
}


// ZZZ addition: bounce to correct renderer-config box based on selected rendering system.
static void rendering_options_dialog_demux(void* arg)
{
	int acceleration = get_selection_control_value((dialog*) arg, iRENDERING_SYSTEM) - 1;
    if (acceleration)
    {
        OpenGLDialog::Create(acceleration)->OpenGLPrefsByRunning();
    }
    else
    {
        software_rendering_options_dialog(arg);
    }
}


std::vector<std::string> build_resolution_labels()
{
	std::vector<std::string> result;
	bool first_mode = true;
	for (std::vector<std::pair<int, int> >::const_iterator it = Screen::instance()->GetModes().begin(); it != Screen::instance()->GetModes().end(); ++it)
	{
		std::ostringstream os;
		os << it->first << "x" << it->second;
		if (first_mode)
		{
			result.push_back("Automatic");
			first_mode = false;
		}
		result.push_back(os.str());
	}

	return result;
}

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

extern float View_FOV_Normal();

extern bool shapes_file_is_m1();
extern void ResetAllMMLValues();

static void graphics_dialog(void *arg)
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
	
	w_select* renderer_w = new w_select(graphics_preferences->screen_mode.acceleration, renderer_labels);
	renderer_w->set_identifier(iRENDERING_SYSTEM);
#ifndef HAVE_OPENGL
	renderer_w->set_selection(false);
	renderer_w->set_enabled(false);
#endif
	table->dual_add(renderer_w->adding_label("Rendering System"), d);
	table->dual_add(renderer_w, d);

	table->add_row(new w_spacer(), true);

	w_select_popup *size_w = new w_select_popup();
	size_w->set_labels(build_resolution_labels());
	if (graphics_preferences->screen_mode.auto_resolution)
		size_w->set_selection(0);
	else
		size_w->set_selection(Screen::instance()->FindMode(graphics_preferences->screen_mode.width, graphics_preferences->screen_mode.height) + 1);
	table->dual_add(size_w->adding_label("Screen Size"), d);
	table->dual_add(size_w, d);
		
	w_toggle *fullscreen_w = new w_toggle(!graphics_preferences->screen_mode.fullscreen);
	table->dual_add(fullscreen_w->adding_label("Windowed Mode"), d);
	table->dual_add(fullscreen_w, d);

	w_toggle *high_dpi_w = NULL;
	high_dpi_w = new w_toggle(graphics_preferences->screen_mode.high_dpi);
#ifdef __MACOSX__
	// SDL's DPI support only enabled on macOS
	table->dual_add(high_dpi_w->adding_label("Use High DPI"), d);
	table->dual_add(high_dpi_w, d);
#endif

	w_select_popup *gamma_w = new w_select_popup();
	gamma_w->set_labels(gamma_labels);
	gamma_w->set_selection(graphics_preferences->screen_mode.gamma_level);
	table->dual_add(gamma_w->adding_label("Brightness"), d);
	table->dual_add(gamma_w, d);

	w_select* fps_target_w = new w_select(0, fps_target_labels);
    for (auto i = 0; i < fps_target_labels.size(); ++i)
	{
		if (fps_target_values[i] == graphics_preferences->fps_target)
		{
			fps_target_w->set_selection(i);
		}
	}
	table->dual_add(fps_target_w->adding_label("Framerate Target"), d);
	table->dual_add(fps_target_w, d);

	table->add_row(new w_spacer(), true);
	
	w_toggle *fixh_w = new w_toggle(!graphics_preferences->screen_mode.fix_h_not_v);
	table->dual_add(fixh_w->adding_label("Limit Vertical View"), d);
	table->dual_add(fixh_w, d);

	w_toggle *override_fov_w = new w_toggle(graphics_preferences->screen_mode.fov != 0);
	w_fov_slider *fov_slider_w = new w_fov_slider((graphics_preferences->screen_mode.fov == 0 ? static_cast<int>(View_FOV_Normal()) : graphics_preferences->screen_mode.fov) - 30);
	fov_slider_w->set_enabled(graphics_preferences->screen_mode.fov != 0);
	override_fov_w->set_selection_changed_callback(
		[&](w_select*) {
			if (override_fov_w->get_selection())
			{
				fov_slider_w->set_enabled(true);
			}
			else
			{
				fov_slider_w->set_enabled(false);
			}
		});

	table->dual_add(override_fov_w->adding_label("Override FOV*"), d);
	auto fov_placer = new horizontal_placer(get_theme_space(ITEM_WIDGET));
	fov_placer->dual_add(override_fov_w, d);
	fov_placer->dual_add(fov_slider_w, d);

	table->add(fov_placer);

	table->dual_add_row(new w_static_text("*may interfere with third-party scenario effects"), d);

	table->add_row(new w_spacer(), true);

	w_select *bobbing_type_w = new w_select(0, bobbing_view_labels);
	bobbing_type_w->set_selection(static_cast<int>(graphics_preferences->screen_mode.bobbing_type));

	table->dual_add(bobbing_type_w->adding_label("View Bobbing"), d);
	table->dual_add(bobbing_type_w, d);

	table->add_row(new w_spacer(), true);
	table->dual_add_row(new w_static_text("Heads-Up Display"), d);

	w_enabling_toggle *hud_w = new w_enabling_toggle(graphics_preferences->screen_mode.hud);
	table->dual_add(hud_w->adding_label("Show HUD"), d);
	table->dual_add(hud_w, d);
	
	std::vector<Plugin*> hud_plugins;
	auto hud_plugin_index = -1;
	for (auto& plugin : *Plugins::instance()) {
		if (!plugin.hud_lua.empty() && plugin.compatible() && plugin.allowed()) {
			hud_plugins.push_back(&plugin);
			if (plugin.enabled) {
				hud_plugin_index = hud_plugins.size() - 1;
			}
		}
	}

	std::vector<std::string> hud_plugin_labels;
	if (!shapes_file_is_m1()) {
		++hud_plugin_index;
		hud_plugin_labels.push_back("Classic (Built-in)");
	}

	for (auto hud_plugin : hud_plugins) {
		hud_plugin_labels.push_back(hud_plugin->name);
	}

	w_select_popup *hud_plugin_w = new w_select_popup();
	hud_plugin_w->set_labels(hud_plugin_labels);
	hud_plugin_w->set_selection(hud_plugin_index >= 0 ? hud_plugin_index : 0);

	table->dual_add(hud_plugin_w->adding_label("HUD Plugin"), d);
	table->dual_add(hud_plugin_w, d);
	
	w_select_popup *hud_scale_w = new w_select_popup();
	hud_scale_w->set_labels(hud_scale_labels);
	hud_scale_w->set_selection(graphics_preferences->screen_mode.hud_scale_level);
	table->dual_add(hud_scale_w->adding_label("HUD Size"), d);
	table->dual_add(hud_scale_w, d);
	hud_w->add_dependent_widget(hud_scale_w);
	
	w_select_popup *term_scale_w = new w_select_popup();
	term_scale_w->set_labels(term_scale_labels);
	term_scale_w->set_selection(graphics_preferences->screen_mode.term_scale_level);
	table->dual_add(term_scale_w->adding_label("Terminal Size"), d);
	table->dual_add(term_scale_w, d);
	
	w_toggle *map_w = new w_toggle(graphics_preferences->screen_mode.translucent_map);
	table->dual_add(map_w->adding_label("Overlay Map"), d);
	table->dual_add(map_w, d);

	placer->add(table, true);

	placer->add(new w_spacer(), true);
	placer->dual_add(new w_button("RENDERING OPTIONS", rendering_options_dialog_demux, &d), d);
	placer->add(new w_spacer(), true);

#ifndef HAVE_OPENGL
	expand_string_vars(temporary, "This copy of $appName$ was built without OpenGL support.");
	placer->dual_add(new w_static_text(temporary), d);
#endif
	placer->add(new w_spacer(), true);

	horizontal_placer *button_placer = new horizontal_placer;
	button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

	placer->add(button_placer, true);
    
	d.set_widget_placer(placer);
	
	// Clear screen
	clear_screen();
    
    // Run dialog
    if (d.run() == 0) {	// Accepted
	    bool changed = false;
	    
	    bool fullscreen = fullscreen_w->get_selection() == 0;
	    if (fullscreen != graphics_preferences->screen_mode.fullscreen) {
		    graphics_preferences->screen_mode.fullscreen = fullscreen;
		    changed = true;
	    }

	    short renderer = static_cast<short>(renderer_w->get_selection());
	    assert_fail(renderer >= 0, "");
	    if(renderer != graphics_preferences->screen_mode.acceleration) {
		    graphics_preferences->screen_mode.acceleration = renderer;
		    if (renderer) graphics_preferences->screen_mode.bit_depth = 32;
		    changed = true;
	    }
	    
	    short resolution = static_cast<short>(size_w->get_selection());
		if (resolution == 0)
		{
			if (!graphics_preferences->screen_mode.auto_resolution) {
				graphics_preferences->screen_mode.auto_resolution = true;
				changed = true;
			}
		}
	    else if (Screen::instance()->ModeWidth(resolution - 1) != graphics_preferences->screen_mode.width || Screen::instance()->ModeHeight(resolution - 1) != graphics_preferences->screen_mode.height || graphics_preferences->screen_mode.auto_resolution)
	    {
		    graphics_preferences->screen_mode.width = Screen::instance()->ModeWidth(resolution - 1);
		    graphics_preferences->screen_mode.height = Screen::instance()->ModeHeight(resolution - 1);
			graphics_preferences->screen_mode.auto_resolution = false;
		    changed = true;
	    }
	    
		bool high_dpi = high_dpi_w->get_selection() != 0;
		if (high_dpi != graphics_preferences->screen_mode.high_dpi) {
			graphics_preferences->screen_mode.high_dpi = high_dpi;
			changed = true;
		}
		
	    short gamma = static_cast<short>(gamma_w->get_selection());
	    if (gamma != graphics_preferences->screen_mode.gamma_level) {
		    graphics_preferences->screen_mode.gamma_level = gamma;
		    changed = true;
	    }

		auto fps_target = fps_target_values[fps_target_w->get_selection()];
		if (fps_target != graphics_preferences->fps_target)
		{
			graphics_preferences->fps_target = fps_target;
			changed = true;
		}
		
        bool fix_h_not_v = fixh_w->get_selection() == 0;
        if (fix_h_not_v != graphics_preferences->screen_mode.fix_h_not_v) {
            graphics_preferences->screen_mode.fix_h_not_v = fix_h_not_v;
            changed = true;
        }

	    bool hud = hud_w->get_selection() != 0;
	    if (hud != graphics_preferences->screen_mode.hud)
	    {
		    graphics_preferences->screen_mode.hud = hud;
		    changed = true;
	    }

		auto hud_plugin = static_cast<int>(hud_plugin_w->get_selection());
		if (hud_plugin != hud_plugin_index) {
			if (!shapes_file_is_m1()) {
				--hud_plugin;
			}

			for (auto i = 0; i < hud_plugins.size(); ++i) {
				hud_plugins[i]->enabled = i == hud_plugin;
			}
			
			changed = true;
		}
	    
	    short hud_scale = static_cast<short>(hud_scale_w->get_selection());
	    if (hud_scale != graphics_preferences->screen_mode.hud_scale_level)
	    {
		    graphics_preferences->screen_mode.hud_scale_level = hud_scale;
		    changed = true;
	    }
	    
	    short term_scale = static_cast<short>(term_scale_w->get_selection());
	    if (term_scale != graphics_preferences->screen_mode.term_scale_level)
	    {
		    graphics_preferences->screen_mode.term_scale_level = term_scale;
		    changed = true;
	    }
		
		bool translucent_map = map_w->get_selection() != 0;
		if (translucent_map != graphics_preferences->screen_mode.translucent_map) {
			graphics_preferences->screen_mode.translucent_map = translucent_map;
			changed = true;
		}

		auto bobbing_type = static_cast<BobbingType>(bobbing_type_w->get_selection());
		if (bobbing_type != graphics_preferences->screen_mode.bobbing_type) {
			graphics_preferences->screen_mode.bobbing_type = bobbing_type;
			changed = true;
		}

		int fov = override_fov_w->get_selection() == 0 ? 0 : fov_slider_w->get_selection() + 30;
		if (fov != graphics_preferences->screen_mode.fov)
		{
			graphics_preferences->screen_mode.fov = fov;
			changed = true;
		}
		
	    if (changed) {
			Plugins::instance()->invalidate();
		    write_preferences();

			ResetAllMMLValues();
			LoadBaseMMLScripts(true);
			Plugins::instance()->load_mml(true);

		    change_screen_mode(&graphics_preferences->screen_mode, true);
		    clear_screen();
		    parent->layout();
		    parent->draw_all_widgets();		// DirectX seems to need this
	    }
    }
}

/*
 *  Sound dialog
 */

class w_volume_slider : public w_percentage_slider {
public:
	w_volume_slider(int vol) : w_percentage_slider(21, vol) {}
	~w_volume_slider() {}

	void item_selected(void)
	{
		if (SoundManager::instance()->IsActive())
		{
			OpenALManager::Get()->SetMasterVolume(SoundManager::From_db((selection - 20) * 2));
			SoundManager::instance()->PlaySound(_snd_adjust_volume, 0, NONE);
		}
	}
};

class w_music_slider : public w_slider {
public:
	w_music_slider(int sel) : w_slider(41, sel) {
		init_formatted_value();
	}

	void item_selected(void)
	{
		if (OpenALManager::Get())
		{
			OpenALManager::Get()->SetMusicVolume(SoundManager::From_db((selection - 20), true));
		}
	}

	virtual std::string formatted_value() {
		std::ostringstream ss;
		ss << (selection * 200 / (num_items - 1)) << "%";
		return ss.str();
	}
};


static const strings_t quality_labels = {"8-bit Slot", "16-bit Slot"};


static void sound_dialog(void *arg)
{
	// Create dialog
	dialog d;
	vertical_placer *placer = new vertical_placer;
	placer->dual_add(new w_title("SOUND SETUP"), d);
	placer->add(new w_spacer(), true);

	table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	table->col_flags(0, placeable::kAlignRight);

	static const std::vector<std::string> channel_labels = { "Mono", "Stereo", "Quad", "5.1", "6.1", "7.1" };
	w_select_popup* channel_w = new w_select_popup();
	channel_w->set_labels(channel_labels);
	channel_w->set_selection(mapping_channel_index.at(sound_preferences->channel_type));
	table->dual_add(channel_w->adding_label("Channels"), d);
	table->dual_add(channel_w, d);

	w_toggle* dynamic_w = new w_toggle(TEST_FLAG(sound_preferences->flags, _dynamic_tracking_flag));
	table->dual_add(dynamic_w->adding_label("Active Panning"), d);
	table->dual_add(dynamic_w, d);

	bool is_3d_sounds_enabled = sound_preferences->flags & _3d_sounds_flag;
	w_toggle *sounds3d_w = new w_toggle(is_3d_sounds_enabled);
	table->dual_add(sounds3d_w->adding_label("3D Sounds"), d);
	table->dual_add(sounds3d_w, d);

	w_toggle *hrtf_w = new w_toggle((OpenALManager::Get() && OpenALManager::Get()->IsHrtfEnabled()) || sound_preferences->flags & _hrtf_flag);
	table->dual_add(hrtf_w->adding_label("HRTF (Headphones)"), d);
	table->dual_add(hrtf_w, d);
	hrtf_w->set_enabled(OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() != OpenALManager::HrtfSupport::Unsupported && is_3d_sounds_enabled && sound_preferences->channel_type == ChannelType::_stereo);

	auto hrtf_enable_callback = [&](void*) {
		bool can_enable_hrtf = sounds3d_w->get_selection() == 1
			&& mapping_index_channel.at(channel_w->get_selection()) == ChannelType::_stereo
			&& OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() == OpenALManager::HrtfSupport::Supported;

		hrtf_w->set_enabled(can_enable_hrtf);
		hrtf_w->set_selection(
			(can_enable_hrtf && hrtf_w->get_selection() == 1) ||
			!can_enable_hrtf && OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() == OpenALManager::HrtfSupport::Required
		);
	};

	sounds3d_w->set_selection_changed_callback(hrtf_enable_callback);
	channel_w->set_popup_callback(hrtf_enable_callback, nullptr);

	table->add_row(new w_spacer(), true);

	w_volume_slider *volume_w = new w_volume_slider(static_cast<int>(sound_preferences->volume_db / 2 + 20));
	table->dual_add(volume_w->adding_label("Master Volume"), d);
	table->dual_add(volume_w, d);

	w_slider *music_volume_w = new w_music_slider(sound_preferences->music_db + 20);
	table->dual_add(music_volume_w->adding_label("Music Volume"), d);
	table->dual_add(music_volume_w, d);

	table->add_row(new w_spacer(), true);
	
	w_toggle *quality_w = new w_toggle(TEST_FLAG(sound_preferences->flags, _16bit_sound_flag), quality_labels);
	table->dual_add(quality_w->adding_label("Source"), d);
	table->dual_add(quality_w, d);

	w_toggle *ambient_w = new w_toggle(TEST_FLAG(sound_preferences->flags, _ambient_sound_flag));
	table->dual_add(ambient_w->adding_label("Ambient Sounds"), d);
	table->dual_add(ambient_w, d);

	w_toggle *more_w = new w_toggle(TEST_FLAG(sound_preferences->flags, _more_sounds_flag));
	table->dual_add(more_w->adding_label("More Sounds"), d);
	table->dual_add(more_w, d);

	table->add_row(new w_spacer(), true);
	table->dual_add_row(new w_static_text("Interface Sounds"), d);
	
	w_toggle *button_sounds_w = new w_toggle(TEST_FLAG(input_preferences->modifiers, _inputmod_use_button_sounds));
	table->dual_add(button_sounds_w->adding_label("In Game (F-key)"), d);
	table->dual_add(button_sounds_w, d);

	w_toggle *dialog_sounds_w = new w_toggle(!TEST_FLAG(sound_preferences->flags, _mute_dialogs));
	table->dual_add(dialog_sounds_w->adding_label("Dialogs"), d);
	table->dual_add(dialog_sounds_w, d);

	table->add_row(new w_spacer(), true);
	table->dual_add_row(new w_static_text("Experimental Sound Options"), d);
		w_toggle *zrd_w = new w_toggle(TEST_FLAG(sound_preferences->flags, _lower_restart_delay));
	table->dual_add(zrd_w->adding_label("Rapid-fire Sounds"), d);
	table->dual_add(zrd_w, d);

	placer->add(table, true);

	placer->add(new w_spacer(), true);

	horizontal_placer *button_placer = new horizontal_placer;
	button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
	
	placer->add(button_placer, true);

	d.set_widget_placer(placer);
	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		uint16 flags = 0;
		if (quality_w->get_selection()) flags |= _16bit_sound_flag;
		if (sounds3d_w->get_selection()) flags |= _3d_sounds_flag;
		if (hrtf_w->get_selection()) flags |= _hrtf_flag;
		if (dynamic_w->get_selection()) flags |= _dynamic_tracking_flag;
		if (ambient_w->get_selection()) flags |= _ambient_sound_flag;
		if (more_w->get_selection()) flags |= _more_sounds_flag;
		if (zrd_w->get_selection()) flags |= _lower_restart_delay;
		if (!dialog_sounds_w->get_selection()) flags |= _mute_dialogs;

		if (flags != sound_preferences->flags) {
			sound_preferences->flags = flags;
			changed = true;
		}

		flags = input_preferences->modifiers & ~_inputmod_use_button_sounds;
		if (button_sounds_w->get_selection()) flags |= _inputmod_use_button_sounds;
		if (flags != input_preferences->modifiers) {
			input_preferences->modifiers = flags;
			changed = true;
		}

		float volume_db = (volume_w->get_selection() - 20) * 2;
		if (volume_db != sound_preferences->volume_db) {
			sound_preferences->volume_db = volume_db;
			changed = true;
		}

		float music_db = music_volume_w->get_selection() - 20;
		if (music_db != sound_preferences->music_db) {
			sound_preferences->music_db = music_db;
			changed = true;
		}

		auto channel = mapping_index_channel.at(channel_w->get_selection());
		if (channel != sound_preferences->channel_type) {
			sound_preferences->channel_type = channel;
			changed = true;
		}

		if (changed) {
			auto slot = Music::instance()->GetSlot(Music::MusicSlot::Intro);
			bool is_music_playing = slot && slot->Playing();
			SoundManager::instance()->SetParameters(*sound_preferences);
			write_preferences();
			if (is_music_playing) Music::instance()->RestartIntroMusic();
		}
	}
	else if (OpenALManager::Get())
	{
		OpenALManager::Get()->SetMasterVolume(SoundManager::From_db(sound_preferences->volume_db));
		OpenALManager::Get()->SetMusicVolume(SoundManager::From_db(sound_preferences->music_db, true));
	}
}


/*
 *  Controls dialog
 */

const float kMinSensitivityLog = -7.0f;
const float kMaxSensitivityLog = 3.0f;
const float kSensitivityLogRange = kMaxSensitivityLog - kMinSensitivityLog;

class w_sens_slider : public w_slider {
public:
	w_sens_slider(int num_items, int sel) : w_slider(num_items, sel) {
		init_formatted_value();
	}
	
	virtual std::string formatted_value(void) {
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

class w_deadzone_slider : public w_slider {
public:
	w_deadzone_slider(int num_items, int sel) : w_slider(num_items, sel) {
		init_formatted_value();
	}
	
	virtual std::string formatted_value(void) {
		std::ostringstream ss;
		ss << selection << "%";
		return ss.str();
	}
};


const int NUM_KEYS = 21;

static const char *action_name[NUM_KEYS] = {
	"Move Forward", "Move Backward", "Turn Left", "Turn Right", "Sidestep Left", "Sidestep Right",
	"Glance Left", "Glance Right", "Look Up", "Look Down", "Recenter View",
	"Previous Weapon", "Next Weapon", "Trigger", "2nd Trigger",
	"Turn -> Sidestep", "Run/Swim", "Move -> Look",
	"Action", "Auto Map", "Aux Trigger"
};



struct key_binding_t
{
    SDL_Scancode keyboard;
    SDL_Scancode controller;
    // mouse?
    
    // action - this is an enum so it can be serialized in Prefs; we need a separate map/switch to convert enums to actions
    
    // would be nice to specify where it appears in Preferences' CONTROL tabs
    
    bool can_rebind; // we could put in-game F-keys in this table and specify they can't rebind (while the UI supports some common F-keys, it won't support them all, e.g. HUD size is only applicable in-game)
};


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

static const char *shell_action_name[NUMBER_OF_SHELL_KEYS] = {
	"Inventory Left", "Inventory Right", "Switch Player View", "Volume Up", "Volume Down", "Zoom Map In", "Zoom Map Out", "Toggle FPS", "Chat/Console", "Network Stats"
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

class w_prefs_key;

typedef std::multimap<int, w_prefs_key*> prefsKeyMap;
typedef std::pair<int, w_prefs_key*> prefsKeyMapPair;
static prefsKeyMap key_w;
static prefsKeyMap shell_key_w;
static prefsKeyMap hotkey_w;

class w_prefs_key : public w_key {
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

static void load_default_keys(void *arg)
{
	for (int i = 0; i < NUM_KEYS; i++) {
		SDL_Scancode kcode = SDL_SCANCODE_UNKNOWN;
		SDL_Scancode mcode = SDL_SCANCODE_UNKNOWN;
		SDL_Scancode jcode = SDL_SCANCODE_UNKNOWN;
		for (auto it = default_key_bindings[i].begin(); it != default_key_bindings[i].end(); ++it) {
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

static void unset_scancode(SDL_Scancode code)
{
	for (int i = 0; i < NUM_KEYS; ++i)
		input_preferences->key_bindings[i].erase(code);
	for (int i = 0; i < NUMBER_OF_SHELL_KEYS; ++i)
		input_preferences->shell_key_bindings[i].erase(code);
	for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
	{
		input_preferences->hotkey_bindings[i].erase(code);
	}
}

enum {
	KEYBOARD_TABS,
	TAB_KEYS,
	TAB_MORE_KEYS
};

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
	if (input_preferences->raw_mouse_input == true &&
		input_preferences->mouse_accel_type == _mouse_accel_classic &&
		input_preferences->classic_vertical_aim == true &&
		input_preferences->extra_mouse_precision == false &&
		input_preferences->classic_aim_speed_limits)
	{
		mouse_feel_w->set_selection(0);
	}
	else if (input_preferences->raw_mouse_input == true &&
			 input_preferences->mouse_accel_type == _mouse_accel_none &&
			 input_preferences->classic_vertical_aim == false &&
			 input_preferences->extra_mouse_precision == true &&
			 !input_preferences->classic_aim_speed_limits)
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
			if (true != input_preferences->raw_mouse_input) {
				input_preferences->raw_mouse_input = true;
				changed = true;
			}
			if (_mouse_accel_classic != input_preferences->mouse_accel_type) {
				input_preferences->mouse_accel_type = _mouse_accel_classic;
				changed = true;
			}
			if (true != input_preferences->classic_vertical_aim) {
				input_preferences->classic_vertical_aim = true;
				changed = true;
			}
			if (false != input_preferences->extra_mouse_precision) {
				input_preferences->extra_mouse_precision = false;
				changed = true;
			}
			if (!input_preferences->classic_aim_speed_limits)
			{
				input_preferences->classic_aim_speed_limits = true;
				changed = true;
			}
			break;
		case 1:
			if (true != input_preferences->raw_mouse_input) {
				input_preferences->raw_mouse_input = true;
				changed = true;
			}
			if (_mouse_accel_none != input_preferences->mouse_accel_type) {
				input_preferences->mouse_accel_type = _mouse_accel_none;
				changed = true;
			}
			if (false != input_preferences->classic_vertical_aim) {
				input_preferences->classic_vertical_aim = false;
				changed = true;
			}
			if (true != input_preferences->extra_mouse_precision) {
				input_preferences->extra_mouse_precision = true;
				changed = true;
			}
			if (input_preferences->classic_aim_speed_limits) {
				input_preferences->classic_aim_speed_limits = false;
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
	
	float hSensitivity = ((float) input_preferences->sens_horizontal) / FIXED_ONE;
	if (hSensitivity <= 0.0f) hSensitivity = 1.0f;
	float hSensitivityLog = std::log(hSensitivity);
	int hSliderPosition =
		(int) ((hSensitivityLog - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);
	w_sens_slider *mouse_h_sens_w = new w_sens_slider(1000, hSliderPosition);
	table->dual_add(mouse_h_sens_w->adding_label("Horizontal Sensitivity"), d);
	table->dual_add(mouse_h_sens_w, d);

	float vSensitivity = ((float) input_preferences->sens_vertical) / FIXED_ONE;
	if (vSensitivity <= 0.0f) vSensitivity = 1.0f;
	float vSensitivityLog = std::log(vSensitivity);
	int vSliderPosition =
		(int) ((vSensitivityLog - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);
	w_sens_slider *mouse_v_sens_w = new w_sens_slider(1000, vSliderPosition);
	table->dual_add(mouse_v_sens_w->adding_label("Vertical Sensitivity"), d);
	table->dual_add(mouse_v_sens_w, d);

	w_toggle *mouse_v_invert_w = new w_toggle(input_preferences->modifiers & _inputmod_invert_mouse);
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
	
	mouse_raw_w = new w_toggle(input_preferences->raw_mouse_input);
	mouse_raw_w->set_selection_changed_callback(update_mouse_feel_details);
	table->dual_add(mouse_raw_w->adding_label("Raw Input Mode"), d);
	table->dual_add(mouse_raw_w, d);
	
	mouse_accel_w = new w_toggle(input_preferences->mouse_accel_type == _mouse_accel_classic);
	mouse_accel_w->set_selection_changed_callback(update_mouse_feel_details);
	table->dual_add(mouse_accel_w->adding_label("Acceleration"), d);
	table->dual_add(mouse_accel_w, d);
	
	mouse_vertical_w = new w_toggle(input_preferences->classic_vertical_aim);
	mouse_vertical_w->set_selection_changed_callback(update_mouse_feel_details);
	table->dual_add(mouse_vertical_w->adding_label("Adjust Vertical Speed"), d);
	table->dual_add(mouse_vertical_w, d);
	
	mouse_precision_w = new w_toggle(!input_preferences->extra_mouse_precision);
	mouse_precision_w->set_selection_changed_callback(update_mouse_feel_details);
	table->dual_add(mouse_precision_w->adding_label("Snap View to Weapon Aim"), d);
	table->dual_add(mouse_precision_w, d);

	mouse_speed_limit_w = new w_toggle(input_preferences->classic_aim_speed_limits);
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
	if (d.run() == 0) {	// Accepted
		bool changed = false;
		
		int hPos = mouse_h_sens_w->get_selection();
		float hLog = kMinSensitivityLog + ((float) hPos) * (kSensitivityLogRange / 1000.0f);
		_fixed hNorm = _fixed(std::exp(hLog) * FIXED_ONE);
		if (hNorm != input_preferences->sens_horizontal) {
			input_preferences->sens_horizontal = hNorm;
			changed = true;
		}
		
		int vPos = mouse_v_sens_w->get_selection();
		float vLog = kMinSensitivityLog + ((float) vPos) * (kSensitivityLogRange / 1000.0f);
		_fixed vNorm = _fixed(std::exp(vLog) * FIXED_ONE);
		if (vNorm != input_preferences->sens_vertical) {
			input_preferences->sens_vertical = vNorm;
			changed = true;
		}

		uint16 flags = input_preferences->modifiers;
		if (mouse_v_invert_w->get_selection()) {
			flags |= _inputmod_invert_mouse;
		} else {
			flags &= ~_inputmod_invert_mouse;
		}
		if (flags != input_preferences->modifiers) {
			input_preferences->modifiers = flags;
			changed = true;
		}
		
		if (mouse_raw_w->get_selection() != input_preferences->raw_mouse_input) {
			input_preferences->raw_mouse_input = mouse_raw_w->get_selection();
			changed = true;
		}
		
		if (mouse_accel_w->get_selection() != input_preferences->mouse_accel_type) {
			input_preferences->mouse_accel_type = mouse_accel_w->get_selection();
			changed = true;
		}
		
		bool vert = mouse_vertical_w->get_selection();
		if (vert != input_preferences->classic_vertical_aim) {
			input_preferences->classic_vertical_aim = vert;
			changed = true;
		}
		
		bool precision = (mouse_precision_w->get_selection() == 0);
		if (precision != input_preferences->extra_mouse_precision) {
			input_preferences->extra_mouse_precision = precision;
			changed = true;
		}

		auto speed_limit = mouse_speed_limit_w->get_selection();
		if (speed_limit != input_preferences->classic_aim_speed_limits) {
			input_preferences->classic_aim_speed_limits = speed_limit;
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
	
	float joySensitivityX = ((float) input_preferences->controller_sensitivity_horizontal) / FIXED_ONE;
	if (joySensitivityX <= 0.0f) joySensitivityX = 1.0f;
	float joySensitivityLogX = std::log(joySensitivityX);
	int joySliderPositionX = (int) ((joySensitivityLogX - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);

	w_sens_slider* sens_joy_w_x = new w_sens_slider(1000, joySliderPositionX);
	table->dual_add(sens_joy_w_x->adding_label("Aiming Horizontal Sensitivity"), d);
	table->dual_add(sens_joy_w_x, d);
	
	int joyDeadzoneX = (int)((input_preferences->controller_deadzone_horizontal / 655.36f) + 0.5f);
	w_deadzone_slider* dead_joy_w_x = new w_deadzone_slider(11, joyDeadzoneX);
	table->dual_add(dead_joy_w_x->adding_label("Analog Horizontal Dead Zone"), d);
	table->dual_add(dead_joy_w_x, d);

	float joySensitivityY = ((float)input_preferences->controller_sensitivity_vertical) / FIXED_ONE;
	if (joySensitivityY <= 0.0f) joySensitivityY = 1.0f;
	float joySensitivityLogY = std::log(joySensitivityY);
	int joySliderPositionY = (int)((joySensitivityLogY - kMinSensitivityLog) * (1000.0f / kSensitivityLogRange) + 0.5f);

	w_sens_slider* sens_joy_w_y = new w_sens_slider(1000, joySliderPositionY);
	table->dual_add(sens_joy_w_y->adding_label("Aiming Vertical Sensitivity"), d);
	table->dual_add(sens_joy_w_y, d);

	int joyDeadzoneY = (int)((input_preferences->controller_deadzone_vertical / 655.36f) + 0.5f);
	w_deadzone_slider* dead_joy_w_y = new w_deadzone_slider(11, joyDeadzoneY);
	table->dual_add(dead_joy_w_y->adding_label("Analog Vertical Dead Zone"), d);
	table->dual_add(dead_joy_w_y, d);

	w_toggle* controller_inverted = new w_toggle(input_preferences->controller_aim_inverted);
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
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		int sensPosX = sens_joy_w_x->get_selection();
		float sensLogX = kMinSensitivityLog + ((float)sensPosX) * (kSensitivityLogRange / 1000.0f);
		_fixed sensNormX = _fixed(std::exp(sensLogX) * FIXED_ONE);
		if (sensNormX != input_preferences->controller_sensitivity_horizontal) {
			input_preferences->controller_sensitivity_horizontal = sensNormX;
			changed = true;
		}
		
		int deadPosX = dead_joy_w_x->get_selection();
		int deadNormX = deadPosX * 655.36f;
		if (deadNormX != input_preferences->controller_deadzone_horizontal) {
			input_preferences->controller_deadzone_horizontal = deadNormX;
			changed = true;
		}

		int sensPosY = sens_joy_w_y->get_selection();
		float sensLogY = kMinSensitivityLog + ((float)sensPosY) * (kSensitivityLogRange / 1000.0f);
		_fixed sensNormY = _fixed(std::exp(sensLogY) * FIXED_ONE);
		if (sensNormY != input_preferences->controller_sensitivity_vertical) {
			input_preferences->controller_sensitivity_vertical = sensNormY;
			changed = true;
		}

		int deadPosY = dead_joy_w_y->get_selection();
		int deadNormY = deadPosY * 655.36f;
		if (deadNormY != input_preferences->controller_deadzone_vertical) {
			input_preferences->controller_deadzone_vertical = deadNormY;
			changed = true;
		}

		bool inverted_controls = controller_inverted->get_selection();
		if (input_preferences->controller_aim_inverted != inverted_controls) {
			input_preferences->controller_aim_inverted = inverted_controls;
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


static void controls_dialog(void *arg)
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
		for (std::set<SDL_Scancode>::const_iterator bit = input_preferences->key_bindings[i].begin(); bit != input_preferences->key_bindings[i].end(); ++bit) {
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
		for (std::set<SDL_Scancode>::const_iterator bit = input_preferences->shell_key_bindings[i].begin(); bit != input_preferences->shell_key_bindings[i].end(); ++bit) {
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
		for (std::set<SDL_Scancode>::const_iterator bit = input_preferences->hotkey_bindings[i].begin(); bit != input_preferences->hotkey_bindings[i].end(); ++bit) {
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

	w_select *run_w = new w_select(input_preferences->modifiers & _inputmod_run_key_toggle ? 2 : input_preferences->modifiers & _inputmod_interchange_run_walk ? 1 : 0, run_option_labels);
	move_options->dual_add(run_w->adding_label("Run/Swim Behavior"), d);
	move_options->dual_add(run_w, d);

	w_select *swim_w = new w_select(input_preferences->modifiers & _inputmod_interchange_swim_sink ? 1 : 0, swim_option_labels);
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
	
	w_toggle* auto_recenter_w = new w_toggle(!(input_preferences->modifiers & _inputmod_dont_auto_recenter));
	look_options->dual_add(auto_recenter_w->adding_label("Auto-Recenter View"), d);
	look_options->dual_add(auto_recenter_w, d);
	
	look_options->add_row(new w_spacer(), true);
	
	table_placer *mouse_options = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	mouse_options->col_flags(0, placeable::kAlignRight);
	mouse_options->col_flags(1, placeable::kAlignLeft);

	w_toggle *enable_mouse_w = new w_toggle(input_preferences->input_device == _mouse_yaw_pitch);
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
	joystick_aiming_w->set_selection(input_preferences->controller_analog ? 0 : 1);
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

	w_toggle *weapon_w = new w_toggle(!(input_preferences->modifiers & _inputmod_dont_switch_to_new_weapon));
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
	clear_screen();

	enter_joystick();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;
		
		uint16 flags = input_preferences->modifiers & (_inputmod_use_button_sounds|_inputmod_invert_mouse);

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
		
		if (flags != input_preferences->modifiers) {
			input_preferences->modifiers = flags;
			changed = true;
		}

		for (int i = 0; i < NUM_KEYS; i++) {
			input_preferences->key_bindings[i].clear();
		}
		for (int i = 0; i < NUMBER_OF_SHELL_KEYS; i++) {
			input_preferences->shell_key_bindings[i].clear();
		}

		for (int i = 0; i < NUMBER_OF_HOTKEYS; ++i)
		{
			input_preferences->hotkey_bindings[i].clear();
		}

		for (auto it = key_w.begin(); it != key_w.end(); ++it) {
			int i = it->first;
			SDL_Scancode key = it->second->get_key();
			if (key != SDL_SCANCODE_UNKNOWN) {
				unset_scancode(key);
				input_preferences->key_bindings[i].insert(key);
				changed = true;
			}
		}

		for (auto it = shell_key_w.begin(); it != shell_key_w.end(); ++it) {
			int i = it->first;
			SDL_Scancode key = it->second->get_key();
			if (key != SDL_SCANCODE_UNKNOWN) {
				unset_scancode(key);
				input_preferences->shell_key_bindings[i].insert(key);
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
				input_preferences->hotkey_bindings[i].insert(key);
				changed = true;
			}
		}
		
		int16 device = enable_mouse_w->get_selection() ? _mouse_yaw_pitch : _keyboard_or_game_pad;
		if (input_preferences->input_device != device) {
			input_preferences->input_device = device;
			changed = true;
		}
		
		bool jaim = (joystick_aiming_w->get_selection() == 0);
		if (input_preferences->controller_analog != jaim) {
			input_preferences->controller_analog = jaim;
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

static void plugins_dialog(void* arg)
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


/*
 *  Environment dialog
 */

static const strings_t film_profile_labels = {"Aleph One 1.0", "Marathon 2", "Marathon Infinity"};

static void environment_dialog(void *arg)
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
	
	w_env_select *replay_net_lua_w = new w_env_select(network_preferences->netscript_file, "AVAILABLE NETSCRIPTS", _typecode_netscript, &d);
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
	clear_screen();

	// Run dialog

	if (d.run() == 0) {	// Accepted
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
		if (path != network_preferences->netscript_file)
        {
			network_preferences->netscript_file = path;
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


extern void hub_set_minimum_send_period(int32);
extern int32& hub_get_minimum_send_period();

struct set_latency_tolerance
{
	void operator() (const std::string& arg) const
    {
		hub_set_minimum_send_period(atoi(arg.c_str()));
        screen_print_f("latency tolerance is now %s", arg.c_str());
		write_preferences();
	}
};

struct get_latency_tolerance
{
	void operator() (const std::string&) const
    {
		screen_print_f("latency tolerance is %d",hub_get_minimum_send_period());
	}
};


/*
 *  Initialize preferences (load from file or setup defaults)
 */

void initialize_preferences(
	void)
{
	log_context("initializing preferences");

	// In case this function gets called more than once...
	if (!PrefsInited)
	{
		graphics_preferences= new graphics_preferences_data;
		player_preferences= new player_preferences_data;
		input_preferences= new input_preferences_data;
		sound_preferences = new SoundManager::Parameters;
		network_preferences= new network_preferences_data;
		
		for (int i = 0; i < NUM_KEYS; ++i)
			input_preferences->key_bindings[i] = std::set<SDL_Scancode>();
		for (int i = 0; i < NUMBER_OF_SHELL_KEYS; ++i)
			input_preferences->shell_key_bindings[i] = std::set<SDL_Scancode>();

		PrefsInited = true;

		CommandParser PreferenceSetCommandParser;
		PreferenceSetCommandParser.register_command("latency_tolerance", set_latency_tolerance());
		CommandParser PreferenceGetCommandParser;
		PreferenceGetCommandParser.register_command("latency_tolerance", get_latency_tolerance());

		CommandParser PreferenceCommandParser;
		PreferenceCommandParser.register_command("set", PreferenceSetCommandParser);
		PreferenceCommandParser.register_command("get", PreferenceGetCommandParser);
		Console::instance()->register_command("preferences", PreferenceCommandParser);
		
		read_preferences ();
	}
}

void read_preferences ()
{
	// Set to defaults; will be overridden by reading in the XML stuff
	default_graphics_preferences(graphics_preferences);
	default_network_preferences(network_preferences);
	default_player_preferences(player_preferences);
	default_input_preferences(input_preferences);
	*sound_preferences = SoundManager::Parameters();
    environment_preferences.reset();

	// Slurp in the file and parse it

    ao_path prefs_path = get_preferences_dir();

	std::string name = get_string(STRID(strFILENAMES, filenamePREFERENCES));
	if (shell_options.editor) { name += " Editor"; } // check for editor prefs
	prefs_path /= name;

	bool defaults = false;
    DataFile OFile;
	ao_err err = OFile.open(prefs_path);

	if (err && shell_options.editor)
	{
		// copy non-editor prefs
		prefs_path = get_preferences_dir() / get_string(STRID(strFILENAMES, filenamePREFERENCES));
		err = OFile.open(prefs_path); // TODO: these don't do anything except check the file can be opened
	}

	if (err)
    {
		defaults = true;
        prefs_path = find_file_at_subpath("Scripts/Default Preferences.xml");
		err = OFile.open(prefs_path);
	}

	// legacy default prefs // EES: presumably still needed as older scenarios will contain it; TODO: need to confirm and keep the code for reading legacy prefs files if so (yuck, but unavoidable)
	if (err)
    {
		defaults = true;
        prefs_path = find_file_at_subpath(get_string(STRID(strFILENAMES, filenamePREFERENCES)));
		err = OFile.open(prefs_path);
	}
	
	if (!err)
	{
		OFile.close();
		try {
			InfoTree prefs = InfoTree::load_xml(prefs_path);
			InfoTree root = prefs.get_child("mara_prefs");
			
			std::string version = "";
			root.read_attr("version", version);
			if (!version.length())
                log_warning_f("Reading older preferences of unknown version. Preferences will be upgraded to version %s when saved. (%s)", A1_DATE_VERSION, prefs_path.c_str());
			else if (version < A1_DATE_VERSION)
                log_warning_f("Reading older preferences of version %s. Preferences will be upgraded to version %s when saved. (%s)", version.c_str(), A1_DATE_VERSION, prefs_path.c_str());
			else if (version > A1_DATE_VERSION)
                log_warning_f("Reading newer preferences of version %s. Preferences will be downgraded to version %s when saved. (%s)", version.c_str(), A1_DATE_VERSION, prefs_path.c_str());
			
			for (const InfoTree &child : root.children_named("graphics"))
				parse_graphics_preferences(child, version);
			for (const InfoTree &child : root.children_named("player"))
				parse_player_preferences(child, version);
			for (const InfoTree &child : root.children_named("input"))
				parse_input_preferences(child, version);
			for (const InfoTree &child : root.children_named("sound"))
				parse_sound_preferences(child, version);
#if !defined(DISABLE_NETWORKING)
			for (const InfoTree &child : root.children_named("network"))
				parse_network_preferences(child, version);
#endif
			for (const InfoTree &child : root.children_named("environment"))
				parse_environment_preferences(child, version);
			
		}
        catch (const InfoTree::Exception& ex)
        {
            log_error_f("Error parsing preferences file (%s): %s", prefs_path.c_str(), ex.what());
            err = STRID(strERRORS, cantParsePreferences);
		}
	}

	if (err)
	{
        if (defaults)
        {
            notify_user(err, "There were default preferences-file parsing errors (see $appLogFile$ for details)");
        }
        else
        {
            notify_user(err, "There were preferences-file parsing errors (see $appLogFile$ for details)");
        }
	}

	// Check on the read-in prefs
	validate_graphics_preferences(graphics_preferences);
	validate_network_preferences(network_preferences);
    
    // Fix bool options
    player_preferences->background_music_on = !!player_preferences->background_music_on;

	// jkvw: If we try to load a default file, but can't, we'll have set the game error.
	//       But that's not useful, because we're just going to try loading the file
	//       from user preferences.  It used to be this code was only called in initialisation,
	//       and any game error generated here was simply clobbered by an init time
	//       clear_game_error().  Since I'm using this more generally now, I'm clearing the
	//       error right here, because it's not like we're bothered when we can't load a
	//       default file.
	//       (Problem is SDL specific - socre one for Carbon? :) )
	//clear_game_error ();
}


/*
 *  Write preferences to file
 */


InfoTree graphics_preferences_tree()
{
	InfoTree root;

	root.put_attr("scmode_width", graphics_preferences->screen_mode.width);
	root.put_attr("scmode_height", graphics_preferences->screen_mode.height);
	root.put_attr("scmode_auto_resolution", graphics_preferences->screen_mode.auto_resolution);
	root.put_attr("scmode_high_dpi", graphics_preferences->screen_mode.high_dpi);
	root.put_attr("scmode_hud", graphics_preferences->screen_mode.hud);
	root.put_attr("scmode_hud_scale", graphics_preferences->screen_mode.hud_scale_level);
	root.put_attr("scmode_term_scale", graphics_preferences->screen_mode.term_scale_level);
	root.put_attr("scmode_translucent_map", graphics_preferences->screen_mode.translucent_map);
	root.put_attr("scmode_camera_bob", static_cast<int>(graphics_preferences->screen_mode.bobbing_type));
	root.put_attr("scmode_accel", graphics_preferences->screen_mode.acceleration);
	root.put_attr("scmode_highres", graphics_preferences->screen_mode.high_resolution);
	root.put_attr("scmode_draw_every_other_line", graphics_preferences->screen_mode.draw_every_other_line);
	root.put_attr("scmode_fov", graphics_preferences->screen_mode.fov);
	root.put_attr("scmode_fullscreen", graphics_preferences->screen_mode.fullscreen);
	root.put_attr("scmode_bitdepth", graphics_preferences->screen_mode.bit_depth);
	root.put_attr("scmode_gamma", graphics_preferences->screen_mode.gamma_level);
	root.put_attr("scmode_fix_h_not_v", graphics_preferences->screen_mode.fix_h_not_v);
	root.put_attr("ogl_flags", graphics_preferences->OGL_Configure.Flags);
	root.put_attr("software_alpha_blending", graphics_preferences->software_alpha_blending);
	root.put_attr("software_sdl_driver", graphics_preferences->software_sdl_driver);
	root.put_attr("fps_target", graphics_preferences->fps_target);
	root.put_attr("anisotropy_level", graphics_preferences->OGL_Configure.AnisotropyLevel);
	root.put_attr("multisamples", graphics_preferences->OGL_Configure.Multisamples);
	root.put_attr("wait_for_vsync", graphics_preferences->OGL_Configure.WaitForVSync);
	root.put_attr("gamma_corrected_blending", graphics_preferences->OGL_Configure.Use_sRGB);
	root.put_attr("use_npot", graphics_preferences->OGL_Configure.Use_NPOT);
	root.put_attr("billboard_xy", graphics_preferences->OGL_Configure.BillboardXY);
	root.put_attr("movie_export_video_quality", graphics_preferences->movie_export_video_quality);
	root.put_attr("movie_export_video_bitrate", graphics_preferences->movie_export_video_bitrate);
	root.put_attr("movie_export_audio_quality", graphics_preferences->movie_export_audio_quality);
	root.put_attr("scripted_effects_quality", graphics_preferences->ephemera_quality);
	
	root.add_color("void.color", graphics_preferences->OGL_Configure.VoidColor);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 2; ++j)
			root.add_color("landscapes.color", graphics_preferences->OGL_Configure.LscpColors[i][j], 2*i+j);

	for (int i = 0; i <= OGL_NUMBER_OF_TEXTURE_TYPES; ++i)
	{
		OGL_Texture_Configure& Config = (i == OGL_NUMBER_OF_TEXTURE_TYPES) ? graphics_preferences->OGL_Configure.ModelConfig : graphics_preferences->OGL_Configure.TxtrConfigList[i];
		
		InfoTree tex;
		tex.put_attr("index", i);
		tex.put_attr("near_filter", Config.NearFilter);
		tex.put_attr("far_filter", Config.FarFilter);
		tex.put_attr("max_size", Config.MaxSize);
		root.add_child("texture", tex);
	}
	return root;
}

InfoTree player_preferences_tree()
{
	InfoTree root;
	
	root.put_attr_cstr("name", player_preferences->name);
	root.put_attr("color", player_preferences->color);
	root.put_attr("team", player_preferences->team);
	root.put_attr("last_time_ran", player_preferences->last_time_ran);
	root.put_attr("difficulty", player_preferences->difficulty_level);
	root.put_attr("bkgd_music", player_preferences->background_music_on);
	root.put_attr("crosshairs_active", player_preferences->crosshairs_active);
	
	ChaseCamData& ChaseCam = player_preferences->ChaseCam;
	InfoTree cam;
	cam.put_attr("behind", ChaseCam.Behind);
	cam.put_attr("upward", ChaseCam.Upward);
	cam.put_attr("rightward", ChaseCam.Rightward);
	cam.put_attr("flags", ChaseCam.Flags);
	cam.put_attr("damping", ChaseCam.Damping);
	cam.put_attr("spring", ChaseCam.Spring);
	cam.put_attr("opacity", ChaseCam.Opacity);
	root.put_child("chase_cam", cam);
	
	CrosshairData& Crosshairs = player_preferences->Crosshairs;
	InfoTree cross;
	cross.put_attr("thickness", Crosshairs.Thickness);
	cross.put_attr("from_center", Crosshairs.FromCenter);
	cross.put_attr("length", Crosshairs.Length);
	cross.put_attr("shape", Crosshairs.Shape);
	cross.put_attr("opacity", Crosshairs.Opacity);
	cross.add_color("color", Crosshairs.Color);
	root.put_child("crosshairs", cross);

	root.put_attr("solo_profile", player_preferences->solo_profile);

	return root;
}

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


static const std::string get_binding_joystick_button_name(int offset)
{
	assert_fail(SDL_CONTROLLER_BUTTON_MAX <= 21 && SDL_CONTROLLER_AXIS_MAX <= 12, "SDL changed the number of buttons/axes again!");

	static const strings_t buttons = {
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

	static const strings_t axes = {
		"controller-ls-right", "controller-ls-down", "controller-rs-right",
		"controller-rs-down", "controller-lt", "controller-rt",
		"controller-ls-left", "controller-ls-up", "controller-rs-left",
		"controller-rs-up", "controller-lt-neg", "controller-rt-neg"
	};

	if (offset < SDL_CONTROLLER_BUTTON_MAX)
	{
		return buttons[offset];
	}
	else
	{
		return axes[offset - SDL_CONTROLLER_BUTTON_MAX];
	}
}



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

enum class BindingType {
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

InfoTree input_preferences_tree()
{
	InfoTree root;
	
	root.put_attr("device", input_preferences->input_device);
	root.put_attr("modifiers", input_preferences->modifiers);
	root.put_attr("sens_horizontal", input_preferences->sens_horizontal);
	root.put_attr("sens_vertical", input_preferences->sens_vertical);
	root.put_attr("classic_vertical_aim", input_preferences->classic_vertical_aim);
	root.put_attr("classic_aim_speed_limits", input_preferences->classic_aim_speed_limits);
	root.put_attr("mouse_accel_type", input_preferences->mouse_accel_type);
	root.put_attr("mouse_accel_scale", input_preferences->mouse_accel_scale);
	root.put_attr("raw_mouse_input", input_preferences->raw_mouse_input);
	root.put_attr("extra_mouse_precision", input_preferences->extra_mouse_precision);
	
	root.put_attr("controller_analog", input_preferences->controller_analog);
	root.put_attr("controller_aim_inverted", input_preferences->controller_aim_inverted);
	root.put_attr("controller_sensitivity_horizontal", input_preferences->controller_sensitivity_horizontal);
	root.put_attr("controller_deadzone_horizontal", input_preferences->controller_deadzone_horizontal);
	root.put_attr("controller_sensitivity_vertical", input_preferences->controller_sensitivity_vertical);
	root.put_attr("controller_deadzone_vertical", input_preferences->controller_deadzone_vertical);
	
	for (int i = 0; i < (NUMBER_OF_KEYS + NUMBER_OF_SHELL_KEYS); ++i)
	{
		std::set<SDL_Scancode> codeset;
		std::string name;
		if (i < NUMBER_OF_KEYS) {
			codeset = input_preferences->key_bindings[i];
			name = binding_action_name[i];
		} else {
			codeset = input_preferences->shell_key_bindings[i - NUMBER_OF_KEYS];
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
		for (auto code : input_preferences->hotkey_bindings[i])
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

InfoTree sound_preferences_tree()
{
	InfoTree root;
	
	root.put_attr("volume_db", sound_preferences->volume_db);
	root.put_attr("music_db", sound_preferences->music_db);
	root.put_attr("flags", sound_preferences->flags);
	root.put_attr("rate", sound_preferences->rate);
	root.put_attr("samples", sound_preferences->samples);
	root.put_attr("video_export_volume_db", sound_preferences->video_export_volume_db);
	root.put_attr("channel", static_cast<int>(sound_preferences->channel_type));

	return root;
}

InfoTree network_preferences_tree()
{
	InfoTree root;

	root.put_attr("untimed", network_preferences->game_is_untimed);
	root.put_attr("game_type", network_preferences->game_type);
	root.put_attr("difficulty", network_preferences->difficulty_level);
	root.put_attr("game_options", network_preferences->game_options);
	root.put_attr("time_limit", network_preferences->time_limit);
	root.put_attr("kill_limit", network_preferences->kill_limit);
	root.put_attr("level_identity", network_preferences->level_identity);
	root.put_attr("autogather", network_preferences->autogather);
	root.put_attr("join_by_address", network_preferences->join_by_address);
	root.put_attr("join_address", network_preferences->join_address);
	root.put_attr("local_game_port", network_preferences->game_port);
	root.put_attr("game_protocol", sNetworkGameProtocolNames[network_preferences->game_protocol]);
	root.put_attr("use_netscript", network_preferences->use_netscript);
	root.put_attr_path("netscript_file", network_preferences->netscript_file);
	root.put_attr("cheat_flags", network_preferences->cheat_flags);
	root.put_attr("advertise_on_metaserver", network_preferences->advertise_on_metaserver);
	root.put_attr("attempt_upnp", network_preferences->attempt_upnp);
	root.put_attr("check_for_updates", network_preferences->check_for_updates);
	root.put_attr("verify_https", network_preferences->verify_https);
	root.put_attr("metaserver_login", network_preferences->metaserver_login);
    
    // TODO: FIX: metaserver_password is variable-size std::string now
    /*
	char passwd[33];
	for (int i = 0; i < 16; i++)
		snprintf(&passwd[2*i], sizeof(passwd), "%.2x", network_preferences->metaserver_password[i] ^ sPasswordMask[i]);
	passwd[32] = '\0';
	root.put_attr("metaserver_password", passwd);
	*/
	root.put_attr("use_custom_metaserver_colors", network_preferences->use_custom_metaserver_colors);
	root.put_attr("mute_metaserver_guests", network_preferences->mute_metaserver_guests);
	root.put_attr("join_metaserver_by_default", network_preferences->join_metaserver_by_default);
	root.put_attr("allow_stats", network_preferences->allow_stats);

	for (int i = 0; i < 2; i++)
		root.add_color("color", network_preferences->metaserver_colors[i], i);

	root.put_child("star_protocol", StarPreferencesTree());
	
	return root;
}

InfoTree environment_preferences_tree()
{
	InfoTree root;

	root.put_attr_path("map_file", environment_preferences.map_file);
	root.put_attr_path("physics_file", environment_preferences.physics_file);
	root.put_attr_path("shapes_file", environment_preferences.shapes_file);
	root.put_attr_path("sounds_file", environment_preferences.sounds_file);
	root.put_attr_path("resources_file", environment_preferences.resources_file);
	root.put_attr("map_checksum", environment_preferences.map_checksum);
	root.put_attr("physics_checksum", environment_preferences.physics_checksum);
    
    // TODO: FIX: _mod_date is std::filesystem::file_time_type now; that said, this seems more of a checksum thing
//	root.put_attr("shapes_mod_date", static_cast<uint32>(environment_preferences.shapes_mod_date));
//	root.put_attr("sounds_mod_date", static_cast<uint32>(environment_preferences.sounds_mod_date));
	
    root.put_attr("group_by_directory", environment_preferences.group_by_directory);
	root.put_attr("reduce_singletons", environment_preferences.reduce_singletons);
	root.put_attr("smooth_text", environment_preferences.smooth_text);
	root.put_attr_path("solo_lua_file", environment_preferences.solo_lua_file);
	root.put_attr("use_solo_lua", environment_preferences.use_solo_lua);
	root.put_attr("use_replay_net_lua", environment_preferences.use_replay_net_lua);
	root.put_attr("hide_alephone_extensions", environment_preferences.hide_extensions);
	root.put_attr("film_profile", static_cast<uint32>(environment_preferences.film_profile));
	root.put_attr("maximum_quick_saves", environment_preferences.maximum_quick_saves);
#ifdef HAVE_NFD
	root.put_attr("use_native_file_dialogs", environment_preferences.use_native_file_dialogs);
#endif
	root.put_attr("auto_play_demos", environment_preferences.auto_play_demos);

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

void write_preferences()
{
	InfoTree root;
	root.put_attr("version", A1_DATE_VERSION);
	
	root.put_child("graphics", graphics_preferences_tree());
	root.put_child("player", player_preferences_tree());
	root.put_child("input", input_preferences_tree());
	root.put_child("sound", sound_preferences_tree());
#if !defined(DISABLE_NETWORKING)
	root.put_child("network", network_preferences_tree());
#endif
	root.put_child("environment", environment_preferences_tree());
	
	InfoTree fileroot;
	fileroot.put_child("mara_prefs", root);
	
    ao_path FileSpec = get_preferences_dir();

	std::string name = get_string(STRID(strFILENAMES, filenamePREFERENCES));
	if (shell_options.editor) { name += " Editor"; }
	FileSpec /= name;
	
	try
    {
		fileroot.save_xml(FileSpec);
	}
    catch (const InfoTree::Exception& ex)
    {
        log_error_f("Error saving preferences file (%s): %s", FileSpec.c_str(), ex.what());
    }
}


/*
 *  Setup default preferences
 */

static void default_graphics_preferences(graphics_preferences_data *preferences)
{
  memset(&preferences->screen_mode, '\0', sizeof(screen_mode_data));
	preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;

	preferences->screen_mode.width = 640;
	preferences->screen_mode.height = 480;
	preferences->screen_mode.auto_resolution = true;
	preferences->screen_mode.high_dpi = true;
	preferences->screen_mode.hud = true;
	preferences->screen_mode.hud_scale_level = 0;
	preferences->screen_mode.term_scale_level = 2;
	preferences->screen_mode.translucent_map = false;
	preferences->screen_mode.acceleration = true;
	preferences->screen_mode.high_resolution = true;
	preferences->screen_mode.fullscreen = true;
	preferences->screen_mode.fix_h_not_v = true;
	preferences->screen_mode.bobbing_type = BobbingType::camera_and_weapon;
	preferences->screen_mode.bit_depth = 32;
	
	preferences->screen_mode.draw_every_other_line= false;

	preferences->screen_mode.fov = 0; // use default
	
	OGL_SetDefaults(preferences->OGL_Configure);

	preferences->software_alpha_blending = _sw_alpha_off;
	preferences->software_sdl_driver = _sw_driver_default;
	preferences->fps_target = 30;

	preferences->movie_export_video_quality = 50;
	preferences->movie_export_audio_quality = 50;
	preferences->movie_export_video_bitrate = 0; // auto

	preferences->ephemera_quality = _ephemera_medium;
}

static void default_network_preferences(network_preferences_data *preferences)
{
	preferences->game_is_untimed = false;
	preferences->difficulty_level = 2;
	preferences->game_options =	_multiplayer_game | _ammo_replenishes | _weapons_replenish
		| _specials_replenish | _burn_items_on_death
		| _force_unique_teams | _live_network_stats;
	preferences->time_limit = 10 * TICKS_PER_SECOND * 60;
	preferences->kill_limit = 10;
	preferences->level_identity= 0;
	preferences->game_type= _game_of_kill_monsters;
	preferences->autogather= false;
	preferences->join_by_address= false;
	obj_clear(preferences->join_address);
	preferences->game_port= DEFAULT_GAME_PORT;
	preferences->game_protocol= _network_game_protocol_default;
#if !defined(DISABLE_NETWORKING)
	DefaultStarPreferences();
#endif // !defined(DISABLE_NETWORKING)
	preferences->use_netscript = false;
    preferences->netscript_file.clear();
	preferences->cheat_flags = _allow_tunnel_vision | _allow_crosshair | _allow_behindview | _allow_overlay_map;
	preferences->advertise_on_metaserver = false;
	preferences->attempt_upnp = false;
	preferences->check_for_updates = true;
	preferences->verify_https = false;
	preferences->metaserver_login = "guest";
    preferences->metaserver_password.clear();
	preferences->mute_metaserver_guests = false;
	preferences->use_custom_metaserver_colors = false;
	preferences->metaserver_colors[0] = get_interface_color(PLAYER_COLOR_BASE_INDEX);
	preferences->metaserver_colors[1] = get_interface_color(PLAYER_COLOR_BASE_INDEX);
	preferences->join_metaserver_by_default = false;
	preferences->allow_stats = false;
}

static void default_player_preferences(player_preferences_data *preferences)
{
	obj_clear(*preferences);

	preferences->difficulty_level= 2;
	preferences->name = get_name_from_system();
		
	preferences->ChaseCam.Behind = 1536;
	preferences->ChaseCam.Upward = 0;
	preferences->ChaseCam.Rightward = 0;
	preferences->ChaseCam.Flags = 0;
	preferences->ChaseCam.Damping = 0.5;
	preferences->ChaseCam.Spring = 0;
	preferences->ChaseCam.Opacity = 1;
	
	preferences->Crosshairs.Thickness = 3;
	preferences->Crosshairs.FromCenter = 2;
	preferences->Crosshairs.Length = 1;
	preferences->Crosshairs.Shape = CHShape_RealCrosshairs;
	preferences->Crosshairs.Color = rgb_white;
	preferences->Crosshairs.Opacity = 0.5;
	preferences->Crosshairs.PreCalced = false;

	preferences->solo_profile = _solo_profile_aleph_one;
}

static void default_input_preferences(input_preferences_data *preferences)
{
	preferences->input_device= _mouse_yaw_pitch;
	preferences->key_bindings = default_key_bindings;
	preferences->shell_key_bindings = default_shell_key_bindings;
	preferences->hotkey_bindings = default_hotkey_bindings;
	
	preferences->modifiers = _inputmod_use_button_sounds;

	preferences->sens_horizontal = FIXED_ONE / 4;
	preferences->sens_vertical = FIXED_ONE / 4;
	preferences->mouse_accel_type = _mouse_accel_none;
	preferences->mouse_accel_scale = 1.f;
	preferences->raw_mouse_input = true;
	preferences->extra_mouse_precision = true;
	preferences->classic_vertical_aim = false;
	preferences->classic_aim_speed_limits = false;

	preferences->controller_aim_inverted = false;
	preferences->controller_analog = true;
	preferences->controller_sensitivity_horizontal = FIXED_ONE;
	preferences->controller_deadzone_horizontal = 3276;
	preferences->controller_sensitivity_vertical = FIXED_ONE;
	preferences->controller_deadzone_vertical = 3276;
}


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


/*
 *  Validate preferences
 */

static bool validate_graphics_preferences(graphics_preferences_data *preferences)
{
	bool changed= false;

	// Fix bool options
	preferences->screen_mode.high_resolution = !!preferences->screen_mode.high_resolution;
	preferences->screen_mode.fullscreen = !!preferences->screen_mode.fullscreen;
	preferences->screen_mode.draw_every_other_line = !!preferences->screen_mode.draw_every_other_line;
    preferences->screen_mode.fix_h_not_v = !!preferences->screen_mode.fix_h_not_v;

	if(preferences->screen_mode.gamma_level<0 || preferences->screen_mode.gamma_level>=NUMBER_OF_GAMMA_LEVELS)
	{
		preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;
		changed= true;
	}

	preferences->screen_mode.acceleration = !!preferences->screen_mode.acceleration;

	// OpenGL requires at least 16 bit color depth
	if (preferences->screen_mode.acceleration && preferences->screen_mode.bit_depth == 8)
	{
		preferences->screen_mode.bit_depth= 16;
		changed= true;
	}

#ifdef TRUE_COLOR_ONLY
	if (preferences->screen_mode.bit_depth == 8)
	{
		preferences->screen_mode.bit_depth = 16;
		changed = true;
	}
#endif

	if (preferences->screen_mode.fov < 30 && preferences->screen_mode.fov != 0)
	{
		preferences->screen_mode.fov = 30;
		changed = true;
	}

	if (preferences->screen_mode.fov > 130)
	{
		preferences->screen_mode.fov = 130;
		changed = true;
	}

	return changed;
}

static bool validate_network_preferences(network_preferences_data *preferences)
{
	bool changed= false;

	// Fix bool options
	preferences->game_is_untimed = !!preferences->game_is_untimed;
	
	if(preferences->game_is_untimed != true && preferences->game_is_untimed != false)
	{
		preferences->game_is_untimed= false;
		changed= true;
	}

	if(preferences->game_type<0 || preferences->game_type >= NUMBER_OF_GAME_TYPES)
	{
		preferences->game_type= _game_of_kill_monsters;
		changed= true;
	}

        // ZZZ: is this relevant anymore now with XML prefs?  if so, should validate autogather, join_by_address, and join_address.

	if(preferences->game_protocol >= NUMBER_OF_NETWORK_GAME_PROTOCOLS)
	{
		preferences->game_protocol= _network_game_protocol_default;
		changed= true;
	}

	return changed;
}



/*
 *  Load the environment
 */

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


// LP addition: get these from the preferences data
ChaseCamData& GetChaseCamData() {return player_preferences->ChaseCam;}
CrosshairData& GetCrosshairData() {return player_preferences->Crosshairs;}
OGL_ConfigureData& Get_OGL_ConfigureData() {return graphics_preferences->OGL_Configure;}


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
    return !sStandardizeModifiers ? TEST_FLAG(input_preferences->modifiers,_inputmod_dont_switch_to_new_weapon) : false;
}


bool dont_auto_recenter()
{
	return TEST_FLAG(input_preferences->modifiers, _inputmod_dont_auto_recenter);
}


// LP additions: MML-like prefs stuff
// These parsers are intended to work correctly on both Mac and SDL prefs files;
// including one crossing over to the other platform (uninterpreted fields become defaults)

// To get around both rgb_color and rgb_color being used in the code
template<class CType1, class CType2> void CopyColor(CType1& Dest, CType2& Src)
{
	Dest.red = Src.red;
	Dest.green = Src.green;
	Dest.blue = Src.blue;
}




void parse_graphics_preferences(InfoTree root, std::string version)
{
	root.read_attr("scmode_height", graphics_preferences->screen_mode.height);
	root.read_attr("scmode_width", graphics_preferences->screen_mode.width);
	root.read_attr("scmode_auto_resolution", graphics_preferences->screen_mode.auto_resolution);
	root.read_attr("scmode_high_dpi", graphics_preferences->screen_mode.high_dpi);
	root.read_attr("scmode_hud", graphics_preferences->screen_mode.hud);
	root.read_attr("scmode_hud_scale", graphics_preferences->screen_mode.hud_scale_level);
	root.read_attr("scmode_term_scale", graphics_preferences->screen_mode.term_scale_level);
	root.read_attr("scmode_translucent_map", graphics_preferences->screen_mode.translucent_map);

	int bobbing_type = -1;
	root.read_attr("scmode_camera_bob", bobbing_type);

	if (bobbing_type != -1)
	{
		graphics_preferences->screen_mode.bobbing_type = static_cast<BobbingType>(bobbing_type);
	}

	root.read_attr("scmode_accel", graphics_preferences->screen_mode.acceleration);
	root.read_attr("scmode_highres", graphics_preferences->screen_mode.high_resolution);
	root.read_attr("scmode_draw_every_other_line", graphics_preferences->screen_mode.draw_every_other_line);
	root.read_attr("scmode_fullscreen", graphics_preferences->screen_mode.fullscreen);
	
	root.read_attr("scmode_fix_h_not_v", graphics_preferences->screen_mode.fix_h_not_v);
	root.read_attr("scmode_bitdepth", graphics_preferences->screen_mode.bit_depth);
	root.read_attr("scmode_gamma", graphics_preferences->screen_mode.gamma_level);
	root.read_attr("scmode_fov", graphics_preferences->screen_mode.fov);
	root.read_attr("ogl_flags", graphics_preferences->OGL_Configure.Flags);
	root.read_attr("software_alpha_blending", graphics_preferences->software_alpha_blending);
	root.read_attr("software_sdl_driver", graphics_preferences->software_sdl_driver);
	root.read_attr("fps_target", graphics_preferences->fps_target);
	root.read_attr("anisotropy_level", graphics_preferences->OGL_Configure.AnisotropyLevel);
	root.read_attr("multisamples", graphics_preferences->OGL_Configure.Multisamples);
	root.read_attr("wait_for_vsync", graphics_preferences->OGL_Configure.WaitForVSync);
	root.read_attr("gamma_corrected_blending", graphics_preferences->OGL_Configure.Use_sRGB);
	root.read_attr("use_npot", graphics_preferences->OGL_Configure.Use_NPOT);
	root.read_attr("billboard_xy", graphics_preferences->OGL_Configure.BillboardXY);
	root.read_attr_bounded<int16>("movie_export_video_quality", graphics_preferences->movie_export_video_quality, 0, 100);
	root.read_attr_bounded<int16>("movie_export_audio_quality", graphics_preferences->movie_export_audio_quality, 0, 100);
	root.read_attr("movie_export_video_bitrate", graphics_preferences->movie_export_video_bitrate);

	root.read_attr("scripted_effects_quality", graphics_preferences->ephemera_quality);
	
	for (const InfoTree &vtree : root.children_named("void"))
	{
		for (const InfoTree &color : vtree.children_named("color"))
		{
			color.read_color(graphics_preferences->OGL_Configure.VoidColor);
		}
	}
	
	for (const InfoTree &landscape : root.children_named("landscapes"))
	{
		for (const InfoTree &color : landscape.children_named("color"))
		{
			int16 index;
			if (color.read_indexed("index", index, 8))
				color.read_color(graphics_preferences->OGL_Configure.LscpColors[index / 2][index % 2]);
		}
	}
	
	for (const InfoTree &tex : root.children_named("texture"))
	{
		int16 index;
		if (tex.read_indexed("index", index, OGL_NUMBER_OF_TEXTURE_TYPES+1))
		{
			OGL_Texture_Configure& Config = (index == OGL_NUMBER_OF_TEXTURE_TYPES) ? graphics_preferences->OGL_Configure.ModelConfig : graphics_preferences->OGL_Configure.TxtrConfigList[index];
			tex.read_attr("near_filter", Config.NearFilter);
			tex.read_attr("far_filter", Config.FarFilter);
			tex.read_attr("max_size", Config.MaxSize);
		}
	}
}


void parse_player_preferences(InfoTree root, std::string version)
{
	root.read_attr("name", player_preferences->name);
	root.read_attr("color", player_preferences->color);
	root.read_attr("team", player_preferences->team);
	root.read_attr("last_time_ran", player_preferences->last_time_ran);
	root.read_attr("difficulty", player_preferences->difficulty_level);
	root.read_attr("bkgd_music", player_preferences->background_music_on);
	root.read_attr("crosshairs_active", player_preferences->crosshairs_active);
	
	for (const InfoTree &child : root.children_named("chase_cam"))
	{
		child.read_attr("behind", player_preferences->ChaseCam.Behind);
		child.read_attr("upward", player_preferences->ChaseCam.Upward);
		child.read_attr("rightward", player_preferences->ChaseCam.Rightward);
		child.read_attr("flags", player_preferences->ChaseCam.Flags);
		child.read_attr("damping", player_preferences->ChaseCam.Damping);
		child.read_attr("spring", player_preferences->ChaseCam.Spring);
		child.read_attr("opacity", player_preferences->ChaseCam.Opacity);
	}
	
	for (const InfoTree &child : root.children_named("crosshairs"))
	{
		child.read_attr("thickness", player_preferences->Crosshairs.Thickness);
		child.read_attr("from_center", player_preferences->Crosshairs.FromCenter);
		child.read_attr("length", player_preferences->Crosshairs.Length);
		child.read_attr("shape", player_preferences->Crosshairs.Shape);
		child.read_attr("opacity", player_preferences->Crosshairs.Opacity);
		
		for (const InfoTree &color : child.children_named("color"))
			color.read_color(player_preferences->Crosshairs.Color);
	}

	if (Scenario::instance()->AllowsClassicGameplay())
	{
		root.read_attr("solo_profile", player_preferences->solo_profile);
	}
}

SDL_Scancode translate_old_key(int code)
{
	static int num_key_lookups = 323;
	static SDL_Keycode key_lookups[] = {
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_BACKSPACE, SDLK_TAB,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_CLEAR, SDLK_RETURN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_PAUSE,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_ESCAPE, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_SPACE, SDLK_EXCLAIM, SDLK_QUOTEDBL,
		SDLK_HASH, SDLK_DOLLAR, SDLK_UNKNOWN, SDLK_AMPERSAND, SDLK_QUOTE,
		SDLK_LEFTPAREN, SDLK_RIGHTPAREN, SDLK_ASTERISK, SDLK_PLUS, SDLK_COMMA,
		SDLK_MINUS, SDLK_PERIOD, SDLK_SLASH, SDLK_0, SDLK_1,
		SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6,
		SDLK_7, SDLK_8, SDLK_9, SDLK_COLON, SDLK_SEMICOLON,
		SDLK_LESS, SDLK_EQUALS, SDLK_GREATER, SDLK_QUESTION, SDLK_AT,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_LEFTBRACKET, SDLK_BACKSLASH, SDLK_RIGHTBRACKET, SDLK_CARET,
		SDLK_UNDERSCORE, SDLK_BACKQUOTE, SDLK_a, SDLK_b, SDLK_c,
		SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h,
		SDLK_i, SDLK_j, SDLK_k, SDLK_l, SDLK_m,
		SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r,
		SDLK_s, SDLK_t, SDLK_u, SDLK_v, SDLK_w,
		SDLK_x, SDLK_y, SDLK_z, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_DELETE, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_UNKNOWN, SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3,
		SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8,
		SDLK_KP_9, SDLK_KP_PERIOD, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY, SDLK_KP_MINUS,
		SDLK_KP_PLUS, SDLK_KP_ENTER, SDLK_KP_EQUALS, SDLK_UP, SDLK_DOWN,
		SDLK_RIGHT, SDLK_LEFT, SDLK_INSERT, SDLK_HOME, SDLK_END,
		SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_F1, SDLK_F2, SDLK_F3,
		SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8,
		SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12, SDLK_F13,
		SDLK_F14, SDLK_F15, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
		SDLK_NUMLOCKCLEAR, SDLK_CAPSLOCK, SDLK_SCROLLLOCK, SDLK_RSHIFT, SDLK_LSHIFT,
		SDLK_RCTRL, SDLK_LCTRL, SDLK_RALT, SDLK_LALT, SDLK_RGUI,
		SDLK_LGUI, SDLK_LGUI, SDLK_RGUI, SDLK_MODE, SDLK_UNKNOWN,
		SDLK_HELP, SDLK_PRINTSCREEN, SDLK_SYSREQ, SDLK_UNKNOWN, SDLK_MENU,
		SDLK_POWER, SDLK_CURRENCYUNIT, SDLK_UNDO
	};

	if (code >= 65 && code < 73)
		return static_cast<SDL_Scancode>(AO_SCANCODE_BASE_MOUSE_BUTTON + (code - 65));
	else if (code >= 73 && code < 91)
		return static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + (code - 73));
	else if (code < num_key_lookups)
		return SDL_GetScancodeFromKey(key_lookups[code]);
	return SDL_SCANCODE_UNKNOWN;
}

void parse_input_preferences(InfoTree root, std::string version)
{
	root.read_attr("device", input_preferences->input_device);
	root.read_attr("modifiers", input_preferences->modifiers);

	// old prefs may have combined sensitivity
	root.read_attr("sensitivity", input_preferences->sens_horizontal);
	root.read_attr("sensitivity", input_preferences->sens_vertical);
	root.read_attr("sens_horizontal", input_preferences->sens_horizontal);
	root.read_attr("sens_vertical", input_preferences->sens_vertical);
	
	if (!version.length() || version < "20181113")
		input_preferences->classic_vertical_aim = true;
	else if (version < "20190317")
		input_preferences->classic_vertical_aim = false;
	root.read_attr("classic_vertical_aim", input_preferences->classic_vertical_aim);
	
	if (!root.read_attr("classic_aim_speed_limits", input_preferences->classic_aim_speed_limits))
	{
		// Assume users with older prefs with "mouse_max_speed" above its default value don't want classic limits
		float mouse_max_speed;
		if (root.read_attr("mouse_max_speed", mouse_max_speed) && mouse_max_speed > 0.25)
			input_preferences->classic_aim_speed_limits = false;
	}

	// old prefs may have boolean acceleration flag
	bool accel = false;
	if (root.read_attr("mouse_acceleration", accel))
	{
		input_preferences->mouse_accel_type = _mouse_accel_classic;
		input_preferences->mouse_accel_scale = 1.f;
	}
	root.read_attr_bounded<int16>("mouse_accel_type",
								  input_preferences->mouse_accel_type,
								  0, NUMBER_OF_MOUSE_ACCEL_TYPES - 1);
	root.read_attr("mouse_accel_scale", input_preferences->mouse_accel_scale);
	
	// old prefs mixed "classic vertical aim" with acceleration type
	if (version >= "20181113" && version < "20190317")
	{
		if (input_preferences->mouse_accel_type == _mouse_accel_symmetric)
		{
			input_preferences->classic_vertical_aim = false;
			input_preferences->mouse_accel_type = _mouse_accel_classic;
		}
		else if (input_preferences->mouse_accel_type == _mouse_accel_classic)
		{
			input_preferences->classic_vertical_aim = true;
			input_preferences->sens_vertical *= 4.f;
		}
	}

	if (!version.length() || version < "20170821")
		input_preferences->raw_mouse_input = false;
	root.read_attr("raw_mouse_input", input_preferences->raw_mouse_input);
	if (!version.length() || version < "20181208")
		input_preferences->extra_mouse_precision = false;
	root.read_attr("extra_mouse_precision", input_preferences->extra_mouse_precision);
	root.read_attr("controller_analog", input_preferences->controller_analog);
	root.read_attr("controller_aim_inverted", input_preferences->controller_aim_inverted);

	_fixed old_controller_sensitivity_pref;
	if (root.read_attr("controller_sensitivity", old_controller_sensitivity_pref))
	{
		input_preferences->controller_sensitivity_vertical =
			input_preferences->controller_sensitivity_horizontal = old_controller_sensitivity_pref;
	}

	short old_controller_deadzone_pref;
	if (root.read_attr("controller_deadzone", old_controller_deadzone_pref))
	{
		input_preferences->controller_deadzone_vertical =
			input_preferences->controller_deadzone_horizontal = old_controller_deadzone_pref;
	}

	root.read_attr("controller_sensitivity_horizontal", input_preferences->controller_sensitivity_horizontal);
	root.read_attr("controller_deadzone_horizontal", input_preferences->controller_deadzone_horizontal);
	root.read_attr("controller_sensitivity_vertical", input_preferences->controller_sensitivity_vertical);
	root.read_attr("controller_deadzone_vertical", input_preferences->controller_deadzone_vertical);

	// remove default key bindings the first time we see one from these prefs
	std::set<std::pair<BindingType, int>> seen_key;
	
	// import old key bindings
	for (const InfoTree &key : root.children_named("sdl_key"))
	{
		int16 index;
		if (key.read_indexed("index", index, NUMBER_OF_KEYS))
		{
			auto k = std::make_pair(BindingType::in_game, index);
			if (seen_key.count(k) == 0)
			{
				input_preferences->key_bindings[index].clear();
				seen_key.insert(k);
			}
			int code;
			if (key.read_attr("value", code))
			{
				SDL_Scancode translated = translate_old_key(code);
				unset_scancode(translated);
				input_preferences->key_bindings[index].insert(translated);
			}
		}
		else if (key.read_indexed("index", index, NUMBER_OF_KEYS + NUMBER_OF_SHELL_KEYS))
		{
			int shell_index = index - NUMBER_OF_KEYS;
			auto k = std::make_pair(BindingType::shell, shell_index);
			if (seen_key.count(k) == 0)
			{
				input_preferences->shell_key_bindings[shell_index].clear();
				seen_key.insert(k);
			}
			int code;
			if (key.read_attr("value", code))
			{
				SDL_Scancode translated = translate_old_key(code);
				unset_scancode(translated);
				input_preferences->shell_key_bindings[shell_index].insert(translated);
			}
		}
	}
	
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
				map = &input_preferences->key_bindings;
				break;
			case BindingType::shell:
				map = &input_preferences->shell_key_bindings;
				break;
			case BindingType::hotkey:
				map = &input_preferences->hotkey_bindings;
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

void parse_sound_preferences(InfoTree root, std::string version)
{
	if (!version.length() || version < "20200803")
	{
		int old_volume;
		root.read_attr("volume", old_volume);
		if (old_volume > 0)
		{
			sound_preferences->volume_db = 10.f * std::log10(static_cast<float>(old_volume) / NUMBER_OF_SOUND_VOLUME_LEVELS);
		}
		else
		{
			sound_preferences->volume_db = SoundManager::MINIMUM_VOLUME_DB;
		}
	}
	else
	{
		root.read_attr("volume_db", sound_preferences->volume_db);
	}

	if (!version.length() || version < "20200803")
	{
		int old_music_volume;
		root.read_attr("music_volume", old_music_volume);
		if (old_music_volume > 0)
		{
			sound_preferences->music_db = 10.f * std::log10(static_cast<float>(old_music_volume) / NUMBER_OF_SOUND_VOLUME_LEVELS);
		}
		else
		{
			sound_preferences->music_db = SoundManager::MINIMUM_VOLUME_DB / 2;
		}
	}
	else
	{
		root.read_attr("music_db", sound_preferences->music_db);
	}

	root.read_attr("flags", sound_preferences->flags);
	root.read_attr("rate", sound_preferences->rate);
	root.read_attr("samples", sound_preferences->samples);
	root.read_attr("video_export_volume_db", sound_preferences->video_export_volume_db);

	int channel_type = 0;
	root.read_attr("channel", channel_type);

	if (channel_type) 
	{
		sound_preferences->channel_type = static_cast<ChannelType>(channel_type);
	}
}




void parse_network_preferences(InfoTree root, std::string version)
{
	root.read_attr("untimed", network_preferences->game_is_untimed);
	root.read_attr("game_type", network_preferences->game_type);
	root.read_attr("difficulty", network_preferences->difficulty_level);
	root.read_attr("game_options", network_preferences->game_options);
	root.read_attr("time_limit", network_preferences->time_limit);
	root.read_attr("kill_limit", network_preferences->kill_limit);
	root.read_attr("level_identity", network_preferences->level_identity);
	root.read_attr("autogather", network_preferences->autogather);
	root.read_attr("join_by_address", network_preferences->join_by_address);
	root.read_attr("join_address", network_preferences->join_address);
	root.read_attr("local_game_port", network_preferences->game_port);
    
    // TODO: stupid motherfuckers: THERE IS ONE PROTOCOL
	std::string protocol;
	if (root.read_attr("game_protocol", protocol))
	{
		for (int i = 0; i < NUMBER_OF_NETWORK_GAME_PROTOCOL_NAMES; ++i)
		{
			if (protocol == sNetworkGameProtocolNames[i])
			{
				network_preferences->game_protocol = i;
				break;
			}
		}
	}
	
	root.read_attr("use_netscript", network_preferences->use_netscript);
	root.read_path("netscript_file", network_preferences->netscript_file); // changed this from `read_path`; check it later
	root.read_attr("cheat_flags", network_preferences->cheat_flags);
	root.read_attr("advertise_on_metaserver", network_preferences->advertise_on_metaserver);
	root.read_attr("attempt_upnp", network_preferences->attempt_upnp);
	root.read_attr("check_for_updates", network_preferences->check_for_updates);
	root.read_attr("verify_https", network_preferences->verify_https);
	root.read_attr("use_custom_metaserver_colors", network_preferences->use_custom_metaserver_colors);
	root.read_attr("metaserver_login", network_preferences->metaserver_login);
	root.read_attr("mute_metaserver_guests", network_preferences->mute_metaserver_guests);
	root.read_attr("metaserver_clear_password", network_preferences->metaserver_password);
	
	if (root.read_attr("metaserver_password", network_preferences->metaserver_password))
	{
        obfuscate_string(network_preferences->metaserver_password);
        /* TODO: someone else can fix this up if they want it, otherwise users need to re-enter their passwords
		for (int i = 0; i < 15; i++)
		{
			unsigned int c;
			sscanf(obscured_password + i*2, "%2x", &c);
			network_preferences->metaserver_password[i] = (char) c ^ sPasswordMask[i];
		}
		network_preferences->metaserver_password[15] = '\0';
         */
	}
	
	root.read_attr("join_metaserver_by_default", network_preferences->join_metaserver_by_default);
	root.read_attr("allow_stats", network_preferences->allow_stats);

	for (const InfoTree &color : root.children_named("color"))
	{
		int16 index;
		if (color.read_indexed("index", index, 2))
			color.read_color(network_preferences->metaserver_colors[index]);
	}
	
	for (const InfoTree &child : root.children_named("star_protocol"))
		StarGameProtocol::ParsePreferencesTree(child, version);
}

void parse_environment_preferences(InfoTree root, std::string version)
{
	root.read_path("map_file", environment_preferences.map_file);
	root.read_path("physics_file", environment_preferences.physics_file);
	root.read_path("shapes_file", environment_preferences.shapes_file);
	root.read_path("sounds_file", environment_preferences.sounds_file);
	root.read_path("resources_file", environment_preferences.resources_file);
	root.read_attr("map_checksum", environment_preferences.map_checksum);
	root.read_attr("physics_checksum", environment_preferences.physics_checksum);
    
    // TODO: FIX: as above
	//root.read_attr("shapes_mod_date", environment_preferences.shapes_mod_date);
	//root.read_attr("sounds_mod_date", environment_preferences.sounds_mod_date);
    
	root.read_attr("group_by_directory", environment_preferences.group_by_directory);
	root.read_attr("reduce_singletons", environment_preferences.reduce_singletons);
	root.read_attr("smooth_text", environment_preferences.smooth_text);
	root.read_path("solo_lua_file", environment_preferences.solo_lua_file);
	root.read_attr("use_solo_lua", environment_preferences.use_solo_lua);
	root.read_attr("use_replay_net_lua", environment_preferences.use_replay_net_lua);
	root.read_attr("hide_alephone_extensions", environment_preferences.hide_extensions);
	
	uint32 profile = FILM_PROFILE_DEFAULT + 1;
	root.read_attr("film_profile", profile);
	if (profile <= FILM_PROFILE_DEFAULT)
		environment_preferences.film_profile = static_cast<FilmProfileType>(profile);
	
	root.read_attr("maximum_quick_saves", environment_preferences.maximum_quick_saves);
#ifdef HAVE_NFD
	root.read_attr("use_native_file_dialogs", environment_preferences.use_native_file_dialogs);
#endif
	root.read_attr("auto_play_demos", environment_preferences.auto_play_demos);
	
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

extern const char* GetSDLKeyName(SDL_Scancode);

const char* get_hotkey_binding(int hotkey, int type)
{
	
	auto bindings = input_preferences->hotkey_bindings[hotkey - 1];
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
