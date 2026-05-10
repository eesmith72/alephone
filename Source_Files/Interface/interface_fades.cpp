/*
 interface_fades.cpp
 
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

#include "interface_fades.hpp"

#include "Screen.hpp"

#include "visual_effect_definitions.hpp"

#include "OGL_Setup.h"
#include "OGL_Render.h"

#include "Music.h"


// TODO: make fades non-blocking, animating on main event loop?


enum class interface_fade_state_t
{
    black,
    fading_in,
    visible,
    fading_out,
};


// -----------------------------------------------------------------------------------------


static interface_fade_state_t fade_state = interface_fade_state_t::black;

int16_t fade_start_tick, fade_duration;

interface_fade_redraw_proc redraw_screen;

bool auto_fade;


static bool update_interface_fade(bool fade_in) // returns false when done
{
    float opacity = std::clamp(float(machine_tick_count() - fade_start_tick) / float(fade_duration), 0.0f, 1.0f);
    if (opacity >= 0.99) { opacity = 1.0; }
    
    redraw_screen(opacity);
    
    if (auto_fade)
    {
        ao_rgbaf overlay = {0.0, 0.0, 0.0, float(fade_in ? 1.0 - opacity : opacity)};
        glEnable(GL_BLEND);
        glColor4fv(overlay);
        OGL_RenderRect(main_screen.virtual_screen_rect());
        glDisable(GL_BLEND);
    }
    
    main_screen.swap();
    
    yield();
    
    return opacity < 0.99;
}


// -----------------------------------------------------------------------------------------


void set_interface_fade_renderer(interface_fade_redraw_proc redraw_screen, bool auto_fade)
{
    ::redraw_screen = redraw_screen;
    ::auto_fade = auto_fade;
}


void animate_interface_fade_in(uint64_t duration)
{
    main_screen.clear();
    
    if (redraw_screen)
    {
        fade_state = interface_fade_state_t::fading_in;
        fade_duration = duration;
        fade_start_tick = machine_tick_count();
        
        while (update_interface_fade(true)) { Music::instance()->Idle(); }
        
        fade_state = interface_fade_state_t::visible;
    }
    
    if (redraw_screen) { redraw_screen(1.0); }
}


void animate_interface_fade_out(uint64_t duration, bool fade_music)
{
    if (fade_music) { Music::instance()->QuickFade(); } // start fading music
    
    if (redraw_screen)
    {
        fade_state = interface_fade_state_t::fading_out;
        fade_duration = duration;
        fade_start_tick = machine_tick_count();
        
        while (update_interface_fade(false)) { Music::instance()->Idle(); }
        
    }
    
    main_screen.clear();
    
    if (fade_music) // if screen fade ends before music fade ends, finish fading music
    {
        while (Music::instance()->Playing()) { Music::instance()->Idle(); }
        Music::instance()->Pause(); // and give up the memory
    }

    fade_state = interface_fade_state_t::black;
    
    redraw_screen = nullptr;
}

