

#include "main_menu.hpp"

#include "app_state.hpp"

#include "main_event_loop.hpp" // game_state
#include "about_ao_dialog.hpp"
#include "chapter_screens.hpp"
#include "game_event_loop.hpp"

#include "screen.h" // change_screen_mode
#include "Music.h"
#include "vbl.h"
#include "Plugins.h"
#include "image_blitter.hpp"
#include "images.h"
//#include "screen_drawing.h" 
#include "fades.h"
#include "preferences.h" // display_main_preferences_dialog
#include "InfoTree.h"
#include "mouse.h" // show_cursor
#include "joystick.h" //

#include "sdl_dialogs.h" //


extern struct game_state game_state;


// -----------------------------------------------------------------------------------------
// main menu buttons

struct button_nav_t
{
    app_state_t up, down, left, right;
    
    button_nav_t(app_state_t up, app_state_t down, app_state_t left, app_state_t right) : up(up), down(down), left(left), right(right) {}
    
    button_nav_t(app_state_t up, app_state_t down) : up(up), down(down), left(up), right(down) {}
    
    button_nav_t() : up(app_state_t::undefined), down(app_state_t::undefined), left(app_state_t::undefined), right(app_state_t::undefined) {}
    
    
    app_state_t get_state_for_movement(direction_t direction) const
    {
        switch (direction)
        {
            case direction_t::left:
                return left;
            case direction_t::up:
                return up;
            case direction_t::down:
                return down;
            case direction_t::right:
                return right;
            default:
                throw_bug_report_f("invalid direction: %d", direction);
        }
    }
};


struct main_menu_button_t
{
    app_state_t action;
    
    SDL_Rect button_rect;
    button_nav_t button_order;
    
    SDL_Keycode key;
    
    bool exists() const { return button_rect.w > 0 && button_rect.h > 0; }
    
    bool is_enabled() const
    {
        if (!exists()) return false;
        switch (action)
        {
#ifdef DISABLE_NETWORKING
            case app_state_t::gather_network_game:
            case app_state_t::join_network_game:
                return false;
#endif
            case app_state_t::load_and_play_last_film:
            case app_state_t::save_last_film:
                return has_recording_file();
                
            default:
                return true;
        }
    }
    
    bool contains_point(int32_t x, int32_t y) const
    {
        // TODO: where to convert between mouse/screen coords and 640x480 grid?
        return (x >= button_rect.x && x < button_rect.x + button_rect.w && y >= button_rect.y && y < button_rect.y + button_rect.h);
    }
    
    void draw_pressed() const
    {
        get_main_menu_pressed()->render_to_screen(&button_rect, &button_rect); // TODO: FIX: dst rect assumes 1:1, which obviously it isn't; need to decide how we're mapping 640x480 grid to modern displays
    }
    
    void draw_unpressed() const
    {
        get_main_menu_unpressed()->render_to_screen(&button_rect, &button_rect); // TODO: FIX: dst rect assumes 1:1, which obviously it isn't; need to decide how we're mapping 640x480 grid to modern displays
    }
    
    // TODO: what about disabled state?
};


// this combines the M2 main menu rects (plus AO additions) with menu item order (now supporting M2-style linear or TSE-style grid layouts) and replaces the iNewGame, iLoadGame, etc enums with the equivalent app_state_t enums
// TODO: these are based on original M2 button grid, extended by AO with 3 optional buttons: About AO, M1 Center, and start 'singleton' game (shows vidmaster dialog to play any level solo). For modern main menu, gather and join can be merged into single 'multiplayer' button and films should be self-managing (always auto-save the most recent film with level name and date, same as quicksave, and use QS dialog to manage both).
// TODO: add SDL_ keyboard keys and joystick/controller buttons to table, e.g. so TSE can define a Help button with 'h' as shortcut (displays a single help screen showing all [default] key mappings)
// important: when defining buttons in a grid layout, it should be possible to reach ALL buttons by repeatedly pressing TAB (e.g. when selection is at bottom of one column, the next TAB moves selection to top of next column, not to top of same column)
// for backwards compatibility with MML definitions these must be ordered same as main menu rects
static const std::vector<main_menu_button_t> main_menu_buttons_std = {
    // this matches order of mInterface enum (iNewGame..iAbout)
    {app_state_t::start_solo_game,                  {101, 179, 167,  31}, {app_state_t::start_solo_game_choosing_level, app_state_t::load_and_resume_saved_game},       SDLK_n},
    {app_state_t::load_and_resume_saved_game,       { 25, 221, 213,  32}, {app_state_t::start_solo_game,                app_state_t::gather_network_game},              SDLK_o},
    {app_state_t::gather_network_game,              { 11, 263, 212,  31}, {app_state_t::load_and_resume_saved_game,     app_state_t::join_network_game},                SDLK_g},
    {app_state_t::join_network_game,                { 38, 301, 198,  32}, {app_state_t::gather_network_game,            app_state_t::load_and_play_saved_film},         SDLK_j},
    {app_state_t::preferences,                      {421, 304, 142,  27}, {app_state_t::save_last_film,                 app_state_t::quit},                             SDLK_p},
    {app_state_t::load_and_play_last_film,          {231, 386, 175,  27}, {app_state_t::load_and_play_saved_film,       app_state_t::save_last_film},                   SDLK_UNKNOWN},
    {app_state_t::save_last_film,                   {363, 345, 153,  27}, {app_state_t::load_and_play_last_film,        app_state_t::preferences},                      SDLK_s},
    {app_state_t::load_and_play_saved_film,         { 83, 344, 188,  30}, {app_state_t::join_network_game,              app_state_t::load_and_play_last_film},          SDLK_r},
    {app_state_t::credits,                          {246, 206, 136, 141}, {app_state_t::quit,                           app_state_t::center},                           SDLK_c},
    {app_state_t::quit,                             {500, 263,  85,  31}, {app_state_t::preferences,                    app_state_t::credits},                          SDLK_q},
    // these 3 are optional buttons specific to AO: M1 center button (Easter egg), 'singleton game', About AO ("POWERED BY ALEPH ONE" overlay) (the first 2 are unused by defafult)
    {app_state_t::center,                           {  0,   0,   0,   0}, {app_state_t::credits,                        app_state_t::start_solo_game_choosing_level},   SDLK_UNKNOWN},
    {app_state_t::start_solo_game_choosing_level,   {  0,   0,   0,   0}, {app_state_t::center,                         app_state_t::about_ao},                         SDLK_UNKNOWN},
    {app_state_t::about_ao,                         {560, 440,  80,  40}, {app_state_t::start_solo_game_choosing_level, app_state_t::start_solo_game},                  SDLK_a},
};


static std::vector<main_menu_button_t> main_menu_buttons = main_menu_buttons_std;

static const main_menu_button_t* selected_button = nullptr; // when using cursor keys to navigate buttons, the 'pressed' image is persistent



static const main_menu_button_t* get_button_at_position(int32_t x, int32_t y)
{
    for (const auto& state : main_menu_buttons)
    {
        if (state.contains_point(x, y)) { return &state; }
    }
    return nullptr; // ignore user clicking on background
}


static const main_menu_button_t* get_button_for_action(app_state_t action)
{
    for (const main_menu_button_t& state : main_menu_buttons)
    {
        if (state.action == action) { return &state; }
    }
    return &main_menu_buttons.at(0);
}


const SDL_Rect& get_main_menu_button_rect_for_action(app_state_t action)
{
    static const SDL_Rect invalid_rect = {0, 0, 0, 0};
    const main_menu_button_t* button = get_button_for_action(action);
    return button ? button->button_rect : invalid_rect;
}


// -----------------------------------------------------------------------------------------
// input handling


void do_action(app_state_t action, bool is_cheat = false)
{
    if (action == app_state_t::start_solo_game && is_cheat) { action = app_state_t::start_solo_game_choosing_level; }
    set_next_app_state(action);
}


// keyboard shortcuts, e.g. pressing "p" key for prefs, momentarily "press" the corresponding button
void draw_main_menu_button_momentarily_pressed(const main_menu_button_t* button)
{
    assert_fail(get_app_state() == app_state_t::main_menu, "");
    
    button->draw_pressed();
    MainScreenSwap();
    sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 12);
    button->draw_unpressed();
}


static void advance_main_menu_selection(direction_t direction) // user pressed up/down/left/right key to select a menu item
{
    if (!selected_button)
    {
        // find the button which comes before/after the button we want (the do...while loop below will advance it)
        selected_button = &main_menu_buttons.at(direction == direction_t::left || direction == direction_t::up ? 0 : main_menu_buttons.size() - 1);
    }
    
    // advance to the new button (if the button is disabled, keep going to find the first one that isn't)
    int32_t count = 0;
    do
    {
        app_state_t new_state = selected_button->button_order.get_state_for_movement(direction);
        selected_button = get_button_for_action(new_state);
        
        // safety check, so poorly formed MML config can't cause infinite looping
        if (++count > 20)
        {
            log_error_f("Found an MML bug: button %d isn't advancing correctly in direction %d", selected_button->action, direction); // TODO: string descriptions of action and direction
            return;
        }
    }
    while (!selected_button->is_enabled());
    
    Blitter* blitter = get_main_menu_unpressed();
    blitter->render_to_screen();
    selected_button->draw_pressed();
    
    restart_app_state_timeout(); // always wait 30sec from last user input before starting a demo film
}


static void process_button_press(app_state_t action, bool is_cheat) // user clicked on a menu item/pressed a shortcut key
{
    selected_button = get_button_for_action(action);
    
    if (selected_button->is_enabled())
    {
        draw_main_menu_button_momentarily_pressed(selected_button);
        do_action(action, is_cheat);
        
    }
    selected_button = nullptr;
}


// -----------------------------------------------------------------------------------------
// public; called by main_event_loop


void handle_main_menu_mouse_input(const SDL_Event &event)
{
    int32_t x = event.button.x, y = event.button.y;
    // need to convert mouse position from screen to 640x480
    alephone::Screen::instance()->window_to_screen(x, y);
    
    // Was the mouse clicked inside a button rect?
    selected_button = get_button_at_position(x, y);
        
    // If it was, show the button's pressed image
    if (selected_button && selected_button->is_enabled())
    {
        // TODO: these need to move
        stop_ui_fade();
        show_cursor();
        
        get_main_menu_unpressed()->render_to_screen();
        selected_button->draw_pressed();
        MainScreenSwap();
        
        // TODO: this is a blocking loop, which is not great (esp. if we want to animate): main loop should be notifying us of mouse events
        
        bool mouse_pressed = true;
        while (mouse_pressed)
        {
            bool mouse_moved = false;
            
            SDL_Event e;
            if (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                    case SDL_MOUSEMOTION:
                        x = e.motion.x;
                        y = e.motion.y;
                        mouse_moved = true;
                        break;
                    case SDL_MOUSEBUTTONUP:
                        mouse_pressed = false;
                        break;
                }
            }
            else
            {
                SDL_Delay(10);
            }
            
            if (mouse_moved)
            {
                alephone::Screen::instance()->window_to_screen(x, y);
                const main_menu_button_t* new_button = get_button_at_position(x, y);
                if (new_button != selected_button) // mouse has moved out of (or back into) button rect
                {
                    get_main_menu_unpressed()->render_to_screen();
                    if (new_button) { new_button->draw_pressed(); }
                    selected_button = new_button;
                    
                    MainScreenSwap();
                    
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

        get_main_menu_unpressed()->render_to_screen();
        MainScreenSwap();
        get_main_menu_unpressed()->render_to_screen();
        
        if (selected_button)
        {
            
            do_action(selected_button->action, has_cheat_keys_modifier(event.key.keysym.mod));
            selected_button = nullptr;
        }
    }
}


void handle_main_menu_keyboard_input(const SDL_Event &event)
{
    /*
     // TODO: fades should be handled by app event loop when changing states
     if (is_fading())
     {
     stop_effect_fade();
     show_cursor();
     }
     */
    
    SDL_Keycode key = event.key.keysym.sym;
    
    for (const auto& button : main_menu_buttons)
    {
        if (key == button.key)
        {
            process_button_press(button.action, has_cheat_keys_modifier(event.key.keysym.mod));
            return;
        }
    }
    
    switch (key)
    {
            // TODO: F-keys should be handled by the caller
            // standard function keys
            //case SDLK_F6: // F6 toggles between windowed and fullscreen modes on UI screens and in-game
            //    toggle_fullscreen();
            //    break;
            //case SDLK_F11: // TO DO: Steam already uses F11 and F12 for screenshots so we probably should macro these for use in non-Steam builds only
            //case SDLK_F12:
            //    dump_screen();
            //    break;
            
            // menu navigation keys (move up/down/left/right and Return to press the selected button)
        case SDLK_UP:
            advance_main_menu_selection(direction_t::up);
            break;
        case SDLK_LEFT:
            advance_main_menu_selection(direction_t::left);
            break;
        case SDLK_DOWN:
            advance_main_menu_selection(direction_t::down);
            break;
        case SDLK_RIGHT:
            advance_main_menu_selection(direction_t::right);
            break;
        case SDLK_TAB:
            advance_main_menu_selection(event.key.keysym.mod & KMOD_SHIFT ? direction_t::up : direction_t::down);
            break;
        case SDLK_RETURN:
            process_button_press(selected_button ? selected_button->action : app_state_t::start_solo_game, has_cheat_keys_modifier(event.key.keysym.mod));
            break;
        default:
        {}
    }
}
 

void handle_main_menu_controller_input(const SDL_Event &event)
{
    switch (static_cast<int>(event.key.keysym.scancode))
    {
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_UP:
            advance_main_menu_selection(direction_t::up);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            advance_main_menu_selection(direction_t::left);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            advance_main_menu_selection(direction_t::down);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            advance_main_menu_selection(direction_t::right);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_A:
            process_button_press(app_state_t::start_solo_game, false);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_GUIDE:
            process_button_press(app_state_t::start_solo_game, true);
            break;
            
            // TODO: what about network games, prefs, and other options? or are users expected to switch back to keyboard/mouse?
            
        default:
        {}
    }
}


// -----------------------------------------------------------------------------------------
// show main menu screen


void display_main_menu()
{
    selected_button = nullptr;
    
    
    // TODO: sort out fades
   // animate_ui_fade_out_blocking(); // does nothing if already black, otherwise fades out current screen

    clear_screen(true);
    
   // animate_ui_fade_in_blocking();
    
    get_main_menu_unpressed()->render_to_screen();
    MainScreenSwap();
    get_main_menu_unpressed()->render_to_screen();
    
   // start_interface_fade(_long_cinematic_fade_in);
    
    static bool can_play_intro_music = true;
        
    // clumsy, but shell can skip splash screens and go straight to main menu; TODO: if user deliberately skips startup screens, shouldn't we skip the startup audio too?
    if (!Music::instance()->Playing() && can_play_intro_music)
    {
        Music::instance()->RestartIntroMusic();
    }
    can_play_intro_music = false;
}


// -----------------------------------------------------------------------------------------
// MML


void reset_mml_main_menu()
{
    main_menu_buttons = main_menu_buttons_std;
}


void parse_mml_main_menu(const InfoTree& root) // <interface>
{    
    for (const InfoTree &rect : root.children_named("rect"))
    {
        int16 index, top = 0, left = 0, bottom = 0, right = 0;
        if (rect.read_indexed("index", index, NUMBER_OF_INTERFACE_RECTANGLES)
            && index >= START_OF_MAIN_MENU_RECTS && index < END_OF_MAIN_MENU_RECTS
            && rect.read_attr("top", top) && rect.read_attr("left", left)
            && rect.read_attr("bottom", bottom) && rect.read_attr("right", right))
        {
            main_menu_buttons[index - START_OF_MAIN_MENU_RECTS].button_rect = {left, top, right - left, bottom - top};
        }
    }
    
#define NO_ITEM (-1)
    
    // stupid convoluted brittle MML crap; the following code expects main_menu_buttons_std to have 13 entries in exact order
    int32_t max = (int32_t)main_menu_buttons.size();
    std::vector<int16_t> tmp;
    
    for (const InfoTree& menu_item : root.children_named("menu_item"))
    {
        int16_t index, item = 0;
        if (menu_item.read_indexed("index", index, max) && menu_item.read_indexed("item", item, max))
        {
            if (tmp.empty())
            {
                tmp.resize(max);
                for (int32_t i = 0; i < max; i++)
                {
                    tmp[i] = (main_menu_buttons[i].button_rect.w == 0) ? NO_ITEM : i + 1;
                }
            }
            tmp[index] = item;
        }
    }
    
    if (!tmp.empty())
    {
        // convert button items from 1-indexed to 0-indexed, discard any unused
        int32_t offset = 0;
        for (int32_t item : tmp)
        {
            if (item != NO_ITEM) { tmp[offset++] = item - 1; }
        }
        // wrap around first and last, so we can easily get previous+next button items
        tmp.resize(offset);
        tmp.push_back(tmp[0]);
        tmp.insert(tmp.begin(), tmp[offset - 1]);
        for (int32_t i = 1; i < tmp.size() - 1; i++)
        {
            button_nav_t& order = main_menu_buttons[tmp[i]].button_order;
            order.left = order.up = main_menu_buttons_std[tmp[i - 1]].action;
            order.right = order.down = main_menu_buttons_std[tmp[i + 1]].action;
        }
    }
}

