

#include "interface_dialogs.hpp"


#include "Music.h"
#include "SoundManager.h"
#include "Canvas_SDL.hpp"
#include "image_blitter.hpp"
#include "OGL_Render.h" // ogl_is_active

// vidmaster dialog
#include "sdl_widgets.h"
#include "map.h" // entry_point (aka level number in Map file)
#include "InfoTree.h"




// Returns false if user cancels. Game has been loaded from file before this is called, so elements like
// dynamic_world->player_count are available.  Cursor has been hidden when called.
bool display_restore_saved_game_as_coop_dialog(const ao_path& file, bool& restore_coop)
{
    dialog d;

    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("RESUME GAME"), d);
    placer->add(new w_spacer, true);
    
    horizontal_placer *resume_as_placer = new horizontal_placer;
    w_toggle* restore_as_coop_toggle = new w_toggle(dynamic_world->player_count > 1);
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
    
    bool success = d.run() == 0;
    if (success) { restore_coop = restore_as_coop_toggle->get_selection(); }
    return success;
}




// dump this here temporarily

bool display_quit_without_saving_dialog()
{
    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add (new w_static_text("Are you sure you wish to"), d);
    placer->dual_add (new w_static_text("cancel the game in progress?"), d);
    placer->add (new w_spacer(), true);
    
    horizontal_placer *button_placer = new horizontal_placer;
    w_button *default_button = new w_button("YES", dialog_ok, &d);
    button_placer->dual_add (default_button, d);
    button_placer->dual_add (new w_button("NO", dialog_cancel, &d), d);
    d.activate_widget(default_button);
    placer->add(button_placer, true);
    d.set_widget_placer(placer);
    return d.run() == 0;
}




// cross-platform static variables
short vidmasterLevelOffset = 1; // can be set with MML (see game_window.cpp) // EES: yeah, might've lost that bit of code (probably with good reason) but, TODO: fix this up right


int16_t display_vidmaster_dialog()
{
    // Get levels
    std::vector<entry_point> levels;
    if (!get_all_levels_for_game_types(levels, all_entry_points))
    {
        entry_point dummy;
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
    bool success = (d.run() == 0);
    
    // Should do noncontiguous map files OK
    return success ? levels[level_w->get_selection()].level_number : NONE;
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
