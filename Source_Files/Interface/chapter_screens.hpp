

#ifndef chapter_screens_hpp
#define chapter_screens_hpp

#include "cseries.h"
#include "app_state.hpp"
#include "interface_support.hpp"


// TODO: once Canvas is complete and exposed as Lua API, the splash, chapter, main menu, credits, etc screens can/should be drawn by a Lua script so that modders can customize presentation (e.g. scrolling credits with music)


ao_err load_screen_sequence(app_state_t screen_type);

uint32_t display_current_screen(); // returns timeout in ticks

ao_err advance_to_next_screen(); // returns no_err/not found // TODO: FIX: need to implement this (including appropriate error codes)



void display_chapter_screen_for_level(short level, bool text_block); // TODO: this needs to go away



#endif /* chapter_screens_hpp */
