

#ifndef gameworld_entrance_h
#define gameworld_entrance_h


#include "cseries.hpp"



void enter_gameworld(bool is_restoring_saved_game); // when restoring a saved game, there may be saved script state (but why isn't that determined automatically by looking for it in the damn wad?)

void exit_gameworld();


#endif /* gameworld_entrance_h */
