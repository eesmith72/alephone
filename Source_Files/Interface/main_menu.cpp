

#include "main_menu.hpp"

#include "app_event_loop.hpp" // game_state
#include "about_aleph_one.hpp"
#include "chapter_screens.hpp"
#include "game_event_loop.hpp"

#include "screen.h" // change_screen_mode
#include "Music.h"
#include "vbl.h"
#include "Plugins.h"
#include "image_blitter.hpp"
#include "images.h"
#include "screen_drawing.h" // get_main_menu_rect (until ui rects move here)
#include "fades.h"
#include "preferences.h" // show_main_preferences_dialog
#include "InfoTree.h"
#include "mouse.h" // show_cursor

#include "sdl_dialogs.h" //


extern struct game_state game_state;




static const std::vector<int32_t> default_menu_item_order = {
    iNewGame,
    iLoadGame,
    iGatherGame,
    iJoinGame,
    iReplaySavedFilm,
    iReplayLastFilm,
    iSaveLastFilm,
    iPreferences,
    iQuit,
    iCredits,
    iAbout,
    -1,
    -1
};

static std::vector<int32_t> menu_item_order;





void display_main_menu()
{
    hide_cursor();
    
    game_state.user                         = _single_player;
    game_state.flags                        = 0;
    game_state.state                        = _display_main_menu;
    game_state.current_screen               = 0;
    game_state.last_ticks_on_idle           = machine_tick_count();
    game_state.ticks_until_next_screen      = TICKS_UNTIL_DEMO_STARTS;
    game_state.highlighted_main_menu_item   = NONE;
    
    Plugins::instance()->set_mode(Plugins::kMode_Menu);
    change_screen_mode(_screentype_menu);
    
   // animate_ui_fade_out_blocking(); // does nothing if already black, otherwise fades out current screen

   // animate_ui_fade_in_blocking();
    
    Blitter* blitter = get_main_menu_unpressed();
    blitter->render_to_screen();
    
   // start_interface_fade(_long_cinematic_fade_in);
        
    // clumsy, but shell can skip splash screens and go straight to main menu; TODO: if user deliberately skips splash screens, should we skip the music too?
    if (!Music::instance()->Playing() && game_state.can_play_intro_music)
    {
        Music::instance()->RestartIntroMusic();
    }
    game_state.can_play_intro_music = false;
}







void draw_main_menu_button_for_rect(short rect_index, bool is_pressed)
{
    Blitter* blitter = is_pressed ? get_main_menu_pressed() : get_main_menu_unpressed();

    const SDL_Rect& screen_rect = get_main_menu_rect(rect_index);
    blitter->render_to_screen(&screen_rect, &screen_rect);
}


// keyboard shortcuts, e.g. pressing "p" key for prefs, momentarily "press" the corresponding button
void draw_main_menu_button_for_command(short command_id)
{
    short rect_index = get_main_menu_rect_for_command_id(command_id);

    assert_fail(get_game_state() == _display_main_menu, "");
    
    // Draw it initially depressed
    draw_main_menu_button_for_rect(rect_index, true);
    MainScreenSwap();
    
    sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 12);
    draw_main_menu_button_for_rect(rect_index, false);
}








inline bool point_in_rectangle(int32_t x, int32_t y, const SDL_Rect& rect)
{
    return (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h);
}


static void handle_interface_menu_screen_click(short x, short y, bool cheatkeys_down)
{
    SDL_Rect screen_rect;

    // Was the mouse clicked inside a button rect?
    short rect_index = START_OF_UI_RECTS;
    for (; rect_index < END_OF_UI_RECTS; rect_index++)
    {
        screen_rect = get_main_menu_rect(rect_index);
        if (point_in_rectangle(x, y, screen_rect)) break;
    }
    
    // If it was, show the button's pressed image
    if (rect_index != END_OF_UI_RECTS)
    {
        if (enabled_item(get_command_id_for_main_menu_rect(rect_index)))
        {
            bool last_state= true;

            stop_ui_fade();
            show_cursor();

            screen_rect = get_main_menu_rect(rect_index);

            // Draw the button initially depressed
            draw_main_menu_button_for_rect(rect_index, last_state);
            MainScreenSwap();
        
            bool mouse_down = true;
            while (mouse_down)
            {
                int mx = x, my = y;
                bool mouse_changed = false;
                
                SDL_Event e;
                if (SDL_PollEvent(&e))
                {
                    switch (e.type)
                    {
                        case SDL_MOUSEBUTTONUP:
                            mx = e.button.x;
                            my = e.button.y;
                            mouse_changed = true;
                            mouse_down = false;
                            break;
                        case SDL_MOUSEMOTION:
                            mx = e.motion.x;
                            my = e.motion.y;
                            mouse_changed = true;
                            break;
                    }
                }
                else
                {
                    SDL_Delay(10);
                }
                if (mouse_changed)
                {
                    alephone::Screen::instance()->window_to_screen(mx, my);
                    bool state = point_in_rectangle(mx, my, screen_rect);
                    if (state != last_state)
                    {
                        draw_main_menu_button_for_rect(rect_index, state);
                        MainScreenSwap();
                        last_state = state;
                    }
                }
                else
                {
                    static uint64_t last_redraw = 0;
                    if (machine_tick_count() > last_redraw + TICKS_PER_SECOND / 30)
                    {
                        // draw_intro_screen(); // not sure why this'd want to redraw if nothing's changed
                        last_redraw = machine_tick_count();
                    }
                }
            }

            // Draw it unpressed
            draw_main_menu_button_for_rect(rect_index, false);
            //MainScreenSwap(); // main event loop should do the swap
            
            if (last_state)
            {
                do_main_menu_item_command(get_command_id_for_main_menu_rect(rect_index), cheatkeys_down);
            }
        }
    }
}









void do_main_menu_item_command(short menu_item, bool cheat)
{
    switch(menu_item)
    {
        case iNewGame:
            begin_game(_single_player, cheat);
            break;
        case iPlaySingletonLevel:
            begin_game(_single_player,2);
            break;

        case iJoinGame:
            handle_network_game(false);
            break;

        case iGatherGame:
            handle_network_game(true);
            break;
            
        case iLoadGame:
            handle_load_game();
            break;

        case iReplayLastFilm:
        case iReplaySavedFilm:
            handle_replay(menu_item==iReplayLastFilm);
            break;
            
        case iCredits:
            display_credits();
            break;
            
        case iPreferences:
            force_system_colors(false);
            show_main_preferences_dialog();
            display_main_menu(); // Re fade in, so that we get the proper colortable loaded.

            game_state.ticks_until_next_screen = TICKS_UNTIL_DEMO_STARTS;
            game_state.last_ticks_on_idle = machine_tick_count();
            break;
            
        case iCenterButton:
            SoundManager::instance()->PlaySound(Sound_Center_Button(), 0, NONE);
            break;
            
        case iSaveLastFilm:
            handle_save_film();
            break;

        case iQuit:
            display_quit_screens();
            break;
            
        case iAbout:
            force_system_colors(false);
            clear_screen();
            display_about_aleph_one_dialog();
            display_main_menu();
            game_state.ticks_until_next_screen= TICKS_UNTIL_DEMO_STARTS;
            game_state.last_ticks_on_idle= machine_tick_count();
            break;

        default:
            assert_fail(false, "");
            break;
    }
}



void portable_process_screen_click(short x, short y, bool cheatkeys_down)
{
    switch (get_game_state())
    {
        case _game_in_progress:
        case _begin_display_of_epilogue:
        case _change_level:
        case _displaying_network_game_dialogs:
        case _quit_game:
        case _close_game:
        case _switch_demo:
        case _revert_game:
            break;

        case _display_intro_screens_for_demo:
            /* Get out of user mode. */
            display_main_menu();
            break;

        case _display_quit_screens:
        case _display_intro_screens:
        case _display_chapter_heading:
        case _display_prologue:
        case _display_epilogue:
        case _display_credits:
            /* Force the state change next time through.. */
            force_game_state_change();
            break;

        case _display_main_menu:
            handle_interface_menu_screen_click(x, y, cheatkeys_down);
            break;
        
        default:
            assert_fail(false, "");
            break;
    }
}


void process_main_menu_highlight_advance(bool reverse)
{
    if (get_game_state() != _display_main_menu) return;
    
    int previous_command_id = game_state.highlighted_main_menu_item;
    
    // if AO were sensible, the menu_item_order vector would not be end-padded with -1s; since AO is not sensible, simplifying is for another day
    const auto last_index = [](){
        return (int32_t)std::distance(std::find_if(menu_item_order.rbegin(), menu_item_order.rend(),
                                                   [](int i){ return i != -1; }), menu_item_order.rend()) - 1;
    };

    if (game_state.highlighted_main_menu_item == -1)
    {
        game_state.highlighted_main_menu_item = menu_item_order[reverse ? 0 : last_index()];
    }
    
    // this is not good: it assumes a single direction of movement (M2's main menu buttons are arranged in a linear curve, but TSE's are in 2x4 grid), so a more flexible configuration is needed

    do
    {
        int32_t index = -1;
        for (auto i = 0; i < menu_item_order.size(); ++i)
        {
            if (menu_item_order[i] == game_state.highlighted_main_menu_item)
            {
                index = i;
                break;
            }
        }

        if (reverse)
        {
            --index;
            if (index < 0)
            {
                index = last_index();
            }
        }
        else
        {
            ++index;
            if (menu_item_order[index] == -1)
            {
                index = 0;
            }
        }
            
        game_state.highlighted_main_menu_item = menu_item_order[index];
    }
    while (!enabled_item(game_state.highlighted_main_menu_item));
    
    if (previous_command_id != -1) { draw_main_menu_button_for_rect(get_main_menu_rect_for_command_id(previous_command_id), false); }
    draw_main_menu_button_for_rect(get_main_menu_rect_for_command_id(game_state.highlighted_main_menu_item), true);
}


void process_main_menu_highlight_select(bool cheatkeys_down)
{
    if (get_game_state() != _display_main_menu)
        return;
    if (game_state.highlighted_main_menu_item == -1)
        return;
    if (!enabled_item(game_state.highlighted_main_menu_item))
        return;
    do_main_menu_item_command(game_state.highlighted_main_menu_item, cheatkeys_down);
    
}


bool enabled_item(short item)
{
    bool enabled = true;

    switch (item)
    {
        case iNewGame:
        case iLoadGame:
        case iPlaySingletonLevel:
        case iPreferences:
        case iReplaySavedFilm:
        case iCredits:
        case iQuit:
        case iAbout:
        case iCenterButton:
            break;
            
        case iReplayLastFilm:
        case iSaveLastFilm:
        {
            ao_path path = get_recording_path();
            enabled = std::filesystem::is_regular_file(path);
            break;
        }
        case iGatherGame:
        case iJoinGame:
#if !defined(DISABLE_NETWORKING)
            enabled = true;
#else
            enabled = false;
#endif
            break;
            
        default:
            throw_bug_report("Invalid button id: %d", item);
    }
    
    return enabled;
}






// MML

void reset_mml_menu_item_order()
{
    menu_item_order = default_menu_item_order;
}


void parse_mml_menu_item_order(const InfoTree& root) // <interface>
{
    for (const InfoTree& menu_item : root.children_named("menu_item"))
    {
        int16_t index;
        if (!menu_item.read_indexed("index", index, (int32_t)menu_item_order.size())) continue;

        int16_t item;
        if (menu_item.read_indexed("item", item, iAbout + 1))
        {
            menu_item_order[index] = item;
        }
    }
}


