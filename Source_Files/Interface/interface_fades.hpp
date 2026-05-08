/*
 interface_fades.hpp -- user interface fade in/out
 
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

#ifndef interface_fades_hpp
#define interface_fades_hpp


#include "cseries.hpp"


//-----------------------------------------------------------------------------
// This ignores the original M2 fade definitions (not that scenario MMLs tend to change them).
// It could be made customizable in future but for now the behavior is hardcoded.


#define STANDARD_FADE_DURATION  (MACHINE_TICKS_PER_SECOND * 1 / 2)
#define LONG_FADE_DURATION      (MACHINE_TICKS_PER_SECOND * 3 / 2)


// A callback that draws the splash screen/main menu/etc.
typedef void (*interface_fade_redraw_proc)(float darkness); // 0.0 = image, 1.0 = none more black


// A  be set before calling animate-in; will be cleared at end of animate-out
void set_interface_fade_renderer(interface_fade_redraw_proc redraw_screen, bool auto_fade = true);


// blocking (runs its own minimal event loop to detect keypresses and skip to end)
void animate_interface_fade_in(uint64_t duration = STANDARD_FADE_DURATION);


// if music is currently playing, fade_music=true will fade it out while fading screen
void animate_interface_fade_out(uint64_t duration = STANDARD_FADE_DURATION, bool fade_music = false);



#endif /* interface_fades_hpp */
