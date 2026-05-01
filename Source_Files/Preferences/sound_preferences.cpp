/*
 sound_preferences.hpp
 
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

#include "sound_preferences.hpp"

#include "preferences_support.hpp"


//*****************************************************************************
// PREFERENCES
//*****************************************************************************


sound_preferences_t sound_preferences;


void sound_preferences_t::read(InfoTree root, std::string version)
{
    if (!version.length() || version < "20200803")
    {
        int old_volume;
        root.read_attr("volume", old_volume);
        if (old_volume > 0)
        {
            volume_db = 10.f * std::log10(static_cast<float>(old_volume) / NUMBER_OF_SOUND_VOLUME_LEVELS);
        }
        else
        {
            volume_db = sound_preferences_t::MINIMUM_VOLUME_DB;
        }
    }
    else
    {
        root.read_attr("volume_db", volume_db);
    }

    if (!version.length() || version < "20200803")
    {
        int old_music_volume;
        root.read_attr("music_volume", old_music_volume);
        if (old_music_volume > 0)
        {
            music_db = 10.f * std::log10(static_cast<float>(old_music_volume) / NUMBER_OF_SOUND_VOLUME_LEVELS);
        }
        else
        {
            music_db = sound_preferences_t::MINIMUM_VOLUME_DB / 2;
        }
    }
    else
    {
        root.read_attr("music_db", music_db);
    }

    root.read_attr("flags", flags);
    root.read_attr("rate", rate);
    root.read_attr("samples", samples);
    root.read_attr("video_export_volume_db", video_export_volume_db);

    int channel;
    if (root.read_attr("channel", channel)) { channel_type = static_cast<ChannelType>(channel); }
    
    volume_db = std::clamp(volume_db, MINIMUM_VOLUME_DB, MAXIMUM_VOLUME_DB);
}


InfoTree sound_preferences_t::write()
{
    InfoTree root;
    
    root.put_attr("volume_db", volume_db);
    root.put_attr("music_db", music_db);
    root.put_attr("flags", flags);
    root.put_attr("rate", rate);
    root.put_attr("samples", samples);
    root.put_attr("video_export_volume_db", video_export_volume_db);
    root.put_attr("channel", static_cast<int>(channel_type));

    return root;
}


//*****************************************************************************
// DIALOGS
//*****************************************************************************


class w_volume_slider : public w_percentage_slider
{
public:
    w_volume_slider(int vol) : w_percentage_slider(21, vol) {}
    ~w_volume_slider() {}

    void item_selected()
    {
        if (sound_manager.IsActive())
        {
            OpenALManager::Get()->SetMasterVolume(SoundManager::From_db((selection - 20) * 2));
            sound_manager.PlaySound(_snd_adjust_volume, 0, NONE);
        }
    }
};


class w_music_slider : public w_slider
{
public:
    w_music_slider(int sel) : w_slider(41, sel) {}

    void item_selected()
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


void sound_dialog(void *arg)
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
    channel_w->set_selection(mapping_channel_index.at(sound_preferences.channel_type));
    table->dual_add(channel_w->adding_label("Channels"), d);
    table->dual_add(channel_w, d);

    w_toggle* dynamic_w = new w_toggle(TEST_FLAG(sound_preferences.flags, _dynamic_tracking_flag));
    table->dual_add(dynamic_w->adding_label("Active Panning"), d);
    table->dual_add(dynamic_w, d);

    bool is_3d_sounds_enabled = sound_preferences.flags & _3d_sounds_flag;
    w_toggle *sounds3d_w = new w_toggle(is_3d_sounds_enabled);
    table->dual_add(sounds3d_w->adding_label("3D Sounds"), d);
    table->dual_add(sounds3d_w, d);

    w_toggle *hrtf_w = new w_toggle((OpenALManager::Get() && OpenALManager::Get()->IsHrtfEnabled()) || sound_preferences.flags & _hrtf_flag);
    table->dual_add(hrtf_w->adding_label("HRTF (Headphones)"), d);
    table->dual_add(hrtf_w, d);
    hrtf_w->set_enabled(OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() != OpenALManager::HrtfSupport::Unsupported && is_3d_sounds_enabled && sound_preferences.channel_type == ChannelType::_stereo);

    auto hrtf_enable_callback = [&](void*) {
        bool can_enable_hrtf = sounds3d_w->get_selection() == 1
            && mapping_index_channel.at(channel_w->get_selection()) == ChannelType::_stereo
            && OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() == OpenALManager::HrtfSupport::Supported;

        hrtf_w->set_enabled(can_enable_hrtf);
        hrtf_w->set_selection(
            (can_enable_hrtf && hrtf_w->get_selection() == 1) ||
                              (!can_enable_hrtf && OpenALManager::Get() && OpenALManager::Get()->GetHrtfSupport() == OpenALManager::HrtfSupport::Required)
        );
    };

    sounds3d_w->set_selection_changed_callback(hrtf_enable_callback);
    channel_w->set_popup_callback(hrtf_enable_callback, nullptr);

    table->add_row(new w_spacer(), true);

    w_volume_slider *volume_w = new w_volume_slider(static_cast<int>(sound_preferences.volume_db / 2 + 20));
    table->dual_add(volume_w->adding_label("Master Volume"), d);
    table->dual_add(volume_w, d);

    w_slider *music_volume_w = new w_music_slider(sound_preferences.music_db + 20);
    table->dual_add(music_volume_w->adding_label("Music Volume"), d);
    table->dual_add(music_volume_w, d);

    table->add_row(new w_spacer(), true);
    
    w_toggle *quality_w = new w_toggle(TEST_FLAG(sound_preferences.flags, _16bit_sound_flag), quality_labels);
    table->dual_add(quality_w->adding_label("Source"), d);
    table->dual_add(quality_w, d);

    w_toggle *ambient_w = new w_toggle(TEST_FLAG(sound_preferences.flags, _ambient_sound_flag));
    table->dual_add(ambient_w->adding_label("Ambient Sounds"), d);
    table->dual_add(ambient_w, d);

    w_toggle *more_w = new w_toggle(TEST_FLAG(sound_preferences.flags, _more_sounds_flag));
    table->dual_add(more_w->adding_label("More Sounds"), d);
    table->dual_add(more_w, d);

    table->add_row(new w_spacer(), true);
    table->dual_add_row(new w_static_text("Interface Sounds"), d);
    
    w_toggle *button_sounds_w = new w_toggle(TEST_FLAG(input_preferences.modifiers, _inputmod_use_button_sounds));
    table->dual_add(button_sounds_w->adding_label("In Game (F-key)"), d);
    table->dual_add(button_sounds_w, d);

    w_toggle *dialog_sounds_w = new w_toggle(!TEST_FLAG(sound_preferences.flags, _mute_dialogs));
    table->dual_add(dialog_sounds_w->adding_label("Dialogs"), d);
    table->dual_add(dialog_sounds_w, d);

    table->add_row(new w_spacer(), true);
    table->dual_add_row(new w_static_text("Experimental Sound Options"), d);
        w_toggle *zrd_w = new w_toggle(TEST_FLAG(sound_preferences.flags, _lower_restart_delay));
    table->dual_add(zrd_w->adding_label("Rapid-fire Sounds"), d);
    table->dual_add(zrd_w, d);

    placer->add(table, true);

    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
    
    placer->add(button_placer, true);

    d.set_widget_placer(placer);
    
    clear_screen();
    if (d.run() == 0)
    {
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

        if (flags != sound_preferences.flags)
        {
            sound_preferences.flags = flags;
            changed = true;
        }
        
        // TODO: just fricking garbage
        flags = input_preferences.modifiers & ~_inputmod_use_button_sounds;
        if (button_sounds_w->get_selection()) flags |= _inputmod_use_button_sounds;
        if (flags != input_preferences.modifiers)
        {
            input_preferences.modifiers = flags;
            changed = true;
        }

        float volume_db = (volume_w->get_selection() - 20) * 2;
        if (volume_db != sound_preferences.volume_db)
        {
            sound_preferences.volume_db = volume_db;
            changed = true;
        }

        float music_db = music_volume_w->get_selection() - 20;
        if (music_db != sound_preferences.music_db)
        {
            sound_preferences.music_db = music_db;
            changed = true;
        }

        auto channel = mapping_index_channel.at(channel_w->get_selection());
        if (channel != sound_preferences.channel_type)
        {
            sound_preferences.channel_type = channel;
            changed = true;
        }

        if (changed)
        {
            auto slot = Music::instance()->GetSlot(Music::MusicSlot::Intro);
            bool is_music_playing = slot && slot->Playing();
            
            sound_manager.SetStatus(true);
            
            write_preferences();
            if (is_music_playing) Music::instance()->RestartIntroMusic();
        }
    }
    else if (OpenALManager::Get())
    {
        OpenALManager::Get()->SetMasterVolume(SoundManager::From_db(sound_preferences.volume_db));
        OpenALManager::Get()->SetMusicVolume(SoundManager::From_db(sound_preferences.music_db, true));
    }
}

