/*
 *  sdl_widgets.h - Widgets for SDL dialogs
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


// EES: TODO: slinging all of this in favor of ImGui is highly highly tempting, especially with Lua binding


// EES: UI being drawn in slow[er] SDL is absolutely fine: draw to SDL_Surface, the use SDL_Renderer or OGL to put it on screen


#ifndef SDL_WIDGETS_H
#define SDL_WIDGETS_H

#include "cseries.h"
#include "sdl_dialogs.h"
#include "FontRenderer_SDL.hpp"
#include "screen_drawing.h"

#include "map.h"         // for entry_point, for w_levels
#include "tags.h"        // for Typecode, for w_file_chooser
#include "FileHandler.h" // for FileSpecifier, for w_file_chooser


#include "metaserver_messages.h" // for GameListMessage, for w_games_in_room and MetaserverPlayerInfo, for w_players_in_room
#include "network.h" // for prospective_joiner_info

#include "binders.h"


typedef std::function<void ()> ControlHitCallback;
typedef std::function<void (char)> GotCharacterCallback; // TODO: `char` is dubious, probably want Unicode char


extern const std::string sFileChooserInvalidFileString; // also used in w_env_select // TODO: so, so many widget classes...


/*
 *  Widget base class
 */

class w_label;

class widget : public placeable
{
    friend class dialog;
    
public:
    enum alignment {
        kAlignNatural,
        kAlignLeft,
        kAlignCenter,
        kAlignRight
    };
    
    // ZZZ: initialize identifier, owning dialog, layout extensions
    widget();
    widget(int32_t theme_widget);
    virtual ~widget() {}
    
    // Draw widget
    virtual void draw(SDL_Surface* s) const = 0;
    
    // ZZZ: (dis)allow user interactions. assume widget alters its drawing behavior for disabled state.
    void set_enabled(bool inEnabled);
    
    // Handle event
    virtual void mouse_move(int32_t x, int32_t y) {}
    virtual void mouse_down(int32_t x, int32_t y) { click(x, y); }
    virtual void mouse_up(int32_t x, int32_t y) {}
    virtual void click(int32_t x, int32_t y) {}
    virtual void event(SDL_Event& e) {}
    
    // Widget selectable?  (ZZZ change to use enabled by default)
    virtual bool is_selectable() const { return enabled; } // was "true"
    
    // ZZZ: Get/set ID - see dialog::get_widget_by_id()
    int16_t get_identifier() const { return identifier; }
    void set_identifier(int16_t id_) { identifier = id_; }
    
    // ZZZ: Get dialog
    dialog* get_owning_dialog() { return owning_dialog; }
    
    // New positioning stuff
    virtual void set_rect(const SDL_Rect &r) { rect = r; }
    
    // implement placeable
    void place(const SDL_Rect &r, placement_flags flags);
    int32_t min_height() { return saved_min_height; }
    int32_t min_width() { return saved_min_width; }
    
    virtual bool is_dirty() { return dirty; }
    
    // labels are activated/enabled/disabled at the same time as this widget
    
    // creates and returns a new label
    w_label* adding_label(const std::string& text);
    
    void set_label(w_label* label_widget);
    
protected:
    // ZZZ: called by friend class dialog when we're added
    void set_owning_dialog(dialog* new_owner) { owning_dialog = new_owner; }
    
    SDL_Rect rect; // Position relative to dialog surface, and dimensions
    
    virtual void set_active(bool new_active);
    bool active;  // Flag: widget active (ZZZ note: this means it has the focus)
    bool dirty;   // Flag: widget needs redraw
    bool enabled; // ZZZ Flag: roughly, should the user be allowed to interact with the widget?
    
    
    
    // TODO: FFS! 1. font should only be on widgets that draw text, 2. get rid of style: the FontRenderer_SDL should represent 1 font at 1 size and style and probably color
    
    FontRenderer_SDL* font;
    uint16 style; // Widget font style
    
    
    
    
    int16_t identifier;    // ZZZ: numeric ID in support of dialog::find_widget_by_id()
    dialog* owning_dialog; // ZZZ: which dialog currently contains us?  for get_dialog()
    
    int32_t saved_min_width;
    int32_t saved_min_height;
    
    w_label* label_widget;
};


/*
 *  Vertical space
 */

class w_spacer : public placeable
{
public:
    w_spacer(uint16 space = get_theme_space(SPACER_WIDGET)) : m_space(space) {}
    
    int32_t min_height() { return m_space; }
    int32_t min_width() { return m_space; }
    
    void place(const SDL_Rect&, placement_flags) {}
    
private:
    uint16 m_space;
};


/*
 *  Static text
 */

class w_static_text : public widget
{
public:
    w_static_text(const std::string& text, int32_t theme_type = MESSAGE_WIDGET);
    
    void draw(SDL_Surface* s) const;
    
    void set_text(const std::string& t);
    
    bool is_selectable() const { return false; }
    
    ~w_static_text();
    
protected:
    std::string text;
    int32_t theme_type;
};


// TODO: what is the logic of wrapping the functional control widget inside the label widget instead of attaching the label widget inside the control widget? (it's possible that clicking on the label should cause the control to do something); not going to change it (Lua+ImGui would be so much preferable to all this, just curious
class w_label : public w_static_text
{
    friend class dialog;
    friend class w_slider;
    
public:
    w_label(const std::string& text) : w_static_text(text, LABEL_WIDGET), wrapped_widget(0), down(false) {}
    
    void wrap_widget(widget* w) { wrapped_widget = w; }
    void draw(SDL_Surface* s) const;
    
    void click(int32_t x, int32_t y);
    void mouse_down(int32_t x, int32_t y);
    void mouse_up(int32_t x, int32_t y);
    
    bool is_selectable() const { return wrapped_widget ? wrapped_widget->is_selectable() : false; }
    
private:
    widget* wrapped_widget;
    bool down;
};


class w_title : public w_static_text
{
public:
    w_title(const std::string& text) : w_static_text(text, TITLE_WIDGET) {}
};


class w_styled_text : public w_static_text
{
public:
    w_styled_text(const std::string& text, int32_t theme_type = MESSAGE_WIDGET);
    
    void set_text(const std::string& t);
    void draw(SDL_Surface* s) const;
private:
    std::string text_string;
};


class w_slider_text : public w_static_text
{
    friend class w_slider;
public:
    w_slider_text(const std::string text) : w_static_text(text) {}
    void draw(SDL_Surface* s) const;
protected:
    class w_slider *associated_slider;
};


/*
 *  Buttons
 */

typedef std::function<void (void*)> action_proc;

class w_button_base : public widget
{
public:
    w_button_base(const std::string& text, action_proc proc = nullptr, void* arg = nullptr, int32_t type = BUTTON_WIDGET);
    virtual ~w_button_base();
    
    void set_callback (action_proc proc, void* arg);
    
    void draw(SDL_Surface* s) const;
    
    void mouse_move(int32_t x, int32_t y);
    void mouse_down(int32_t x, int32_t y);
    void mouse_up(int32_t x, int32_t y);
    void click(int32_t x, int32_t y);
    
protected:
    const std::string text;
    action_proc proc;
    void* arg;
    
    bool down, pressed;
    
    int32_t type;
    // cache button centers since they are tiled or scaled
    SDL_Surface* button_c_default;
    SDL_Surface* button_c_active;
    SDL_Surface* button_c_disabled;
    SDL_Surface* button_c_pressed;
};




class w_button : public w_button_base
{
public:
    w_button(const std::string& text, action_proc proc = nullptr, void* arg = nullptr) : w_button_base(text, proc, arg, BUTTON_WIDGET) {}
};

class w_tiny_button : public w_button_base
{
public:
    w_tiny_button(const std::string& text, action_proc proc = nullptr, void* arg = nullptr) : w_button_base(text, proc, arg, TINY_BUTTON) {}
};



class w_hyperlink : public w_button_base
{
public:
    w_hyperlink(const std::string& url, const std::string& label = "");
    
    void draw(SDL_Surface* s) const;
    void prochandler(void* arg);
    
protected:
    const std::string url;
    
};


/*
 * Tabs
 */

class w_tab : public widget
{
public:
    w_tab(const strings_t& labels, tab_placer *placer);
    ~w_tab();
    void draw(SDL_Surface* s) const;
    
    void click(int32_t x, int32_t y);
    void event(SDL_Event& e);
    
    void choose_tab(int32_t i);
    
private:
    strings_t labels;
    std::vector<int32_t> widths;
    tab_placer* placer;
    
    std::vector<std::vector<SDL_Surface*>> images;
    
    int32_t pressed_tab;
    int32_t active_tab;
};


/*
 *  Selection button
 */

class w_select_button : public widget
{
public:
    w_select_button(const std::string& selection, action_proc proc = nullptr, void* arg = nullptr);
    
    void draw(SDL_Surface* s) const;
    
    void click(int32_t x, int32_t y);
    void mouse_down(int32_t x, int32_t y);
    void mouse_up(int32_t x, int32_t y);
    
    void set_selection(const std::string selection);
    void set_callback(action_proc p, void* a) { proc = p; arg = a; }
    
    void place(const SDL_Rect& r, placement_flags flags = placeable::kDefault);
    
protected:
    void set_arg(void* arg) { this->arg = arg; }
    placement_flags p_flags;
    
private:
    std::string selection;
    action_proc proc;
    void* arg;
    bool down;
    int16 selection_x; // X offset of selection display
};


/*
 *  Selection widget (base class)
 */

class w_select : public widget
{
public:
    // TODO:
    w_select(int32_t selection, const strings_t& labels) : widget(LABEL_WIDGET), selection_changed_callback(nullptr)
    {
        set_labels(labels);
        force_selection(selection);
        saved_min_height = font->get_line_height();
    }
    w_select(int32_t selection, const keyed_strings_t& labels) : widget(LABEL_WIDGET), selection_changed_callback(nullptr)
    {
        set_labels(labels);
        force_selection(selection);
        saved_min_height = font->get_line_height();
    }

    ~w_select() {}
    
    // ZZZ: set selection-changed callback - this will be called (if set) at the end of selection_changed (currently, anytime it "beeps")
    // removes need to subclass this class (or its subclasses!) just to add selection-changed behavior.
    typedef std::function<void (w_select*)> selection_changed_callback_t; // TODO: this should probably be more general and moved up hierarchy, but if eventual goal is to replace with Lua+ImGui then it's not worth bothering
    
    void set_selection_changed_callback(selection_changed_callback_t proc) { selection_changed_callback = proc; }
    
    int32_t count() const { return (int32_t)labels.size(); }
    
    int32_t get_selection() const { return selection; } // this returns the menu item index
    void set_selection(int32_t selection);
    
    string_id_t get_selected_string_id() const { return labels[selection].first; } // this returns the ID of the selected string, e.g.
    
    // New label strings should have same max width as old, or call set_full_width() to adjust menu width to fit.
    void set_labels(const keyed_strings_t& strings);
    void set_labels(const strings_t& strings);
    void load_labels(resource_id_t resource_id);
    
    void click(int32_t x, int32_t y);
    void event(SDL_Event &e);
    
    int32_t min_width();
    void place(const SDL_Rect& r, placement_flags flags = placeable::kDefault);
    void draw(SDL_Surface* s) const;
    
protected:
    void force_selection(int32_t selection);
    
    virtual void selection_changed();
    
    keyed_strings_t labels;
    
    int32_t selection; // UNONE means unknown selection // TODO: use -1 (NONE)
    
    // ZZZ: storage for callback function
    selection_changed_callback_t selection_changed_callback;
    
    // ZZZ: ripped this out for sharing
    uint16 get_largest_label_width();
};


/*
 *  On-off toggle
 */

class w_toggle : public w_select
{
public:
    static const strings_t default_onoff_labels;
    
    w_toggle(bool selection, const strings_t = default_onoff_labels); //caution: there should be exactly 2
    
    int32_t min_width();
    
    void draw(SDL_Surface *) const;
};


/*
 * Enabling toggle (ZZZ)
 *
 * Can enable/disable a bank of other widgets according to its state // TODO: FFS, just merge this feature into w_toggle
 */

class w_enabling_toggle : public w_toggle
{
public:
    w_enabling_toggle(bool inSelection, bool inEnablesWhenOn = true, const strings_t inLabels = default_onoff_labels)
        : w_toggle(inSelection, inLabels), enables_when_on(inEnablesWhenOn) {}
    
    void add_dependent_widget(widget* inWidget)
    {
        dependents.insert(inWidget);
        update_widget_enabled(inWidget);
    }
    void remove_dependent_widget(widget* inWidget) { dependents.erase(inWidget); }
    
protected:
    void selection_changed()
    {
        w_toggle::selection_changed();
        for (widget* it : dependents) { update_widget_enabled(it); }
    }
    
private:
    void update_widget_enabled(widget* inWidget) { inWidget->set_enabled(selection == enables_when_on); }
    
    std::set<widget*> dependents;
    bool enables_when_on; // if false, the other widgets are disabled when this widget is on
};



/*
 *  Text entry widget
 */

class w_text_entry : public widget
{
    friend class w_number_entry;
    friend class w_password_entry;
    
public:
    typedef std::function<void (w_text_entry*)> Callback;
    
    w_text_entry(size_t max_chars, const std::string& initial_text = "");
    ~w_text_entry() {}
    
    void event(SDL_Event &e);
    void click(int32_t x, int32_t y);
    
    void set_text(const std::string& text);
    const std::string get_text() { return text_buffer; }
    
    void set_min_width(int32_t w) { saved_min_width = w; }
    
    // ZZZ: set callback for "enter" or "return" keypress
    void set_enter_pressed_callback(Callback func) { enter_pressed_callback = func; }
    
    // ZZZ: set callback for value changed (will be called if value changed programmatically also)
    // (thought: this probably ought to be unified with w_select selection changed callback)
    void set_value_changed_callback(Callback func) { value_changed_callback = func; }
    
    void place(const SDL_Rect& r, placement_flags flags);
    
    void draw(SDL_Surface* s) const;
    
protected:
    std::string text_buffer; // Text entry buffer
    void set_active(bool new_active);
    
    Callback enter_pressed_callback;
    Callback value_changed_callback;
    
private:
    void modified_text();
    
    size_t num_chars;       // Length of text in buffer
    size_t max_chars;       // Maximum number of chars in buffer
    int16 text_x;           // X offset of text display
    uint16 max_text_width;  // Maximum width of text display
    size_t cursor_position; // cursor position within buffer
};


class w_password_entry : public w_text_entry
{
public:
    w_password_entry(size_t max_chars, const std::string& initial_text) : w_text_entry(max_chars, initial_text) {}

    void draw(SDL_Surface *s) const;

};


class w_chat_entry : public w_text_entry
{
public:
    w_chat_entry(size_t max_c) : w_text_entry(max_c, "")
    {
        font = get_theme_font(CHAT_ENTRY, style);
        saved_min_height = font->get_ascent() + font->get_descent() + font->get_leading();
    }
};


class w_number_entry : public w_text_entry
{
public:
    w_number_entry(int32_t initial_number = 0);
    
    void event(SDL_Event &e);
    
    void set_number(int32_t number) { set_text(std::to_string(number)); }

    int32_t get_number() { return std::stoi(text_buffer); }
};


/*
 *  Key name widget
 */

class w_key : public widget
{
public:
    
    enum Type {
        KeyboardKey,
        MouseButton,
        JoystickButton
    } event_type;
    
    static Type event_type_for_key(SDL_Scancode key);
    
    w_key(SDL_Scancode key, w_key::Type event_type);
    
    void draw(SDL_Surface* s) const;
    void click(int32_t x, int32_t y);
    void event(SDL_Event &e);
    
    virtual void set_key(SDL_Scancode key);
    SDL_Scancode get_key() { return key; }
    void place(const SDL_Rect& r, placement_flags flags);
    
protected:
    virtual void set_active(bool new_active);
    
private:
    const std::string name;
    
    int16 key_x;  // X offset of key name
    
    SDL_Scancode key;
    bool binding; // Flag: next key press will bind key
};


/*
 *  Progress Bar (ZZZ)
 */

class w_progress_bar : public widget
{
public:
    w_progress_bar(int32_t inWidth) : widget(), max_value(10), value(0)
    {
        rect.w = inWidth;
        rect.h = 14;
        
        saved_min_width = rect.w;
        saved_min_height = rect.h;
    }
    
    ~w_progress_bar() {}
    
    void draw(SDL_Surface* s) const;
    
    bool is_selectable() { return false; }
    
    void set_progress(int32_t inValue, int32_t inMaxValue);
    
protected:
    int32_t max_value;
    int32_t value;
};



/*
 *  Slider
 */

class w_slider;
typedef std::function<void (w_slider*)> slider_changed_callback_t;

class w_slider : public widget
{
    friend class w_slider_text;
    
public:
    w_slider(int32_t num_items, int32_t sel);
    ~w_slider();
    
    void draw(SDL_Surface* s) const;
    void mouse_move(int32_t x, int32_t y);
    void click(int32_t x, int32_t y);
    void event(SDL_Event &e);
    
    int32_t get_selection() { return selection; }
    void set_selection(int32_t selection);
    
    virtual void item_selected() {}
    virtual std::string formatted_value();
    
    void place(const SDL_Rect& r, placement_flags flags);
    
    void set_slider_changed_callback(slider_changed_callback_t proc) { slider_changed_callback = proc; }
    
protected:
    void init_formatted_value();
    
    w_slider_text *readout; // display current slider value
    int32_t readout_x;      // relative X offset where readout is drawn
    
    int16 slider_x;         // X offset of slider image
    
    int32_t selection;      // Currently selected item
    int32_t num_items;      // Total number of items
    
    bool thumb_dragging;    // Flag: currently dragging thumb
    int32_t thumb_x;        // X position of thumb
    int32_t trough_width;   // Width of trough
    
    int32_t thumb_drag_x;   // X start position when dragging
    
    SDL_Surface* slider_l, *slider_c, *slider_r, *thumb;
    
    slider_changed_callback_t slider_changed_callback;
    
private:
    int32_t thumb_width() const;
    void selection_changed();
};


class w_percentage_slider : public w_slider
{
public:
    w_percentage_slider(int32_t num_items, int32_t sel) : w_slider(num_items, sel)
    {
        init_formatted_value();
    }
    
    virtual std::string formatted_value();
};


class w_color_picker : public widget
{
public:
    w_color_picker(rgb_color &color) : widget(MESSAGE_WIDGET), m_color(color)
    {
        saved_min_width = 48;
        saved_min_height = font->get_line_height();
    }
    
    const rgb_color& get_selection() { return m_color; }
    
    void draw(SDL_Surface* s) const;
    void click(int32_t x, int32_t y);
    
private:
    rgb_color m_color;
    
    struct update_color
    {
        update_color(w_percentage_slider *red, w_percentage_slider *green, w_percentage_slider *blue, uint16 *i_red, uint16 *i_green, uint16 *i_blue) : red_w(red), green_w(green), blue_w(blue), red(i_red), blue(i_blue), green(i_green) { }
        
        void operator()(dialog *)
        {
            *red = red_w->get_selection()     << 12;
            *green = green_w->get_selection() << 12;
            *blue = blue_w->get_selection()   << 12;
        }
        
        w_percentage_slider *red_w;
        w_percentage_slider *green_w;
        w_percentage_slider *blue_w;
        
        uint16* red;
        uint16* blue;
        uint16* green;
    };
};


/*
 *  Template for list selection widgets
 */

class w_list_base : public widget
{
public:
    w_list_base(uint16_t width, int32_t lines);
    ~w_list_base();
    
    void draw(SDL_Surface* s) const;
    
    void mouse_move(int32_t x, int32_t y);
    void click(int32_t x, int32_t y);
    void event(SDL_Event &e);
    
    int32_t get_selection() { return selection; }
    
    virtual bool is_item_selectable(int32_t index) {return true;}
    virtual void item_selected() = 0;
    
    void place(const SDL_Rect& r, placement_flags flags);
    
    virtual int32_t count() const { return 0; } // TODO: =0 breaks shit
    
protected:
    virtual void draw_items(SDL_Surface* s) const = 0;
    void draw_image(SDL_Surface* dst, SDL_Surface* s, int16 x, int16 y) const;
    
    void set_selection(int32_t s);
    void new_items();
    void center_item(int32_t i);
    void set_top_item(int32_t i);
    
    virtual uint16 item_height() const { return font->get_line_height(); }
    
    static const int32_t kListScrollSpeed = 1;
    
    int32_t selection;         // Currently selected item
    
    //int32_t num_items;         // Total number of items
    int32_t shown_items;       // Number of shown items
    int32_t top_item;          // Number of first visible item
    
    bool thumb_dragging;       // Flag: currently dragging scroll bar thumb
    SDL_Rect trough_rect;      // Dimensions of trough
    uint16_t thumb_height;     // Height of thumb
    uint16_t min_thumb_height; // Minimal height of thumb
    int16_t thumb_y;           // Y position of thumb
    
    int32_t thumb_drag_y;      // Y start position when dragging
    
    SDL_Surface* frame_tl, *frame_t, *frame_tr, *frame_l, *frame_r, *frame_bl, *frame_b, *frame_br;
    SDL_Surface* thumb_t, *thumb_tc, *thumb_c, *thumb_bc, *thumb_b;
};


template <class T>
class w_list : public w_list_base
{
public:
    w_list(const std::vector<T>& it, uint16_t width, int32_t lines, int32_t sel) : w_list_base(width, lines), items(it)
    {
        new_items();
        set_selection(sel);
        center_item(selection);
    }
    
    ~w_list() {}
    
    uint16_t item_height() const { return font->get_line_height(); }
    
    int32_t count() const { return (int32_t)items.size(); }
    
protected:
    void draw_items(SDL_Surface* s) const
    {
        typename std::vector<T>::const_iterator it = items.begin() + top_item;
        int16_t x = rect.x + get_theme_space(LIST_WIDGET, L_SPACE);
        int16_t y = rect.y + get_theme_space(LIST_WIDGET, T_SPACE);
        uint16_t width = rect.w - get_theme_space(LIST_WIDGET, L_SPACE) - get_theme_space(LIST_WIDGET, R_SPACE);
        for (int32_t n = top_item; n < top_item + MIN(shown_items, items.size()); n++, it++, y=y+item_height())
        {
            draw_item(it, s, x, y, width, n == selection && active);
        }
    }
    
    const std::vector<T> &items; // List of items
        
private:
    virtual void draw_item(typename std::vector<T>::const_iterator it, SDL_Surface* s, int16_t x, int16_t y, uint16 width, bool selected) const = 0;
    
    w_list(const w_list<T>&);
    w_list<T>& operator =(const w_list<T>&);
};



/*
 *  Level number dialog
 */

class w_levels : public w_list<entry_point>
{
public:
    w_levels(const std::vector<entry_point>& items, dialog* d, uint16_t inWidth = 400,
             int32_t inNumLines = 8, int32_t inSelectedItem = 0, bool in_show_level_numbers = true)
        : w_list<entry_point>(items, inWidth, inNumLines, inSelectedItem), parent(d), show_level_numbers(in_show_level_numbers), offset{1} {}

    void item_selected() { parent->quit(0); }
    
    void draw_item(std::vector<entry_point>::const_iterator it, SDL_Surface* s, int16_t x, int16_t y, uint16_t width, bool selected) const;
    
    void set_offset(int32_t offset) { this->offset = offset; }
    
    int32_t count() const { return (int32_t)items.size(); }
    
private:
    dialog* parent;
    int32_t offset;
    bool    show_level_numbers;
};


/*
 *  String List
 */

class w_string_list : public w_list<std::string>
{
public:
    w_string_list(const strings_t &items, dialog* d, int32_t sel) : w_list<std::string>(items, 400, 8, sel), parent(d) {}

    void item_selected() { parent->quit(0); }
        
    void draw_item(strings_t::const_iterator it, SDL_Surface* s, int16 x, int16 y, uint16 width, bool selected) const;
    
    int32_t count() const { return (int32_t)items.size(); }
    
private:
    dialog *parent;
};


/*
 *  Selection Popup
 */

class w_select_popup : public w_select_button
{
public:
    w_select_popup(action_proc proc = nullptr, void* arg = nullptr);
    
    void set_labels(const strings_t& inLabels);
    
    void set_selection(int32_t value);
    int32_t get_selection() { return selection; }
    
    // Not to be confused with set_callback as inherited from w_select_button
    void set_popup_callback(action_proc p, void* a) {action = p; arg = a;}
    
private:
    int32_t selection;
    strings_t labels;
    
    action_proc action;
    void* arg;
    
    static void gotSelectedCallback(void* arg) { reinterpret_cast<w_select_popup*>(arg)->gotSelected(); }
    void gotSelected();
};


/*
 * General file-chooser (ZZZ)
 */

class w_file_chooser : public w_select_button
{
public:
    w_file_chooser(const std::string& inDialogPrompt, Typecode inTypecode)
        : dialog_prompt(inDialogPrompt), w_select_button(inDialogPrompt, std::bind(&w_file_chooser::proc, this), nullptr), typecode(inTypecode)
    {
        set_selection(sFileChooserInvalidFileString);
    }
    
    void proc();
    
    void set_file(const FileSpecifier& inFile)
    {
        file = inFile;
        update_filename();
    }
    const FileSpecifier& get_file() { return file; }
    
    // we also have void set_callback(action_proc, void*) as inherited from w_select_button
    void set_callback(ControlHitCallback callback) { m_callback = callback; }
    
private:
    void update_filename();
    
    FileSpecifier file;
    std::string   filename;
    std::string   dialog_prompt;
    Typecode      typecode;
    
    ControlHitCallback m_callback;
};


class w_directory_chooser : public w_select_button
{
public:
    w_directory_chooser() : w_select_button("", std::bind(&w_directory_chooser::proc, this), nullptr)
    {
        set_selection(sFileChooserInvalidFileString);
    }
    
    void proc();
    
    void set_directory(const FileSpecifier& inDirectory)
    {
        directory = inDirectory;
        update_directoryname();
    }
    const FileSpecifier& get_directory() { return directory; }
    
    // we also have void set_callback(action_proc, void*) as inherited from w_select_button
    void set_callback(ControlHitCallback callback) { m_callback = callback; }
    
private:
    void update_directoryname();
    
    FileSpecifier directory;
    std::string   directory_name;
    
    ControlHitCallback m_callback;
};


/*
 * Lists for metaserver dialog; moved from SdlMetaserverClientUi.cpp
 */

extern void set_drawing_clip_rectangle(int16_t top, int16_t left, int16_t bottom, int16_t right);


template <typename tElement>
class w_items_in_room : public w_list_base
{
public:
    typedef typename std::function<void (const tElement& item)> ItemClickedCallback;
    typedef typename std::vector<tElement> ElementVector;
    
    w_items_in_room(ItemClickedCallback itemClicked, int32_t width, int32_t numRows) : w_list_base(width, numRows), m_itemClicked(itemClicked)
    {
        new_items();
    }
    
    void set_collection(const std::vector<tElement>& elements)
    {
        m_items = elements;
        new_items();
        
        // do other crap - manage selection, force redraw, etc.
        get_owning_dialog ()->draw_dirty_widgets();
    }
    
    void set_item_clicked_callback(ItemClickedCallback itemClicked) { m_itemClicked = itemClicked; }
    
    void item_selected()
    {
        if (m_itemClicked) { m_itemClicked(m_items[selection]); }
    }
    
    uint16 item_height() const { return font->get_line_height(); }
    
    int32_t count() const { return (int32_t)m_items.size(); }
    
protected:
    void draw_items(SDL_Surface* s) const
    {
        typename ElementVector::const_iterator i = m_items.begin();
        int16 x = rect.x + get_theme_space(LIST_WIDGET, L_SPACE);
        int16 y = rect.y + get_theme_space(LIST_WIDGET, T_SPACE);
        uint16 width = rect.w - get_theme_space(LIST_WIDGET, L_SPACE) - get_theme_space(LIST_WIDGET, R_SPACE);
        
        for(size_t n = 0; n < top_item; n++) { ++i; }
        
        for (size_t n=top_item; n<top_item + MIN(shown_items, m_items.size()); n++, ++i, y=y+item_height())
        {
            draw_item(*i, s, x, y, width, n == selection && active);
        }
    }
    
private:
    ElementVector       m_items;
    ItemClickedCallback m_itemClicked;
    
    // This should be factored out into a "drawer" object/Strategy
    virtual void draw_item(const tElement& item, SDL_Surface* s, int16 x, int16 y, uint16 width, bool selected) const
    {
        y += font->get_ascent();
        set_drawing_clip_rectangle(0, x, static_cast<int16_t>(s->h), x + width);
    
        // TODO: FIX
       // draw_text(s, w_items_in_room(item).name(), x, y, (selected ? get_theme_color(ITEM_WIDGET, ACTIVE_STATE) : get_theme_color(ITEM_WIDGET, DEFAULT_STATE)), font, style);
        
        
        set_drawing_clip_rectangle(SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX);
    }
    
    w_items_in_room(const w_items_in_room<tElement>&);
    w_items_in_room<tElement>& operator =(const w_items_in_room<tElement>&);
};


typedef w_items_in_room<prospective_joiner_info> w_joining_players_in_room;

class w_games_in_room : public w_items_in_room<GameListMessage::GameListEntry>
{
public:
    w_games_in_room(w_items_in_room<GameListMessage::GameListEntry>::ItemClickedCallback itemClicked, int32_t width, int32_t numRows)
    : w_items_in_room<GameListMessage::GameListEntry>(itemClicked, width, numRows), kGameSpacing(get_theme_space(METASERVER_GAMES, GAME_SPACING))
    {
        font = get_theme_font(METASERVER_GAMES, style);
        saved_min_height = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
    }
    
    uint16 item_height() const { return 3 * font->get_line_height() + 2 + kGameSpacing; }
    
    void refresh()
    {
        dirty = true;
        get_owning_dialog()->draw_dirty_widgets();
    }
    
    enum {
        GAME_ENTRIES,
        GAME_SPACING,
    };
    
    enum {
        GAME,
        RUNNING_GAME,
        INCOMPATIBLE_GAME,
        SELECTED_GAME,
        SELECTED_RUNNING_GAME,
        SELECTED_INCOMPATIBLE_GAME,
    };
    
private:
    const int32_t kGameSpacing;
    void draw_item(const GameListMessage::GameListEntry& item, SDL_Surface* s, int16 x, int16 y, uint16 width, bool selected) const;
};

class w_players_in_room : public w_items_in_room<MetaserverPlayerInfo>
{
public:
    w_players_in_room(w_items_in_room<MetaserverPlayerInfo>::ItemClickedCallback itemClicked, int32_t width, int32_t numRows)
    : w_items_in_room<MetaserverPlayerInfo>(itemClicked, width, numRows)
    {
        font = get_theme_font(METASERVER_PLAYERS, style);
        saved_min_height = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
    }
    
protected:
    uint16 item_height() const { return font->get_line_height() + 4; }
private:
    static const int32_t kPlayerColorSwatchWidth = 8;
    static const int32_t kTeamColorSwatchWidth   = 4;
    static const int32_t kSwatchGutter           = 2;
    
    void draw_item(const MetaserverPlayerInfo& item, SDL_Surface* s, int16_t x, int16 y, uint16_t width, bool selected) const;
};


struct ColoredChatEntry
{
    enum Type {
        ChatMessage,
        PrivateMessage,
        ServerMessage,
        LocalMessage,
    } type;
    
    // these next two are only valid for chat and private otherwise they should be gray and ""
    rgb_color color;
    std::string sender;
    
    std::string message;
    
    ColoredChatEntry() : type(ChatMessage)
    {
        color.red = color.blue = color.green = 0x7fff;
    }
};


class w_colorful_chat : public w_list<ColoredChatEntry>
{
private:
    std::vector<ColoredChatEntry> entries;
    
public:
    w_colorful_chat(int32_t width, int32_t numRows)
    : w_list<ColoredChatEntry>(entries, width, numRows, 0), kNameWidth(get_theme_space(CHAT_ENTRY) - taper_width())
    {
        font = get_theme_font(CHAT_ENTRY, style);
        saved_min_height = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
    }
    
    virtual bool is_selectable() const { return true; }
    
    void item_selected() {}
    
    void append_entry(const ColoredChatEntry&);
    
    void clear()
    {
        entries.clear();
        new_items();
    }
    
    ~w_colorful_chat() {}
    
    uint16 item_height() const { return font->get_line_height() + 2; }
    
private:
    const int32_t kNameWidth;
    
    uint16 taper_width() const { return (font->get_line_height() + 1) / 2 - 1; }
    
    void draw_item(std::vector<ColoredChatEntry>::const_iterator i, SDL_Surface* s, int16 x, int16 y, uint16 width, bool selected) const;
};





/*
 * Wrappers to common widget interface follow // TODO: FUCKING WHY
 */

class SDLWidgetWidget
{
public:
    void hide();
    void show();
    
    void activate();
    void deactivate();
    
protected:
    SDLWidgetWidget(widget* in_widget) : m_widget(in_widget), hidden(false), inactive(false) {}
    
    widget* m_widget;
    
private:
    bool hidden, inactive;
};


class ColorfulChatWidgetImpl : public SDLWidgetWidget
{
public:
    ColorfulChatWidgetImpl(w_colorful_chat* w) : SDLWidgetWidget(w), m_chat(w) { }
    
    void Append(const ColoredChatEntry& e) { m_chat->append_entry(e); }
    void Clear () { m_chat->clear(); }
    
private:
    w_colorful_chat *m_chat;
};


class ToggleWidget : public SDLWidgetWidget, public Bindable<bool>
{
public:
    ToggleWidget(w_toggle* toggle) : SDLWidgetWidget(toggle), m_toggle(toggle), m_callback(nullptr)
    {
        m_toggle->set_selection_changed_callback(std::bind(&ToggleWidget::massage_callback, this, std::placeholders::_1));
    }
    
    void set_callback(ControlHitCallback callback) { m_callback = callback; }
    
    bool get_value() { return m_toggle->get_selection(); }
    void set_value(bool value) { m_toggle->set_selection(value); } // not sure
    
    bool bind_export() { return get_value(); }
    void bind_import(bool value) { set_value(value); }
    
private:
    void massage_callback (w_select* ignored)
    {
        if (m_callback) { m_callback (); }
    }
    
    w_toggle* m_toggle;
    ControlHitCallback m_callback;
};


class SelectorWidget : public SDLWidgetWidget, public Bindable<int32_t>
{
public:
    virtual void set_callback(ControlHitCallback callback) { m_callback = callback; }
    
    virtual void load_labels(resource_id_t string_resources_id)
    {
        //load_labels(get_strings_for_resource(string_resources_id));
    }
    virtual void set_labels (const strings_t& labels) = 0;
    
    virtual int32_t get_value() = 0;
    virtual void set_value(int32_t value) = 0;
    
    int32_t bind_export() { return get_value(); }
    void bind_import(int32_t value) { set_value(value); }
    
    virtual ~SelectorWidget() {}
    
protected:
    SelectorWidget(widget* in_widget) : SDLWidgetWidget (in_widget), m_callback (nullptr) {}
    
    ControlHitCallback m_callback;
};


class PopupSelectorWidget : public SelectorWidget
{
public:
    PopupSelectorWidget(w_select_popup* select_popup_w) : SelectorWidget(select_popup_w), m_select_popup(select_popup_w)
    {
        select_popup_w->set_popup_callback(std::bind(&PopupSelectorWidget::massage_callback, this, std::placeholders::_1), nullptr);
    }
    
    virtual void set_labels(const strings_t& labels) { m_select_popup->set_labels(labels); }
    
    virtual int32_t get_value() { return m_select_popup->get_selection(); }
    virtual void set_value(int32_t value) { m_select_popup->set_selection(value); }
    
private:
    w_select_popup* m_select_popup;
    
    void massage_callback (void* ignored) { if (m_callback) m_callback (); }
};


class SelectSelectorWidget : public SelectorWidget // welp that's a recursive name if ever there was
{
public:
    SelectSelectorWidget(w_select* select_w) : SelectorWidget(select_w), m_select(select_w)
    {
        m_select->set_selection_changed_callback(std::bind(&SelectSelectorWidget::massage_callback, this, std::placeholders::_1));
    }
    
    virtual void set_labels(const strings_t& labels) {}
    
    virtual int32_t get_value() { return (int32_t)m_select->get_selection(); }
    virtual void set_value(int32_t value) { m_select->set_selection(value); } // not sure
    
private:
    w_select* m_select;
    
    void massage_callback(w_select* ignored) // TODO: one wonders who is getting massage... or should it be "message"?
    {
        if (m_callback) { m_callback(); }
    }
};


class ColourSelectorWidget : public SelectSelectorWidget
{
public:
    ColourSelectorWidget(w_select* player_color_w) : SelectSelectorWidget(player_color_w) {}
    
    // We ignore the labels and use swatches of colour instead
    virtual void set_labels(int32_t stringset) {}
    virtual void set_labels(const strings_t& labels) {}
};


class SliderSelectorWidget : public SelectorWidget
{
public:
    SliderSelectorWidget(w_slider* slider_w) : SelectorWidget(slider_w), m_slider(slider_w) {}
    
    // Sliders don't get labels
    virtual void set_labels(const strings_t& labels) {};
    
    virtual int32_t get_value() { return m_slider->get_selection (); }
    virtual void set_value(int32_t value) { m_slider->set_selection (value); }
    
    // Sliders don't get callbacks either
    
private:
    w_slider* m_slider;
};


class ButtonWidget : public SDLWidgetWidget
{
public:
    ButtonWidget(w_button_base* button) : SDLWidgetWidget(button) , m_button(button), m_callback(nullptr)
    {
        m_button->set_callback (bounce_callback, this);
    }
    
    void set_callback(ControlHitCallback callback) { m_callback = callback; }
    
    void push() { if (m_callback) m_callback(); }
    
private:
    static void bounce_callback(void* arg)
    {
        reinterpret_cast<ButtonWidget*>(arg)->push();
    }
    
    w_button_base* m_button;
    ControlHitCallback m_callback;
};


class StaticTextWidget : public SDLWidgetWidget
{
public:
    StaticTextWidget(w_static_text* static_text_w) : SDLWidgetWidget(static_text_w), m_static_text(static_text_w) {}
    
    void set_text(const std::string& s) { m_static_text->set_text(s); }
    
private:
    w_static_text* m_static_text;
};


class EditTextWidget : public SDLWidgetWidget, public Bindable<std::string>
{
public:
    EditTextWidget(w_text_entry* text_entry) : SDLWidgetWidget(text_entry), m_text_entry(text_entry)
    {
        m_text_entry->set_enter_pressed_callback(std::bind(&EditTextWidget::got_submit, this, std::placeholders::_1));
    }
    
    void set_callback(GotCharacterCallback callback) { m_callback = callback; }
    
    void set_text(const std::string& s) { m_text_entry->set_text(s); }
    const std::string get_text() { return m_text_entry->get_text(); }
    
    std::string bind_export() { return get_text (); }
    void bind_import(std::string s) { set_text (s); }
    
private:
    w_text_entry* m_text_entry;
    GotCharacterCallback m_callback;
    
    // Yeah, I know.  Interface can be refined later.
    void got_submit(w_text_entry* ignored) { if (m_callback) { m_callback ('\r'); } }
};


class EditNumberWidget : public SDLWidgetWidget, public Bindable<int32_t>
{
public:
    EditNumberWidget(w_number_entry* number_entry) : SDLWidgetWidget(number_entry), m_number_entry(number_entry) {}
    
    void set_label(const std::string& s) {}
    
    void set_value(int32_t value) { m_number_entry->set_number(value); }
    int32_t get_value() { return m_number_entry->get_number(); }
    
    int32_t bind_export() { return get_value(); }
    void bind_import(int32_t value) { set_value(value); }
    
private:
    w_number_entry* m_number_entry;
};


class FileChooserWidget : public SDLWidgetWidget, public Bindable<FileSpecifier>
{
public:
    FileChooserWidget(w_file_chooser* file_chooser) : SDLWidgetWidget(file_chooser), m_file_chooser(file_chooser) {}
    
    void set_callback(ControlHitCallback callback) { m_file_chooser->set_callback(callback); }
    
    void set_file(const FileSpecifier& file) { m_file_chooser->set_file(file); }
    FileSpecifier get_file() { return m_file_chooser->get_file(); }
    
    virtual FileSpecifier bind_export() { return get_file(); }
    virtual void bind_import(FileSpecifier f) { set_file(f); }
    
private:
    w_file_chooser* m_file_chooser;
};


class GameListWidget
{
public:
    GameListWidget(w_games_in_room* games_in_room) : m_games_in_room(games_in_room)
    {
        m_games_in_room->set_item_clicked_callback(std::bind(&GameListWidget::bounce_callback, this, std::placeholders::_1));
    }
    
    void SetItems(const std::vector<GameListMessage::GameListEntry>& items) { m_games_in_room->set_collection(items); }
    
    void SetItemSelectedCallback(const std::function<void (GameListMessage::GameListEntry)> itemSelected)
    {
        m_callback = itemSelected;
    }
    
private:
    w_games_in_room* m_games_in_room;
    std::function<void (GameListMessage::GameListEntry)> m_callback;
    
    void bounce_callback(GameListMessage::GameListEntry thingy) { m_callback(thingy); }
};


class PlayerListWidget
{
public:
    PlayerListWidget(w_players_in_room* players_in_room) : m_players_in_room(players_in_room)
    {
        m_players_in_room->set_item_clicked_callback(std::bind(&PlayerListWidget::bounce_callback, this, std::placeholders::_1));
    }
    
    void SetItems(const std::vector<MetaserverPlayerInfo>& items) { m_players_in_room->set_collection(items); }
    void SetItemSelectedCallback(const std::function<void (MetaserverPlayerInfo)> itemSelected) { m_callback = itemSelected; }
    
private:
    w_players_in_room* m_players_in_room;
    std::function<void (MetaserverPlayerInfo)> m_callback;
    
    void bounce_callback(MetaserverPlayerInfo thingy) { m_callback(thingy); }
};


class JoiningPlayerListWidget
{
public:
    JoiningPlayerListWidget(w_joining_players_in_room* joining_players_in_room) : m_joining_players_in_room(joining_players_in_room)
    {
        m_joining_players_in_room->set_item_clicked_callback(std::bind(&JoiningPlayerListWidget::bounce_callback, this, std::placeholders::_1));
    }
    
    void SetItems(const std::vector<prospective_joiner_info>& items) { m_joining_players_in_room->set_collection(items); }
    
    void SetItemSelectedCallback(const std::function<void (prospective_joiner_info)> itemSelected) { m_callback = itemSelected; }
    
private:
    w_joining_players_in_room* m_joining_players_in_room;
    std::function<void (prospective_joiner_info)> m_callback;
    
    void bounce_callback(prospective_joiner_info thingy) { m_callback (thingy); }
};


class w_players_in_game2;

class PlayersInGameWidget : SDLWidgetWidget
{
public:
    PlayersInGameWidget(w_players_in_game2*);
    
    void redraw();
    
private:
    w_players_in_game2* m_pig;
};

// There are no colour pickers in sdl; we never try to actually construct one of these guys
class ColourPickerWidget : public Bindable<RGBColor> {};


#endif
