/*
 terminal_renderer.hpp
 
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

#include "terminal_renderer.hpp"

#include "overhead_map.h" // overhead_map_data type, _rendering_checkpoint_map enum
#include "interface.h" // strErrors and pictureNotFound+checkpointNotFound enums are defined here but should be down in CSeries; terminal_canvas->set_clip (used to clip checkpoint map drawing) is also declared here (bizarre) but implemented in screen_drawing.cpp (sensible)
#include "screen.h"
#include "screen_drawing.h" // screen_rectangle
#include "Canvas.hpp"
#include "shapes.h" // get_shape_surface (for M1 terminal logo)
#include "images.h" // pict resources
#include "fonts.hpp" // Font
#include "sdl_resize.h"


// -----------------------------------------------------------------------------------------
// nasty externs


static Canvas* terminal_canvas;



int32_t get_pict_header_width(LoadedResource &); // implemented in images.cpp but not decladed in images.h; only used in display_picture()


extern SDL_PixelFormat pixel_format_32; // randomize_pixel uses its Amask; unclear why


// -----------------------------------------------------------------------------------------
// calculated screen rects for the computer terminal display and each of its visual elements

static SDL_Rect terminal_screen_rect;
static SDL_Rect terminal_logon_graphic_rect;
static SDL_Rect terminal_logon_title_rect;
static SDL_Rect terminal_logon_location_rect;
static SDL_Rect terminal_header_rect;
static SDL_Rect terminal_footer_rect;
static SDL_Rect terminal_full_text_rect;
static SDL_Rect terminal_left_rect;
static SDL_Rect terminal_right_rect;

static int32_t screen_width = 0, screen_height = 0; // ideally screen.cpp would notify us of changes
static float pixel_scale = 0.0; // a 3860x2160 HD display reports as 1920x1080, which is annoying as all we care about is physical pixel dimensions

static double screen_scale = 0.0;

// Blitter = SDL
// Blitter_OGL = subclass(!)
// both are awful but if we can use them for drawing then do so; otherwise refactor and rename ImageRenderer, ImageRenderer_SDL, ImageRenderer_OGL
// see also: Term_Blitter; however, we should generalize terminal drawing so that screen.cpp calls render_computer_terminal() here, as it's more efficient for us to keep separate IR instances for per-group text block + pictures, and pre-rendered logon/logoff screens

// pushing terminal drawing out to Lua is a job for another time; ditto dynamic static effects


inline void scale_rect(SDL_Rect& r, double scale)
{
    r.x = (int32_t)(r.x * scale);
    r.y = (int32_t)(r.y * scale);
    r.w = (int32_t)(r.w * scale);
    r.h = (int32_t)(r.h * scale);
}

inline SDL_Rect get_screen_rect(int32_t rect_id)
{
    SDL_Rect sr = get_term_rect(_terminal_screen_rect);
    SDL_Rect r = get_term_rect(rect_id);
    r.x -= sr.x;
    r.y -= sr.y;
    scale_rect(r, screen_scale);
    r.x += terminal_screen_rect.x;
    r.y += terminal_screen_rect.y;
    return r;
}


// ideally screen.cpp would initialize components when it is ready, then subsequently notify components whenever screen size, color depth, etc changes, so that each component looks after itself and its own state; ditto level change notifications and `render_to_screen` calls


void initialize_terminal_renderer()
{
    alephone::Screen* screen = alephone::Screen::instance();
    MainScreenSurfaceSize(&screen_width, &screen_height);
    pixel_scale = MainScreenPixelScale();
    
    // we need to convert from original M2 rects (which assume 640x480 display) to screen rects
    double scale = screen_height / 480.0 * pixel_scale; // screen is 4x3 or wider aspect, so we treat the screen's true height as equivalent to old-school 480px, and convert old M2 rects from MML config into real screen coordinates
    SDL_Rect dst_rect = screen->term_rect(); // this is the available drawing area on screen
    printf("Terminal: dst_rect = {%i, %i, %i, %i} delta-scale=%f\n", dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h, scale);
    
    terminal_screen_rect = get_term_rect(_terminal_screen_rect); // M2 default was 640x320
    scale_rect(terminal_screen_rect, scale);
    terminal_screen_rect.x += (screen_width - (int32_t)(640.0 * scale)) / 2; // if screen is wider than 4x3, this is left+right margins
    
    // TODO: within the available rect, additional left+right or top+bottom margins may be needed
    
    printf("Terminal: terminal_screen_rect = {%i, %i, %i, %i}\n", terminal_screen_rect.x, terminal_screen_rect.y, terminal_screen_rect.w, terminal_screen_rect.h);
    
    //
    terminal_logon_graphic_rect  = get_screen_rect(_terminal_logon_graphic_rect);
    terminal_logon_title_rect    = get_screen_rect(_terminal_logon_title_rect);
    terminal_logon_location_rect = get_screen_rect(_terminal_logon_location_rect);
    terminal_header_rect         = get_screen_rect(_terminal_header_rect);
    terminal_footer_rect         = get_screen_rect(_terminal_footer_rect);
    
    // positioning; we also need their widths to allocate Surfaces for drawing per-group text and their heights to ensure correct scrolling/paging
    terminal_full_text_rect      = get_screen_rect(_terminal_full_text_rect);
    terminal_left_rect           = get_screen_rect(_terminal_left_rect);
    terminal_right_rect          = get_screen_rect(_terminal_right_rect);

    printf("Terminal: terminal_right_rect = {%i, %i, %i, %i}\n", terminal_right_rect.x, terminal_right_rect.y, terminal_right_rect.w, terminal_right_rect.h);

}


bool has_screen_size_changed()
{
    alephone::Screen* screen = alephone::Screen::instance();
    
    int w, h;
    MainScreenSurfaceSize(&w, &h);
    
    return (w != screen_width || h != screen_height || MainScreenPixelScale() != pixel_scale);
}


// -----------------------------------------------------------------------------------------
// current font style


static SDL_Color current_color; // Current color pixel value
static font_style_t current_style = ::normal; // bitflags


static void set_current_style(TerminalText* text_face)
{
    current_style = text_face->style;
    current_color = get_interface_color(text_face->color_id + _computer_interface_text_color); 
}


// -----------------------------------------------------------------------------------------
// fill


static void fill_terminal_with_black() // TODO: bounds? or no bounds?
{
   // SDL_Rect frame = get_term_rect(_terminal_screen_rect); // TODO: this shouldn't be necessary as the Surface should be pre-sized to the dimensions at which the terminal displays on screen (typically 4:3, sized to fit in free screen space away from HUD, which is presumably what _terminal_screen_rect specifies on assumption it's drawing to a 640x480 screen)
    
    terminal_canvas->draw_filled_rect({0, 0, terminal_canvas->w, terminal_canvas->h}, {0x00, 0x00, 0x00, 0xff});
}


template <typename T>
static inline T randomize_pixel(uint16_t pixel)
{
    return static_cast<T>(pixel);
}

template <>
inline uint32_t randomize_pixel(uint16_t pixel)
{
    return ((uint32_t)pixel ^ (((uint32_t)pixel) << 8)) | pixel_format_32.Amask;
}


template <typename T>
static inline void randomize_line(T* start, uint32_t count)
{
    static uint16_t random_seed = 6906;
    for (int32_t i = 0; i < count; ++i)
    {
        *start++ = randomize_pixel<T>(random_seed);
        if (random_seed & 1) random_seed = (random_seed >> 1) ^ 0xb400;
        else random_seed = random_seed >> 1;
    }
}

static void fill_terminal_with_static() // TODO: this probably wants to look blocky; would be useful to see where it was used originally
{
    TODO("implement static effect fill");
    /*
    //SDL_Rect bounds = get_term_rect(_terminal_screen_rect);
    
    for (int32_t y = 0; y < terminal_canvas->h; y++)
    {
        int32_t bpp = target_surface->format->BytesPerPixel;
        uint8_t* p = (uint8_t*)target_surface->pixels + y * target_surface->pitch * bpp;
        switch (bpp)
        {
            case 1:
                randomize_line<uint8_t>(p, terminal_canvas->w);
                break;
            case 2:
                randomize_line<uint16_t>(reinterpret_cast<uint16_t*>(p), terminal_canvas->w);
                break;
            case 4:
                randomize_line<uint32_t>(reinterpret_cast<uint32_t*>(p), terminal_canvas->w);
                break;
        }
    }
     */
}


// -----------------------------------------------------------------------------------------
// draw line


static void draw_line_of_text(char* base_text, int16_t start_index, int16_t end_index,
                              screen_rectangle* bounds, ComputerTerminal* terminal_text, int16_t* text_face_start_index, int16_t line_number)
{
    TODO("redo this once Render2D/ is done");
    //printf("draw_line_of_text: %i..%i '%s'\n", start_index, end_index, base_text+start_index);

    /*
    int16_t line_height = _get_font_line_height(_computer_interface_font);
    
    uint32_t text_index = *text_face_start_index == NONE ? 0 : *text_face_start_index;
    
    // Get to the first one that concerns us.
    TerminalText* face_data = NULL;
    if (text_index < terminal_text->texts.size())
    {
        do {
            face_data = terminal_text->get_indexed_font_changes(text_index);
            if (!face_data) return;
            if (face_data->start_index < start_index) text_index++;
        } while (face_data->start_index < start_index && text_index < terminal_text->texts.size());
    }
    
    int16_t current_start = start_index, current_end = end_index;
    Font* terminal_font = get_interface_font(_computer_interface_font);
    int32_t xpos = bounds->left;

    bool done = false;
    while (!done)
    {
        if (text_index < terminal_text->texts.size())
        {
            face_data = terminal_text->get_indexed_font_changes(text_index);
            if (!face_data) return;

            if (face_data->start_index >= current_start && face_data->start_index < current_end)
            {
                current_end = face_data->start_index;
                text_index++;
                *text_face_start_index = text_index;
            }
        }

        
        xpos += draw_text(base_text + current_start, current_end - current_start,
                          xpos, bounds->top + line_height * (line_number + FUDGE_FACTOR),
                          current_pixel, terminal_font, current_style);
        if (current_end != end_index)
        {
            current_start = current_end;
            current_end = end_index;
            assert_fail(face_data, "");
            set_current_style(face_data);
        }
        else
        {
            done = true;
        }
    }
     */
}


// -----------------------------------------------------------------------------------------
// draw text


static void draw_computer_text(TerminalPage* current_page, int16_t current_line, const SDL_Rect& bounds)
{
    TODO("redo this once Render2D/ is done");
    /*
    bool done = false;
    if (!current_page) return;
    
    uint16_t old_style = current_style; // urgh; freaking globals everywhere
    current_style = GetInterfaceStyle(_computer_interface_font);

    int16_t start_index = current_page->mr_start_index;
    int16_t end_index = current_page->mr_length + current_page->mr_start_index;
    
    
    // eat the previous lines // yeesh
    for (int16_t i = 0; i < current_line; ++i)
    {
        // Calculate one line.
        if (!calculate_line_end_index(base_text, current_style, RECTANGLE_WIDTH(bounds), start_index, current_page->start_index + current_page->length, &end_index))
        {
            if (end_index > current_page->start_index + current_page->length)
            {
                // ao__dprintf__("Start: %d Length: %d End: %d;g", current_page->start_index, current_page->length, end_index);
                // ao__dprintf__("Width: %d", RECTANGLE_WIDTH(bounds));
                end_index = current_page->start_index + current_page->length;
            }
            //ao__dprintf__("calculate line: %d start: %d end: %d", index, start_index, end_index);
            assert_fail(end_index <= current_page->start_index + current_page->length, "");
            
            start_index = end_index;
        }
        else // End of text.
        {
            done = true;
        }
    }

    if (!done)
    {
        // Go backwards, and see if there were any other face changes...
        int16_t last_index = current_page->start_index;
        int16_t last_text_index = NONE;
        for (uint32_t text_index = 0; text_index < terminal_text->texts.size(); ++text_index)
        {
            TerminalText* font_face = terminal_text->get_indexed_font_changes(text_index);
            if (!font_face) return;
            
            // Go backwards from the scrolled starting location.
            if (font_face->start_index>last_index && font_face->start_index<start_index)
            {
                // ao__dprintf__("ff index: %d last: %d end: %d", font_face->index, last_index, start_index);
                last_index = font_face->start_index;
                last_text_index = text_index;
            }
        }
        
        // ao__dprintf__("last index: %d", last_text_index);
        
        TerminalText text_face;
        if (last_text_index == NONE) // Default-> plain, etc.
        {
            text_face.color_id = 0;
            text_face.style    = 0;
        }
        else // Figure out the font->
        {
            TerminalText* font_face = terminal_text->get_indexed_font_changes(last_text_index);
            if (!font_face) return;
            text_face = *font_face;
        }
    
        set_current_style(&text_face);
    
        // Draw what is one the screen
        for (int16_t i = 0; !done && i < terminal_text->lines_per_page; ++i)
        {
            //ao__dprintf__("calculating the line");
            if (!calculate_line_end_index(base_text, current_style, RECTANGLE_WIDTH(bounds), start_index, current_page->start_index + current_page->length, &end_index))
            {
                //ao__dprintf__("draw calculate line: %d start: %d end: %d text: %x length: %d lti: %d", index, start_index, end_index, base_text, current_page->length, last_text_index);
                if (end_index>current_page->start_index + current_page->length)
                {
                    // ao__dprintf__("Start: %d Length: %d End: %d;g", current_page->start_index, current_page->length, end_index);
                    // ao__dprintf__("Width: %d", RECTANGLE_WIDTH(bounds));
                    end_index = current_page->start_index + current_page->length;
                }
                assert_fail(end_index <= current_page->start_index + current_page->length, "");
                draw_line_of_text(base_text, start_index, end_index, bounds, terminal_text, &last_text_index, i);
                start_index = end_index;
            }
            else // End of text.
            {
                done = true;
            }
        }
    }

    current_style = old_style;
     
     */
}


// -----------------------------------------------------------------------------------------
// draw image


static SDL_Rect draw_terminal_picture(TerminalPage* current_page)
{
    SDL_Surface* picture_surface = get_pict_resource_from_map(current_page->permutation); // TODO: best consolidate under get_pict_rsrc, with a search_order arg (although I'm fairly sure pict ID ranges are unique across the entire scenario, so splash/main/chapter/terminal pict IDs should never conflict; if so, use a single search order that is most convenient)
    if (picture_surface)
    {
        SDL_Rect bounds = {0, 0, picture_surface->w, picture_surface->h};

        bool cinemascopeHack = (double)picture_surface->w / (double)picture_surface->h > 1.5; // TODO: simplified logic to just check the aspect ratio; confirm this behaves as before
        
        OffsetRect(bounds, -bounds.x, -bounds.y);

        SDL_Rect screen_bounds = current_page->calculate_bounds_for_object_box(&bounds);

        if (bounds.w <= screen_bounds.w && bounds.h <= screen_bounds.h) // It fits. Center it.
        {
            OffsetRect(bounds, screen_bounds.x + (screen_bounds.w - bounds.w) / 2,
                               screen_bounds.y + (screen_bounds.h - bounds.h) / 2);
        }
        else // Doesn't fit.  Make it, but preserve the aspect ratio.
        {
            if (bounds.h - screen_bounds.h >= bounds.w -screen_bounds.w)
            {
                int16_t adjusted_width = screen_bounds.h * bounds.w / bounds.h;
                bounds = screen_bounds;
                InsetRect(bounds, (screen_bounds.w - adjusted_width) / 2, 0);
            }
            else // Width is the predominant factor
            {
                int16_t adjusted_height = screen_bounds.w * bounds.h / bounds.w;
                bounds = screen_bounds;
                InsetRect(bounds, 0, (screen_bounds.h - adjusted_height) / 2);
            }
        }
        
        if ((picture_surface->w == bounds.w && picture_surface->h == bounds.h) || cinemascopeHack)
        {
            terminal_canvas->draw_surface(picture_surface, bounds);
        }
        else // Rescale picture
        {
            SDL_Surface* s2 = SDL_Resize(picture_surface, bounds.w, bounds.h, false);
            terminal_canvas->draw_surface(s2, bounds);
            SDL_FreeSurface(s2);
        }
        // And let the caller know where we drew the picture
        return bounds;
    }
    else // pict resource not found, so draw "missing image" message
    {
        SDL_Rect bounds = current_page->calculate_bounds_for_object_box(nullptr);
        terminal_canvas->draw_filled_rect(bounds, {0x00, 0x00, 0x00, 0xff});
        
        const std::string message = get_string(STRID(strERRORS, pictureNotFound), {
            {"$objectID$", [current_page]{ return std::to_string(current_page->permutation); }},
        });

        const font_t* font = get_interface_font(_computer_interface_title_font);
        int32_t width = font->measure_width(message);
        SDL_Rect r = {bounds.x + (bounds.w - width) / 2, bounds.y  + bounds.h / 2};
        terminal_canvas->draw_text(message, font, {0xff, 0xff, 0xff, 0xff}, r);
        return {0, 0, 0, 0};
    }
}


static void display_picture_with_text(TerminalPage* current_page, ComputerTerminal* terminal_text, int16_t current_line)
{
    assert_fail(current_page->type == _pict_page, "");
    
    draw_terminal_picture(current_page);

    SDL_Rect text_bounds = current_page->calculate_bounds_for_text_box();
    draw_computer_text(current_page, current_line, text_bounds);
}


// -----------------------------------------------------------------------------------------
// logon icon


#define M1_LOGON_SHAPE (44)


static SDL_Rect draw_m1_logon_shape(TerminalPage* current_page) // TODO: this is used to draw M1 logon icon but it seems pretty generic
{
    // should we use this or current "center logo in screen" implementation?
    //SDL_Rect frame = get_term_rect(_terminal_logon_graphic_rect);
    
    // TODO: scaling
    SDL_Surface* surface = get_shape_surface(M1_LOGON_SHAPE);
    if (!surface) return {0, 0, 0, 0};
    
    SDL_Rect bounds = {0, 0, surface->w, surface->h};
        
    SDL_Rect screen_bounds = current_page->calculate_bounds_for_object_box(_draw_object_on_center, &bounds);
    
    OffsetRect(bounds, screen_bounds.x + (screen_bounds.w - bounds.w) / 2,
                       screen_bounds.y + (screen_bounds.h - bounds.h) / 2);

    SDL_SetSurfaceAlphaMod(surface, 255);
    terminal_canvas->draw_surface(surface, bounds);
    
    SDL_FreeSurface(surface);
    return bounds;
}



static void draw_connection_screen(TerminalPage* current_page)
{
    // TODO: da math aint mathin
    SDL_Rect picture_bounds = get_term_rect(_terminal_logon_graphic_rect);
    if (!current_page) return;
    
    SDL_Rect text_bounds;
    if (current_page->flags & _terminal_is_m1)
    {
        // M1 logon/logoff screen is laid out like this:
        //
        //                 ---
        //               / ( ) \
        //               \  |  /
        //                 ---
        //          config-defined line
        //           term-defined line
        //
        draw_m1_logon_shape(current_page);
        
        // originally 2 separate rects, but we combine into 1
        SDL_Rect title_line_bounds = get_term_rect(_terminal_logon_title_rect);
        SDL_Rect location_line_bounds = get_term_rect(_terminal_logon_location_rect);
        
        text_bounds.x = std::min(title_line_bounds.x, location_line_bounds.x);
        text_bounds.y = title_line_bounds.y + title_line_bounds.h;
        text_bounds.w = std::max(title_line_bounds.w, location_line_bounds.w);
        text_bounds.h = title_line_bounds.h;
    }
    else
    {
        // the design of M2 logon/logoff screens is terminal-specific logo plus terminal-specific text
        SDL_Rect bounds = picture_bounds;
        picture_bounds = draw_terminal_picture(current_page);
        
        // Use the picture bounds to create the logon text crap
        text_bounds.x = bounds.x;
        text_bounds.y = picture_bounds.y + picture_bounds.h;
        text_bounds.w = bounds.w; // TODO: sus
        text_bounds.h = bounds.h; // lazy
    }
    
    // This is always just a line, so we can do this here // not any more: it's 2 lines for M1
    const font_t* terminal_font = get_interface_font(_computer_interface_font);
    
    // center string on screen // TODO: this will move into draw_ function
    
    std::string base_text = current_page->texts.at(0).utf8_string; // TODO: there may be more than one style and/or line so need to math it
    
   int16_t width = terminal_font->measure_width(base_text); // TODO: FIX: find the widest line
    
    text_bounds.x += (picture_bounds.w - width) / 2;
    
    draw_computer_text(current_page, 0, text_bounds);
}


// -----------------------------------------------------------------------------------------
// M1-style checkpoint map


static bool find_checkpoint_location(int16_t checkpoint_index, world_point2d* location, int16_t* polygon_index)
{
    bool success = false;
    map_object* saved_object = saved_objects;
    int16_t match_count = 0;
    
    location->x = location->y = 0;
    for (int16_t i = 0; i < dynamic_world->initial_objects_count; i++, saved_object++)
    {
        if (saved_object->type == _saved_goal && saved_object->index == checkpoint_index)
        {
            location->x += saved_object->location.x;
            location->y += saved_object->location.y;
//            *polygon_index = saved_object->polygon_index;
            match_count++;
        }
    }
    
    if (match_count)
    {
        // Now average
        location->x /= match_count;
        location->y /= match_count;
        *polygon_index = world_point_to_polygon_index(location);
        success = (*polygon_index != NONE);
    }

    return success;
}


static void present_checkpoint_text(ComputerTerminal* terminal_text, TerminalPage* current_page, int16_t current_line)
{
    // draw the overhead map.
    SDL_Rect bounds = current_page->calculate_bounds_for_object_box(NULL);
    
    overhead_map_data overhead_data;
    if (find_checkpoint_location(current_page->permutation, &overhead_data.origin, &overhead_data.origin_polygon_index))
    {
        overhead_data.scale       =  1;
        overhead_data.top         = bounds.y;
        overhead_data.left        = bounds.x;
        overhead_data.half_width  = bounds.w / 2;
        overhead_data.half_height = bounds.h / 2;
        overhead_data.width       = bounds.w;
        overhead_data.height      = bounds.h;
        overhead_data.mode        = _rendering_checkpoint_map;
        
        //
        terminal_canvas->set_clip(bounds);
        _render_overhead_map(&overhead_data);
        terminal_canvas->clear_clip();
    }
    else // draw "checkpoint not found" error message
    {
        terminal_canvas->draw_filled_rect(bounds, {0x00, 0x00, 0x00, 0xff});
        
        const std::string message = get_string(STRID(strERRORS, checkpointNotFound), {
            {"$objectID$", [current_page]{ return std::to_string(current_page->permutation); }},
        });
        
        const font_t* font = get_interface_font(_computer_interface_title_font);
        int32_t width = font->measure_width(message);
        SDL_Rect r = {bounds.x + (bounds.w - width) / 2, bounds.y + bounds.h / 2, width, terminal_canvas->h};
        terminal_canvas->draw_text(message, font, {0xff, 0xff, 0xff, 0xff}, {});
    }
    
    // draw the text
    bounds = current_page->calculate_bounds_for_text_box();
    draw_computer_text(current_page, current_line, bounds);
}


// -----------------------------------------------------------------------------------------
// top + bottom bars


// TODO: for l10n, might want to use Lua to draw borders
static void draw_terminal_borders(PlayerTerminalState* terminal_state)
{
    ComputerTerminal* terminal_text = get_terminal_for_id(terminal_state->terminal_id);
    if (!terminal_text) return;
    
    TerminalPage* current_page = terminal_text->get_page_at_index(terminal_state->page_id);
    if (!current_page) return;
     
    int16_t top_message, bottom_left_message, bottom_right_message;
    switch (current_page->type)
    {
        case _logon_page:
            top_message          = _computer_starting_up;
            bottom_left_message  = _computer_manufacturer;
            bottom_right_message = _computer_address;
            break;
            
        case _logoff_page:
            top_message          = _disconnecting_message;
            bottom_left_message  = _computer_manufacturer;
            bottom_right_message = _computer_address;
            break;

        default:
            top_message          = _computer_terminal;
            bottom_left_message  = _scrolling_message;
            bottom_right_message = _acknowledgement_message;
            break;
    }
    /*
    // Draw the top rectangle
    screen_rectangle border = get_term_rectangle(_terminal_header_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);

    // Draw the top login header text
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    draw_text_to_surface(get_string(STRID(strCOMPUTER_TERMINAL_LABELS, top_message)),
                         border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    
    draw_text_to_surface(get_date_string(current_page->flags & _terminal_is_m1),
                         border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);

    // Draw the the bottom rectangle & text
    border = get_term_rectangle(_terminal_footer_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    
    draw_text_to_surface(get_string(STRID(strCOMPUTER_TERMINAL_LABELS, bottom_left_message)),
                         border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    
    draw_text_to_surface(get_string(STRID(strCOMPUTER_TERMINAL_LABELS, bottom_right_message)),
                         border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);
     */
}


// -----------------------------------------------------------------------------------------
// draw terminal to SDL_Surface


bool draw_computer_terminal()
{
    if (has_screen_size_changed())
    {
        initialize_terminal_renderer();
    }
    
    bool needs_rendered_to_screen = false;
    
    PlayerTerminalState* terminal_state = get_terminal_state_for_player(current_player_index);
    if (terminal_state->is_active != true) return false;
    
    if (terminal_state->needs_redraw)
    {
        terminal_state->needs_redraw = false;
        
        ComputerTerminal* terminal_text = get_terminal_for_id(terminal_state->terminal_id);
        if (!terminal_text) return false;
        
        if (terminal_state->is_active)
        {
            TerminalPage* current_page = terminal_text->get_page_at_index(terminal_state->page_id);
            if (!current_page) return false;
            
            fill_terminal_with_black();

            switch (current_page->type)
            {
                case _logon_page: // permutation = logon logo
                case _logoff_page:
                {
                    draw_connection_screen(current_page);
                    break;
                }
                case _unfinished_page:
                case _success_page:
                case _failure_page:
                    // ao__dprintf__("You shouldn't try to render this view.;g");
                    break;
                    
                case _information_page: // Draw as normal
                {
                    SDL_Rect bounds = get_term_rect(_terminal_full_text_rect);
                    draw_computer_text(current_page, terminal_state->line_number, bounds);
                    break;
                }
                case _checkpoint_page: // permutation = the goal to show
                    // note that checkpoints can only be equal to one screenful
                    present_checkpoint_text(terminal_text, current_page, terminal_state->line_number);
                    break;
                    
                case _end_page:
                case _interlevel_teleport_page: // permutation = level to go to
                case _intralevel_teleport_page: // permutation = polygon to go to.
                case _sound_page:               // permutation = sound id to play
                case _tag_page:
                    break; // These are all handled elsewhere
                    
                case _movie_page:
                case _track_page:
                    if (!game_is_networked)
                    {
                        // ao__dprintf__("Movies/Music Tracks not supported on playback (yet);g");
                    } else {
                        // ao__dprintf__("On networked games, should we display a PICT here?;g");
                    }
                    break;
                    
                case _pict_page:
                    display_picture_with_text(current_page, terminal_text, terminal_state->line_number);
                    break;
                    
                case _static_page:
                {
                    fill_terminal_with_static();
                    terminal_state->needs_redraw = true;
                    break;
                }
                case _camera_page:
                    break;
                    
                default:
                    break;
            }
            draw_terminal_borders(terminal_state); // borders will overdraw any overlapping content
        }
        needs_rendered_to_screen = true;
    }
    return needs_rendered_to_screen;
}

