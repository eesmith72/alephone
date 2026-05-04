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

#ifndef sound_preferences_hpp
#define sound_preferences_hpp

#include "cseries.hpp"

#include "InfoTree.h"



const int32_t NUMBER_OF_SOUND_VOLUME_LEVELS = 8;

const int32_t MAXIMUM_SOUND_VOLUME_BITS = 8;

const int32_t MAXIMUM_SOUND_VOLUME = 1 << MAXIMUM_SOUND_VOLUME_BITS;



enum // sound sources
{
    _8bit_22k_source,
    _16bit_22k_source,
    
    NUMBER_OF_SOUND_SOURCES
};


enum class AudioFormat : int32_t
{
    _8_bit,
    _16_bit,
    _32_float
};


enum class ChannelType : int32_t
{
    _mono   = 1,
    _stereo = 2,
    _quad   = 4,
    _5_1    = 6,
    _6_1    = 7,
    _7_1    = 8
};




struct sound_preferences_t
{
    // master and music volumes are now in dB
    static constexpr float   DEFAULT_SOUND_LEVEL_DB         = -8.f;
    static constexpr float   MAXIMUM_VOLUME_DB              = 0.f;
    static constexpr float   MINIMUM_VOLUME_DB              = -40.f;
    static constexpr float   DEFAULT_MUSIC_LEVEL_DB         = -12.f;
    static constexpr float   DEFAULT_VIDEO_EXPORT_VOLUME_DB = -8.f;
    static constexpr int32_t MAX_SOUNDS_FOR_SOURCE          = 3;
    static constexpr int32_t DEFAULT_RATE                   = 44100;
    static constexpr int32_t DEFAULT_SAMPLES                = 1024;
    
    float volume_db; // db
    float music_db;  // music volume in dB
    uint16 rate;     // in Hz
    uint16 samples;  // size of buffer
    
    bool ambient_sound;       // plays and tracks ambient sounds [prefs]
    bool use_3d_sounds;       // enable 3D sounds instead of emulating 2D panning
    bool use_hrtf;            // play sounds using HRTF [prefs]
    bool lower_restart_delay; // ghs: restart sounds faster
    bool ui_sounds;           // enable dialog button sounds
    
    float video_export_volume_db; // TODO: odd; would expect export volume to be permanently fixed (it's set to DEFAULT_VIDEO_EXPORT_VOLUME_DB and there's no GUI control for adjusting it but it is stored in Prefs file)

    ChannelType channel_type;

    void reset()
    {
        volume_db              = DEFAULT_SOUND_LEVEL_DB;
        music_db               = DEFAULT_MUSIC_LEVEL_DB;
        rate                   = DEFAULT_RATE;
        samples                = DEFAULT_SAMPLES;
        
        ambient_sound          = true;
        use_3d_sounds          = false;
        use_hrtf               = false;
        lower_restart_delay    = false;
        ui_sounds              = false; // TODO: replace the awful sounds and make true

        video_export_volume_db = DEFAULT_VIDEO_EXPORT_VOLUME_DB;
        
        channel_type           = ChannelType::_stereo;
    }
    
    void read(InfoTree root, std::string version);

    InfoTree write();
};




extern sound_preferences_t sound_preferences;




void sound_dialog(void *arg);



#endif /* sound_preferences_hpp */
