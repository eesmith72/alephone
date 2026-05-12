/*
 main_menu.cpp
 
 Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
 and the "Aleph One" developers.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 This license is contained in the file "COPYING",
 which is included with this source code; it is available online at
 http://www.gnu.org/licenses/gpl.html
 */

#include "main_menu.hpp"

#include "app_state.hpp"
#include "about_ao_dialog.hpp"
#include "chapter_screens.hpp"
#include "interface_fades.hpp"


#include "Screen.hpp"
#include "Music.h"
#include "vbl.h"
#include "Plugins.h"
#include "ImageBlitter.hpp"
#include "images.h"
#include "screen_drawing.h" // NUMBER_OF_INTERFACE_RECTANGLES
#include "visual_effects.hpp"
#include "preferences.hpp" // display_main_preferences_dialog
#include "InfoTree.h"
#include "mouse.h" // show_cursor
#include "joystick.h" //

#include "sdl_dialogs.h" //

// About AO button bitmaps; these will be composited into the main menu images in `load_main_menu_picts` below.
// (The button's rect is defined by the `about_ao` entry of main_menu_buttons. Use {0,0,0,0} to omit the button,
// e.g. if the AO developers' credits appear in the main Credits instead.)
#include "powered_by_alephone.h"
#include "powered_by_alephone_h.h"


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
    
    bool contains_point(SDL_Point p) const
    {
        // TODO: where to convert between mouse/screen coords and 640x480 grid?
        return (p.x >= button_rect.x && p.x < button_rect.x + button_rect.w
             && p.y >= button_rect.y && p.y < button_rect.y + button_rect.h);
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
    {app_state_t::start_new_campaign,               {101, 179, 167,  31}, {app_state_t::choose_vidmaster_level,         app_state_t::choose_saved_game},                SDLK_n},
    {app_state_t::choose_saved_game,                { 25, 221, 213,  32}, {app_state_t::start_new_campaign,             app_state_t::gather_network_game},              SDLK_o},
    {app_state_t::gather_network_game,              { 11, 263, 212,  31}, {app_state_t::choose_saved_game,              app_state_t::join_network_game},                SDLK_g},
    {app_state_t::join_network_game,                { 38, 301, 198,  32}, {app_state_t::gather_network_game,            app_state_t::load_and_play_saved_film},         SDLK_j},
    {app_state_t::preferences,                      {421, 304, 142,  27}, {app_state_t::save_last_film,                 app_state_t::quit},                             SDLK_p},
    {app_state_t::load_and_play_last_film,          {231, 386, 175,  27}, {app_state_t::load_and_play_saved_film,       app_state_t::save_last_film},                   SDLK_UNKNOWN},
    {app_state_t::save_last_film,                   {363, 345, 153,  27}, {app_state_t::load_and_play_last_film,        app_state_t::preferences},                      SDLK_s},
    {app_state_t::load_and_play_saved_film,         { 83, 344, 188,  30}, {app_state_t::join_network_game,              app_state_t::load_and_play_last_film},          SDLK_r},
    {app_state_t::credits,                          {246, 206, 136, 141}, {app_state_t::quit,                           app_state_t::center},                           SDLK_c},
    {app_state_t::quit,                             {500, 263,  85,  31}, {app_state_t::preferences,                    app_state_t::credits},                          SDLK_q},
    // these 3 are optional buttons specific to AO: M1 center button (Easter egg), 'singleton game', About AO ("POWERED BY ALEPH ONE" overlay) (the first 2 are unused by defafult)
    {app_state_t::center,                           {  0,   0,   0,   0}, {app_state_t::credits,                        app_state_t::choose_vidmaster_level},           SDLK_UNKNOWN},
    {app_state_t::choose_vidmaster_level,           {  0,   0,   0,   0}, {app_state_t::center,                         app_state_t::about_ao},                         SDLK_UNKNOWN},
    {app_state_t::about_ao,                         {560, 440,  80,  40}, {app_state_t::choose_vidmaster_level,         app_state_t::start_new_campaign},               SDLK_a},
};


static std::vector<main_menu_button_t> main_menu_buttons = main_menu_buttons_std;

static const main_menu_button_t* selected_button = nullptr; // when using cursor keys to navigate buttons, the 'pressed' image is persistent



static const main_menu_button_t* get_button_at_position(SDL_Point point)
{
    for (const auto& state : main_menu_buttons)
    {
        if (state.contains_point(point)) { return &state; }
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
// main menu images

static ImageBlitter* main_menu_unpressed = nullptr;
static ImageBlitter* main_menu_pressed = nullptr;


// TODO: extract this crap to legacy importer, transforming to modern file format

static void m1_add_shape_to_surface(SDL_Surface* surface, int32_t shape_id, const SDL_Point& position)
{
    SDL_Surface* shape = get_shape_surface(shape_id, 10);
    if (!shape) return;
    SDL_Rect src = {0, 0, shape->w, shape->h};
    SDL_Rect dst = {position.x, position.y, shape->w, shape->h};
    SDL_BlitSurface(shape, &src, surface, &dst);
    SDL_FreeSurface(shape);
}


static void m1_add_pressed_button_to_surface(SDL_Surface* surface, app_state_t action, int32_t shape_id)
{
    SDL_Rect dst = get_main_menu_button_rect_for_action(action);
    m1_add_shape_to_surface(surface, shape_id, {dst.x, dst.y});
}


// In M1, the main menu is assembled from multiple bitmaps in Shapes collection 10, so composite into M2-style 640x480 picts here.
static void create_m1_main_menu(SDL_Surface*& unpressed, SDL_Surface*& pressed)
{
    unpressed = create_sdl_surface_32(640, 480);
    SDL_FillRect(unpressed, nullptr, SDL_MapRGB(unpressed->format, 0, 0, 0));
    
    // load M1 Shapes' HUD collection (10)
    mark_collection_for_loading(10);
    load_collections(false);
    
    // construct the unpressed background image
    m1_add_shape_to_surface(unpressed,  0, { 75,   0}); // MARATHON logo
    m1_add_shape_to_surface(unpressed, 19, {191, 466}); // copyright line
    m1_add_shape_to_surface(unpressed,  1, {102, 117}); // panel
    
    // now copy the unpressed image and add pressed buttons to it
    pressed = SDL_ConvertSurface(unpressed, unpressed->format, SDL_SWSURFACE);
    
    // MML-defined rects must already be loaded
    m1_add_pressed_button_to_surface(pressed, app_state_t::start_new_campaign,           11);
    m1_add_pressed_button_to_surface(pressed, app_state_t::choose_saved_game,            12);
    m1_add_pressed_button_to_surface(pressed, app_state_t::gather_network_game,           3);
    m1_add_pressed_button_to_surface(pressed, app_state_t::join_network_game,             4);
    m1_add_pressed_button_to_surface(pressed, app_state_t::preferences,                   5);
    m1_add_pressed_button_to_surface(pressed, app_state_t::load_and_play_last_film,       6);
    m1_add_pressed_button_to_surface(pressed, app_state_t::save_last_film,                7);
    m1_add_pressed_button_to_surface(pressed, app_state_t::load_and_play_saved_film,      8);
    m1_add_pressed_button_to_surface(pressed, app_state_t::credits,                       9);
    m1_add_pressed_button_to_surface(pressed, app_state_t::quit,                         10);
    m1_add_pressed_button_to_surface(pressed, app_state_t::center,                        2);
}


// used by load_main_menu_picts below to compose the embedded "Aleph One" button images into main menu
static SDL_Surface* read_bmp_data(const uint8_t* data, int32_t size)
{
    SDL_RWops* rw = SDL_RWFromConstMem(data, size);
    SDL_Surface* surface = SDL_LoadBMP_RW(rw, 0);
    SDL_RWclose(rw);
    return surface;
}


// if using OGL blitters, their GPU textures will be unloaded when ImageBlitter::unload_all is called
SDL_Surface* unpressed_surface = nullptr;
SDL_Surface* pressed_surface   = nullptr;

// TODO: FIX: this needs called each time a scenario is loaded or when switching between SW and OGL in Preferences; for now, the menu won't change after the scenario does
static void load_main_menu_picts()
{
    if (shapes_file_is_m1())
    {
        create_m1_main_menu(unpressed_surface, pressed_surface);
        assert_fail(unpressed_surface && pressed_surface, "");
    }
    else
    {
        unpressed_surface = get_pict_resource_from_images(M2_MAIN_MENU_BASE);
        pressed_surface = get_pict_resource_from_images(M2_MAIN_MENU_BASE + 1);
    }
    
    if (unpressed_surface && pressed_surface)
    {
        // compose the "About Aleph One" button into the menu picts...
        SDL_Rect rect = get_main_menu_button_rect_for_action(app_state_t::about_ao);
        if (rect.w > 0 && rect.h > 0)
        {
            SDL_Surface* unpressed_button = read_bmp_data(powered_by_alephone_bmp, sizeof(powered_by_alephone_bmp));
            SDL_BlitSurface(unpressed_button, nullptr, unpressed_surface, &rect);
            SDL_FreeSurface(unpressed_button);
            
            SDL_Surface* pressed_button = read_bmp_data(powered_by_alephone_h_bmp, sizeof(powered_by_alephone_h_bmp));
            SDL_BlitSurface(pressed_button, nullptr, pressed_surface, &rect);
            SDL_FreeSurface(pressed_button);
        }
        
        // ...and add the picts to the main menu blitters
        main_menu_unpressed = new ImageBlitter();
        main_menu_pressed = new ImageBlitter();
        
        main_menu_unpressed->borrow_surface(unpressed_surface);
        main_menu_pressed->borrow_surface(pressed_surface);
    }
    else
    {
        // if one loads but other doesn't, this'll free the one that did so we don't leak memory
        SDL_FreeSurface(unpressed_surface);
        SDL_FreeSurface(pressed_surface);
        unpressed_surface = nullptr;
        pressed_surface   = nullptr;
        // TODO: draw text-only main menu using dialog widgets
        TODO("generate a default main menu (text only) when images aren't available");
    }
}


ImageBlitter* get_main_menu_unpressed()
{
    if (!main_menu_unpressed) // TODO: FIX: temporary; see above
    {
        load_main_menu_picts();
    }
    else if (!main_menu_unpressed->has_surface()) // TODO: it might be better if ImageBlitter.unload doesn't discard the Surface (if it owns the surface, it can free it in ~ImageBlitter or when a new surface is loaded)
    {
        main_menu_unpressed->borrow_surface(unpressed_surface);
        main_menu_pressed->borrow_surface(pressed_surface);
    }
    assert_fail(main_menu_unpressed, "");
    return main_menu_unpressed;
}


ImageBlitter* get_main_menu_pressed()
{
    // unpressed is always initialized first
    assert_fail(main_menu_pressed, "");
    return main_menu_pressed;
}


// -----------------------------------------------------------------------------------------
// input handling


void do_action(app_state_t action, bool is_cheat = false)
{
    if (action == app_state_t::start_new_campaign && is_cheat) { action = app_state_t::choose_vidmaster_level; }
    set_next_app_state(action);
}


// keyboard shortcuts, e.g. pressing "p" key for prefs, momentarily "press" the corresponding button
void draw_main_menu_button_momentarily_pressed(const main_menu_button_t* button)
{
    assert_fail(get_app_state() == app_state_t::main_menu, "");
    
    button->draw_pressed();
    main_screen.swap();
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
    
    ImageBlitter* blitter = get_main_menu_unpressed();
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
// called by main_event_loop during app_state_t::main_menu


void handle_main_menu_mouse_input(const SDL_Event &event)
{
    // Was the mouse clicked inside a button rect?
    SDL_Point position = main_screen.get_mouse_virtual_position();
    selected_button = get_button_at_position(position);
        
    // If it was, show the button's pressed image
    if (selected_button && selected_button->is_enabled())
    {
        // TODO: these need to move
    //    stop_ui_fade();
        show_cursor();
        
        get_main_menu_unpressed()->render_to_screen();
        selected_button->draw_pressed();
        main_screen.swap();
        
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
                        position = main_screen.get_mouse_virtual_position();
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
                const main_menu_button_t* new_button = get_button_at_position(position);
                if (new_button != selected_button) // mouse has moved out of (or back into) button rect
                {
                    get_main_menu_unpressed()->render_to_screen();
                    if (new_button) { new_button->draw_pressed(); }
                    selected_button = new_button;
                    
                    main_screen.swap();
                    
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
        main_screen.swap();
        get_main_menu_unpressed()->render_to_screen();
        
        if (selected_button)
        {
            do_action(selected_button->action, has_cheat_keys_modifier());
            selected_button = nullptr;
        }
    }
}


void handle_main_menu_keyboard_input(const SDL_Event &event)
{    
    SDL_Keycode key = event.key.keysym.sym;
    
    // TODO: redo button sounds later
    switch (key)
    {
        case SDLK_F1:
            if (main_screen.decrease_mode())
            {
                main_screen.clear(); // TODO: there's a momentary blink when resizing which isn't ideal
                get_main_menu_unpressed()->render_to_screen();
            }
            return;
        case SDLK_F2:
            if (main_screen.increase_mode())
            {
                main_screen.clear();
                get_main_menu_unpressed()->render_to_screen();
            }
            return;
            
        case SDLK_F3:
            sound_manager.decrease_volume();
            return;
        case SDLK_F4:
            sound_manager.increase_volume();
            return;
            
        case SDLK_F5:
        case SDLK_F6:
            // unused ()decrease/increase gameworld gamma
            return;
                  
        case SDLK_F7:
        case SDLK_F8:
            // unused (decrease/increase HUD size in-game)
            return;
            
        case SDLK_F9:
            // TODO: activate/deactivate Console
            return;
            
        case SDLK_F10:
            set_next_app_state(app_state_t::preferences);
            return;
            
#ifndef HAVE_STEAM
        case SDLK_F11:
        case SDLK_F12:
            dump_screen();
            return;
#endif
    }
    
    
    for (const auto& button : main_menu_buttons)
    {
        if (key == button.key)
        {
            process_button_press(button.action, has_cheat_keys_modifier());
            return;
        }
    }
    
    switch (key)
    {
            // TODO: F-keys should be handled by the caller
            // standard function keys
            //case SDLK_F6: // F6 toggles between windowed and fullscreen modes on UI screens and in-game
            //    main_screen.toggle_fullscreen();
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
            process_button_press(selected_button ? selected_button->action : app_state_t::start_new_campaign, has_cheat_keys_modifier());
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
            process_button_press(app_state_t::start_new_campaign, false);
            break;
        case AO_SCANCODE_BASE_JOYSTICK_BUTTON + SDL_CONTROLLER_BUTTON_GUIDE:
            process_button_press(app_state_t::start_new_campaign, true);
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
    
    main_screen.clear();
    main_screen.configure_for_classic_ui(); // TODO: make this configurable in scenario
    
    set_interface_fade_renderer([](float opacity){ get_main_menu_unpressed()->render_to_screen(); });
    animate_interface_fade_in(LONG_FADE_DURATION);
    
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

