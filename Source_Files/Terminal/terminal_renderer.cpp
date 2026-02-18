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

#include "SDL2/SDL.h" // SDL_Color

#include "overhead_map.h" // overhead_map_data type, _rendering_checkpoint_map enum

#include "interface.h" // strErrors and pictureNotFound+checkpointNotFound enums are defined here but should be down in CSeries; set_drawing_clip_rectangle (used to clip checkpoint map drawing) is also declared here (bizarre) but implemented in screen_drawing.cpp (sensible)

#include "images.h" //

#include "sdl_fonts.h" // font_info
#include "screen_drawing.h" // screen_rectangle

//#include "Logging.h"



// -----------------------------------------------------------------------------------------
// nasty externs

extern SDL_Surface* Term_Buffer; // over in screen.cpp; TODO: replace with with ImageBlitter and start thinking about drawing API


// implemented in screen_drawing.cpp but not declared in screen_drawing.h
font_info* GetInterfaceFont(short font_index);
uint16_t GetInterfaceStyle(short font_index);
void _get_interface_color(size_t color_index, SDL_Color *color);


// TODO: bizarrely there isn't a shapes.h; instead, get_shape_surface() is declared in shell.h which is ridiculously circular, so for now re-declare here
SDL_Surface* get_shape_surface(int32_t shape, int32_t collection = NONE, byte** outPointerToPixelData = NULL, float inIllumination = -1.0f, bool inShrinkImage = false); // shapes.cpp; used to get M1 logon icon which is stored in Shapes file collection


int32_t get_pict_header_width(LoadedResource &); // implemented in images.cpp but not decladed in images.h; only used in display_picture()


// not currently extern but should be moved to cstrings or similar
static void format_something_not_found_error(int32_t error_code, int16_t de_ting, char buffer[256])
{
    char format_string[128];
    getcstr(format_string, strERRORS, error_code);
    snprintf(buffer, sizeof(&buffer), format_string, de_ting); // TODO: this is dangerously insecure! the ways in which a malformed or malicious format string could misbehave are legion; this needs to be straightened out as essential to i18n support; see `expand_app_variables` and think about how to specify arguments safely
}


extern SDL_PixelFormat pixel_format_32; // randomize_pixel uses its Amask; unclear why


// -----------------------------------------------------------------------------------------
// current font style


static uint32_t current_pixel; // Current color pixel value
static font_style_t current_style = styleNormal; // bitflags


static void set_current_style(SDL_Surface* target_surface, TerminalTextStyleRange* text_face)
{
    current_style = text_face->style;
    SDL_Color color;
    _get_interface_color(text_face->color_id + _computer_interface_text_color, &color); // TODO: moving this color conversion into TerminalTextStyleRange is a job for later
    current_pixel = SDL_MapRGB(target_surface->format, color.r, color.g, color.b);
}


// -----------------------------------------------------------------------------------------
// fill


static void fill_terminal_with_black(SDL_Surface* target_surface) // TODO: bounds? or no bounds?
{
    SDL_Rect frame = get_term_rect(_terminal_screen_rect); // TODO: this shouldn't be necessary as the Surface should be pre-sized to the dimensions at which the terminal displays on screen (typically 4:3, sized to fit in free screen space away from HUD, which is presumably what _terminal_screen_rect specifies on assumption it's drawing to a 640x480 screen)
    SDL_FillRect(target_surface, &frame, SDL_MapRGB(target_surface->format, 0, 0, 0));
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

static void fill_terminal_with_static(SDL_Surface* target_surface)
{
    Rect bounds = get_term_rectangle(_terminal_screen_rect); // TODO: as in fill_terminal_with_black
    
    for (int32_t y = bounds.top; y < bounds.bottom; ++y)
    {
        int32_t width = bounds.right - bounds.left;
        int32_t bpp = target_surface->format->BytesPerPixel;
        uint8_t* p = (uint8_t*)target_surface->pixels + y * target_surface->pitch + bounds.left * bpp;
        switch (bpp)
        {
            case 1:
                randomize_line<uint8_t>(p, width);
                break;
            case 2:
                randomize_line<uint16_t>(reinterpret_cast<uint16_t*>(p), width);
                break;
            case 4:
                randomize_line<uint32_t>(reinterpret_cast<uint32_t*>(p), width);
                break;
        }
    }
}


// -----------------------------------------------------------------------------------------
// draw text to surface

// TODO: kludge: the surface is being passed here as argument, upon which we ignore it and call _draw_screen_text in the awful screen_drawing.cpp which draws to "ports" nonsense via a dozen levels of indirection; the next step is to get rid of _draw_screen_text and use new text renderer

void draw_text_to_surface(SDL_Surface* target_surface, char* text, Rect dst_rect, int16_t flags, int16_t font_id, int16_t color_id) // I think it's a color id
{
    _draw_screen_text(text, (screen_rectangle*)&dst_rect, flags, font_id, color_id);
}


// -----------------------------------------------------------------------------------------
// draw line


static void draw_line_of_text(SDL_Surface* target_surface, char* base_text, int16_t start_index, int16_t end_index,
                              Rect* bounds, TerminalText* terminal_text, int16_t* text_face_start_index, int16_t line_number)
{
    //printf("draw_line_of_text: %i..%i '%s'\n", start_index, end_index, base_text+start_index);

    
    int16_t line_height = _get_font_line_height(_computer_interface_font);
    
    uint32_t text_index = *text_face_start_index == NONE ? 0 : *text_face_start_index;
    
    // Get to the first one that concerns us.
    TerminalTextStyleRange* face_data = NULL;
    if (text_index < terminal_text->style_ranges.size())
    {
        do {
            face_data = terminal_text->get_indexed_font_changes(text_index);
            if (!face_data) return;
            if (face_data->start_index < start_index) text_index++;
        } while (face_data->start_index < start_index && text_index < terminal_text->style_ranges.size());
    }
    
    int16_t current_start = start_index, current_end = end_index;
    font_info* terminal_font = GetInterfaceFont(_computer_interface_font);
    int32_t xpos = bounds->left;

    bool done = false;
    while (!done)
    {
        if (text_index < terminal_text->style_ranges.size())
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

        xpos += draw_text(target_surface, base_text + current_start, current_end - current_start,
                          xpos, bounds->top + line_height * (line_number + FUDGE_FACTOR),
                          current_pixel, terminal_font, current_style);
        if (current_end != end_index)
        {
            current_start = current_end;
            current_end = end_index;
            assert(face_data);
            set_current_style(target_surface, face_data);
        }
        else
        {
            done = true;
        }
    }
}


// -----------------------------------------------------------------------------------------
// draw text


static void draw_computer_text(SDL_Surface* target_surface, TerminalText* terminal_text, char* base_text, int16_t current_group_index, int16_t current_line, Rect* bounds)
{
    bool done = false;
    TerminalTextGroup* current_group = terminal_text->get_grouping(current_group_index);
    if (!current_group) return;
    
    uint16_t old_style = current_style; // urgh; freaking globals everywhere
    current_style = GetInterfaceStyle(_computer_interface_font);

    int16_t start_index = current_group->start_index;
    int16_t end_index = current_group->length + current_group->start_index;
    
    // eat the previous lines // yeesh
    for (int16_t i = 0; i < current_line; ++i)
    {
        // Calculate one line.
        if (!calculate_line_end_index(base_text, current_style, RECTANGLE_WIDTH(bounds), start_index, current_group->start_index + current_group->length, &end_index))
        {
            if (end_index > current_group->start_index + current_group->length)
            {
                // dprintf("Start: %d Length: %d End: %d;g", current_group->start_index, current_group->length, end_index);
                // dprintf("Width: %d", RECTANGLE_WIDTH(bounds));
                end_index = current_group->start_index + current_group->length;
            }
            //dprintf("calculate line: %d start: %d end: %d", index, start_index, end_index);
            assert(end_index <= current_group->start_index + current_group->length);
            
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
        int16_t last_index = current_group->start_index;
        int16_t last_text_index = NONE;
        for (uint32_t text_index = 0; text_index < terminal_text->style_ranges.size(); ++text_index)
        {
            TerminalTextStyleRange* font_face = terminal_text->get_indexed_font_changes(text_index);
            if (!font_face) return;
            
            // Go backwards from the scrolled starting location.
            if (font_face->start_index>last_index && font_face->start_index<start_index)
            {
                // dprintf("ff index: %d last: %d end: %d", font_face->index, last_index, start_index);
                last_index = font_face->start_index;
                last_text_index = text_index;
            }
        }
        
        // dprintf("last index: %d", last_text_index);
        
        TerminalTextStyleRange text_face;
        if (last_text_index == NONE) // Default-> plain, etc.
        {
            text_face.color_id = 0;
            text_face.style    = 0;
        }
        else // Figure out the font.
        {
            TerminalTextStyleRange* font_face = terminal_text->get_indexed_font_changes(last_text_index);
            if (!font_face) return;
            text_face = *font_face;
        }
    
        set_current_style(target_surface, &text_face);
    
        // Draw what is one the screen
        for (int16_t i = 0; !done && i < terminal_text->lines_per_page; ++i)
        {
            //dprintf("calculating the line");
            if (!calculate_line_end_index(base_text, current_style, RECTANGLE_WIDTH(bounds), start_index, current_group->start_index + current_group->length, &end_index))
            {
                //dprintf("draw calculate line: %d start: %d end: %d text: %x length: %d lti: %d", index, start_index, end_index, base_text, current_group->length, last_text_index);
                if (end_index>current_group->start_index + current_group->length)
                {
                    // dprintf("Start: %d Length: %d End: %d;g", current_group->start_index, current_group->length, end_index);
                    // dprintf("Width: %d", RECTANGLE_WIDTH(bounds));
                    end_index = current_group->start_index + current_group->length;
                }
                assert(end_index <= current_group->start_index + current_group->length);
                draw_line_of_text(target_surface, base_text, start_index, end_index, bounds, terminal_text, &last_text_index, i);
                start_index = end_index;
            }
            else // End of text.
            {
                done = true;
            }
        }
    }

    current_style = old_style;
}


// -----------------------------------------------------------------------------------------
// draw image


static void display_terminal_picture(SDL_Surface* target_surface, int16_t picture_id, Rect* frame, int16_t flags)
{
    LoadedResource PictRsrc;
    bool found = get_picture_resource_from_scenario(picture_id, PictRsrc);
    if (found)
    {
        auto picture_surface = picture_to_surface(PictRsrc);
        Rect bounds;
        bounds.left = bounds.top = 0;
        bounds.right = picture_surface->w;
        bounds.bottom = picture_surface->h;

        int32_t pict_header_width = get_pict_header_width(PictRsrc);
        bool cinemascopeHack = false;
        if (bounds.right != pict_header_width && bounds.right == 614)
        {
            cinemascopeHack = true;
            bounds.right = pict_header_width;
        }
        OffsetRect(&bounds, -bounds.left, -bounds.top);

        Rect screen_bounds;
        calculate_bounds_for_object(flags, &screen_bounds, &bounds);

        if (RECTANGLE_WIDTH(&bounds) <= RECTANGLE_WIDTH(&screen_bounds)
            && RECTANGLE_HEIGHT(&bounds) <= RECTANGLE_HEIGHT(&screen_bounds))
        {
            // It fits-> center it.
            OffsetRect(&bounds, screen_bounds.left + (RECTANGLE_WIDTH(&screen_bounds) - RECTANGLE_WIDTH(&bounds)) / 2,
                       screen_bounds.top+(RECTANGLE_HEIGHT(&screen_bounds) - RECTANGLE_HEIGHT(&bounds)) / 2);
        }
        else
        {
            // Doesn't fit.  Make it, but preserve the aspect ratio like a good little boy
            if (RECTANGLE_HEIGHT(&bounds)-RECTANGLE_HEIGHT(&screen_bounds)>=
                RECTANGLE_WIDTH(&bounds)-RECTANGLE_WIDTH(&screen_bounds))
            {
                int16_t adjusted_width = RECTANGLE_HEIGHT(&screen_bounds) * RECTANGLE_WIDTH(&bounds) / RECTANGLE_HEIGHT(&bounds);
                bounds = screen_bounds;
                InsetRect(&bounds, (RECTANGLE_WIDTH(&screen_bounds) - adjusted_width) / 2, 0);
                // dprintf("Warning: Not large enough for pict: %d (height);g", picture_id);
            }
            else
            {
                // Width is the predominant factor
                int16_t adjusted_height = RECTANGLE_WIDTH(&screen_bounds) * RECTANGLE_HEIGHT(&bounds) / RECTANGLE_WIDTH(&bounds);
                bounds = screen_bounds;
                InsetRect(&bounds, 0, (RECTANGLE_HEIGHT(&screen_bounds) - adjusted_height) / 2);
                // dprintf("Warning: Not large enough for pict: %d (width);g", picture_id);
            }
        }

//        warn(HGetState((Handle) picture) & 0x40); // assert it is purgable.

        SDL_Rect r = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
        if ((picture_surface->w == r.w && picture_surface->h == r.h) || cinemascopeHack)
        {
            SDL_BlitSurface(picture_surface.get(), NULL, target_surface, &r);
        }
        else // Rescale picture
        {
            SDL_Surface* s2 = rescale_surface(picture_surface.get(), r.w, r.h);
            if (s2)
            {
                SDL_BlitSurface(s2, NULL, target_surface, &r);
                SDL_FreeSurface(s2);
            }
        }
        // And let the caller know where we drew the picture
        *frame = bounds;
    }
    else
    {
        Rect bounds;
        calculate_bounds_for_object(flags, &bounds, NULL);
    
        SDL_Rect rect = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
        SDL_FillRect(target_surface, &rect, SDL_MapRGB(target_surface->format, 0, 0, 0));
        
        char message[256];
        format_something_not_found_error(pictureNotFound, picture_id, message);

        const font_info* font = GetInterfaceFont(_computer_interface_title_font);
        int32_t width = text_width(message, font, styleNormal);
        draw_text(target_surface, message,
                  bounds.left + (RECTANGLE_WIDTH(&bounds) - width) / 2,
                  bounds.top + RECTANGLE_HEIGHT(&bounds) / 2,
                  SDL_MapRGB(target_surface->format, 0xff, 0xff, 0xff),
                  font, styleNormal);
    }
}


static void display_picture_with_text(SDL_Surface* target_surface, PlayerTerminalState* terminal_state, TerminalText* terminal_text, int16_t current_line)
{
    TerminalTextGroup* current_group = terminal_text->get_grouping(terminal_state->current_group);
    if (!current_group) return;
    assert(current_group->type == _pict_group);
    
    Rect picture_bounds;
    calculate_bounds_for_object(current_group->flags, &picture_bounds, NULL);
    display_terminal_picture(target_surface, current_group->permutation, &picture_bounds, current_group->flags);

    Rect text_bounds;
    calculate_bounds_for_text_box(current_group->flags, &text_bounds);
    draw_computer_text(target_surface, terminal_text, terminal_text->get_cstr(), terminal_state->current_group, current_line, &text_bounds);
}


// -----------------------------------------------------------------------------------------
// logon icon


static void display_shape(SDL_Surface* target_surface, int16_t shape, Rect* frame) // TO DO: this is used to draw M1 logon icon but it seems pretty generic
{
    SDL_Surface* s = get_shape_surface(shape);
    if (s)
    {
        Rect bounds;
        Rect screen_bounds;

        bounds.left = bounds.top = 0;
        bounds.right = s->w;
        bounds.bottom = s->h;
        
        OffsetRect(&bounds, -bounds.left, -bounds.top);
        calculate_bounds_for_object(_center_object, &screen_bounds, &bounds);
        
        OffsetRect(&bounds, screen_bounds.left + (RECTANGLE_WIDTH(&screen_bounds)-RECTANGLE_WIDTH(&bounds))/2, screen_bounds.top + (RECTANGLE_HEIGHT(&screen_bounds)-RECTANGLE_HEIGHT(&bounds))/2);

        SDL_Rect rect = { bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top };
        SDL_SetSurfaceAlphaMod(s, 255);
        SDL_BlitSurface(s, NULL, target_surface, &rect);

        SDL_FreeSurface(s);
        *frame = bounds;
    }
}


#define M1_LOGON_SHAPE (44)

static void draw_logon_text(SDL_Surface* target_surface, TerminalText* terminal_text, int16_t current_group_index, int16_t logon_shape_id)
{
    Rect picture_bounds = get_term_rectangle(_terminal_logon_graphic_rect);
    TerminalTextGroup* current_group = terminal_text->get_grouping(current_group_index);
    if (!current_group) return;
    
    if (current_group->flags & _group_is_marathon_1) // the design of M1 logon/logoff screens is mostly hardcoded: standard icon and text
    {
        display_shape(target_surface, M1_LOGON_SHAPE, &picture_bounds);
        
        // draw static title below logo
        char message[256];
        picture_bounds = get_term_rectangle(_terminal_logon_title_rect);
        getcstr(message, strCOMPUTER_LABELS, _m1_marathon_name);
        draw_text_to_surface(target_surface, message, picture_bounds, _center_vertical | _center_horizontal, _computer_interface_title_font, _computer_interface_text_color);
        picture_bounds = get_term_rectangle(_terminal_logon_location_rect);
    }
    else
    {
        Rect bounds = picture_bounds;
        display_terminal_picture(target_surface, logon_shape_id, &picture_bounds, _center_object);

        // Use the picture bounds to create the logon text crap
        picture_bounds.top    = picture_bounds.bottom;
        picture_bounds.bottom = bounds.bottom;
        picture_bounds.left   = bounds.left;
        picture_bounds.right  = bounds.right;
    }

    // This is always just a line, so we can do this here
    font_info* terminal_font = GetInterfaceFont(_computer_interface_font);
    uint16_t terminal_style = GetInterfaceStyle(_computer_interface_font);
    
    char* base_text = terminal_text->get_cstr();
    int16_t width = text_width(base_text + current_group->start_index, current_group->length, terminal_font, terminal_style);
    
    picture_bounds.left += (RECTANGLE_WIDTH(&picture_bounds) - width) / 2;
    
    draw_computer_text(target_surface, terminal_text, base_text, current_group_index, 0, &picture_bounds);
}


// -----------------------------------------------------------------------------------------
// M1-style checkpoint map


static bool find_checkpoint_location(SDL_Surface* target_surface, int16_t checkpoint_index, world_point2d* location, int16_t* polygon_index)
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


static void present_checkpoint_text(SDL_Surface* target_surface, TerminalText* terminal_text, int16_t current_group_index, int16_t current_line)
{
    TerminalTextGroup* current_group = terminal_text->get_grouping(current_group_index);
    if (!current_group) return;

    // draw the overhead map.
    Rect bounds;
    calculate_bounds_for_object(current_group->flags, &bounds, NULL);
    
    overhead_map_data overhead_data;
    if (find_checkpoint_location(target_surface, current_group->permutation, &overhead_data.origin, &overhead_data.origin_polygon_index))
    {
        overhead_data.scale       =  1;
        overhead_data.top         = bounds.top;
        overhead_data.left        = bounds.left;
        overhead_data.half_width  = RECTANGLE_WIDTH(&bounds)/2;
        overhead_data.half_height = RECTANGLE_HEIGHT(&bounds)/2;
        overhead_data.width       = RECTANGLE_WIDTH(&bounds);
        overhead_data.height      = RECTANGLE_HEIGHT(&bounds);
        overhead_data.mode        = _rendering_checkpoint_map;
        
        //
        set_drawing_clip_rectangle(bounds.top, bounds.left, bounds.bottom, bounds.right);
        _render_overhead_map(&overhead_data);
        set_drawing_clip_rectangle(SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX);
    }
    else // draw "checkpoint not found" error message
    {
        SDL_Rect rect = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
        SDL_FillRect(target_surface, &rect, SDL_MapRGB(target_surface->format, 0, 0, 0));
        
        char message[256];
        format_something_not_found_error(checkpointNotFound, current_group->permutation, message);
        
        const font_info* font = GetInterfaceFont(_computer_interface_title_font);
        int32_t width = text_width(message, font, styleNormal);
        draw_text(target_surface, message,
                  bounds.left + (RECTANGLE_WIDTH(&bounds) - width) / 2,
                  bounds.top + RECTANGLE_HEIGHT(&bounds) / 2,
                  SDL_MapRGB(target_surface->format, 0xff, 0xff, 0xff),
                  font, styleNormal);
    }
    
    // draw the text
    calculate_bounds_for_text_box(current_group->flags, &bounds);
    draw_computer_text(target_surface, terminal_text, terminal_text->get_cstr(), current_group_index, current_line, &bounds);
}


// -----------------------------------------------------------------------------------------
// top + bottom bars


// TODO: for l10n, might want to use Lua to draw borders
static void draw_terminal_borders(SDL_Surface* target_surface, PlayerTerminalState* terminal_state)
{
    TerminalText* terminal_text = get_terminal_text_for_terminal_id(terminal_state->terminal_id);
    if (!terminal_text) return;
    
    TerminalTextGroup* current_group = terminal_text->get_grouping(terminal_state->current_group);
    if (!current_group) return;
     
    int16_t top_message, bottom_left_message, bottom_right_message;
    switch (current_group->type)
    {
        case _logon_group:
            top_message          = _computer_starting_up;
            bottom_left_message  = _computer_manufacturer;
            bottom_right_message = _computer_address;
            break;
            
        case _logoff_group:
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
    
    // Draw the top rectangle
    Rect border = get_term_rectangle(_terminal_header_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);

    // Draw the top login header text
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    char message[256];
    getcstr(message, strCOMPUTER_LABELS, top_message);
    draw_text_to_surface(target_surface, message, border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    get_date_string(message, current_group->flags);
    draw_text_to_surface(target_surface, message, border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);

    // Draw the the bottom rectangle & text
    border = get_term_rectangle(_terminal_footer_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    getcstr(message, strCOMPUTER_LABELS, bottom_left_message);
    draw_text_to_surface(target_surface, message, border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    getcstr(message, strCOMPUTER_LABELS, bottom_right_message);
    draw_text_to_surface(target_surface, message, border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);
}


// -----------------------------------------------------------------------------------------
// draw terminal to SDL_Surface


bool draw_computer_terminal()
{
    SDL_Surface* target_surface = Term_Buffer;
    
    bool needs_rendered_to_screen = false;
    
    PlayerTerminalState* terminal_state = get_terminal_state_for_player(current_player_index);
    if (terminal_state->is_active != true) return false;
    
    if (terminal_state->needs_redraw)
    {
        terminal_state->needs_redraw = false;
        
        TerminalText* terminal_text = get_terminal_text_for_terminal_id(terminal_state->terminal_id);
        if (!terminal_text) return false;
        
        if (terminal_state->is_active)
        {
            TerminalTextGroup* current_group = terminal_text->get_grouping(terminal_state->current_group);
            if (!current_group) return false;
            
            fill_terminal_with_black(target_surface);

            switch (current_group->type)
            {
                case _logon_group: // permutation = logon logo
                case _logoff_group:
                    draw_logon_text(target_surface, terminal_text, terminal_state->current_group, current_group->permutation);
                    break;
                    
                case _unfinished_group:
                case _success_group:
                case _failure_group:
                    // dprintf("You shouldn't try to render this view.;g");
                    break;
                    
                case _information_group: // Draw as normal
                {
                    Rect bounds = get_term_rectangle(_terminal_full_text_rect);
                    
                    draw_computer_text(target_surface, terminal_text, terminal_text->get_cstr(), terminal_state->current_group, terminal_state->current_line, &bounds);
                    break;
                }
                case _checkpoint_group: // permutation = the goal to show
                    // note that checkpoints can only be equal to one screenful
                    present_checkpoint_text(target_surface, terminal_text, terminal_state->current_group, terminal_state->current_line);
                    break;
                    
                case _end_group:
                case _interlevel_teleport_group: // permutation = level to go to
                case _intralevel_teleport_group: // permutation = polygon to go to.
                case _sound_group:               // permutation = sound id to play
                case _tag_group:
                    break; // These are all handled elsewhere
                    
                case _movie_group:
                case _track_group:
                    if (!game_is_networked)
                    {
                        // dprintf("Movies/Music Tracks not supported on playback (yet);g");
                    } else {
                        // dprintf("On networked games, should we display a PICT here?;g");
                    }
                    break;
                    
                case _pict_group:
                    display_picture_with_text(target_surface, terminal_state, terminal_text, terminal_state->current_line);
                    break;
                    
                case _static_group:
                {
                    fill_terminal_with_static(target_surface);
                    terminal_state->needs_redraw = true;
                    break;
                }
                case _camera_group:
                    break;
                    
                default:
                    break;
            }
            draw_terminal_borders(target_surface, terminal_state); // borders will overdraw any overlapping content
        }
        needs_rendered_to_screen = true;
    }
    return needs_rendered_to_screen;
}

