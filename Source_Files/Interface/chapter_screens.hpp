

#ifndef chapter_screens_hpp
#define chapter_screens_hpp

#include "cseries.h"
#include "app_state.hpp"
#include "interface_support.hpp"



ao_err load_screen_sequence(app_state_t screen_type);

uint32_t display_current_screen(); // returns timeout in ticks

ao_err advance_to_next_screen(); // returns no_err/not found // TODO: FIX: need to implement this (including appropriate error codes)



void try_and_display_chapter_screen(short level, bool interface_table_is_valid, bool text_block); // TODO: this needs to go away



#endif /* chapter_screens_hpp */
