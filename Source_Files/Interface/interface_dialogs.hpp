

#ifndef interface_dialogs_hpp
#define interface_dialogs_hpp

#include "cseries.h"


ao_err display_restore_saved_game_as_coop_dialog(const ao_path& file, bool& restore_coop);


// returns false if cancelled
bool display_confirm_exit_game_dialog();

ao_err display_vidmaster_dialog(int16_t& level_number);



// MML

struct InfoTree;
void reset_mml_vidmaster_dialog_strings();
void parse_mml_vidmaster_dialog_strings(const InfoTree& root);



#endif /* interface_dialogs_hpp */
