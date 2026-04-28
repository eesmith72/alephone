

#include "interface_dialogs.hpp"


#include "Music.h"
#include "SoundManager.h"
//#include "Canvas_SDL.hpp"
#include "ImageBlitter.hpp"
//#include "OGL_Render.h" 
#include "player.h" // get_number_of_players
// vidmaster dialog
#include "sdl_widgets.h"
#include "map.h" // level_identity (aka level number in Map file)
#include "InfoTree.h"



// TODO: ideally display_load_saved_game_dialog would show which saved games are co-op and enable a 'Resume as Solo/Coop' option when a co-op game is selected, so it doesn't have to display this separate dialog; however, this dialog is still needed when a drag-n-dropped saved game file is co-op
ao_err display_restore_saved_game_as_coop_dialog(const ao_path& file, bool& restore_coop)
{
    dialog d;

    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("RESUME GAME"), d);
    placer->add(new w_spacer, true);
    
    horizontal_placer *resume_as_placer = new horizontal_placer;
    w_toggle* restore_as_coop_toggle = new w_toggle(get_number_of_players() > 1);
    restore_as_coop_toggle->load_labels(strSoloOrCoop);
    resume_as_placer->dual_add(restore_as_coop_toggle->adding_label("Resume as"), d);
    resume_as_placer->dual_add(restore_as_coop_toggle, d);

    placer->add(resume_as_placer, true);
    
    placer->add(new w_spacer(), true);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("RESUME", dialog_ok, &d), d);
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

    placer->add(button_placer, true);
    
    d.set_widget_placer(placer);
    
    if (d.run() != 0) { return err_user_canceled; }
    restore_coop = restore_as_coop_toggle->get_selection();
    return no_err;
}




// dump this here temporarily

bool display_confirm_exit_game_dialog()
{
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add (new w_static_text("Are you sure you wish to"), d);
    placer->dual_add (new w_static_text("cancel the game in progress?"), d);
    placer->add (new w_spacer(), true);
    
    horizontal_placer *button_placer = new horizontal_placer;
    w_button *default_button = new w_button("LEAVE", dialog_ok, &d);
    button_placer->dual_add (default_button, d);
    button_placer->dual_add (new w_button("RESUME", dialog_cancel, &d), d);
    d.activate_widget(default_button);
    placer->add(button_placer, true);
    d.set_widget_placer(placer);
    return d.run() == 0;
}




// cross-platform static variables
short vidmasterLevelOffset = 1; // can be set with MML (see game_window.cpp) // EES: yeah, might've lost that bit of code (probably with good reason) but, TODO: fix this up right


ao_err display_vidmaster_dialog(int16_t& level_number)
{
    // Get levels
    std::vector<level_identity> levels;
    if (!get_all_levels_for_game_types(levels, all_entry_points))
    {
        level_identity dummy;
        dummy.level_number = 0;
        dummy.utf8_level_name = "Untitled Level";
        levels.push_back(dummy);
    }

    // Create dialog
    dialog d;
    vertical_placer *placer = new vertical_placer;
    
    std::stringstream introduction(get_string(STRID(vidmasterStringSetID, strVidmasterIntroduction)));
    std::string line;
    while (std::getline(introduction, line, '\n')) // we will ignore the potential for naughtily-crafted MML strings
    {
        placer->dual_add(new w_static_text(line.c_str()), d);
    }
    placer->add(new w_spacer(), true);
    std::stringstream oath(get_string(STRID(vidmasterStringSetID, strVidmasterOath)));
    while (std::getline(oath, line, '\n')) // we will ignore the potential for naughtily-crafted MML strings
    {
        placer->dual_add(new w_static_text(line.c_str()), d);
    }
    
    std::string start_at_text = get_string(STRID(vidmasterStringSetID, strVidmasterIntroduction));
    placer->add(new w_spacer(), true);
    placer->dual_add(new w_static_text(start_at_text.c_str()), d);

    w_levels *level_w = new w_levels(levels, &d);
    level_w->set_offset(vidmasterLevelOffset);
    placer->dual_add(level_w, d);
    placer->add(new w_spacer(), true);
    placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);

    d.activate_widget(level_w);
    d.set_widget_placer(placer);

    // Run dialog
    if (d.run() != 0) return err_user_canceled; // TODO: run should return ao_err, but that's all entangled in `result` ivar
    
    // Should do noncontiguous map files OK
    level_number = levels[level_w->get_selection()].level_number;
    return no_err;
}






void reset_mml_vidmaster_dialog_strings()
{
    // TODO: reimplement
}


void parse_mml_vidmaster_dialog_strings(const InfoTree& root)
{
    for (const InfoTree &vid : root.children_named("vidmaster"))
    {
        // TODO: reimplement
    }
}
