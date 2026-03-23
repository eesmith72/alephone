

#ifndef interface_dialogs_hpp
#define interface_dialogs_hpp

#include "cseries.h"

// returns
bool display_restore_saved_game_as_coop_dialog(const ao_path& file, bool& restore_coop);


// returns false if cancelled
bool display_quit_without_saving_dialog();

// returns NONE if cancelled
int16_t display_vidmaster_dialog();


// MML

struct InfoTree;
void reset_mml_vidmaster_dialog_strings();
void parse_mml_vidmaster_dialog_strings(const InfoTree& root);



#endif /* interface_dialogs_hpp */
