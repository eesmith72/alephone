/*
 about_ao_dialog.cpp
 
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

#include "about_ao_dialog.hpp"

#include "alephversion.h"
#include "sdl_widgets.h"
#include "steamshim_child.h"




class w_authors_list : public w_string_list
{
public:
    w_authors_list(const std::vector<string>& items, dialog* d) :
        w_string_list(items, d, 0) {}

    void item_selected(void) { }
};


#ifdef HAVE_STEAM


struct steam_workshop_uploader_ui_data
{
    uint64_t item_id;
    int item_type;
    int content_type;
    ao_path directory_path;
    ao_path thumbnail_path;
    bool is_scenarios_compatible;
};

extern steam_game_information steam_game_info;



static item_upload_data steam_workshop_prepare_upload(steam_workshop_uploader_ui_data& data)
{
    item_upload_data workshop_item;

    if (steam_game_info.support_workshop_item_scenario && !data.is_scenarios_compatible)
    {
        const auto scenario_name = Scenario::instance()->GetName();

        if (scenario_name.empty())
        {
            throw std::runtime_error("The scenario for this item couldn't be deduced");
        }

        workshop_item.required_scenario = scenario_name;
    }

    workshop_item.id = data.item_id;
    workshop_item.item_type = static_cast<ItemType>(data.item_type);
    workshop_item.content_type = static_cast<ContentType>(data.content_type);
    workshop_item.directory_path = data.directory_path;
    workshop_item.thumbnail_path = data.thumbnail_path;
    return workshop_item;
}

static const STEAMSHIM_Event* steam_workshop_upload_result()
{
    const STEAMSHIM_Event* result = nullptr;
    bool end_of_upload = false;
    int progress_value = 0, upload_status = 0;

    std::unordered_map<int, int> upload_messages =
    {
        {STEAM_EItemUpdateStatus::k_EItemUpdateStatusPreparingConfig, _uploading_steam_workshop_prepare},
        {STEAM_EItemUpdateStatus::k_EItemUpdateStatusPreparingContent, _uploading_steam_workshop_prepare},
        {STEAM_EItemUpdateStatus::k_EItemUpdateStatusUploadingContent, _uploading_steam_workshop_upload},
        {STEAM_EItemUpdateStatus::k_EItemUpdateStatusUploadingPreviewFile, _uploading_steam_workshop_upload},
        {STEAM_EItemUpdateStatus::k_EItemUpdateStatusCommittingChanges, _uploading_steam_workshop_upload}
    };

    open_progress_dialog(_uploading_steam_workshop_default, true);

    while (STEAMSHIM_alive() && !end_of_upload)
    {
        progress_dialog_event();

        result = STEAMSHIM_pump();

        if (result)
        {
            switch (result->type)
            {
            case SHIMEVENT_WORKSHOP_UPLOAD_PROGRESS:
                if (upload_status != result->okay)
                {
                    upload_status = result->okay;
                    auto message_id = upload_messages.find(upload_status);
                    set_progress_dialog_message(message_id != upload_messages.end() ? message_id->second : _uploading_steam_workshop_default);
                }
                progress_value = result->ivalue;
                break;
            case SHIMEVENT_WORKSHOP_UPLOAD_RESULT:
                progress_value = 100;
                end_of_upload = true;
                break;
            default:
                break;
            }
        }

        draw_progress_bar(progress_value, 100);
    }

    close_progress_dialog();
    return result;
}

static void steam_workshop_upload_item_callback(void* arg)
{
    auto params = reinterpret_cast<std::pair<steam_workshop_uploader_ui_data*, dialog*>*>(arg);
    auto item = params->first;
    auto dialog = params->second;

    if (!item->item_id && !std::filesystem::is_directory(item->directory_path))
    {
        notify_user("The item directory is not valid.");
        return;
    }

    item_upload_data steam_upload_data;

    try
    {
        steam_upload_data = steam_workshop_prepare_upload(*item);
    }
    catch (std::exception& ex)
    {
        notify_user(ex.what());
        return;
    }

    STEAMSHIM_uploadWorkshopItem(steam_upload_data);
    auto result = steam_workshop_upload_result();

    if (result && result->type == SHIMEVENT_WORKSHOP_UPLOAD_RESULT)
    {
        if (result->okay)
        {
            std::string message = "Your item was correctly uploaded on Steam.";
            message += result->needs_to_accept_workshop_agreement ? " However, your item will remain hidden until you accept the Steam workshop legal agreement." : "";
            notify_user(message.c_str(), alert_level_t::info);
            dialog->quit(0);
        }
        else
        {
            std::string error_code = std::to_string(result->ivalue);
            std::string message = "Your item couldn't be uploaded on Steam. Steam error code was: " + error_code;
            notify_user(message.c_str());
        }
    }
    else
        notify_user("Something went wrong while uploading to Steam. Restart the game and try again.");
}

static item_owned_query_result steam_get_owned_items(const std::string& scenario_name)
{
    open_progress_dialog(_loading);

    STEAMSHIM_queryWorkshopItemOwned(scenario_name);

    const STEAMSHIM_Event* result = nullptr;
    while (STEAMSHIM_alive())
    {
        progress_dialog_event();

        result = STEAMSHIM_pump();

        if (result && result->type == SHIMEVENT_WORKSHOP_QUERY_ITEM_OWNED_RESULT)
        {
            break;
        }
    }

    close_progress_dialog();
    return result ? result->items_owned : item_owned_query_result { 0 };
}

static void display_steam_workshop_uploader_dialog(void* arg)
{
    animate_interface_fade_out(false);

    const auto scenario_name = Scenario::instance()->GetName();

    auto item_list = steam_get_owned_items(scenario_name);
    std::vector<std::string> item_labels;

    item_owned_query_result::item new_item;
    new_item.id = 0;
    new_item.title = "New Item";
    new_item.item_type = ItemType::Plugin;
    new_item.content_type = ContentType::Graphics;
    new_item.is_scenarios_compatible = true;
    item_list.items.insert(item_list.items.begin(), new_item);

    if (item_list.result_code != 1)
    {
        std::string error_code = std::to_string(item_list.result_code);
        std::string message = "Your workshop items couldn't be retrieved from Steam. Steam error code was: " + error_code;
        notify_user(message.c_str());
    }

    for (const auto& item : item_list.items)
    {
        auto label = item.title.size() > 28 ? item.title.substr(0, 25) + "..." : item.title;
        item_labels.push_back(label);
    }

    steam_workshop_uploader_ui_data ui_data;
    ui_data.item_type = static_cast<int>(new_item.item_type);
    ui_data.content_type = static_cast<int>(new_item.content_type);
    ui_data.is_scenarios_compatible = new_item.is_scenarios_compatible;
    ui_data.item_id = new_item.id;

    dialog d;

    auto placer = new vertical_placer;

    placer->dual_add(new w_title("STEAM WORKSHOP UPLOADER"), d);
    placer->add(new w_spacer, true);

    auto table = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
    table->col_flags(0, placeable::kAlignRight);

    auto items_popup = new w_select_popup();
    items_popup->set_labels(item_labels);
    items_popup->set_selection(0);
    table->dual_add(items_popup->adding_label("Upload For"), d);
    table->dual_add(items_popup, d);

    auto get_content_types_tags = [&](ItemType item_type) -> std::vector<std::string>
    {
        static const std::vector<std::string> content_types_tags[] = {
            { },
            { "Graphics", "HUD", "Music", "Script", "Theme" },
            { "Solo & Net", "Solo Only", "Net Only" }
        };

        switch (item_type)
        {
            case ItemType::Scenario:
                return content_types_tags[0];
            case ItemType::Plugin:
                return content_types_tags[1];
            default:
                return content_types_tags[2];
        }
    };

    std::vector<std::string> item_types = { "Plugin", "Map", "Physics", "Script", "Sounds", "Shapes" };

    if (steam_game_info.support_workshop_item_scenario)
    {
        item_types.insert(item_types.begin(), "Scenario");
    }

    auto item_types_popup = new w_select_popup();
    item_types_popup->set_labels(item_types);
    item_types_popup->set_selection(steam_game_info.support_workshop_item_scenario ? static_cast<int>(new_item.item_type) : static_cast<int>(new_item.item_type) - 1);

    table->dual_add(item_types_popup->adding_label("Item Type"), d);
    table->dual_add(item_types_popup, d);

    auto content_types_popup = new w_select_popup();
    content_types_popup->set_labels(get_content_types_tags(new_item.item_type));
    content_types_popup->set_selection(0);

    table->dual_add(content_types_popup->adding_label("Content Type"), d);
    table->dual_add(content_types_popup, d);

    char label[64];
    snprintf(label, 64, "%s Only", Scenario::instance()->GetName().c_str());
    auto custom_scenarios_label = new w_label(label);
    auto custom_scenarios = new w_toggle(false);
    custom_scenarios->set_label(custom_scenarios_label);
    custom_scenarios->visible(steam_game_info.support_workshop_item_scenario);
    custom_scenarios_label->visible(steam_game_info.support_workshop_item_scenario);

    table->dual_add(custom_scenarios_label, d);
    table->dual_add(custom_scenarios, d);

    if (steam_game_info.support_workshop_item_scenario)
    {
        table->add_row(new w_spacer(), true);
    }

    auto thumbnail_path = new w_file_chooser("Choose Preview Image", _typecode_unknown);
    table->dual_add(thumbnail_path->adding_label("Preview Image"), d);
    table->dual_add(thumbnail_path, d);

    auto directory_path = new w_directory_chooser();
    table->dual_add(directory_path->adding_label("Item Directory"), d);
    table->dual_add(directory_path, d);

    placer->add(table, true);

    placer->add(new w_spacer, true);

    placer->dual_add(new w_hyperlink("https://steamcommunity.com/sharedfiles/workshoplegalagreement",
                                     "By submitting this item, you agree to the workshop terms of service"), d);

    placer->add(new w_spacer, true);

    auto button_placer = new horizontal_placer;

    auto callback_params = std::make_pair<steam_workshop_uploader_ui_data*, dialog*>(&ui_data, &d);
    button_placer->dual_add(new w_button("UPLOAD", steam_workshop_upload_item_callback, &callback_params), d);
    button_placer->dual_add(new w_button("RETURN", dialog_cancel, &d), d);

    placer->add(button_placer, true);

    d.set_widget_placer(placer);

    auto can_update_content_type = [&]() -> bool
    {
        switch (static_cast<ItemType>(ui_data.item_type))
        {
            case ItemType::Plugin:
                return !ui_data.item_id;
            case ItemType::Map:
            case ItemType::Script:
                return true;
            default:
                return false;
        }
    };

    auto update_content_type_value = [&]()
    {
        int start_enum_index;
        switch (static_cast<ItemType>(ui_data.item_type))
        {
            case ItemType::Scenario:
                start_enum_index = static_cast<int>(ContentType::START_SCENARIO);
                break;
            case ItemType::Plugin:
                start_enum_index = static_cast<int>(ContentType::START_PLUGIN);
                break;
            default:
                start_enum_index = static_cast<int>(ContentType::START_OTHER);
                break;
        }

        ui_data.content_type = start_enum_index + std::max(content_types_popup->get_selection(), 0);
    };

    auto get_selection_for_content_type = [&]() -> int
    {
        switch (static_cast<ItemType>(ui_data.item_type))
        {
            case ItemType::Scenario:
                return ui_data.content_type - static_cast<int>(ContentType::START_SCENARIO);
            case ItemType::Plugin:
                return ui_data.content_type - static_cast<int>(ContentType::START_PLUGIN);
            default:
                return ui_data.content_type - static_cast<int>(ContentType::START_OTHER);
        }
    };

    auto update_common_widgets = [&]()
    {
        bool is_scenario = static_cast<ItemType>(ui_data.item_type) == ItemType::Scenario;
        custom_scenarios->set_enabled(!is_scenario);
        custom_scenarios->set_selection(!ui_data.is_scenarios_compatible);

        content_types_popup->set_labels(get_content_types_tags(static_cast<ItemType>(ui_data.item_type)));
        content_types_popup->set_enabled(can_update_content_type());
    };

    items_popup->set_popup_callback([&](void*)
    {
        auto item_index = items_popup->get_selection();
        auto& item = item_list.items.at(item_index);

        ui_data.item_id = item.id;
        ui_data.item_type = static_cast<int>(item.item_type);
        ui_data.content_type = static_cast<int>(item.content_type);
        ui_data.directory_path = "";
        ui_data.thumbnail_path = "";
        ui_data.is_scenarios_compatible = item.is_scenarios_compatible;

        item_types_popup->set_selection(steam_game_info.support_workshop_item_scenario ? ui_data.item_type : ui_data.item_type - 1);
        item_types_popup->set_enabled(!ui_data.item_id);

        directory_path->set_directory(ui_data.directory_path);
        thumbnail_path->set_file(ui_data.thumbnail_path);

        update_common_widgets();
        content_types_popup->set_selection(get_selection_for_content_type());

    }, nullptr);

    item_types_popup->set_popup_callback([&](void*)
    {
        ui_data.item_type = steam_game_info.support_workshop_item_scenario ? item_types_popup->get_selection() : item_types_popup->get_selection() + 1;
        update_common_widgets();
        content_types_popup->set_selection(0);
        update_content_type_value();

    }, nullptr);

    content_types_popup->set_popup_callback([&](void*)
    {
        update_content_type_value();

    }, nullptr);

    custom_scenarios->set_selection_changed_callback([&](void*)
    {
        ui_data.is_scenarios_compatible = !custom_scenarios->get_selection();
    });

    directory_path->set_callback([&]()
    {
        ui_data.directory_path = directory_path->get_directory();
    });

    thumbnail_path->set_callback([&]()
    {
        ui_data.thumbnail_path = thumbnail_path->get_file();
    });

    main_screen.clear();

    d.run();
}

#endif




void display_about_ao_dialog()
{
    dialog d;

    tab_placer* tabs = new tab_placer();

    vertical_placer* placer = new vertical_placer;
    std::vector<std::string> labels;
    labels.push_back("ABOUT");
    labels.push_back("AUTHORS");
    w_tab *tab_w = new w_tab(labels, tabs);
    
    placer->dual_add(new w_title("ALEPH ONE"), d);
    placer->add(new w_spacer, true);

    placer->dual_add(tab_w, d);
    placer->add(new w_spacer, true);

    vertical_placer* about_placer = new vertical_placer;
    
    if (get_application_name().compare("Aleph One") != 0)
    {
        about_placer->dual_add(new w_static_text(expand_string_vars("$appName$ is powered by")), d);
    }
#ifdef HAVE_STEAM
    about_placer->dual_add(new w_static_text(expand_string_vars("Aleph One $appVersion$ Steam ($appDate$)")), d);
#else
    about_placer->dual_add(new w_static_text(expand_string_vars("Aleph One $appVersion$ ($appDate$)")), d);
#endif

    about_placer->add(new w_spacer, true);

    about_placer->dual_add(new w_hyperlink(A1_HOMEPAGE_URL), d);

    about_placer->add(new w_spacer(2 * get_theme_space(SPACER_WIDGET)), true);
    
    about_placer->dual_add(new w_static_text("Aleph One is free software with ABSOLUTELY NO WARRANTY."), d);
    about_placer->dual_add(new w_static_text("You are welcome to redistribute it under certain conditions."), d);
    about_placer->dual_add(new w_hyperlink("http://www.gnu.org/licenses/gpl-3.0.html"), d);

    about_placer->add(new w_spacer, true);

    about_placer->dual_add(new w_static_text("This license does not apply to game content."), d);

    about_placer->add(new w_spacer, true);

    about_placer->dual_add(new w_static_text(expand_string_vars("Scenario loaded: $scenarioName$ $scenarioVersion$")), d);

#ifdef HAVE_STEAM
    about_placer->add(new w_spacer, true);
    about_placer->dual_add(new w_button("STEAM WORKSHOP UPLOADER", display_steam_workshop_uploader_dialog, &d), d);
#endif

    vertical_placer *authors_placer = new vertical_placer();
    
    authors_placer->dual_add(new w_static_text("Aleph One is based on the source code for Marathon 2 and"), d);
    authors_placer->dual_add(new w_static_text("Marathon Infinity, which was developed by Bungie software."), d);
    authors_placer->add(new w_spacer, true);
    
    authors_placer->dual_add(new w_static_text("The enhancements and extensions to Marathon 2 and Marathon"), d);
    authors_placer->dual_add(new w_static_text("Infinity that constitute Aleph One have been made by:"), d);

    authors_placer->add(new w_spacer, true);

    std::vector<std::string> authors;
    authors.push_back("Joey Adams");
    authors.push_back("Michael Adams (mdmkolbe)");
    authors.push_back("Falko Axmann");
    authors.push_back("Christian Bauer");
    authors.push_back("Mike Benonis");
    authors.push_back("Steven Bytnar");
    authors.push_back("Glen Ditchfield");
    authors.push_back("Will Dyson");
    authors.push_back("Carl Gherardi");
    authors.push_back("Thomas Herzog");
    authors.push_back("Chris Hallock (LidMop)");
    authors.push_back("Benoît Hauquier (Kolfering)");
    authors.push_back("Peter Hessler");
    authors.push_back("Matthew Hielscher");
    authors.push_back("Rhys Hill");
    authors.push_back("Alan Jenkins");
    authors.push_back("Solra Bizna");
    authors.push_back("Jeremy, the MSVC guy");
    authors.push_back("Mark Levin");
    authors.push_back("Bo Lindbergh");
    authors.push_back("Chris Lovell");
    authors.push_back("Jesse Luehrs");
    authors.push_back("Marshall (darealshinji)");
    authors.push_back("Derek Moeller");
    authors.push_back("Jeremiah Morris");
    authors.push_back("Sam Morris");
    authors.push_back("Benoit Nadeau (Benad)");
    authors.push_back("Mihai Parparita");
    authors.push_back("Jeremy Parsons (brefin)");
    authors.push_back("Eric Peterson");
    authors.push_back("Loren Petrich");
    authors.push_back("Ian Pitcher");
    authors.push_back("Chris Pruett");
    authors.push_back("Matthew Reda");
    authors.push_back("Ian Rickard");
    authors.push_back("Etienne Samson (tiennou)");
    authors.push_back("Catherine Seppanen");
    authors.push_back("Gregory Smith (treellama)");
    authors.push_back("Scott Smith (pickle136)");
    authors.push_back("Wolfgang Sourdeau");
    authors.push_back("Peter Stirling");
    authors.push_back("Alexander Strange (mrvacbob)");
    authors.push_back("Alexei Svitkine");
    authors.push_back("Ben Thompson");
    authors.push_back("TrajansRow");
    authors.push_back("Clemens Unterkofler (hogdotmac)");
    authors.push_back("James Willson");
    authors.push_back("Woody Zenfell III");

    w_authors_list *authors_w = new w_authors_list(authors, &d);
    authors_placer->dual_add(authors_w, d);

    tabs->add(about_placer, true);
    tabs->add(authors_placer, true);

    placer->add(tabs, true);
    
    placer->add(new w_spacer, true);

    placer->dual_add(new w_button("OK", dialog_ok, &d), d);
    
    d.set_widget_placer(placer);
    
    d.run();
}

