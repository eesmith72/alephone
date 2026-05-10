/*
 chapter_screens.cpp
 
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

#include "chapter_screens.hpp"

#include "interface_fades.hpp"

#include "shapes.h" // shapes_file_is_m1
#include "visual_effects.hpp"
#include "SoundManager.h"
#include "Music.h"
#include "images.h" // get_sound_resource_from_images
#include "Screen.hpp" // main_screen.clear
//#include "main_menu.hpp" // display_main_menu (temporary till these functions are unknotted)
#include "mouse.h" // hide_cursor
#include "XML_LevelScript.h" // EndScreenIndex, NumEndScreens
#include "Statistics.h" // StatsManager, used in display_shutdown_screen

#include "shell_options.h" // annoying bit of coupling

#include "movie_screen.hpp"

#include "ImageBlitter.hpp"

#include "FilmExporter.h"


//************************************************************************************************
// sounds/music


#define M1_STARTUP_SOUND_ID (1240)

static std::shared_ptr<SoundPlayer> introduction_sound = nullptr;

static LoadedResource SoundRsrc;

static void play_optional_sound_resource(int32_t resource_id, bool is_m1, ao_fixed pitch = _normal_frequency)
{
    if (introduction_sound)
    {
        introduction_sound->AskStop();
        introduction_sound.reset();
    }
    SoundRsrc.Unload();
    // bodgy, but redoing API is for another time
    if (is_m1 ? get_sound_resource_from_sounds(resource_id, SoundRsrc) : get_sound_resource_from_images(resource_id, SoundRsrc))
    {
        SoundParameters parameters;
        parameters.pitch = pitch * 1.f / _normal_frequency;
        introduction_sound = sound_manager.PlaySound(SoundRsrc, parameters);
    }
}



typedef void (*start_audio_proc)(int32_t screen_id);


void play_m1_startup_sound(int32_t screen_id)
{
    switch (screen_id)
    {
        case M1_STARTUP_SCREEN_BASE:
        case M1_EPILOGUE_SCREEN_BASE:
            play_optional_sound_resource(M1_STARTUP_SOUND_ID, true);
            break;
            
        case 1114:
        case M1_EPILOGUE_SCREEN_BASE + 1:
            play_optional_sound_resource(M1_STARTUP_SOUND_ID, true, _m1_high_frequency);
            break;
            
        default:
        {}
    }
}


void play_m2_chapter_sound(int32_t screen_id)
{
    play_optional_sound_resource(screen_id, false);
}


void play_m2_startup_music(int32_t screen_id)
{
    Music::instance()->RestartIntroMusic();
}


void play_m2_epilogue_music(int32_t screen_id)
{
    Music::instance()->RestartIntroMusic();
    auto ticks = machine_tick_count();
    do { Music::instance()->Idle(); } while (machine_tick_count() - ticks < 10);
}


//************************************************************************************************
// screens config

// this could be inlined in display_ functions but going to leave it as table for now; TODO: how practical to move screen-specific behaviors, e.g. starting music, into table?

struct screen_data_t
{
    int32_t base_id;
    int32_t screen_count;
    uint32_t duration;
    bool slow_scroll;
    start_audio_proc sound;
    
    int32_t last_id() const {
        return base_id + screen_count - 1;
    }
};

struct screen_t
{
    app_state_t state;
    screen_data_t m1;
    screen_data_t m2;
};


#define STARTUP_SCREEN_DURATION     (215     * MACHINE_TICKS_PER_SECOND / TICKS_PER_SECOND) // fudge to align with sound
#define PROLOGUE_DURATION           (10      * MACHINE_TICKS_PER_SECOND)
#define CREDIT_SCREEN_DURATION      (15 * 60 * MACHINE_TICKS_PER_SECOND)
#define CHAPTER_SCREEN_DURATION     (7       * MACHINE_TICKS_PER_SECOND)


static const std::array<screen_t, 8> screens_std = {(screen_t)
    {app_state_t::startup_screen,   {M1_STARTUP_SCREEN_BASE,    4,   STARTUP_SCREEN_DURATION,   false,  play_m1_startup_sound},
                                    {M2_STARTUP_SCREEN_BASE,    3,   STARTUP_SCREEN_DURATION,   false,  play_m2_startup_music}},
    
    {app_state_t::prologue_screen,  {PROLOGUE_SCREEN_BASE,      1,   PROLOGUE_DURATION,         true,   nullptr},
                                    {PROLOGUE_SCREEN_BASE,      1,   PROLOGUE_DURATION,         true,   nullptr}},
    
    {app_state_t::chapter_screen,   {M1_CHAPTER_SCREEN_BASE,    1,   CHAPTER_SCREEN_DURATION,   false, play_m1_startup_sound},
                                    {M2_CHAPTER_SCREEN_BASE,    1,   CHAPTER_SCREEN_DURATION,   false, play_m2_chapter_sound}},
    
    {app_state_t::epilogue_screen,  {M1_EPILOGUE_SCREEN_BASE,   2,   INFINITE_TIME_DELAY,       true,   play_m1_startup_sound},
                                    {M2_EPILOGUE_SCREEN_BASE,   1,   INFINITE_TIME_DELAY,       true,   play_m2_epilogue_music}},
    
    {app_state_t::credit_screen,    {M1_CREDIT_SCREEN_BASE,     1,   CREDIT_SCREEN_DURATION,    false,  nullptr},
                                    {M2_CREDIT_SCREEN_BASE,     7,   CREDIT_SCREEN_DURATION,    false,  nullptr}},
    
    {app_state_t::shutdown_screen,  {SHUTDOWN_SCREEN_BASE,      1,   INFINITE_TIME_DELAY,       false,  nullptr},
                                    {SHUTDOWN_SCREEN_BASE,      1,   INFINITE_TIME_DELAY,       false,  nullptr}},
};


// scenarios may use MML to change value of M2_EPILOGUE_SCREEN_BASE and number of epilogues; this is awkward and annoying, and possibly problematic, but we have to support it here for backwards compatibility with existing scenarios
screen_data_t epilogue_screen_data;


const screen_data_t* screen_data;

app_state_t screen_type;

static int32_t current_screen_id = 0;

SDL_Surface* screen_surface = nullptr;


const screen_data_t* get_data_for_screen_type(app_state_t screen_type)
{
    for (const auto& screen : screens_std)
    {
        if (screen_type == screen.state)
        {
            return shapes_file_is_m1() ? &screen.m1 : &screen.m2;
        }
    }
    throw_bug_report_f("invalid screen type: %d", screen_type);
}



//************************************************************************************************
// load and display

ImageBlitter screen_blitter;


ao_err advance_to_next_screen()
{
    do
    {
        current_screen_id++;
        
        // EES: how confident am I that all pict IDs are unique across scenario? not entirely, so hedging bets here for now // TODO: ideally this can be folded into a single `get_pict_resource_from_scenario(resource_id)` in future
        if (get_app_state() == app_state_t::chapter_screen)
        {
            screen_surface = get_pict_resource_from_map(current_screen_id);
            if (!screen_surface) { screen_surface = get_pict_resource_from_images(current_screen_id); }
        }
        else
        {
            screen_surface = get_pict_resource_from_images(current_screen_id);
            if (!screen_surface) { screen_surface = get_pict_resource_from_map(current_screen_id); }
        }
    }
    while (!screen_surface && current_screen_id < screen_data->last_id());
    
    return screen_surface ? no_err : STRID(strERRORS, pictureNotFound);
}


ao_err load_screen_sequence(app_state_t screen_type)
{
    ::screen_type = screen_type;
    screen_data = get_data_for_screen_type(screen_type);
    
    // scenarios may fiddle with epilogue screen in MML (which is annoyingly half-assed); we support for backwards compatibility
    if (screen_type == app_state_t::epilogue_screen)
    {
        epilogue_screen_data = *screen_data;
        get_epilogue_screen_base_id_and_count(epilogue_screen_data.base_id, epilogue_screen_data.screen_count);
        screen_data = &epilogue_screen_data;
    }
    
    current_screen_id = screen_data->base_id - 1;
    
    return advance_to_next_screen();
}


uint32_t display_current_screen() // displays the currently selected screen in the loaded screen sequence; returns the timeout in machine ticks to display it
{
    // TODO: not sure why chapter screen had these; sounds should be stopped when exiting the previous level
    //Music::instance()->StopInGameMusic();
    //sound_manager.StopAllSounds();

    main_screen.clear();
    main_screen.configure_for_classic_ui(); // TODO: make this configurable in scenario
    
    screen_blitter.borrow_surface(screen_surface);
    
    if (screen_data->sound) { screen_data->sound(current_screen_id); }
    
    set_interface_fade_renderer([](float opacity){ screen_blitter.render_to_screen(); });
    
    // TODO: fade in+out durations and fade_music flag should probably be in table above
    animate_interface_fade_in(LONG_FADE_DURATION);

    // TODO: what about animating scrolling image? consider pushing this out to Lua script
    
    return screen_data->duration;
}


// TODO: integrate scrolling animation support


#define SCROLLING_SPEED (MACHINE_TICKS_PER_SECOND / 20)

static void animate_scrolling_screen(ImageBlitter* blitter, bool is_slow_text_scroll)
{
    // Find out in which direction to scroll // TODO: legacy importer should calculate direction, speed, etc
    int picture_width       = blitter->width();
    int picture_height      = blitter->height();
    int screen_width        = 640;
    int screen_height       = 480;
    bool scroll_horizontal  = picture_width > screen_width;
    bool scroll_vertical    = picture_height > screen_height;

    if (scroll_horizontal || scroll_vertical)
    {

        // Flush events
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

        // Prepare source and destination rectangles
        SDL_Rect src_rect = {0, 0, scroll_horizontal ? screen_width : picture_width, scroll_vertical ? screen_height : picture_height};
        //SDL_Rect dst_rect = {0, 0, screen_width, screen_height};

        // Scroll loop
        bool done = false, aborted = false;
        uint64_t start_tick = machine_tick_count();
        
        // this function runs its own event loop, which isn't ideal
        do
        {
            int32_t delta = (int32_t)((machine_tick_count() - start_tick) / (is_slow_text_scroll ? (2 * SCROLLING_SPEED) : SCROLLING_SPEED));
            if (scroll_horizontal && delta > picture_width - screen_width)
            {
                delta = picture_width - screen_width;
                done = true;
            }
            if (scroll_vertical && delta > picture_height - screen_height)
            {
                delta = picture_height - screen_height;
                done = true;
            }

            // Blit part of picture
            src_rect.x = scroll_horizontal ? delta : 0;
            src_rect.y = scroll_vertical ? delta : 0;
            
            blitter->render_to_screen(nullptr, &src_rect);
            main_screen.swap();
            
            // Give system time
            update_audio_on_idle();
            yield();

            // Check for events to abort
            SDL_Event event;
            if (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_KEYDOWN:
                    case SDL_CONTROLLERBUTTONDOWN:
                        aborted = true;
                        break;
                }
            }
        }
        while (!done && !aborted);
    }
}


// TODO: this needs to go away, subsumed into code for displaying a single screen above

// Note that this is modal. This sucks...
void display_chapter_screen_for_level(short level_number, bool is_slow_text_scroll)
{
    if (FilmExporter::instance()->IsExporting() || !shell_options.replay_directory.empty()) return; // TODO: pull this crap out into chapter_screen_is_disabled function
    /*
    show_movie(level_number); // where should this be called?
    
    short pict_resource_number = get_data_for_screen_type(app_state_t::chapter_screen)->base_id + level_number;
    
    SDL_Surface* surface = get_pict_resource_from_map(pict_resource_number);

    if (surface)
    {
         
     animate_interface_fade_out();
         
     
     ImageBlitter* blitter = new ImageBlitter();
     blitter->take_surface(surface);
        blitter->render_to_screen();

     LoadedResource SoundRsrc;
        std::shared_ptr<SoundPlayer> soundPlayer;
        if (get_sound_resource_from_map(pict_resource_number,SoundRsrc))
        {
            ao_fixed pitch = (shapes_file_is_m1() && level_number == 101) ? _m1_high_frequency : _normal_frequency;
            SoundParameters parameters;
            parameters.pitch = pitch * 1.f / _normal_frequency;
            soundPlayer = sound_manager.PlaySound(SoundRsrc, parameters);
        }
        
        // Fade in...
        animate_interface_fade_in(_long_cinematic_fade_in);
        
        animate_scrolling_screen(blitter, is_slow_text_scroll); // this is no-op if image is 640x480

        wait_for_click_or_keypress(is_slow_text_scroll ? -1 : 10 * MACHINE_TICKS_PER_SECOND);
        
        //animate_interface_fade_out(false);
        
        if (soundPlayer) soundPlayer->AskStop();
        
        //set_app_state(existing_state);
    }
      */
}

