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
#include "interface.h" // strErrors and pictureNotFound+checkpointNotFound enums are defined here but should be down in CSeries; set_drawing_clip_rectangle (used to clip checkpoint map drawing) is also declared here (bizarre) but implemented in screen_drawing.cpp (sensible)
#include "screen.h"
#include "screen_drawing.h" // screen_rectangle
#include "images.h" // pict resources
#include "FontRenderer_SDL.hpp" // FontRenderer_SDL


// -----------------------------------------------------------------------------------------
// nasty externs

extern SDL_Surface* Term_Buffer; // over in screen.cpp; TODO: replace with with ImageBlitter and start thinking about drawing API


// implemented in screen_drawing.cpp but not declared in screen_drawing.h
FontRenderer_SDL* GetInterfaceFont(short font_index);
uint16_t GetInterfaceStyle(short font_index);
void _get_interface_color(size_t color_index, SDL_Color *color);


// TODO: bizarrely there isn't a shapes.h; instead, get_shape_surface() is declared in shell.h which is ridiculously circular, so for now re-declare here
SDL_Surface* get_shape_surface(int32_t shape, int32_t collection = NONE, byte** outPointerToPixelData = NULL, float inIllumination = -1.0f, bool inShrinkImage = false); // shapes.cpp; used to get M1 logon icon which is stored in Shapes file collection


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

// Image_Blitter = SDL
// OGL_Blitter = subclass(!)
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


static uint32_t current_pixel; // Current color pixel value
static font_style_t current_style = ::normal; // bitflags


static void set_current_style(SDL_Surface* target_surface, TerminalText* text_face)
{
    current_style = text_face->style;
    SDL_Color color;
    _get_interface_color(text_face->color_id + _computer_interface_text_color, &color); // TODO: moving this color conversion into TerminalText is a job for later
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

// TODO: kludge: the surface is being passed here as argument, upon which we ignore it and call screen_drawing___draw_screen_text in the awful screen_drawing.cpp which draws to "ports" nonsense via a dozen levels of indirection; the next step is to get rid of screen_drawing___draw_screen_text and use new text renderer

void draw_text_to_surface(SDL_Surface* target_surface, const std::string text, Rect dst_rect, int16_t flags, int16_t font_id, int16_t color_id) // I think it's a color id
{
    TODO("redo this once Render2D/ is done");
    //screen_drawing___draw_screen_text(text, (screen_rectangle*)&dst_rect, flags, font_id, color_id);
}


SDL_Surface* draw_multiline_text()
{
    // problem: this doesn't allow for style changes
    SDL_Surface* surface = NULL;
    //SDL_Surface * TTF_RenderUTF8_Blended_Wrapped(TTF_Font *font, const char *text, SDL_Color fg, Uint32 wrapLength);
    // https://wiki.libsdl.org/SDL2_ttf/TTF_RenderUTF8_Blended
    //SDL_Surface * TTF_RenderUTF8_Blended(TTF_Font *font, const char *text, SDL_Color fg);
    return surface;
}


// -----------------------------------------------------------------------------------------
// draw line


static void draw_line_of_text(SDL_Surface* target_surface, char* base_text, int16_t start_index, int16_t end_index,
                              Rect* bounds, ComputerTerminal* terminal_text, int16_t* text_face_start_index, int16_t line_number)
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
    FontRenderer_SDL* terminal_font = GetInterfaceFont(_computer_interface_font);
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

        xpos += draw_text(target_surface, base_text + current_start, current_end - current_start,
                          xpos, bounds->top + line_height * (line_number + FUDGE_FACTOR),
                          current_pixel, terminal_font, current_style);
        if (current_end != end_index)
        {
            current_start = current_end;
            current_end = end_index;
            assert_fail(face_data, "");
            set_current_style(target_surface, face_data);
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


static void draw_computer_text(SDL_Surface* target_surface, TerminalPage* current_page, int16_t current_line, Rect* bounds)
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
    
        set_current_style(target_surface, &text_face);
    
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
     
     */
}


// -----------------------------------------------------------------------------------------
// draw image


static Rect draw_terminal_picture(SDL_Surface* target_surface, TerminalPage* current_page)
{
    LoadedResource PictRsrc;
    bool found = get_picture_resource_from_scenario(current_page->permutation, PictRsrc);
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

        Rect screen_bounds = current_page->calculate_bounds_for_object_box(&bounds);

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
            }
            else
            {
                // Width is the predominant factor
                int16_t adjusted_height = RECTANGLE_WIDTH(&screen_bounds) * RECTANGLE_HEIGHT(&bounds) / RECTANGLE_WIDTH(&bounds);
                bounds = screen_bounds;
                InsetRect(&bounds, 0, (RECTANGLE_HEIGHT(&screen_bounds) - adjusted_height) / 2);
            }
        }

//        assert_warn(HGetState((Handle) picture) & 0x40); // assert it is purgable.

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
        return bounds;
    }
    else // pict resource not found, so draw "missing image" message
    {
        Rect bounds = current_page->calculate_bounds_for_object_box(NULL);
    
        SDL_Rect rect = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
        SDL_FillRect(target_surface, &rect, SDL_MapRGB(target_surface->format, 0, 0, 0));
        
        const std::string message = get_resource_string(STRING_KEY(strERRORS, pictureNotFound), {
            {"$objectID$", [current_page]{ return std::to_string(current_page->permutation); }},
        });

        const FontRenderer_SDL* font = GetInterfaceFont(_computer_interface_title_font);
        int32_t width = text_width(message, font, ::normal);
        draw_text(target_surface, message,
                  bounds.left + (RECTANGLE_WIDTH(&bounds) - width) / 2,
                  bounds.top  + RECTANGLE_HEIGHT(&bounds) / 2,
                  SDL_MapRGB(target_surface->format, 0xff, 0xff, 0xff), font, ::normal);
        return {0, 0, 0, 0};
    }
}


static void display_picture_with_text(SDL_Surface* target_surface, TerminalPage* current_page, ComputerTerminal* terminal_text, int16_t current_line)
{
    assert_fail(current_page->type == _pict_page, "");
    
    draw_terminal_picture(target_surface, current_page);

    Rect text_bounds = current_page->calculate_bounds_for_text_box();
    draw_computer_text(target_surface, current_page, current_line, &text_bounds);
}


// -----------------------------------------------------------------------------------------
// logon icon


#define M1_LOGON_SHAPE (44)


static Rect draw_m1_logon_shape(SDL_Surface* target_surface, TerminalPage* current_page) // TODO: this is used to draw M1 logon icon but it seems pretty generic
{
    Rect frame = get_term_rectangle(_terminal_logon_graphic_rect);

    SDL_Surface* s = get_shape_surface(M1_LOGON_SHAPE);
    if (!s) return {0, 0, 0, 0};
    
    Rect bounds;

    bounds.left = bounds.top = 0;
    bounds.right = s->w;
    bounds.bottom = s->h;
    
    OffsetRect(&bounds, -bounds.left, -bounds.top);
    
    Rect screen_bounds = current_page->calculate_bounds_for_object_box(_draw_object_on_center, &bounds);
    
    OffsetRect(&bounds, screen_bounds.left + (RECTANGLE_WIDTH(&screen_bounds)-RECTANGLE_WIDTH(&bounds))/2, screen_bounds.top + (RECTANGLE_HEIGHT(&screen_bounds)-RECTANGLE_HEIGHT(&bounds))/2);

    SDL_Rect rect = { bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top };
    SDL_SetSurfaceAlphaMod(s, 255);
    SDL_BlitSurface(s, NULL, target_surface, &rect);

    SDL_FreeSurface(s);
    return bounds;
}



static void draw_connection_screen(SDL_Surface* target_surface, TerminalPage* current_page)
{
    // TODO: da math aint mathin
    Rect picture_bounds = get_term_rectangle(_terminal_logon_graphic_rect);
    if (!current_page) return;
    
    Rect text_bounds;
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
        draw_m1_logon_shape(target_surface, current_page);
        
        Rect title_line_bounds = get_term_rectangle(_terminal_logon_title_rect);
        Rect location_line_bounds = get_term_rectangle(_terminal_logon_location_rect);
        text_bounds.top    = title_line_bounds.top;
        text_bounds.left   = std::min(title_line_bounds.left,  location_line_bounds.left);
        text_bounds.right  = std::max(title_line_bounds.right, location_line_bounds.right);
        text_bounds.bottom = title_line_bounds.bottom;
    }
    else
    {
        // the design of M2 logon/logoff screens is terminal-specific logo plus terminal-specific text
        Rect bounds = picture_bounds;
        picture_bounds = draw_terminal_picture(target_surface, current_page);
        
        // Use the picture bounds to create the logon text crap
        picture_bounds.top    = picture_bounds.bottom;
        picture_bounds.bottom = bounds.bottom;
        picture_bounds.left   = bounds.left;
        picture_bounds.right  = bounds.right;
    }
    
    TODO("redo this once Render2D/ is done");
    /*
    
    // This is always just a line, so we can do this here
    FontRenderer_SDL* terminal_font = GetInterfaceFont(_computer_interface_font);
    uint16_t terminal_style = GetInterfaceStyle(_computer_interface_font);
    
    char* base_text = current_page->texts.at(0).utf8_string.data();
    
    // center string on screen // TODO: this will move into draw_ function
    int16_t width = text_width(base_text + current_page->mr_start_index, current_page->mr_length, terminal_font, terminal_style);
    picture_bounds.left += (RECTANGLE_WIDTH(&picture_bounds) - width) / 2;
    
    draw_computer_text(target_surface, current_page, 0, &picture_bounds);
     */
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


static void present_checkpoint_text(SDL_Surface* target_surface, ComputerTerminal* terminal_text, TerminalPage* current_page, int16_t current_line)
{
    // draw the overhead map.
    Rect bounds = current_page->calculate_bounds_for_object_box(NULL);
    
    overhead_map_data overhead_data;
    if (find_checkpoint_location(target_surface, current_page->permutation, &overhead_data.origin, &overhead_data.origin_polygon_index))
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
        TODO("redo this once Render2D/ is done");
        /*
        SDL_Rect rect = {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top};
        SDL_FillRect(target_surface, &rect, SDL_MapRGB(target_surface->format, 0, 0, 0));
        
        const std::string message = get_resource_string(STRING_KEY(strERRORS, checkpointNotFound), {
            {"$objectID$", [current_page]{ return std::to_string(current_page->permutation); }},
        });
        
        const FontRenderer_SDL* font = GetInterfaceFont(_computer_interface_title_font);
        int32_t width = text_width(message, font, ::normal);
        draw_text(target_surface, message,
                  bounds.left + (RECTANGLE_WIDTH(&bounds) - width) / 2,
                  bounds.top  + RECTANGLE_HEIGHT(&bounds) / 2,
                  SDL_MapRGB(target_surface->format, 0xff, 0xff, 0xff), font, ::normal);
         */
    }
    
    // draw the text
    bounds = current_page->calculate_bounds_for_text_box();
    draw_computer_text(target_surface, current_page, current_line, &bounds);
}


// -----------------------------------------------------------------------------------------
// top + bottom bars


// TODO: for l10n, might want to use Lua to draw borders
static void draw_terminal_borders(SDL_Surface* target_surface, PlayerTerminalState* terminal_state)
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
    
    // Draw the top rectangle
    Rect border = get_term_rectangle(_terminal_header_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);

    // Draw the top login header text
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    draw_text_to_surface(target_surface, get_resource_string(STRING_KEY(strCOMPUTER_TERMINAL_LABELS, top_message)),
                         border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    
    draw_text_to_surface(target_surface, get_date_string(current_page->flags & _terminal_is_m1),
                         border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);

    // Draw the the bottom rectangle & text
    border = get_term_rectangle(_terminal_footer_rect);
    _fill_screen_rectangle((screen_rectangle*)&border, _computer_border_background_text_color);
    border.left += LABEL_INSET; border.right -= LABEL_INSET;
    
    draw_text_to_surface(target_surface, get_resource_string(STRING_KEY(strCOMPUTER_TERMINAL_LABELS, bottom_left_message)),
                         border, _center_vertical, _computer_interface_font, _computer_border_text_color);
    
    draw_text_to_surface(target_surface, get_resource_string(STRING_KEY(strCOMPUTER_TERMINAL_LABELS, bottom_right_message)),
                         border, _right_justified | _center_vertical, _computer_interface_font, _computer_border_text_color);
}


// -----------------------------------------------------------------------------------------
// draw terminal to SDL_Surface


bool draw_computer_terminal()
{
    if (has_screen_size_changed())
    {
        initialize_terminal_renderer();
    }
    
    SDL_Surface* target_surface = Term_Buffer;
    
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
            
            fill_terminal_with_black(target_surface);

            switch (current_page->type)
            {
                case _logon_page: // permutation = logon logo
                case _logoff_page:
                {
                    draw_connection_screen(target_surface, current_page);
                    break;
                }
                case _unfinished_page:
                case _success_page:
                case _failure_page:
                    // ao__dprintf__("You shouldn't try to render this view.;g");
                    break;
                    
                case _information_page: // Draw as normal
                {
                    Rect bounds = get_term_rectangle(_terminal_full_text_rect);
                    draw_computer_text(target_surface, current_page, terminal_state->line_number, &bounds);
                    break;
                }
                case _checkpoint_page: // permutation = the goal to show
                    // note that checkpoints can only be equal to one screenful
                    present_checkpoint_text(target_surface, terminal_text, current_page, terminal_state->line_number);
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
                    display_picture_with_text(target_surface, current_page, terminal_text, terminal_state->line_number);
                    break;
                    
                case _static_page:
                {
                    fill_terminal_with_static(target_surface);
                    terminal_state->needs_redraw = true;
                    break;
                }
                case _camera_page:
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

