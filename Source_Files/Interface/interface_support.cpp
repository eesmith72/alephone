

#include "interface_support.hpp"

#include "Music.h"
#include "SoundManager.h"
#include "Canvas_SDL.hpp"
#include "image_blitter.hpp"
#include "OGL_Render.h" // ogl_is_active

// vidmaster dialog
#include "sdl_widgets.h"
#include "map.h" // entry_point (aka level number in Map file)
#include "InfoTree.h"



void global_idle_proc()
{
    Music::instance()->Idle();
    SoundManager::instance()->Idle();
}


// these _Blitter classes are overcomplicated (so many many different rects...), but they'll do for interim; main thing is we can load an entire Surface into one and use its Draw(dst,src) method for scrolling (there might be questions raised wrt M3 chapter screens once they're at HD as to whether we want to throw the entire thing into tiled GPU textures at once or just those that are visible at any one time, but those can be decided later)
static Blitter* ui_blitter = new Blitter_SDL;
static bool is_ogl_blitter = false;


void load_ui_blitter(SDL_Surface* surface, Blitter*& blitter)
{
    if (ogl_is_active() != is_ogl_blitter)
    {
        is_ogl_blitter = ogl_is_active();
        if (blitter) delete blitter;
        blitter = is_ogl_blitter ? (Blitter*)new Blitter_OGL : new Blitter_SDL;
    }
    
    blitter->borrow_surface(surface);
}


Blitter* get_ui_blitter()
{
    return ui_blitter;
}




// dump this here for now; currently only used by sdl_dialogs (which always draw Surface to be rendered)

static Canvas_SDL* ui_canvas = nullptr;



// TODO: sdl_dialogs/widgets will need this as they do all their drawing in SDL_Surfaces; chapter screens shouldn't need it as they can load directly into SDL/OGL Blitter; main screen is probably simple enough that Blitter API (which can do src+dst rects on SW as well as OGL now it's Texture-based)

Canvas_SDL* get_ui_canvas()
{
    assert_fail(ui_canvas, "");
    return ui_canvas;
}


// keep Canvas (dialogs, HUD, terminals) and Blitter (splash, main, chapter screens) separate for now (drawing Canvas to screen via Blitter)

void initialize_ui()
{
    // Allocate surface for dialogs (this surface is needed because when OpenGL is active, we can't write directly to the screen)
    ui_canvas = new Canvas_SDL(CreateSDLSurface(640, 480));
}


void shutdown_ui()
{
    delete ui_canvas;
}






// dump this here temporarily

bool show_quit_without_saving_dialog()
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


// dump vidmaster dialog here for now


const int32 AllPlayableLevels = _single_player_entry_point | _multiplayer_carnage_entry_point | _multiplayer_cooperative_entry_point | _kill_the_man_with_the_ball_entry_point | _king_of_hill_entry_point | _rugby_entry_point | _capture_the_flag_entry_point;

// cross-platform static variables
short vidmasterLevelOffset = 1; // can be set with MML (see game_window.cpp)


bool show_vidmaster_dialog(int16_t& level_number)
{
    // Get levels
    std::vector<entry_point> levels;
    if (!get_entry_points(levels, AllPlayableLevels))
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
    if (success) { level_number = levels[level_w->get_selection()].level_number; }
    return success;
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
