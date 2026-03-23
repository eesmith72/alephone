

#ifndef main_event_loop_hpp
#define main_event_loop_hpp

#include "cseries.h"

#include "app_state.hpp"

#include "interface_support.hpp"



void handle_open_replay(const ao_path& film_path);


void update_interface();

void main_event_loop();


#endif /* main_event_loop_hpp */
