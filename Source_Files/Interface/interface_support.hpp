/*
 interface_support.hpp
 
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

#ifndef interface_support_hpp
#define interface_support_hpp

#include "cseries.hpp"

#include "interface_dialogs.hpp"



// moved this enum here from screen_definitions.h
//
// 'pict' resource ids for the 8 bit picts
// the 16 bit versions are these ids + 10000
// the 32 bit versions are these ids + 20000
enum {
    // xxxx_SCREEN_BASE ids are for 'pict' resources stored in Map.sce2 or Marathon.appl
    
    M1_STARTUP_SCREEN_BASE  = 1111,
    M2_STARTUP_SCREEN_BASE  = 1000,
    
    M2_MAIN_MENU_BASE       = 1100, // M1's main menu images are stored piecemeal in collection 10 of Shapes.shps
    
    PROLOGUE_SCREEN_BASE    = 1200,
    
    M1_EPILOGUE_SCREEN_BASE = 1300, // TODO: FIX
    M2_EPILOGUE_SCREEN_BASE = 1300,
    
    M1_CREDIT_SCREEN_BASE   = 1000,
    M2_CREDIT_SCREEN_BASE   = 1400,
    
    M1_CHAPTER_SCREEN_BASE  = 10000,
    M2_CHAPTER_SCREEN_BASE  = 1500,
    
    M2_HUD_BACKGROUND_BASE  = 1700, // the original M2 HUD's background image
    
    SHUTDOWN_SCREEN_BASE    = 1800,
};


// TODO: time_of_next_transition is int64
#define INFINITE_TIME_DELAY (INT32_MAX)


#define TICKS_UNTIL_DEMO_FILM_STARTS (30 * MACHINE_TICKS_PER_SECOND)


/* For teleportation, end movie, etc. */
#define M1_EPILOGUE_LEVEL_NUMBER  (100)
#define M2_EPILOGUE_LEVEL_NUMBER  (256)

#define get_epilogue_screen_number()  (shapes_file_is_m1() ? M1_EPILOGUE_LEVEL_NUMBER : M2_EPILOGUE_LEVEL_NUMBER)


inline bool has_cheat_keys_modifier()
{
    SDL_Keymod m = SDL_GetModState();
    return (m & KMOD_SHIFT) && (m & KMOD_CTRL) && !(m & KMOD_ALT) && !(m & KMOD_GUI); // standardize on Ctrl+Shift for cross-platform consistency? (Mac originally used Command+Option)
}



#endif /* interface_support_hpp */
