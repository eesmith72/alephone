

#ifndef game_event_loop_hpp
#define game_event_loop_hpp

#include "cseries.h"

#include "interface_support.hpp"


void game_event_loop();

void handle_window_event(const SDL_Event &event); // used here and by main_event_loop


#endif /* game_event_loop_hpp */
