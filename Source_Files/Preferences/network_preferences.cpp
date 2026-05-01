/*
 network_preferences.hpp
 
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

#include "network_preferences.hpp"

#include "preferences_support.hpp"


static const char sPasswordMask[] = "reverof nohtaram";



//*****************************************************************************
// PREFERENCES
//*****************************************************************************


network_preferences_data network_preferences;


void network_preferences_data::read(InfoTree root, std::string version)
{
    root.read_attr("untimed", game_is_untimed);
    root.read_attr("game_type", game_type);
    root.read_attr("difficulty", difficulty_level);
    root.read_attr("game_options", game_options);
    root.read_attr("time_limit", time_limit);
    root.read_attr("kill_limit", kill_limit);
    root.read_attr("level_identity", level_identity);
    root.read_attr("autogather", autogather);
    root.read_attr("join_by_address", join_by_address);
    root.read_attr("join_address", join_address);
    root.read_attr("local_game_port", game_port);
    root.read_attr("use_netscript", use_netscript);
    root.read_path("netscript_file", netscript_file); // changed this from `read_path`; check it later
    root.read_attr("cheat_flags", cheat_flags);
    root.read_attr("advertise_on_metaserver", advertise_on_metaserver);
    root.read_attr("attempt_upnp", attempt_upnp);
    root.read_attr("check_for_updates", check_for_updates);
    root.read_attr("verify_https", verify_https);
    root.read_attr("use_custom_metaserver_colors", use_custom_metaserver_colors);
    root.read_attr("metaserver_login", metaserver_login);
    root.read_attr("mute_metaserver_guests", mute_metaserver_guests);
    root.read_attr("metaserver_clear_password", metaserver_password);
    
    if (root.read_attr("metaserver_password", metaserver_password))
    {
        obfuscate_string(metaserver_password);
        /* TODO: someone else can fix this up if they want it, otherwise users need to re-enter their passwords
        for (int i = 0; i < 15; i++)
        {
            unsigned int c;
            sscanf(obscured_password + i*2, "%2x", &c);
            metaserver_password[i] = (char) c ^ sPasswordMask[i];
        }
        metaserver_password[15] = '\0';
         */
    }
    
    root.read_attr("join_metaserver_by_default", join_metaserver_by_default);
    root.read_attr("allow_stats", allow_stats);

    for (const InfoTree &color : root.children_named("color"))
    {
        int16 index;
        if (color.read_indexed("index", index, 2))
            color.read_color(metaserver_colors[index]);
    }
    
    for (const InfoTree &child : root.children_named("star_protocol"))
        StarGameProtocol::ParsePreferencesTree(child, version);
    
    // from 'validate...' function
    // Fix bool options // EES: I seriously wonder...
    game_is_untimed = !!game_is_untimed;
    
    if (game_is_untimed != true && game_is_untimed != false)
    {
        game_is_untimed= false;
    }

    if (game_type<0 || game_type >= NUMBER_OF_GAME_TYPES)
    {
        game_type= _game_of_kill_monsters;
    }
}


InfoTree network_preferences_data::write()
{
    InfoTree root;

    root.put_attr("untimed", game_is_untimed);
    root.put_attr("game_type", game_type);
    root.put_attr("difficulty", difficulty_level);
    root.put_attr("game_options", game_options);
    root.put_attr("time_limit", time_limit);
    root.put_attr("kill_limit", kill_limit);
    root.put_attr("level_identity", level_identity);
    root.put_attr("autogather", autogather);
    root.put_attr("join_by_address", join_by_address);
    root.put_attr("join_address", join_address);
    root.put_attr("local_game_port", game_port);
    root.put_attr("use_netscript", use_netscript);
    root.put_attr_path("netscript_file", netscript_file);
    root.put_attr("cheat_flags", cheat_flags);
    root.put_attr("advertise_on_metaserver", advertise_on_metaserver);
    root.put_attr("attempt_upnp", attempt_upnp);
    root.put_attr("check_for_updates", check_for_updates);
    root.put_attr("verify_https", verify_https);
    root.put_attr("metaserver_login", metaserver_login);
    
    // TODO: FIX: metaserver_password is variable-size std::string now
    /*
    char passwd[33];
    for (int i = 0; i < 16; i++)
        snprintf(&passwd[2*i], sizeof(passwd), "%.2x", metaserver_password[i] ^ sPasswordMask[i]);
    passwd[32] = '\0';
    root.put_attr("metaserver_password", passwd);
    */
    root.put_attr("use_custom_metaserver_colors", use_custom_metaserver_colors);
    root.put_attr("mute_metaserver_guests", mute_metaserver_guests);
    root.put_attr("join_metaserver_by_default", join_metaserver_by_default);
    root.put_attr("allow_stats", allow_stats);

    for (int i = 0; i < 2; i++)
        root.add_color("color", metaserver_colors[i], i);

    root.put_child("star_protocol", StarPreferencesTree());
    
    return root;
}


//*****************************************************************************
// DIALOGS
//*****************************************************************************
// Online (lhowon.org) dialog


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
    
    main_screen.set_fullscreen(false);
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
                network_preferences.metaserver_login = login_w->get_text();
                network_preferences.metaserver_password = password_w->get_text();
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
    
    w_text_entry *login_w = new w_text_entry(network_preferences_data::kMetaserverLoginLength, network_preferences.metaserver_login);
    login_w->set_identifier(iSIGNUP_USERNAME_W);
    table->dual_add(login_w->adding_label("Username"), d);
    table->dual_add(login_w, d);
    
    w_password_entry *password_w = new w_password_entry(network_preferences_data::kMetaserverLoginLength, network_preferences.metaserver_password);
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
        login_w->set_text(network_preferences.metaserver_login);
        w_password_entry *password_w = static_cast<w_password_entry *>(parent->get_widget_by_id(iONLINE_PASSWORD_W));
        password_w->set_text(network_preferences.metaserver_password);
    }
}



void online_dialog(void *arg)
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
    
    w_text_entry *login_w = new w_text_entry(network_preferences_data::kMetaserverLoginLength, network_preferences.metaserver_login);
    login_w->set_identifier(iONLINE_USERNAME_W);
    account_table->dual_add(login_w->adding_label("Username"), d);
    account_table->dual_add(login_w, d);
    
    w_password_entry *password_w = new w_password_entry(network_preferences_data::kMetaserverLoginLength, network_preferences.metaserver_password);
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
    
    w_text_entry *name_w = new w_text_entry(MAXIMUM_PLAYER_NAME_LENGTH, player_preferences.name);
    name_w->set_identifier(0); // NAME_W
    name_w->set_enter_pressed_callback(dialog_try_ok);
    name_w->set_value_changed_callback(dialog_disable_ok_if_empty);
    lobby_table->dual_add(name_w->adding_label("Name"), d);
    lobby_table->dual_add(name_w, d);
    
    w_enabling_toggle *custom_colors_w = new w_enabling_toggle(network_preferences.use_custom_metaserver_colors);
    lobby_table->dual_add(custom_colors_w->adding_label("Custom Chat Colors"), d);
    lobby_table->dual_add(custom_colors_w, d);
    
    w_color_picker *primary_w = new w_color_picker(network_preferences.metaserver_colors[0]);
    lobby_table->dual_add(primary_w->adding_label("Primary"), d);
    lobby_table->dual_add(primary_w, d);
    
    w_color_picker *secondary_w = new w_color_picker(network_preferences.metaserver_colors[1]);
    lobby_table->dual_add(secondary_w->adding_label("Secondary"), d);
    lobby_table->dual_add(secondary_w, d);
    
    custom_colors_w->add_dependent_widget(primary_w);
    custom_colors_w->add_dependent_widget(secondary_w);

    w_toggle *mute_guests_w = new w_toggle(network_preferences.mute_metaserver_guests);
    lobby_table->dual_add(mute_guests_w->adding_label("Mute All Guest Chat"), d);
    lobby_table->dual_add(mute_guests_w, d);

    lobby_table->add_row(new w_spacer(), true);
    
    w_toggle *join_meta_w = new w_toggle(network_preferences.join_metaserver_by_default);
    lobby_table->dual_add(join_meta_w->adding_label("Join Pregame Lobby by Default"), d);
    lobby_table->dual_add(join_meta_w, d);
    
    lobby_table->add_row(new w_spacer(), true);
    
    lobby->add(lobby_table, true);
    
    vertical_placer *stats = new vertical_placer();
    stats->dual_add(new w_hyperlink(A1_LEADERBOARD_URL, "Visit the leaderboards"), d);
    stats->add(new w_spacer(), true);
    
    horizontal_placer *stats_box = new horizontal_placer();
    
    w_toggle *allow_stats_w = new w_toggle(network_preferences.allow_stats);
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
    if (d.run() == 0) {    // Accepted
        bool changed = false;
        
        const std::string name = name_w->get_text();
        if (name != player_preferences.name)
        {
            player_preferences.name = name;
            changed = true;
        }
        
        const std::string metaserver_login = login_w->get_text();
        if (metaserver_login != network_preferences.metaserver_login)
        {
            network_preferences.metaserver_login = metaserver_login;
            changed = true;
        }
        
        // clear password if login has been cleared
        if (metaserver_login.empty() && !network_preferences.metaserver_password.empty())
        {
            network_preferences.metaserver_password.clear();
            changed = true;
        }
        else
        {
            const std::string metaserver_password = password_w->get_text();
            if (metaserver_password != network_preferences.metaserver_password)
            {
                network_preferences.metaserver_password = metaserver_password;
                changed = true;
            }
        }
        
        bool use_custom_metaserver_colors = custom_colors_w->get_selection();
        if (use_custom_metaserver_colors != network_preferences.use_custom_metaserver_colors)
        {
            network_preferences.use_custom_metaserver_colors = use_custom_metaserver_colors;
            changed = true;
        }
        
        if (use_custom_metaserver_colors)
        {
            SDL_Color primary_color = primary_w->get_selection();
            if (primary_color != network_preferences.metaserver_colors[0])
            {
                network_preferences.metaserver_colors[0] = primary_color;
                changed = true;
            }
            
            SDL_Color secondary_color = secondary_w->get_selection();
            if (secondary_color != network_preferences.metaserver_colors[1])
            {
                network_preferences.metaserver_colors[1] = secondary_color;
                changed = true;
            }
            
        }
        
        bool mute_metaserver_guests = mute_guests_w->get_selection() == 1;
        if (mute_metaserver_guests != network_preferences.mute_metaserver_guests)
        {
            network_preferences.mute_metaserver_guests = mute_metaserver_guests;
            changed = true;
        }
        
        bool join_meta = join_meta_w->get_selection() == 1;
        if (join_meta != network_preferences.join_metaserver_by_default)
        {
            network_preferences.join_metaserver_by_default = join_meta;
            changed = true;
        }
        
        bool allow_stats = allow_stats_w->get_selection() == 1;
        if (allow_stats != network_preferences.allow_stats)
        {
            network_preferences.allow_stats = allow_stats;
            Plugins::instance()->invalidate();
            changed = true;
        }
        
        
        if (changed)
            write_preferences();
    }
}
