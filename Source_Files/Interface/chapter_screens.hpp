

#ifndef chapter_screens_hpp
#define chapter_screens_hpp

#include "cseries.h"

#include "interface_support.hpp"



#define INFINITE_TIME_DELAY (INT32_MAX)

#define DEMO_INTRO_SCREEN_DURATION (10 * MACHINE_TICKS_PER_SECOND)

#define TICKS_UNTIL_DEMO_STARTS (30 * MACHINE_TICKS_PER_SECOND)


#define NUMBER_OF_INTRO_SCREENS (3)
#define INTRO_SCREEN_DURATION (215 * MACHINE_TICKS_PER_SECOND / TICKS_PER_SECOND) // fudge to align with sound

#ifdef DEMO
#define INTRO_SCREEN_TO_START_SONG_ON (1)
#else
#define INTRO_SCREEN_TO_START_SONG_ON (0)
#endif

#define INTRO_SCREEN_BETWEEN_DEMO_BASE (INTRO_SCREEN_BASE+1) /* +1 to get past the powercomputing */
#define NUMBER_OF_INTRO_SCREENS_BETWEEN_DEMOS (1)


#define NUMBER_OF_PROLOGUE_SCREENS 0
#define PROLOGUE_DURATION (10 * MACHINE_TICKS_PER_SECOND)

#define NUMBER_OF_EPILOGUE_SCREENS 1
#define EPILOGUE_DURATION (INFINITE_TIME_DELAY)


#define NUMBER_OF_CREDIT_SCREENS 7
#define CREDIT_SCREEN_DURATION (15 * 60 * MACHINE_TICKS_PER_SECOND)


#define NUMBER_OF_CHAPTER_HEADINGS 0
#define CHAPTER_HEADING_DURATION (7*MACHINE_TICKS_PER_SECOND)


// For exiting the Marathon app
// #if defined(DEBUG) || !defined(DEMO)
#define NUMBER_OF_FINAL_SCREENS 0
// #else
// #define NUMBER_OF_FINAL_SCREENS 1
// #endif
#define FINAL_SCREEN_DURATION (INFINITE_TIME_DELAY)

/* For teleportation, end movie, etc. */
#define M1_EPILOGUE_LEVEL_NUMBER  (100)
#define M2_EPILOGUE_LEVEL_NUMBER  (256)




// moved this enum here from screen_definitions.h; mostly (but not entirely) 2D UI resource IDs: main menu
// this should not be its permanent home, but converting old M2 hardcoded rect ids to modern extensible ids is TODO
//
// 'pict' resource ids for the 8 bit picts
// the 16 bit versions are these ids + 10000
// the 32 bit versions are these ids + 20000
enum {
    INTRO_SCREEN_BASE       = 1000, // splash screen[s] (included in Images.img2)
    MAIN_MENU_BASE          = 1100, // main menu screen (ditto)
    
    PROLOGUE_SCREEN_BASE    = 1200, // the remaining SCREEN ids are for 'pict' resources stored in Map.sce2
    EPILOGUE_SCREEN_BASE    = 1300,
    CREDIT_SCREEN_BASE      = 1400,
    CHAPTER_SCREEN_BASE     = 1500,
    //COMPUTER_INTERFACE_BASE = 1600,
    INTERFACE_PANEL_BASE    = 1700, // just to be awkward, the M2 SW HUD's background image was stored in Images.img2 as (iirc) 1700 + 2700 'pict' resources
    FINAL_SCREEN_BASE       = 1800,
};




void display_splash_screen();

void display_introduction_screen_for_demo();

void display_credits();

void display_epilogue();

void display_quit_screens();


void next_game_screen();


void try_and_display_chapter_screen(short level, bool interface_table_is_valid, bool text_block);

void display_screen(short base_pict_id);



#endif /* chapter_screens_hpp */
