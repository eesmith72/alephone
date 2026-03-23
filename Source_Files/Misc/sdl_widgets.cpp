/*
 *  sdl_widgets.cpp - Widgets for SDL dialogs
 *
 *  Written in 2000 by Christian Bauer
 
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

#include "cseries.h"
#include "sdl_dialogs.h"
#include "network_dialog_widgets_sdl.h"
#include "fonts.hpp"
#include "sdl_widgets.h"
#include "resource_manager.h"

#include "choose_file_dialogs_os.hpp"

#include "shapes.h"
#include "screen_drawing.h"
#include "images.h"
#include "shell.h"
#include "world.h"
#include "SoundManager.h"
#include "interface.h"
#include "player.h"

#include "screen.h"

#include "mouse.h"   // (ZZZ) NUM_SDL_MOUSE_BUTTONS, SDLK_BASE_MOUSE_BUTTON
#include "joystick.h"


/*
 *  Widget base class
 */

// ZZZ: initialize my additional storage elements
// (thought: I guess "widget" could be simplified, and have a subclass "useful_widget" to handle most things
// other than spacers.  Spacers are common and I guess we're starting to eat a fair amount of storage for a
// widget that does nothing and draws nothing... oh well, at least RAM is cheap.  ;) )
widget::widget() : widget_theme_id(-1), active(false), dirty(false), enabled(true), cached_font(nullptr),
                   identifier(NONE), owning_dialog(nullptr), saved_min_width(0), saved_min_height(0), label_widget(0)
{
    rect.x = 0;
    rect.y = 0;
    rect.w = 0;
    rect.h = 0;
}

widget::widget(int32_t widget_theme_id) : widget_theme_id(widget_theme_id), active(false), dirty(false), enabled(true), cached_font(nullptr),
                                       identifier(NONE), owning_dialog(nullptr), saved_min_width(0), saved_min_height(0), label_widget(0)
{
    rect.x = 0;
    rect.y = 0;
    rect.w = 0;
    rect.h = 0;
}


void widget::set_label(w_label* label_widget_)
{
    label_widget = label_widget_;
}


w_label* widget::adding_label(const std::string& text)
{
    if (!label_widget)
    {
        label_widget = new w_label(text);
        label_widget->wrap_widget(this);
    }
    
    return label_widget;
}


// ZZZ: enable/disable
void widget::set_enabled(bool inEnabled)
{
    if (enabled != inEnabled)
    {
        enabled = inEnabled;
        
        // If we had the focus when we were disabled, we should not have the focus afterward.
        if(active && !enabled)
            owning_dialog->activate_next_widget();
        
        // Assume we need a redraw to reflect new state
        dirty = true;
        
        if (label_widget) { label_widget->set_enabled(inEnabled); }
    }
}

void widget::set_active(bool new_active)
{
    // Assume we need a redraw to reflect new state
    if (enabled && (active != new_active)) { dirty = true; }
    active = new_active;
}

void widget::place(const SDL_Rect &r, placement_flags flags)
{
    rect.h = r.h;
    rect.y = r.y;
    
    if (flags & placeable::kFill)
    {
        rect.x = r.x;
        rect.w = r.w;
    }
    else
    {
        rect.w = saved_min_width;
        if (flags & placeable::kAlignLeft)
        {
            rect.x = r.x;
        }
        else if (flags & placeable::kAlignRight)
        {
            rect.x = r.x + r.w - saved_min_width;
        }
        else
        {
            rect.x = r.x + (r.w - saved_min_width) / 2;
        }
    }
}


/*
 *  Static text
 */

w_static_text::w_static_text(const std::string& text, int32_t _theme_type) : text(text), widget(_theme_type), theme_type(_theme_type)
{
    rect.w = get_font()->measure_width(text);
    rect.h = get_font()->line_height;
    saved_min_height = rect.h;
    saved_min_width = rect.w;
}


void w_static_text::draw(Canvas* canvas)
{
    canvas->draw_text(text, get_font(), get_theme_color(theme_type, DEFAULT_STATE, 0), {rect.x, rect.y + get_font()->ascent});
}


void w_label::click(int32_t x, int32_t y)
{
    // simulate a mouse press
    mouse_down(0, 0);
    sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 12);
    mouse_up(0, 0);
}


void w_label::mouse_down(int32_t, int32_t)
{
    if (!wrapped_widget) return;
    down = true;
}


void w_label::mouse_up(int32_t x, int32_t y)
{
    if (!wrapped_widget || !down) return;
    down = false;
    if (x >= 0 && x <= rect.w && y >= 0 && y <= rect.h) { wrapped_widget->click(0, 0); }
}

void w_label::draw(Canvas* canvas)
{
    int32_t state = enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
    uint16 style = 0;
    canvas->draw_text(text, get_font(), get_theme_color(LABEL_WIDGET, state, FOREGROUND_COLOR),
                      {rect.x, rect.y + get_font()->ascent + (rect.h - get_font()->line_height) / 2});
}


void w_static_text::set_text(const std::string& text_)
{
    text = text_;
    dirty = true;
}


w_static_text::~w_static_text() { }


// TODO: not clear why `text_string` is also captured
w_styled_text::w_styled_text(const std::string& text, int32_t _theme_type) : w_static_text(text, _theme_type), text_string(text)
{
    TODO("needs StyledFontRenderer");
 //   rect.w = font->styled_text_width(text_string, style);
 //   saved_min_width = rect.w;
}


void w_styled_text::set_text(const std::string& text_)
{
    w_static_text::set_text(text_);
    text_string = text_;
    // maybe reset rect.width here, but parent w_static_text doesn't, so we won't either
}


void w_styled_text::draw(Canvas* canvas)
{
    canvas->draw_styled_text(text_string, get_font(), get_theme_color(theme_type, DEFAULT_STATE, 0), {rect.x, rect.y + get_font()->ascent});
}


void w_slider_text::draw(Canvas* canvas)
{
    int32_t state = associated_slider->enabled ? (associated_slider->active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
    canvas->draw_text(text, get_font(), get_theme_color(LABEL_WIDGET, state, FOREGROUND_COLOR),
                      {rect.x, rect.y + get_font()->ascent + (rect.h - get_font()->line_height) / 2});
}


/*
 *  Button
 */

w_button_base::w_button_base(const std::string& t, action_proc p, void* a, int32_t _type) : widget(_type), text(t), proc(p), arg(a), down(false), pressed(false), type(_type)
{
    rect.w = get_font()->measure_width(text) + get_theme_space(_type, BUTTON_L_SPACE) + get_theme_space(_type, BUTTON_R_SPACE);
    button_c_default = get_theme_image(_type, DEFAULT_STATE, BUTTON_C_IMAGE, rect.w - get_theme_image(_type, DEFAULT_STATE, BUTTON_L_IMAGE)->w - get_theme_image(_type, DEFAULT_STATE, BUTTON_R_IMAGE)->w);
    button_c_active = get_theme_image(_type, ACTIVE_STATE, BUTTON_C_IMAGE, rect.w - get_theme_image(_type, ACTIVE_STATE, BUTTON_L_IMAGE)->w - get_theme_image(_type, ACTIVE_STATE, BUTTON_R_IMAGE)->w);
    button_c_disabled = get_theme_image(_type, DISABLED_STATE, BUTTON_C_IMAGE, rect.w - get_theme_image(_type, DISABLED_STATE, BUTTON_L_IMAGE)->w - get_theme_image(_type, DISABLED_STATE, BUTTON_R_IMAGE)->w);
    button_c_pressed = get_theme_image(_type, PRESSED_STATE, BUTTON_C_IMAGE, rect.w - get_theme_image(_type, PRESSED_STATE, BUTTON_L_IMAGE)->w - get_theme_image(_type, PRESSED_STATE, BUTTON_R_IMAGE)->w);
    
    rect.h = static_cast<uint16>(get_theme_space(_type, BUTTON_HEIGHT));
    
    saved_min_width = rect.w;
    saved_min_height = rect.h;
}


w_button_base::~w_button_base()
{
    SDL_FreeSurface(button_c_default);
    SDL_FreeSurface(button_c_active);
    SDL_FreeSurface(button_c_disabled);
    SDL_FreeSurface(button_c_pressed);
}


void w_button_base::set_callback(action_proc p, void* a)
{
    proc = p;
    arg = a;
}


void w_button_base::draw(Canvas* canvas)
{
    // Label (ZZZ: different color for disabled)
    int32_t state = DEFAULT_STATE;
    if (pressed)
        state = PRESSED_STATE;
    else if (!enabled)
        state = DISABLED_STATE;
    else if (active)
        state = ACTIVE_STATE;
    
    if (use_theme_images(type))
    {
        SDL_Surface* button_l = get_theme_image(type, state, BUTTON_L_IMAGE);
        SDL_Surface* button_r = get_theme_image(type, state, BUTTON_R_IMAGE);
        SDL_Surface* button_c = button_c_default;
        if (pressed)
            button_c = button_c_pressed;
        else if (!enabled)
            button_c = button_c_disabled;
        else if (active)
            button_c = button_c_active;
        
        // Button image
        SDL_Rect r = {rect.x, rect.y, button_l->w, button_l->h};
        canvas->draw_surface(button_l, r);
        r = {r.x + button_l->w, r.y, button_c->w, button_c->h};
        canvas->draw_surface(button_c, r);
        r = {r.x + button_c->w, r.y, button_r->w, button_r->h};
        canvas->draw_surface(button_r, r);
    }
    else
    {
        if (use_theme_color(type, BACKGROUND_COLOR))
        {
            SDL_Rect r = {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
            canvas->draw_filled_rect(r, get_theme_color(type, state, BACKGROUND_COLOR));
        }
        canvas->draw_outlined_rect(rect, get_theme_color(type, state, FRAME_COLOR));
    }
    
    canvas->draw_text(text, get_font(), get_theme_color(type, state),
                      {rect.x + get_theme_space(type, BUTTON_L_SPACE), rect.y + get_theme_space(type, BUTTON_T_SPACE) + get_font()->ascent});
}


void w_button_base::mouse_move(int32_t x, int32_t y)
{
    if (down)
    {
        if (x >= 0 && x <= rect.w && y >= 0 && y <= rect.h)
        {
            if (!pressed) { dirty = true; }
            pressed = true;
        }
        else
        {
            if (pressed) { dirty = true; }
            pressed = false;
        }
        get_owning_dialog()->draw_dirty_widgets();
    }
}


void w_button_base::mouse_down(int32_t, int32_t)
{
    if (!enabled) return;
    down = true;
    pressed = true;
    dirty = true;
    get_owning_dialog()->draw_dirty_widgets();
}


void w_button_base::mouse_up(int32_t x, int32_t y)
{
    if (!enabled) return;
    
    down = false;
    pressed = false;
    dirty = true;
    get_owning_dialog()->draw_dirty_widgets();
    
    if (proc && x >= 0 && x <= rect.w && y >= 0 && y <= rect.h) { proc(arg); }
}


void w_button_base::click(int32_t /*x*/, int32_t /*y*/)
{
    // simulate a mouse press
    mouse_down(0, 0);
    sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 12);
    mouse_up(0, 0);
}


/*
 * Clickable link
 */

void w_hyperlink::prochandler(void* arg)
{
    set_full_screen_enabled(false);
    open_url_in_browser(static_cast<const w_hyperlink*>(arg)->url);
    get_owning_dialog()->draw();
}


// this seems to be URL plus the text to display over it, but its usage is all over the farm

w_hyperlink::w_hyperlink(const std::string& url, const std::string& label)

    : w_button_base(
                // the visible text label
                (!label.empty() ? label : url), // if the [descriptive] text [label] is empty, the URL will appear on screen
                
                // click callback
                std::bind(&w_hyperlink::prochandler, this, std::placeholders::_1),
                
                
                this, // was &url, but that's getting mess // argument to the callback (which should be the fucking widget, but it was architected by fools)
                
                HYPERLINK_WIDGET // why the fuck the base class needs a 'type' enum... smh, it's procedural "OOP", in C++ of all things
                    
        ), url(url)
{
    rect.w = get_font()->measure_width(text);
    rect.h = get_font()->line_height;
    saved_min_height = rect.h;
    saved_min_width = rect.w;
}


void w_hyperlink::draw(Canvas* canvas)
{
    int32_t state = DEFAULT_STATE;
    if (pressed)
        state = PRESSED_STATE;
    else if (!enabled)
        state = DISABLED_STATE;
    else if (active)
        state = ACTIVE_STATE;
    
    canvas->draw_text(text, get_font(), get_theme_color(HYPERLINK_WIDGET, state, 0), {rect.x, rect.y + get_font()->ascent});
}


/*
 * Tabs
 */

w_tab::w_tab(const std::vector<string>& _labels, tab_placer *_placer) : widget(TAB_WIDGET), labels(_labels), placer(_placer), active_tab(1), pressed_tab(0)
{
    saved_min_height = get_theme_space(TAB_WIDGET, BUTTON_HEIGHT);
    for (std::vector<string>::iterator it = labels.begin(); it != labels.end(); ++it)
    {
        int32_t l_space = (it == labels.begin()) ? get_theme_space(TAB_WIDGET, BUTTON_L_SPACE) : get_theme_space(TAB_WIDGET, TAB_LC_SPACE);
        int32_t r_space = (it == labels.end() - 1) ? get_theme_space(TAB_WIDGET, BUTTON_R_SPACE) : get_theme_space(TAB_WIDGET, TAB_RC_SPACE);
        int32_t width = l_space + r_space + get_font()->measure_width(*it);
        widths.push_back(width);
        saved_min_width += width;
        
        // load center images
        const int32_t states[] = { DEFAULT_STATE, PRESSED_STATE, ACTIVE_STATE, DISABLED_STATE };
        images.resize(PRESSED_STATE + 1);
        for (int32_t i = 0; i < 4; ++i)
        {
            int32_t li_space = (it == labels.begin()) ? get_theme_image(TAB_WIDGET, states[i], TAB_L_IMAGE)->w : get_theme_image(TAB_WIDGET, states[i], TAB_LC_IMAGE)->w;
            int32_t ri_space = (it == labels.end() - 1) ? get_theme_image(TAB_WIDGET, states[i], TAB_R_IMAGE)->w : get_theme_image(TAB_WIDGET, states[i], TAB_RC_IMAGE)->w;
            int32_t c_space = width - li_space - ri_space;
            images[states[i]].push_back(get_theme_image(TAB_WIDGET, states[i], TAB_C_IMAGE, c_space));
        }
    }
    
}


w_tab::~w_tab()
{
    for (std::vector<std::vector<SDL_Surface *> >::iterator it = images.begin(); it != images.end(); ++it)
    {
        for (std::vector<SDL_Surface *>::iterator it2 = it->begin(); it2 != it->end(); ++it2)
        {
            SDL_FreeSurface(*it2);
        }
    }
}


void w_tab::draw(Canvas* canvas)
{
    int32_t x = rect.x;
    for (int32_t i = 0; i < labels.size(); ++i)
    {
        int32_t state;
        if (!enabled)
            state = DISABLED_STATE;
        else if (i == pressed_tab)
            state = PRESSED_STATE;
        else if (active && i == active_tab)
            state = ACTIVE_STATE;
        else
            state = DEFAULT_STATE;
        
        SDL_Surface* l_image;
        int32_t l_space;
        int32_t l_offset = 0;
        
        if (i == 0)
        {
            l_image = get_theme_image(TAB_WIDGET, state, TAB_L_IMAGE);
            l_space = get_theme_space(TAB_WIDGET, BUTTON_L_SPACE);
            l_offset = 1;
        }
        else
        {
            l_image = get_theme_image(TAB_WIDGET, state, TAB_LC_IMAGE);
            l_space = get_theme_space(TAB_WIDGET, TAB_LC_SPACE);
        }
        
        SDL_Surface* r_image;
        int32_t r_space;
        int32_t r_offset = 0;
        
        if (i == labels.size() - 1)
        {
            r_image = get_theme_image(TAB_WIDGET, state, TAB_R_IMAGE);
            r_space = get_theme_space(TAB_WIDGET, BUTTON_R_SPACE);
            r_offset = 1;
        }
        else
        {
            r_image = get_theme_image(TAB_WIDGET, state, TAB_RC_IMAGE);
            r_space = get_theme_space(TAB_WIDGET, TAB_RC_SPACE);
        }
        
        SDL_Rect r;
        int32_t c_space;
        SDL_Surface* c_image = images[state][i];
        c_space = get_font()->measure_width(labels[i]);
        
        if (use_theme_images(TAB_WIDGET))
        {
            r = {x, rect.y, l_image->w, l_image->h};
            canvas->draw_surface(l_image, r);
            r = {r.x + l_image->w, r.y, c_image->w, c_image->h};
            canvas->draw_surface(c_image, r);
            r = {r.x + c_image->w, r.y, r_image->w, r_image->h};
            canvas->draw_surface(r_image, r);
        }
        else if (use_theme_color(TAB_WIDGET, BACKGROUND_COLOR))
        {
            r = {x + l_offset, rect.y + 1, l_space + c_space + r_space - l_offset - r_offset, get_theme_space(TAB_WIDGET, BUTTON_HEIGHT) - 2};
            canvas->draw_filled_rect(r, get_theme_color(TAB_WIDGET, state, BACKGROUND_COLOR));
        }
        
        canvas->draw_text(labels[i], get_font(), get_theme_color(TAB_WIDGET, state, FOREGROUND_COLOR),
                          {x + l_space, rect.y + get_theme_space(TAB_WIDGET, BUTTON_T_SPACE) + get_font()->ascent});
        
        x += l_space + c_space + r_space;
    }
    
    if (!use_theme_images(TAB_WIDGET)) // draw the frame
    {
        canvas->draw_outlined_rect({rect.x, rect.y, x - rect.x, get_theme_space(TAB_WIDGET, BUTTON_HEIGHT)},
                                   get_theme_color(TAB_WIDGET, DEFAULT_STATE, FRAME_COLOR));
    }
}

void w_tab::choose_tab(int32_t i)
{
    pressed_tab = i;
    active_tab = (i + 1) % labels.size();
    placer->choose_tab(i);
    get_owning_dialog()->draw();
}

void w_tab::click(int32_t x, int32_t y)
{
    if (enabled)
    {
        if (!x && !y)
        {
            choose_tab(active_tab);
        }
        else
        {
            int32_t width = 0;
            for (int32_t i = 0; i < labels.size(); ++i)
            {
                if (x > width && x < width + widths[i])
                {
                    choose_tab(i);
                    return;
                }
                
                width += widths[i];
            }
        }
    }
}

void w_tab::event(SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN)
    {
        switch (e.key.keysym.sym) {
            case SDLK_LEFT:
                if (active_tab > 0)
                {
                    if (active_tab - 1== pressed_tab)
                    {
                        if (pressed_tab > 0) { active_tab -= 2; }
                    }
                    else
                    {
                        active_tab--;
                    }
                }
                dirty = true;
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_RIGHT:
                if (active_tab < labels.size() - 1)
                {
                    if (active_tab + 1 == pressed_tab)
                    {
                        if (pressed_tab < labels.size() - 1) { active_tab += 2; }
                    }
                    else
                    {
                        active_tab++;
                    }
                }
                dirty = true;
                e.type = SDL_LASTEVENT;
                break;
                
            default:
                break;
                
        }
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                if (active_tab > 0)
                {
                    if (active_tab - 1== pressed_tab)
                    {
                        if (pressed_tab > 0) { active_tab -= 2; }
                    }
                    else
                    {
                        active_tab--;
                    }
                }
                dirty = true;
                e.type = SDL_LASTEVENT;
                break;
                
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                if (active_tab < labels.size() - 1)
                {
                    if (active_tab + 1 == pressed_tab)
                    {
                        if (pressed_tab < labels.size() - 1) { active_tab += 2; }
                    }
                    else
                    {
                        active_tab++;
                    }
                }
                dirty = true;
                e.type = SDL_LASTEVENT;
                break;
                
            default:
                break;
                
        }
    }
}



/*
 *  Selection button
 */

const uint16 MAX_TEXT_WIDTH = 200;

// ZZZ: how come we have to do this?  because of that "&" in the typedef for action_proc?
// Anyway, this fixes the "crash when clicking in the Environment menu" bug we've seen in the Windows version all this time.

w_select_button::w_select_button(const std::string& s, action_proc p, void* a)
    : widget(LABEL_WIDGET), selection(s), proc(p), arg(a), p_flags(placeable::kDefault), down(false)
{
    uint16 max_selection_width = MAX_TEXT_WIDTH;
    
    saved_min_width = max_selection_width;
    saved_min_height = get_font()->line_height;
}

void w_select_button::mouse_down(int32_t, int32_t)
{
    if (!enabled) return;
    down = true;
}

void w_select_button::mouse_up(int32_t x, int32_t y)
{
    if (!enabled || !down) return;
    down = false;
    
    if (proc && x >= 0 && x <= rect.w && y >= 0 && y <= rect.h) { proc(arg); }
}

void w_select_button::click(int32_t /*x*/, int32_t /*y*/)
{
    // simulate a mouse press
    mouse_down(0, 0);
    sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 12);
    mouse_up(0, 0);
}


void w_select_button::draw(Canvas* canvas)
{
    int32_t y = rect.y + get_font()->ascent;
    SDL_Color color = get_theme_color(ITEM_WIDGET, enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE);
    
    canvas->set_clip({rect.x + selection_x, 0, rect.w, canvas->h});
    canvas->draw_text(selection, get_font(), color, {rect.x + selection_x, y});
    canvas->clear_clip();
}


void w_select_button::set_selection(const std::string s)
{
    selection = s;
    if (p_flags & placeable::kAlignRight)
    {
        selection_x = rect.w - get_font()->measure_width(selection);
    }
    dirty = true;
}


void w_select_button::place(const SDL_Rect &r, placement_flags flags)
{
    rect.h = r.h;
    rect.y = r.y;
    
    rect.x = r.x;
    rect.w = r.w;
    p_flags = flags;
    
    if (flags & placeable::kAlignRight)
    {
        selection_x = rect.w - get_font()->measure_width(selection);
    }
    else
    {
        selection_x = 0;
    }
    
}


/*
 *  Selection widget (base class)
 */

// ZZZ: change of behavior/semantics:
// if passed an invalid selection, reset to a VALID one (i.e. 0)
// if no valid labels, returns -1 when asked for selection
// draw(), get_selection() check count() directly instead of trying to keep selection set at -1

static const std::string sNoValidOptionsString = "(no valid options)"; // TODO: any user-visible strings should move to string_resources


void w_select::place(const SDL_Rect& r, placement_flags flags)
{
    rect.h = r.h;
    rect.y = r.y;
    rect.x = r.x;
    rect.w = r.w;
    
}


int32_t w_select::min_width()
{
    return get_largest_label_width();
}


void w_select::draw(Canvas* canvas)
{
    const std::string& str = labels[selection].second;
    int32_t state = enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
    
    canvas->draw_text(str, get_font(), get_theme_color(ITEM_WIDGET, state),
                      {rect.x, rect.y + get_font()->ascent + (rect.h - get_font()->line_height) / 2});
}


void w_select::click(int32_t x, int32_t y) // TODO: huh?
{
    set_selection(selection + 1);
}


void w_select::event(SDL_Event &e)
{
    if (e.type == SDL_KEYDOWN)
    {
        if (e.key.keysym.sym == SDLK_LEFT)
        {
            set_selection(selection - 1);
            e.type = SDL_LASTEVENT; // Swallow event
        }
        else if (e.key.keysym.sym == SDLK_RIGHT)
        {
            set_selection(selection + 1);
            e.type = SDL_LASTEVENT;
        }
        
    } else if (e.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
        {
            set_selection(selection - 1);
            e.type = SDL_LASTEVENT;
        }
        else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
        {
            set_selection(selection + 1);
            e.type = SDL_LASTEVENT;
        }
    }
}

void w_select::set_selection(int32_t sel)
{
    if (enabled && selection != sel)
    {
        selection = PIN(sel, 0, count() - 1);
        selection_changed();
    }
    dirty = true;
}


void w_select::force_selection(int32_t sel)
{
    selection = PIN(sel, -1, count() - 1); // NONE is allowed to unset
    dirty = true;
}

void w_select::set_labels(const id_strings_t& strings)
{
    assert_fail(strings.size() > 0, "");
    labels = strings;
    force_selection(selection);
}

void w_select::set_labels(const strings_t& strings)
{
    assert_fail(strings.size() > 0, "");
    labels.resize(strings.size());
    for (int32_t i = 0; i < strings.size(); i++) { labels[i] = {i, strings[i]}; }
    force_selection(selection);
}


void w_select::load_labels(resource_id_t resource_id)
{
    labels = get_strings_for_resource(resource_id, true); // don't include empty strings
    assert_fail(labels.size() > 0, "");
    force_selection(selection);
}

// TODO: FFS
void w_select_popup::set_labels(const std::vector<string>& inLabels)
{
    labels = inLabels;
    
    // recalculate min width
    saved_min_width = 0;
    for (std::vector<string>::iterator it = labels.begin(); it != labels.end(); ++it)
    {
        uint16 width = get_font()->measure_width(*it);
        if (width > saved_min_width) { saved_min_width = width; }
    }
}


void w_select::selection_changed()
{
    play_dialog_sound(DIALOG_CLICK_SOUND);
    dirty = true;
    if (selection_changed_callback) { selection_changed_callback(this); }
}


// ZZZ addition
uint16 w_select::get_largest_label_width()
{
    uint16 max_label_width = 0;
    for (size_t i = 0; i < count(); i++)
    {
        uint16 width = get_font()->measure_width(labels[i].second);
        if (width > max_label_width) { max_label_width = width; }
    }
    
    // ZZZ: account for "no valid options" string // TODO: move to string_resources
    if(count() <= 0) { max_label_width = get_font()->measure_width(sNoValidOptionsString); }
    
    return max_label_width;
}


/*
 *  On-off toggle
 */

static const strings_t default_onoff_labels = {std::string("\342\230\220"), std::string("\342\230\221")};

w_toggle::w_toggle(bool is_enabled, const strings_t labels) : w_select(is_enabled ? 1 : 0, labels.size() == 2 ? labels : default_onoff_labels)
{
    if (!use_theme_images(CHECKBOX)) // not sure what this is checking for?
    {
        widget_theme_id = CHECKBOX;
    }
    saved_min_height = get_theme_space(CHECKBOX, BUTTON_HEIGHT);
}


void w_toggle::draw(Canvas* canvas)
{
    // Selection (ZZZ: different color for disabled)
    const std::string str = (count() > 0 ? labels[selection].second : sNoValidOptionsString);
    
    printf("active=%d\n", active);
    int32_t state = enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
    bool uses_default_labels = labels[0].second == default_onoff_labels[0] && labels[1].second == default_onoff_labels[1];
    if (uses_default_labels && use_theme_images(CHECKBOX))
    {
        SDL_Surface* image = get_theme_image(CHECKBOX, state, (int32_t)selection);
        canvas->draw_surface(image,
                             {rect.x, rect.y + (rect.h - saved_min_height) / 2 + get_theme_space(CHECKBOX, BUTTON_T_SPACE), image->w, image->h});
    }
    else if (uses_default_labels)
    {
        canvas->draw_text(str, get_font(), get_theme_color(LABEL_WIDGET, state, FOREGROUND_COLOR),
                          {rect.x, rect.y + (rect.h - saved_min_height) / 2 + get_theme_space(CHECKBOX, BUTTON_T_SPACE)});
    }
    else
    {
        canvas->draw_text(str, get_font(), get_theme_color(ITEM_WIDGET, state), {rect.x, rect.y + get_font()->ascent});
    }
}


int32_t w_toggle::min_width()
{
    bool uses_default_labels = labels[0].second == default_onoff_labels[0] && labels[1].second == default_onoff_labels[1];
    if (uses_default_labels && use_theme_images(CHECKBOX))
    {
        return get_theme_image(CHECKBOX, DEFAULT_STATE, 0)->w;
    }
    else
    {
        return w_select::min_width();
    }
    return 0;
}



class w_color_block : public widget
{
public:
    w_color_block(const SDL_Color& color) : m_color(color)
    {
        saved_min_height = 64;
        saved_min_width = 64;
    }
    
    void draw(Canvas* canvas)
    {
        canvas->draw_filled_rect({ rect.x, rect.y, 64, 64 }, m_color);
    }
    
    bool is_dirty() { return true; }
    
private:
    const SDL_Color m_color;
};


void w_color_picker::click(int32_t, int32_t)
{
    if (!enabled) return;
    dialog d;
    
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_title("CHOOSE A COLOR"), d);
    placer->add(new w_spacer(), true);
    
    w_color_block *color_block = new w_color_block(m_color);
    placer->dual_add(color_block, d);
    
    table_placer *table = new table_placer(2, get_theme_space(ITEM_WIDGET));
    table->col_flags(0, placeable::kAlignRight);
    
    w_percentage_slider *r_slider = new w_percentage_slider(16, m_color.r / 16);
    table->dual_add(r_slider->adding_label("Red"), d);
    table->dual_add(r_slider, d);
    
    w_percentage_slider *g_slider = new w_percentage_slider(16, m_color.g / 16);
    table->dual_add(g_slider->adding_label("Green"), d);
    table->dual_add(g_slider, d);
    
    w_percentage_slider *b_slider = new w_percentage_slider(16, m_color.b / 16);
    table->dual_add(b_slider->adding_label("Blue"), d);
    table->dual_add(b_slider, d);
    
    placer->add(table, true);
    placer->add(new w_spacer(), true);
    
    horizontal_placer *button_placer = new horizontal_placer;
    button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
    button_placer->dual_add(new w_button("OK", dialog_ok, &d), d);
    
    placer->add(button_placer, true);
    
    SDL_Color old_color = m_color;
    
    d.set_widget_placer(placer);
    d.set_processing_function(w_color_picker::update_color(r_slider, g_slider, b_slider, &m_color));
    
    if (d.run() == 0)
    {
        m_color.r = r_slider->get_selection() * 4;
        m_color.g = g_slider->get_selection() * 4;
        m_color.b = b_slider->get_selection() * 4;
        
        dirty = true;
        get_owning_dialog()->draw_dirty_widgets();
    }
    else
    {
        m_color = old_color;
    }
}

void w_color_picker::draw(Canvas* canvas)
{
    canvas->draw_filled_rect({rect.x, rect.y + 1, 48, rect.h - 2 }, m_color);
}

/*
 *  Text entry widget
 */

w_text_entry::w_text_entry(size_t max_c, const std::string& initial_text)
    : widget(TEXT_ENTRY_WIDGET), enter_pressed_callback(nullptr), value_changed_callback(nullptr), max_chars(max_c)
{
    set_text(initial_text);
    
    saved_min_width = MAX_TEXT_WIDTH;
    
    saved_min_height = get_font()->line_height;
}


void w_text_entry::place(const SDL_Rect& r, placement_flags flags)
{
    rect.h = get_font()->line_height;
    
    rect.y = r.y + (r.h - rect.h) / 2;
    
    text_x = 0;
    rect.x = r.x;
    rect.w = r.w;
    max_text_width = rect.w;
}

void w_text_entry::draw(Canvas* canvas)
{
    int32_t y = rect.y + get_font()->ascent;
    
    int16 theRectX = rect.x;
    uint16 theRectW = rect.w;
    int16 theTextX = text_x;
    
    // Text
    int32_t x = theRectX + theTextX;
    int32_t width = get_font()->measure_width(text_buffer);
    if (width > max_text_width) { x -= width - max_text_width; }
    
    SDL_Color color = get_theme_color(TEXT_ENTRY_WIDGET, enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE);
    
    canvas->set_clip({theRectX + theTextX, 0, theRectW, canvas->h});
    canvas->draw_text(text_buffer, get_font(), color, {x, y});
    canvas->clear_clip();
    
    // Cursor
    if (active)
    {
        canvas->draw_filled_rect({x + width - (width ? 1 : 0), rect.y, 1, rect.h}, get_theme_color(TEXT_ENTRY_WIDGET, CURSOR_STATE));
    }
}


void w_text_entry::set_active(bool new_active)
{
    if (new_active && !active)
    {
        cursor_position = num_chars;
        SDL_StartTextInput();
    }
    else if (!new_active && active)
    {
        SDL_StopTextInput();
    }
    widget::set_active(new_active);
}


void w_text_entry::event(SDL_Event &e)
{
    if (e.type == SDL_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
            case SDLK_LEFT:
                left:
                if (cursor_position > 0)
                {
                    cursor_position--;
                    dirty = true;
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_RIGHT:
                right:
                if (cursor_position < num_chars)
                {
                    cursor_position++;
                    dirty = true;
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (enter_pressed_callback) { enter_pressed_callback(this); }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_BACKSPACE:
                backspace:
                if (num_chars && cursor_position)
                {
                    // TODO: this probably isn't UTF8-safel need to check
                    memmove(&text_buffer[cursor_position - 1], &text_buffer[cursor_position], num_chars - cursor_position);
                    text_buffer[--num_chars] = 0;
                    --cursor_position;
                    modified_text();
                    play_dialog_sound(DIALOG_DELETE_SOUND);
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_DELETE:
                del:
                if (cursor_position < num_chars)
                {
                    // TODO: this probably isn't UTF8-safel need to check
                    memmove(&text_buffer[cursor_position], &text_buffer[cursor_position + 1], num_chars - cursor_position - 1);
                    text_buffer[--num_chars] = 0;
                    modified_text();
                    play_dialog_sound(DIALOG_DELETE_SOUND);
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_HOME:
                home:
                if (cursor_position > 0)
                {
                    cursor_position = 0;
                    dirty = true;
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_END:
                end:
                if (cursor_position < num_chars)
                {
                    cursor_position = num_chars;
                    dirty = true;
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_UP:
            case SDLK_DOWN:
                break;
                
            case SDLK_a:
                if (e.key.keysym.mod & KMOD_CTRL) { goto home; }
                break;
            case SDLK_b:
                if (e.key.keysym.mod & KMOD_CTRL) { goto left; }
                break;
            case SDLK_d:
                if (e.key.keysym.mod & KMOD_CTRL) { goto del; }
                break;
            case SDLK_e:
                if (e.key.keysym.mod & KMOD_CTRL) { goto end; }
                break;
            case SDLK_f:
                if (e.key.keysym.mod & KMOD_CTRL) { goto right; }
                break;
            case SDLK_h:
                if (e.key.keysym.mod & KMOD_CTRL) { goto backspace; }
                break;
                
            case SDLK_k:
                if (e.key.keysym.mod & KMOD_CTRL)
                {
                    if (cursor_position < num_chars)
                    {
                        // TODO: UTF8
                        num_chars = cursor_position;
                        text_buffer[num_chars] = 0;
                        modified_text();
                        play_dialog_sound(DIALOG_ERASE_SOUND);
                    }
                }
                e.type = SDL_LASTEVENT;
                break;
                
            case SDLK_t:
                if (e.key.keysym.mod & KMOD_CTRL)
                {
                    if (cursor_position)
                    {
                        // TODO: UTF8
                        if (cursor_position == num_chars) { --cursor_position; }
                        char tmp = text_buffer[cursor_position - 1];
                        text_buffer[cursor_position - 1] = text_buffer[cursor_position];
                        text_buffer[cursor_position] = tmp;
                        ++cursor_position;
                        modified_text();
                        play_dialog_sound(DIALOG_TYPE_SOUND);
                    }
                }
                e.type = SDL_LASTEVENT;
                break;
        
            case SDLK_u:
                if (e.key.keysym.mod & KMOD_CTRL)
                {
                    // TODO: UTF8
                    if (num_chars && cursor_position)
                    {
                        memmove(&text_buffer[0], &text_buffer[cursor_position], num_chars - cursor_position);
                        num_chars -= cursor_position;
                        text_buffer[num_chars] = 0;
                        cursor_position = 0;
                        modified_text();
                        play_dialog_sound(DIALOG_ERASE_SOUND);
                    }
                }
                e.type = SDL_LASTEVENT;
                break;
        
            case SDLK_w:
                if (e.key.keysym.mod & KMOD_CTRL)
                {
                    // TODO: UTF8
                    size_t erase_position = cursor_position;
                    while (erase_position && text_buffer[erase_position - 1] == ' ') { --erase_position; }
                    while (erase_position && text_buffer[erase_position - 1] != ' ') { --erase_position; }
                    if (erase_position < cursor_position)
                    {
                        if (cursor_position < num_chars)
                        {
                            memmove(&text_buffer[erase_position], &text_buffer[cursor_position], num_chars - cursor_position);
                        }
                        num_chars -= cursor_position - erase_position;
                        cursor_position = erase_position;
                        modified_text();
                        play_dialog_sound(DIALOG_ERASE_SOUND);
                    }
                }
                e.type = SDL_LASTEVENT;
                break;
        
            default:
                break;
        }
    }
    else if (e.type == SDL_TEXTINPUT)
    {
        // TODO: use UTF8
        /*
        std::string input_utf8 = e.text.text;
        std::string input_roman = utf8_to_mac_roman(input_utf8);
        for (std::string::iterator it = input_roman.begin(); it != input_roman.end(); ++it)
        {
            uint16 uc = *it;
            if (uc >= ' ' && (uc < 0x80) && (num_chars + 1) < max_chars)
            {
                memmove(&text_buffer[cursor_position + 1], &text_buffer[cursor_position], num_chars - cursor_position);
                text_buffer[cursor_position++] = static_cast<char>(uc);
                text_buffer[++num_chars] = 0;
                modified_text();
                play_dialog_sound(DIALOG_TYPE_SOUND);
            }
        }
         */
        e.type = SDL_LASTEVENT;
    }
}


void w_text_entry::click(int32_t x, int32_t y)
{
    TODO("redo this once Render2D/ is overhauled");
    /*
    bool was_active = active;
    get_owning_dialog()->activate_widget(this);
    
    // Don't reposition cursor if:
    // - we were inactive before this click
    // - our text field is empty
    // - the click was simulated (0, 0) or out of bounds
    if (!was_active || !num_chars || (x == 0 && y == 0) || x < 0 || y < 0 || x >= rect.w || y >= rect.h) { return; }
    
    // Find closest character boundary to click
    int32_t width_remaining = x - text_x;
    size_t pos = 0;
    while (pos < num_chars && width_remaining > 0)
    {
        int32_t cw = char_width_muckroman(buf[pos], font, style); TODO: FIX!!!
        if (width_remaining > cw/2) { pos++; } // right side is closer to target than left
        width_remaining -= cw;
    }
    cursor_position = pos;
    dirty = true;
     */
}


void w_text_entry::set_text(const std::string& text)
{
    text_buffer = text;
    num_chars = text_buffer.size();
    cursor_position = num_chars;
    modified_text();
}


void w_text_entry::modified_text()
{
    dirty = true;
    if (value_changed_callback) { value_changed_callback(this); }
}



void w_password_entry::draw(Canvas* canvas)
{
    std::string tmp;
    tmp.resize(text_buffer.size());
    std::fill(tmp.begin(), tmp.end(), '*');
    
    // copy-pasted from w_text entry cos there's only so many STL errors in a day
    int32_t y = rect.y + get_font()->ascent;
    
    int16 theRectX = rect.x;
    uint16 theRectW = rect.w;
    int16 theTextX = text_x;
    
    // Text
    int16 x = theRectX + theTextX;
    uint16 width = get_font()->measure_width(text_buffer);
    if (width > max_text_width) { x -= width - max_text_width; }
    
    int32_t state = enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
    
    canvas->set_clip({theRectX + theTextX, 0, theRectW, canvas->h});
    canvas->draw_text(tmp, get_font(), get_theme_color(TEXT_ENTRY_WIDGET, state), {x, y});
    canvas->clear_clip();
    
    // Cursor
    if (active)
    {
        width = get_font()->measure_width(text_buffer);
        canvas->draw_filled_rect({x + width - (width ? 1 : 0), rect.y, 1, rect.h}, get_theme_color(TEXT_ENTRY_WIDGET, CURSOR_STATE));
    }
}




/*
 *  Number entry widget
 */

w_number_entry::w_number_entry(int32_t initial_number) : w_text_entry(/*16*/4, nullptr)
{
    set_number(initial_number);
    saved_min_width = MAX_TEXT_WIDTH / 2;
}


void w_number_entry::event(SDL_Event &e)
{
    if (e.type == SDL_TEXTINPUT)
    {
        // TODO: use UTF8
        /*
        std::string input_utf8 = e.text.text;
        std::string input_roman = utf8_to_mac_roman(input_utf8);
        for (std::string::iterator it = input_roman.begin(); it != input_roman.end(); ++it)
        {
            uint16 uc = *it;
            if (uc >= '0' && uc <= '9' && (num_chars + 1) < max_chars) {
                memmove(&text_buffer[cursor_position + 1], &text_buffer[cursor_position], num_chars - cursor_position);
                text_buffer[cursor_position++] = static_cast<char>(uc);
                text_buffer[++num_chars] = 0;
                modified_text();
                play_dialog_sound(DIALOG_TYPE_SOUND);
            }
        }
         */
    }
    else
    {
        w_text_entry::event(e);
    }
}



/*
 *  Key name widget
 */

static const strings_t WAITING_TEXT = { "waiting for key", "waiting for button", "waiting for button" };
static const strings_t UNBOUND_TEXT = { "none", "none", "none" };

w_key::w_key(SDL_Scancode key, w_key::Type event_type) : widget(LABEL_WIDGET), binding(false), event_type(event_type)
{
    set_key(key);
    
    saved_min_width = get_font()->measure_width(WAITING_TEXT[event_type]);
    saved_min_height = get_font()->line_height;
}

void w_key::place(const SDL_Rect& r, placement_flags flags)
{
    rect.h = r.h;
    rect.y = r.y;
    
    key_x = 0;
    rect.x = r.x;
    rect.w = r.w;
}

// ZZZ: we provide phony key names for the phony keys used for mouse buttons.
static const char* sMouseButtonKeyName[NUM_SDL_MOUSE_BUTTONS] = {
    "Mouse Left",
    "Mouse Middle",
    "Mouse Right",
    "Mouse X1",
    "Mouse X2",
    "Mouse Scroll Up",
    "Mouse Scroll Down"
};

static const char* get_joystick_button_key_name(int32_t offset)
{
    assert_fail(SDL_CONTROLLER_BUTTON_MAX <= 21 && SDL_CONTROLLER_AXIS_MAX <= 12, "SDL changed the number of buttons/axes again!");
    
    static const char* buttons[] = {
        "A", "B", "X", "Y", "Back", "Guide", "Start",
        "LS", "RS", "LB", "RB", "Up", "Down", "Left", "Right",
        // new in SDL 2.0.14
        "Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "TP Button",
    };
    
    static const char* axes[] = {
        "LS Right", "LS Down", "RS Right", "RS Down", "LT", "RT",
        "LS Left", "LS Up", "RS Left", "RS Up", "LT Neg", "RT Neg"
    };
    
    if (offset < SDL_CONTROLLER_BUTTON_MAX)
    {
        return buttons[offset];
    }
    else
    {
        return axes[offset - SDL_CONTROLLER_BUTTON_MAX];
    }
}

// ZZZ: this injects our phony key names but passes along the rest.
const char* GetSDLKeyName(SDL_Scancode inKey) // TODO: return type
{
    if (w_key::event_type_for_key(inKey) == w_key::MouseButton)
        return sMouseButtonKeyName[inKey - AO_SCANCODE_BASE_MOUSE_BUTTON];
    else if (w_key::event_type_for_key(inKey) == w_key::JoystickButton)
        return get_joystick_button_key_name(inKey - AO_SCANCODE_BASE_JOYSTICK_BUTTON);
    else
        return SDL_GetKeyName(SDL_GetKeyFromScancode(inKey));
}

void w_key::draw(Canvas* canvas)
{
    int32_t y = rect.y + get_font()->ascent;
    
    // Key
    int16 x = rect.x + key_x;
    if (binding)
    {
        canvas->draw_text(WAITING_TEXT[event_type], get_font(), get_theme_color(ITEM_WIDGET, ACTIVE_STATE), {x, y});
    }
    else if (key == SDL_SCANCODE_UNKNOWN)
    {
        int32_t state = enabled ? (active ? ACTIVE_STATE : DISABLED_STATE) : DISABLED_STATE;
        canvas->draw_text(UNBOUND_TEXT[event_type], get_font(), get_theme_color(ITEM_WIDGET, state), {x, y});
    }
    else
    {
        int32_t state = enabled ? (active ? ACTIVE_STATE : DEFAULT_STATE) : DISABLED_STATE;
        canvas->draw_text(GetSDLKeyName(key), get_font(), get_theme_color(ITEM_WIDGET, state), {x, y});
    }
}

void w_key::click(int32_t /*x*/, int32_t /*y*/)
{
    get_owning_dialog()->activate_widget(this);
    if(enabled) {
        if (!binding) {
            binding = true;
            dirty = true;
        }
    }
}

void w_key::set_active(bool new_active) {
    if (!new_active && binding) {
        binding = false;
        dirty = true;
    }
    widget::set_active(new_active);
}

w_key::Type w_key::event_type_for_key(SDL_Scancode key) {
    if (key >= AO_SCANCODE_BASE_MOUSE_BUTTON && key < (AO_SCANCODE_BASE_MOUSE_BUTTON + NUM_SDL_MOUSE_BUTTONS))
        return MouseButton;
    else if (key >= AO_SCANCODE_BASE_JOYSTICK_BUTTON && key < (AO_SCANCODE_BASE_JOYSTICK_BUTTON + NUM_SDL_JOYSTICK_BUTTONS))
        return JoystickButton;
    return KeyboardKey;
}

void w_key::event(SDL_Event &e)
{
    if(binding) {
        bool handled = false;
        bool up = false;
        switch (e.type) {
            case SDL_MOUSEBUTTONDOWN:
                if (event_type == MouseButton) {
                    if (e.button.button < NUM_SDL_REAL_MOUSE_BUTTONS + 1) {
                        set_key(static_cast<SDL_Scancode>(AO_SCANCODE_BASE_MOUSE_BUTTON + e.button.button - 1));
                        handled = true;
                    }
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if ((e.cbutton.button + AO_SCANCODE_BASE_JOYSTICK_BUTTON) == AO_SCANCODE_JOYSTICK_ESCAPE) {
                    set_key(SDL_SCANCODE_UNKNOWN);
                    handled = true;
                } else if (event_type == JoystickButton) {
                    if (e.cbutton.button < SDL_CONTROLLER_BUTTON_MAX) {
                        set_key(static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_BUTTON + e.cbutton.button));
                        handled = true;
                    }
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (event_type == JoystickButton) {
                    if (e.caxis.value >= 16384) {
                        set_key(static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_POSITIVE + e.caxis.axis));
                        handled = true;
                    } else if (e.caxis.value <= -16384) {
                        set_key(static_cast<SDL_Scancode>(AO_SCANCODE_BASE_JOYSTICK_AXIS_NEGATIVE + e.caxis.axis));
                        handled = true;
                    }
                }
                break;
            case SDL_MOUSEWHEEL:
                if (event_type == MouseButton) {
                    up = (e.wheel.y > 0);
#if SDL_VERSION_ATLEAST(2,0,4)
                    if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                        up = !up;
#endif
                    set_key(static_cast<SDL_Scancode>(up ? AO_SCANCODE_MOUSESCROLL_UP : AO_SCANCODE_MOUSESCROLL_DOWN));
                    handled = true;
                }
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    set_key(SDL_SCANCODE_UNKNOWN);
                    handled = true;
                } else if (event_type == KeyboardKey) {
                    set_key(e.key.keysym.scancode);
                    handled = true;
                }
                break;
            case SDL_MOUSEMOTION:
                e.type = SDL_LASTEVENT; // suppress motion while assigning
                break;
            default:
                break;
        }
        
        if (handled) {
            dirty = true;
            binding = false;
            // activate next widget by faking a key press
            e.type = SDL_KEYDOWN;
            e.key.keysym.sym = SDLK_DOWN;
            e.key.keysym.scancode = SDL_SCANCODE_DOWN;
        }
    }
}


void w_key::set_key(SDL_Scancode k)
{
    key = k;
}

/*
 * Progress
 */
void w_progress_bar::draw(Canvas* canvas)
{
    int32_t filled_width = (rect.w - 2) * value / max_value;
    SDL_Rect dst_rect = rect;
    dst_rect.h -= 2;
    dst_rect.y += 2;
    canvas->draw_filled_rect(dst_rect, get_theme_color(MESSAGE_WIDGET, DEFAULT_STATE, FOREGROUND_COLOR));
    dst_rect.x += filled_width + 1;
    dst_rect.y++;
    dst_rect.h -= 2;
    dst_rect.w = dst_rect.w - filled_width - 2;
    if (use_theme_color(DIALOG_FRAME, BACKGROUND_COLOR))
    {
        canvas->draw_filled_rect(dst_rect, get_theme_color(DIALOG_FRAME, DEFAULT_STATE, BACKGROUND_COLOR));
    }
}

void w_progress_bar::set_progress(int32_t inValue, int32_t inMaxValue)
{
    value = inValue;
    max_value = inMaxValue;
    dirty = true;
}



/*
 *  Slider
 */

const int32_t SLIDER_WIDTH = 160;
const int32_t SLIDER_THUMB_HEIGHT = 14;
const int32_t SLIDER_THUMB_WIDTH = 8;
const int32_t SLIDER_TROUGH_HEIGHT = 8;
const int32_t SLIDER_LABEL_SPACE = 5;

w_slider::w_slider(int32_t num, int32_t s)
    : widget(LABEL_WIDGET), selection(s), num_items(num), thumb_dragging(false), readout(nullptr), slider_changed_callback(nullptr)
{
    slider_l = get_theme_image(SLIDER_WIDGET, DEFAULT_STATE, SLIDER_L_IMAGE);
    slider_r = get_theme_image(SLIDER_WIDGET, DEFAULT_STATE, SLIDER_R_IMAGE);
    slider_c = get_theme_image(SLIDER_WIDGET, DEFAULT_STATE, SLIDER_C_IMAGE, SLIDER_WIDTH - slider_l->w - slider_r->w);
    thumb = get_theme_image(SLIDER_THUMB, DEFAULT_STATE, 0);
    
    trough_width = SLIDER_WIDTH - get_theme_space(SLIDER_WIDGET, SLIDER_L_SPACE) - get_theme_space(SLIDER_WIDGET, SLIDER_R_SPACE);
    
    saved_min_width = SLIDER_WIDTH;
    saved_min_height = use_theme_images(SLIDER_WIDGET) ? std::max(static_cast<uint16>(slider_c->h), static_cast<uint16>(thumb->h))
                                                       : SLIDER_THUMB_HEIGHT + 2;
    readout_x = saved_min_width + SLIDER_LABEL_SPACE;
    init_formatted_value();
}


w_slider::~w_slider() { SDL_FreeSurface(slider_c); }


void w_slider::place(const SDL_Rect& r, placement_flags flags)
{
    rect.h = r.h;
    rect.y = r.y + (r.h - saved_min_height) / 2;
    rect.x = r.x;
    slider_x = 0;
    rect.w = r.w;
    
    SDL_Rect r2;
    r2.h = r.h;
    r2.y = r.y;
    r2.x = r.x + readout_x;
    r2.w = readout->min_width();
    readout->place(r2, placeable::kDefault);
    
    set_selection(selection);
}

void w_slider::draw(Canvas* canvas)
{
    SDL_Rect r;
    
    if (use_theme_images(SLIDER_WIDGET))
    {
        // Slider trough
        r = {rect.x + slider_x, rect.y + (saved_min_height - slider_l->h) / 2, slider_l->w, slider_l->h};
        canvas->draw_surface(slider_l, r);
        r = {r.x + slider_l->w, r.y, slider_c->w, slider_c->h};
        canvas->draw_surface(slider_c, r);
        r = {r.x + slider_c->w, r.y, slider_r->w, slider_r->h};
        canvas->draw_surface(slider_r, r);
        
        // Slider thumb
        r = {rect.x + thumb_x, rect.y + (saved_min_height - thumb->h) / 2 + get_theme_space(SLIDER_WIDGET, SLIDER_T_SPACE), thumb->w, thumb->h};
        canvas->draw_surface(thumb, r);
    }
    else
    {
        r = {rect.x, rect.y + (saved_min_height - SLIDER_TROUGH_HEIGHT) / 2 + get_theme_space(SLIDER_WIDGET, SLIDER_T_SPACE), SLIDER_WIDTH, SLIDER_TROUGH_HEIGHT};
        canvas->draw_outlined_rect(r, get_theme_color(SLIDER_WIDGET, DEFAULT_STATE, FRAME_COLOR));
        
        r = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
        canvas->draw_filled_rect(r, get_theme_color(SLIDER_WIDGET, DEFAULT_STATE, FOREGROUND_COLOR));
        
        r = {rect.x + thumb_x, rect.y + (saved_min_height - SLIDER_THUMB_HEIGHT) / 2, thumb_width(), SLIDER_THUMB_HEIGHT};
        canvas->draw_outlined_rect(r, get_theme_color(SLIDER_THUMB, DEFAULT_STATE, FRAME_COLOR));
        
        r = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
        canvas->draw_filled_rect(r, get_theme_color(SLIDER_THUMB, DEFAULT_STATE, FOREGROUND_COLOR));
    }
    readout->draw(canvas);
}


void w_slider::mouse_move(int32_t x, int32_t y)
{
    if (thumb_dragging)
    {
        int32_t delta_x = (x - slider_x - get_theme_space(SLIDER_WIDGET, SLIDER_L_SPACE)) - thumb_drag_x;
        set_selection(delta_x * num_items / (trough_width - thumb_width()));
    }
}


void w_slider::click(int32_t x, int32_t y)
{
    if(enabled)
    {
        if (x >= thumb_x && x < thumb_x + thumb_width())
        {
            thumb_dragging = dirty = true;
            thumb_drag_x = x - thumb_x;
        }
    }
}

void w_slider::event(SDL_Event &e)
{
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_LEFT) {
            set_selection(selection - 1);
            item_selected();
            e.type = SDL_LASTEVENT; // Swallow event
        } else if (e.key.keysym.sym == SDLK_RIGHT) {
            set_selection(selection + 1);
            item_selected();
            e.type = SDL_LASTEVENT; // Swallow event
        }
    } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            set_selection(selection - 1);
            item_selected();
            e.type = SDL_LASTEVENT; // Swallow event
        } else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            set_selection(selection + 1);
            item_selected();
            e.type = SDL_LASTEVENT; // Swallow event
        }
    } else if (e.type == SDL_MOUSEBUTTONUP) {
        if (thumb_dragging) {
            thumb_dragging = false;
            dirty = true;
            item_selected();
        }
    }
}

void w_slider::set_selection(int32_t s)
{
    if (s >= num_items)
        s = num_items - 1;
    else if (s < 0)
        s = 0;
    selection = s;
    thumb_x = int32_t(float(selection * (trough_width - thumb_width())) / (num_items - 1) + 0.5);
    thumb_x += get_theme_space(SLIDER_WIDGET, SLIDER_L_SPACE) + slider_x;
    dirty = true;
    selection_changed();
}

int32_t w_slider::thumb_width() const
{
    if (use_theme_images(SLIDER_WIDGET))
        return thumb->w;
    else
        return SLIDER_THUMB_WIDTH;
}

std::string w_slider::formatted_value()
{
    std::ostringstream ss;
    ss << selection;
    return ss.str();
}

void w_slider::selection_changed()
{
    readout->set_text(formatted_value().c_str());
    
    if (slider_changed_callback != nullptr)
        slider_changed_callback(this);
}

void w_slider::init_formatted_value()
{
    int32_t temp = selection;
    selection = num_items;
    std::string tempstr = formatted_value();
    selection = temp;
    delete readout;
    readout = new w_slider_text(tempstr.c_str());
    readout->associated_slider = this;
    saved_min_height = std::max(saved_min_height, readout->min_height());
    saved_min_width = readout_x + readout->min_width();
}

std::string w_percentage_slider::formatted_value()
{
    std::ostringstream ss;
    ss << (selection * 100 / (num_items - 1)) << "%";
    return ss.str();
}


/*
 *  List selection
 */

w_list_base::w_list_base(uint16_t width, int32_t lines)
    : widget(ITEM_WIDGET), selection(0), shown_items(lines), thumb_dragging(false), top_item(0)
{
    rect.w = width;
    rect.h = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
    
    frame_tl = get_theme_image(LIST_WIDGET, DEFAULT_STATE, TL_IMAGE);
    frame_tr = get_theme_image(LIST_WIDGET, DEFAULT_STATE, TR_IMAGE);
    frame_bl = get_theme_image(LIST_WIDGET, DEFAULT_STATE, BL_IMAGE);
    frame_br = get_theme_image(LIST_WIDGET, DEFAULT_STATE, BR_IMAGE);
    frame_t = get_theme_image(LIST_WIDGET, DEFAULT_STATE, T_IMAGE, rect.w - frame_tl->w - frame_tr->w, 0);
    frame_l = get_theme_image(LIST_WIDGET, DEFAULT_STATE, L_IMAGE, 0, rect.h - frame_tl->h - frame_bl->h);
    frame_r = get_theme_image(LIST_WIDGET, DEFAULT_STATE, R_IMAGE, 0, rect.h - frame_tr->h - frame_br->h);
    frame_b = get_theme_image(LIST_WIDGET, DEFAULT_STATE, B_IMAGE, rect.w - frame_bl->w - frame_br->w, 0);
    
    thumb_t = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_T_IMAGE);
    thumb_tc = nullptr;
    SDL_Surface* thumb_tc_unscaled = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_TC_IMAGE);
    thumb_c = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_C_IMAGE);
    SDL_Surface* thumb_bc_unscaled = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_BC_IMAGE);
    thumb_bc = nullptr;
    thumb_b = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_B_IMAGE);
    
    min_thumb_height = static_cast<uint16>(thumb_t->h + thumb_tc_unscaled->h + thumb_c->h + thumb_bc_unscaled->h + thumb_b->h);
    
    trough_rect.x = rect.w - get_theme_space(LIST_WIDGET, TROUGH_R_SPACE);
    trough_rect.y = get_theme_space(LIST_WIDGET, TROUGH_T_SPACE);
    trough_rect.w = get_theme_space(LIST_WIDGET, TROUGH_WIDTH);
    trough_rect.h = rect.h - get_theme_space(LIST_WIDGET, TROUGH_T_SPACE) - get_theme_space(LIST_WIDGET, TROUGH_B_SPACE);
    
    saved_min_width = rect.w;
    saved_min_height = rect.h;
}


w_list_base::~w_list_base()
{
    SDL_FreeSurface(frame_t);
    SDL_FreeSurface(frame_l);
    SDL_FreeSurface(frame_r);
    SDL_FreeSurface(frame_b);
    SDL_FreeSurface(thumb_tc);
    SDL_FreeSurface(thumb_bc);
}


#define draw_image(canvas, surface, x, y)  ((canvas)->draw_surface((surface), {(x), (y), (surface)->w, (surface)->h}))

void w_list_base::draw(Canvas* canvas)
{
    if (use_theme_images(LIST_WIDGET))
    {
        // Draw frame
        int16 x = rect.x;
        int16 y = rect.y;
        draw_image(canvas, frame_tl, x,                             y);
        draw_image(canvas, frame_t,  x + frame_tl->w,               y);
        draw_image(canvas, frame_tr, x + frame_tl->w + frame_t->w,  y);
        draw_image(canvas, frame_l,  x,                             y + frame_tl->h);
        draw_image(canvas, frame_r,  x + rect.w - frame_r->w,       y + frame_tr->h);
        draw_image(canvas, frame_bl, x,                             y + frame_tl->h + frame_l->h);
        draw_image(canvas, frame_b,  x + frame_bl->w,               y + rect.h - frame_b->h);
        draw_image(canvas, frame_br, x + frame_bl->w + frame_b->w,  y + frame_tr->h + frame_r->h);
        
        // Draw thumb
        x = rect.x + trough_rect.x;
        y = rect.y + thumb_y;
        draw_image(canvas, thumb_t,  x, y);
        draw_image(canvas, thumb_tc, x, (y = y + thumb_t->h));
        draw_image(canvas, thumb_c,  x, (y = y + thumb_tc->h));
        draw_image(canvas, thumb_bc, x, (y = y + thumb_c->h));
        draw_image(canvas, thumb_b,  x, (y = y + thumb_bc->h));
    }
    else
    {
        SDL_Color color = get_theme_color(LIST_WIDGET, DEFAULT_STATE, FRAME_COLOR);
        canvas->draw_outlined_rect(rect, color);
        SDL_Rect real_trough, thumb_rect;
        
        real_trough = {rect.x + trough_rect.x, rect.y + trough_rect.y, trough_rect.w, trough_rect.h};
        canvas->draw_outlined_rect(real_trough, color);
        real_trough = {real_trough.x + 1, real_trough.y + 1, real_trough.w - 2, real_trough.h - 2};
        if (use_theme_color(LIST_THUMB, BACKGROUND_COLOR))
        {
            canvas->draw_filled_rect(real_trough, get_theme_color(LIST_THUMB, DEFAULT_STATE, BACKGROUND_COLOR));
        }
        
        thumb_rect = {rect.x + trough_rect.x, rect.y + thumb_y, trough_rect.w, thumb_t->h + thumb_tc->h + thumb_c->h + thumb_bc->h + thumb_b->h};
        canvas->draw_outlined_rect(thumb_rect, get_theme_color(LIST_THUMB, DEFAULT_STATE, FRAME_COLOR));
        
        thumb_rect = {thumb_rect.x + 1, thumb_rect.y + 1, thumb_rect.w - 2, thumb_rect.h - 2};
        canvas->draw_filled_rect(thumb_rect, get_theme_color(LIST_THUMB, DEFAULT_STATE, FOREGROUND_COLOR));
    }
    draw_items(canvas);
}


void w_list_base::mouse_move(int32_t x, int32_t y)
{
    if (thumb_dragging)
    {
        int32_t delta_y = y - thumb_drag_y;
        if (delta_y > 0 && count() > shown_items && trough_rect.h > thumb_height)
        {
            set_top_item(delta_y * (count() - shown_items) / (trough_rect.h - thumb_height));
        }
        else
        {
            set_top_item(0);
        }
    }
    else if (x < get_theme_space(LIST_WIDGET, L_SPACE) || x >= rect.w - get_theme_space(LIST_WIDGET, R_SPACE)
            || y < get_theme_space(LIST_WIDGET, T_SPACE) || y >= rect.h - get_theme_space(LIST_WIDGET, B_SPACE))
    {
    }
    else if ((y - get_theme_space(LIST_WIDGET, T_SPACE)) / item_height() + top_item < std::min(count(), top_item + shown_items))
    {
        set_selection((y - get_theme_space(LIST_WIDGET, T_SPACE)) / item_height() + top_item);
    }
}


void w_list_base::place(const SDL_Rect& r, placement_flags flags)
{
    widget::place(r, flags);
    
    trough_rect.x = rect.w - get_theme_space(LIST_WIDGET, TROUGH_R_SPACE);
    trough_rect.y = get_theme_space(LIST_WIDGET, TROUGH_T_SPACE);
    trough_rect.w = get_theme_space(LIST_WIDGET, TROUGH_WIDTH);
    trough_rect.h = rect.h - get_theme_space(LIST_WIDGET, TROUGH_T_SPACE) - get_theme_space(LIST_WIDGET, TROUGH_B_SPACE);
    
    frame_t = get_theme_image(LIST_WIDGET, DEFAULT_STATE, T_IMAGE, rect.w - frame_tl->w - frame_tr->w, 0);
    frame_l = get_theme_image(LIST_WIDGET, DEFAULT_STATE, L_IMAGE, 0, rect.h - frame_tl->h - frame_bl->h);
    frame_r = get_theme_image(LIST_WIDGET, DEFAULT_STATE, R_IMAGE, 0, rect.h - frame_tr->h - frame_br->h);
    frame_b = get_theme_image(LIST_WIDGET, DEFAULT_STATE, B_IMAGE, rect.w - frame_bl->w - frame_br->w, 0);
}


void w_list_base::click(int32_t x, int32_t y)
{
    if (x >= trough_rect.x && x < trough_rect.x + trough_rect.w && y >= thumb_y && y <= thumb_y + thumb_height)
    {
        thumb_dragging = dirty = true;
        thumb_drag_y = y - thumb_y;
    }
    else if (count() > 0 && is_item_selectable(selection))
    {
        item_selected();
    }
}


void w_list_base::event(SDL_Event &e)
{
    if (e.type == SDL_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
            case SDLK_UP:
                if (selection != 0) { set_selection(selection - 1); }
                e.type = SDL_LASTEVENT; // Prevent selection of previous widget
                break;
                
            case SDLK_DOWN:
                if (selection < count() - 1) { set_selection(selection + 1); }
                e.type = SDL_LASTEVENT; // Prevent selection of next widget
                break;
                
            case SDLK_PAGEUP:
                set_selection(selection > shown_items ? selection - shown_items : 0);
                break;
                
            case SDLK_PAGEDOWN:
                set_selection(selection + shown_items < count() - 1 ? selection + shown_items : count() - 1);
                break;
                
            case SDLK_HOME:
                set_selection(0);
                break;
                
            case SDLK_END:
                set_selection(count() - 1);
                break;
                
            default:
                break;
        }
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (e.cbutton.button)
        {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (selection != 0) { set_selection(selection - 1); }
                e.type = SDL_LASTEVENT; // Prevent selection of previous widget
                break;
                
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (selection < count() - 1) { set_selection(selection + 1); }
                e.type = SDL_LASTEVENT; // Prevent selection of next widget
                break;
        }
    }
    else if (e.type == SDL_MOUSEBUTTONUP)
    {
        if (thumb_dragging)
        {
            thumb_dragging = false;
            dirty = true;
        }
    }
    else if (e.type == SDL_MOUSEWHEEL)
    {
        int32_t amt = e.wheel.y * -1 * kListScrollSpeed;
        if (amt < 0)
        {
            amt = amt * -1;
            set_top_item(top_item > amt ? top_item - amt : 0);
        }
        else if (amt > 0)
        {
            set_top_item(top_item + amt < count() - shown_items ? top_item + amt : count() - shown_items);
        }
    }
}


void w_list_base::set_selection(int32_t sel)
{
    // Set selection, check for bounds
    assert_fail(sel == PIN(sel, 0, count() - 1), ""); // TODO: FIX: libc++abi: terminating due to uncaught exception of type AOException: ERROR 420002: /sdl_widgets.cpp, set_selection(): failed assertion (sel == PIN(sel, 0, count() - 1)):

    selection = sel;
    dirty = (sel != selection);
    
    // Make selection visible
    if (sel < top_item)
        set_top_item(sel);
    else if (sel >= top_item + shown_items)
        set_top_item(sel - shown_items + 1);
}


void w_list_base::new_items()
{
    // Reset top item and selection
    // ghs: actually, remember top item and selection
    int32_t saved_top_item = top_item;
    int32_t saved_selection = get_selection();
    top_item = selection = 0;
    dirty = true;
    
    // Calculate thumb height
    if (count() <= shown_items)
        thumb_height = trough_rect.h;
    else if (count() == 0)
        thumb_height = static_cast<uint16>(shown_items) * trough_rect.h;
    else
        thumb_height = uint16(float(shown_items * trough_rect.h) / count() + 0.5);
    if (thumb_height < min_thumb_height)
        thumb_height = min_thumb_height;
    else if (thumb_height > trough_rect.h)
        thumb_height = trough_rect.h;
    
    // Create dynamic thumb images
    SDL_FreeSurface(thumb_tc);
    SDL_FreeSurface(thumb_bc);
    int32_t rem_height = thumb_height - thumb_t->h - thumb_c->h - thumb_b->h;
    int32_t dyn_height = rem_height / 2;
    thumb_tc = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_TC_IMAGE, 0, dyn_height);
    thumb_bc = get_theme_image(LIST_THUMB, DEFAULT_STATE, THUMB_BC_IMAGE, 0, (rem_height & 1) ? dyn_height + 1 : dyn_height);
    
    thumb_y = 0;
    if (thumb_y > trough_rect.h - thumb_height) { thumb_y = trough_rect.h - thumb_height; }
    thumb_y = thumb_y + trough_rect.y;
    
    if (saved_selection < count()) { set_selection(saved_selection); }
    if (saved_top_item) { set_top_item(saved_top_item); }
}

void w_list_base::center_item(int32_t i)
{
    set_top_item((i > shown_items / 2) ? i - shown_items / 2 : 0);
}

void w_list_base::set_top_item(int32_t i)
{
    // Set top item (check for bounds)
    i = (count() > shown_items) ? PIN(i, 0, count() - shown_items) : 0;
    if (i != top_item)
        dirty = true;
    top_item = i;
    
    // Calculate thumb y position
    if (count() <= shown_items)
        thumb_y = 0;
    else
        thumb_y = int16(float(top_item * (trough_rect.h - thumb_height)) / (count() - shown_items) + 0.5);
    if (thumb_y > trough_rect.h - thumb_height)
        thumb_y = trough_rect.h - thumb_height;
    thumb_y = thumb_y + trough_rect.y;
}



// ZZZ: maybe this belongs in a new file or something - it's definitely A1-related
// whereas most (but not all) of the other widgets here are sort of 'general-purpose'.
// Anyway, moved here from shell_sdl.h and enhanced ever so slightly, will now be
// using it in the setup network game dialog as well.

/*
 *  Level number dialog
 */

void w_levels::draw_item(std::vector<entry_point>::const_iterator it, Canvas* canvas, int16_t x, int16_t y, uint16_t width, bool selected)
{
    y = y + get_font()->ascent;
    
    std::string str;
    
    if (show_level_numbers)
    {
        str = std::to_string(it->level_number + offset) + " - ";
    }
    str += it->utf8_level_name;
    canvas->set_clip({x, 0, width, canvas->h});
    canvas->draw_text(str, get_font(), get_theme_color(ITEM_WIDGET, selected ? ACTIVE_STATE : DEFAULT_STATE), {x, y});
    canvas->clear_clip();
}


/*
 *  String List
 */

void w_string_list::draw_item(strings_t::const_iterator it, Canvas* canvas, int16_t x, int16_t y, uint16_t width, bool selected)
{
    y = y + get_font()->ascent;
    
    canvas->set_clip({x, 0, width, canvas->h});
    canvas->draw_text(*it, get_font(), get_theme_color(ITEM_WIDGET, selected ? ACTIVE_STATE : DEFAULT_STATE), {x, y});
    canvas->clear_clip();
}


/*
 *  Selection Popup
 */

w_select_popup::w_select_popup(action_proc p, void* a) : w_select_button("", gotSelectedCallback, nullptr)
{
    set_arg(this);
    selection = -1;
    action = p;
    arg = a;
}


void w_select_popup::set_selection (int32_t value)
{
    selection = (value < labels.size() && value >= 0) ? value : -1;
    w_select_button::set_selection((selection == -1) ? "" : labels[selection]);
}


void w_select_popup::gotSelected()
{
    if (labels.size() > 1)
    {
        dialog theDialog;
        vertical_placer *placer = new vertical_placer;
        
        w_string_list* string_list_w = new w_string_list (labels, &theDialog, selection >= 0 ? selection : 0);
        placer->dual_add(string_list_w, theDialog);
        theDialog.activate_widget(string_list_w);
        
        theDialog.set_widget_placer(placer);
        if (theDialog.run() == 0) { set_selection (string_list_w->get_selection()); }
    }
    
    if (action) { action (arg); }
}


const std::string sFileChooserInvalidFileString = "(no valid selection)";

void w_file_chooser::proc()
{
    if (enabled)
    {
        ao_path path = display_read_file_dialog(typecode, dialog_prompt);
        if (!path.empty())
        {
            file = path;
            update_filename();
            if (m_callback) { m_callback(); }
        }
    }
}


void w_file_chooser::update_filename()
{
    if(std::filesystem::is_regular_file(file))
    {
        filename = hide_ao_filename_extension(file.filename());
        set_selection(filename);
    }
    else
    {
        set_selection(sFileChooserInvalidFileString);
    }
}


void w_directory_chooser::proc()
{
    if (enabled)
    {
        ao_path new_dir = display_open_directory_dialog(directory);
        if (!new_dir.empty())
        {
            directory = new_dir;
            update_directoryname();
            if (m_callback) { m_callback(); }
        }
    }
}


void w_directory_chooser::update_directoryname()
{
    if (std::filesystem::is_directory(directory))
    {
        directory_name = directory.filename();
        set_selection(directory_name);
    }
    else
    {
        set_selection(sFileChooserInvalidFileString);
    }
}


const string w_items_in_room_get_name_of_item(GameListMessage::GameListEntry item)
{
    return item.name();
}

const string w_items_in_room_get_name_of_item(prospective_joiner_info item)
{
    return item.name;
}

const string w_items_in_room_get_name_of_item(MetaserverPlayerInfo item)
{
    return item.name();
}


void w_games_in_room::draw_item(const GameListMessage::GameListEntry& item, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected)
{
    int32_t state;
    if (!item.compatible())
    {
        state = item.target() ? SELECTED_INCOMPATIBLE_GAME : INCOMPATIBLE_GAME;
    }
    else if (item.running())
    {
        state = item.target() ? SELECTED_RUNNING_GAME : RUNNING_GAME;
    }
    else
    {
        state = item.target() ? SELECTED_GAME : GAME;
    }
    SDL_Color fg_color = selected ? get_theme_color(ITEM_WIDGET, ACTIVE_STATE) : get_theme_color(METASERVER_GAMES, state, FOREGROUND_COLOR);
    
    SDL_Rect r = { x, y, width, 3 * get_font()->line_height + 2};
    canvas->draw_filled_rect(r, get_theme_color(METASERVER_GAMES, state, BACKGROUND_COLOR));
    
    if (use_theme_color(METASERVER_GAMES, FRAME_COLOR))
    {
        canvas->draw_outlined_rect(r, get_theme_color(METASERVER_GAMES, state, FRAME_COLOR));
    }
    
    x += 1;
    width -= 2;
    y += get_font()->ascent + 1;
    
    std::ostringstream time_or_ping;
    int32_t right_text_width = 0;
    
    // first line, game name, ping or time remaining
    if (item.running())
    {
        if (item.m_description.m_timeLimit && !(item.m_description.m_timeLimit == INT32_MAX || item.m_description.m_timeLimit == -1))
        {
            if (item.minutes_remaining() == 1)
            {
                time_or_ping << "~1 Minute";
            }
            else
            {
                time_or_ping << item.minutes_remaining() << " Minutes";
            }
        }
        else
        {
            time_or_ping << "Untimed";
        }
    }
    else if (item.m_description.m_latency != UINT16_MAX)
    {
        time_or_ping << item.m_description.m_latency << " ms";
    }
    
    right_text_width = get_font()->measure_width(time_or_ping.str());
    
    // draw game name
    canvas->set_clip({x, 0, width - right_text_width, canvas->h});
    canvas->draw_styled_text(item.name(), get_font(), fg_color, {x, y});
    
    // draw remaining or ping
    canvas->set_clip({x, 0, width, canvas->h});
    canvas->draw_text(time_or_ping.str(), get_font(), fg_color, {x + width - right_text_width, y});
    
    y += get_font()->line_height;
    
    std::ostringstream game_and_map;
    
    if (!item.compatible())
    {
        game_and_map << "|i" << item.m_description.m_scenarioName;
        if (item.m_description.m_scenarioVersion != "")
        {
            game_and_map << ", Version " << item.m_description.m_scenarioVersion;
        }
    }
    else
    {
        game_and_map << item.game_string() << " on |i" << item.m_description.m_mapName;
    }
    
    canvas->draw_styled_text(game_and_map.str(), get_font(), fg_color, {x, y});
    
    y += get_font()->line_height;
    
    right_text_width = get_font()->measure_styled_width(item.m_hostPlayerName);
    canvas->set_clip({x, 0, width - right_text_width, canvas->h});
    
    std::ostringstream game_settings;
    if (item.running())
    {
        if (item.m_description.m_numPlayers == 1)
        {
            game_settings << "1 Player";
        }
        else
        {
            game_settings << item.m_description.m_numPlayers << " Players";
        }
    }
    else
    {
        game_settings << item.m_description.m_numPlayers << "/" << item.m_description.m_maxPlayers << " Players";
    }
    
    if (item.m_description.m_timeLimit && !(item.m_description.m_timeLimit == INT32_MAX || item.m_description.m_timeLimit == -1))
    {
        game_settings << ", " << (item.m_description.m_timeLimit / 60 / TICKS_PER_SECOND) << " Minutes";
    }
    
    if (item.m_description.m_teamsAllowed)
    {
        game_settings << ", Teams";
    }
    
    canvas->draw_text(game_settings.str(), get_font(), fg_color, {x, y});
    
    canvas->set_clip({x, 0, width, canvas->h});
    canvas->draw_styled_text(item.m_hostPlayerName, get_font(), fg_color, {x + width - right_text_width});
    canvas->clear_clip();
}


static inline uint8 darken(uint8_t component, uint8_t amount)
{
    return PIN((uint32_t)component * (255 - amount) / 255, 0, 255);
}


static inline uint8_t lighten(uint8_t component, uint8_t amount)
{
    return PIN((uint32_t)component + (255 - component) * amount / 255, 0, 255);
}


void w_players_in_room::draw_item(const MetaserverPlayerInfo& item, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected)
{
    canvas->set_clip({x, 0, width, canvas->h});
    
    SDL_Rect r = {x, y, width, get_font()->line_height + 4};
    
    if (item.target()) {
        canvas->draw_filled_rect(r, {0xff, 0xff, 0xff, 0xff});
    }
    
    // background is player color
    SDL_Color color;
    if (item.target())
    {
        int32_t amount = 0x7f;
        color = {lighten(item.color().r, amount), lighten(item.color().g, amount), lighten(item.color().b, amount)};
    }
    else
    {
        int32_t amount = item.away() ? 0xbf : 0;
        color = {darken(item.color().r, amount), darken(item.color().g, amount), darken(item.color().b, amount)};
    }
    
    r.x = x + kPlayerColorSwatchWidth + kSwatchGutter + 1;
    r.y = y + 1;
    r.w = width - kPlayerColorSwatchWidth - kSwatchGutter - 2;
    r.h = get_font()->line_height + 2;
    canvas->draw_filled_rect(r, color);
    
    // team swatch
    r.x = x + 1;
    r.y = y + 1;
    r.w = kPlayerColorSwatchWidth;
    r.h = get_font()->line_height + 2;
    
    if (item.target())
    {
        int32_t amount = 0x7f;
        color = {lighten(item.team_color().r, amount), lighten(item.team_color().g, amount), lighten(item.team_color().b, amount)};
    }
    else
    {
        int32_t amount = item.away() ? 0x7f : 0;
        color = {darken(item.team_color().r, amount), darken(item.team_color().g, amount), darken(item.team_color().b, amount)};
    }
    
    canvas->draw_filled_rect(r, color);
    
    y += get_font()->ascent;
    if (selected)
    {
        color = get_theme_color(ITEM_WIDGET, ACTIVE_STATE);
    }
    else if (item.away())
    {
        color = {0x7f, 0x7f, 0x7f, 0xff};
    }
    else
    {
        color = {0xff, 0xff, 0xff, 0xff};
    }
    
    const font_t* styled_font = item.away() ? get_font() : get_font()->shadowed();
    canvas->draw_styled_text(item.name(), styled_font, color, {x + kPlayerColorSwatchWidth + kSwatchGutter + 2, y + 1});
    canvas->clear_clip();
}


void w_colorful_chat::append_entry(const ColoredChatEntry& e)
{
    if (e.message.empty())
    {
        get_owning_dialog()->draw_dirty_widgets();
        return;
    }
    
    const font_t* shadow_font = get_font()->shadowed();
    
    string name;
    if (shadow_font->measure_styled_width(e.sender) > kNameWidth)
    {
        name = ""; // TODO: FIX: std::string(e.sender, 0, font->trunc_styled_text(e.sender, kNameWidth, style | styleShadow));
    }
    else
    {
        name = e.sender;
    }
    
    const font_t* message_font = get_font();
    int32_t available_width = rect.w - get_theme_space(LIST_WIDGET, L_SPACE) - get_theme_space(LIST_WIDGET, R_SPACE);
    if (e.type == ColoredChatEntry::ChatMessage)
    {
        available_width -= kNameWidth + taper_width() + 2;
    }
    else if (e.type == ColoredChatEntry::PrivateMessage)
    {
        message_font = shadow_font;
        available_width -= kNameWidth + taper_width() + 4;
    }
    else
    {
        message_font = shadow_font;
        available_width -= 2;
    }
    
    size_t usable_characters = 0; // TODO: FIX: font->trunc_styled_text(e.message, available_width, message_style);
    std::string::const_iterator middle;
    std::string::const_iterator rest;
    if (usable_characters != e.message.size())
    {
        size_t last_space = e.message.find_last_of(' ', usable_characters);
        if (last_space != 0 && last_space <= usable_characters)
        {
            middle = e.message.begin() + last_space;
            rest = middle + 1;
        }
        else
        {
            middle = e.message.begin() + usable_characters;
            rest = middle;
        }
    }
    else
    {
        middle = e.message.begin() + usable_characters;
        rest = middle;
    }
    
    ColoredChatEntry e_begin = e;
    e_begin.message = std::string(e.message.begin(), middle);
    e_begin.sender = name;
    
    ColoredChatEntry e_rest = e;
    if (rest != e.message.end())
    {
        e_rest.message = ""; // TODO: FIX: font->style_at(e.message, middle, message_style) + std::string(rest, e.message.end());
    }
    else
    {
        e_rest.message = std::string(rest, e.message.end());
    }
    e_rest.sender = name;
    
    bool save_top_item = top_item < count() - shown_items;
    size_t saved_top_item = top_item;
    entries.push_back(e_begin);
    
    new_items();
    if (save_top_item)
    {
        set_top_item((int32_t)saved_top_item);
    }
    else if ((int32_t)entries.size() > shown_items)
    {
        set_top_item((int32_t)entries.size() - shown_items);
    }
    append_entry(e_rest);
}


void w_colorful_chat::draw_item(std::vector<ColoredChatEntry>::const_iterator it, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected)
{
    const font_t* shadow_font = get_font()->shadowed();

    int32_t computed_y = y + get_font()->ascent;
    uint16_t message_x = x;
    uint16_t message_width = width;
    
    if (it->type == ColoredChatEntry::ChatMessage || it->type == ColoredChatEntry::PrivateMessage)
    {
        // draw the name
        SDL_Rect r = { x, y, kNameWidth, get_font()->line_height + 1};
        canvas->draw_filled_rect(r, it->color);
        
        // draw taper
        r.x += kNameWidth ;
        if (it->type == ColoredChatEntry::PrivateMessage)
        {
            r.w = taper_width() + 2;
            canvas->draw_filled_rect(r, {0x7f, 0x00, 0x00, 0xff}); // red bar under taper
        }
        
        r.w = 1;
        for (int32_t i = 0; i < taper_width(); ++i)
        {
            r.y++;
            r.h -= 2;
            canvas->draw_filled_rect(r, it->color);
            r.x++;
        }
        
        canvas->set_clip({x, 0, kNameWidth, canvas->h});
        canvas->draw_styled_text(it->sender, shadow_font, {0xff, 0xff, 0xff, 0xff}, {x + 1, computed_y});
        
        message_x += kNameWidth + taper_width() + 2;
        message_width -= kNameWidth + taper_width() + 2;
    }
    
    SDL_Color message_color = it->type == ColoredChatEntry::ChatMessage ? (SDL_Color){0xff, 0xff, 0xff, 0xff}
                                                                        : get_theme_color(CHAT_ENTRY, DEFAULT_STATE, FOREGROUND_COLOR);
    
    const font_t* message_font = get_font();
    if (it->type != ColoredChatEntry::ChatMessage) { message_font = shadow_font; }
    
    switch (it->type)
    {
        case ColoredChatEntry::ServerMessage:
            // draw the blue bar
            canvas->draw_filled_rect({message_x, y, message_width, get_font()->line_height + 1}, {0x00, 0x00, 0x7f, 0xff});
            message_x += 1;
            message_width -= 2;
            break;
            
        case ColoredChatEntry::PrivateMessage:
            // draw a red bar
            canvas->draw_filled_rect({message_x, y, message_width, get_font()->line_height + 1}, {0x7f, 0x00, 0x00, 0xff});
            message_x += 1;
            message_width -= 2;
            break;
            
        case ColoredChatEntry::LocalMessage:
            // draw a gray bar
            canvas->draw_filled_rect({message_x, y, message_width, get_font()->line_height + 1}, {0x3f, 0x3f, 0x3f, 0xff});
            message_x += 1;
            message_width -= 2;
            break;
        
        default:
        {}
    }
    
    canvas->set_clip({message_x, 0, message_width, canvas->h});
    canvas->draw_styled_text(it->message, message_font, message_color, {message_x, computed_y});
    canvas->clear_clip();
}

void SDLWidgetWidget::hide()
{
    hidden = true;
    m_widget->set_enabled (false);
}

void SDLWidgetWidget::show()
{
    hidden = false;
    m_widget->set_enabled (!inactive);
}

void SDLWidgetWidget::deactivate()
{
    inactive = true;
    m_widget->set_enabled (false);
}

void SDLWidgetWidget::activate()
{
    inactive = false;
    m_widget->set_enabled (!hidden);
}


PlayersInGameWidget::PlayersInGameWidget(w_players_in_game2* pig) : SDLWidgetWidget(pig), m_pig(pig) {}


void PlayersInGameWidget::redraw()
{
    m_pig->start_displaying_actual_information();
    m_pig->update_display();
    m_pig->get_owning_dialog()->draw_dirty_widgets();
}



