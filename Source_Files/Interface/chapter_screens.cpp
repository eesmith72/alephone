

#include "chapter_screens.hpp"

#include "shapes.h" // shapes_file_is_m1
#include "fades.h"
#include "SoundManager.h"
#include "Music.h"
#include "images.h" // get_sound_resource_from_images
#include "screen.h" // clear_screen
#include "main_menu.hpp" // display_main_menu (temporary till these functions are unknotted)
#include "mouse.h" // hide_cursor
#include "XML_LevelScript.h" // EndScreenIndex, NumEndScreens
#include "Statistics.h" // StatsManager, used in display_quit_screens
#include "screen_shared.h" // interface_bit_depth
#include "shell_options.h"

#include "image_blitter.hpp"

#include "Movie.h"

#include "app_event_loop.hpp" // game_state (which should eventually move out of the functions below and into app_event_loop.cpp)



static std::shared_ptr<SoundPlayer> introduction_sound = nullptr;

static LoadedResource SoundRsrc;



struct screen_data
{
    int16_t screen_base;
    int16_t screen_count;
    int32_t duration;
};


// TODO: convert to a state machine table (including fades, sounds, music start/stop, and app state transitions) so we can simplify the hardcoded 'display_SCREEN' functions to 'animate_screens(_starting_at_id)'


static const std::array<screen_data, 8> m1_display_screens = {(screen_data)
    { 1111, 4, INTRO_SCREEN_DURATION },
    { MAIN_MENU_BASE, 1, 0 },
    { 10000, NUMBER_OF_CHAPTER_HEADINGS, CHAPTER_HEADING_DURATION },
    { PROLOGUE_SCREEN_BASE, NUMBER_OF_PROLOGUE_SCREENS, PROLOGUE_DURATION },
    { EPILOGUE_SCREEN_BASE, NUMBER_OF_EPILOGUE_SCREENS, EPILOGUE_DURATION },
    { 1000, 1, CREDIT_SCREEN_DURATION},
    { INTRO_SCREEN_BETWEEN_DEMO_BASE, NUMBER_OF_INTRO_SCREENS_BETWEEN_DEMOS, DEMO_INTRO_SCREEN_DURATION },
    { FINAL_SCREEN_BASE, NUMBER_OF_FINAL_SCREENS, FINAL_SCREEN_DURATION }
};


static const std::array<screen_data, 8> m2_display_screens = {(screen_data)
    { INTRO_SCREEN_BASE, NUMBER_OF_INTRO_SCREENS, INTRO_SCREEN_DURATION },
    { MAIN_MENU_BASE, 1, 0 },
    { CHAPTER_SCREEN_BASE, NUMBER_OF_CHAPTER_HEADINGS, CHAPTER_HEADING_DURATION },
    { PROLOGUE_SCREEN_BASE, NUMBER_OF_PROLOGUE_SCREENS, PROLOGUE_DURATION },
    { EPILOGUE_SCREEN_BASE, NUMBER_OF_EPILOGUE_SCREENS, EPILOGUE_DURATION },
    { CREDIT_SCREEN_BASE, NUMBER_OF_CREDIT_SCREENS, CREDIT_SCREEN_DURATION},
    { INTRO_SCREEN_BETWEEN_DEMO_BASE, NUMBER_OF_INTRO_SCREENS_BETWEEN_DEMOS, DEMO_INTRO_SCREEN_DURATION },
    { FINAL_SCREEN_BASE, NUMBER_OF_FINAL_SCREENS, FINAL_SCREEN_DURATION }
};


const screen_data* get_screen_data(short index)
{
    return shapes_file_is_m1() ? &m1_display_screens.at(index) : &m2_display_screens.at(index);
}



//

void display_splash_screen()
{
    clear_screen();
    
    const screen_data* screen_data = get_screen_data(_display_intro_screens);
    
    if (screen_data->screen_count > 0)
    {
        if (game_state.state == _display_intro_screens && game_state.current_screen == INTRO_SCREEN_TO_START_SONG_ON)
        {
            Music::instance()->RestartIntroMusic();
        }

        game_state.state                    = _display_intro_screens;
        game_state.current_screen           = 0;
        game_state.last_ticks_on_idle       = machine_tick_count();
        game_state.ticks_until_next_screen  = screen_data->duration;
        
        display_screen(screen_data->screen_base);

        if (introduction_sound)
        {
            introduction_sound->AskStop();
            introduction_sound.reset();
        }
        SoundRsrc.Unload();
        if (get_sound_resource_from_images(screen_data->screen_base, SoundRsrc))
        {
            SoundParameters parameters;
            introduction_sound = SoundManager::instance()->PlaySound(SoundRsrc, parameters);
        }
    }
    else
    {
        display_main_menu();
    }
}


void display_introduction_screen_for_demo() // TODO: is there any reason to keep this? demo should just be another scenario
{
    const screen_data* screen_data = get_screen_data(_display_intro_screens_for_demo);

    if (screen_data->screen_count > 0)
    {
        game_state.state                    = _display_intro_screens_for_demo;
        game_state.current_screen           = 0;
        game_state.last_ticks_on_idle       = machine_tick_count();
        game_state.ticks_until_next_screen  = screen_data->duration;
        
        display_screen(screen_data->screen_base);
    }
    else
    {
        display_main_menu();
    }
}


void display_epilogue()
{
    hide_cursor();
    
    Music::instance()->RestartIntroMusic();
    
    auto ticks = machine_tick_count();
    do { Music::instance()->Idle(); } while (machine_tick_count() - ticks < 10);
    
    game_state.state                = _display_epilogue;
    game_state.ticks_until_next_screen                = 0;
    game_state.current_screen       = 0;
    game_state.last_ticks_on_idle   = machine_tick_count();
    
    int32_t offset, count;
    get_end_screen_offset_and_count(offset, count);

    for (int32_t i = 0; i < count; i++)
    {
        try_and_display_chapter_screen(offset + i, true, true);
    }
    
    show_cursor();
}


void display_credits()
{
    if (NUMBER_OF_CREDIT_SCREENS)
    {
        const screen_data* screen_data = get_screen_data(_display_credits);
        
        game_state.user                 = _single_player;
        game_state.flags                = 0;
        game_state.state                = _display_credits;
        game_state.ticks_until_next_screen                = screen_data->duration;
        game_state.current_screen       = 0;
        game_state.last_ticks_on_idle   = machine_tick_count();
        
        display_screen(screen_data->screen_base);
    }
}


void display_quit_screens()
{
    const screen_data* screen_data = get_screen_data(_display_quit_screens);

    if(screen_data->screen_count)
    {
        game_state.user                     = _single_player;
        game_state.flags                    = 0;
        game_state.state                    = _display_quit_screens;
        game_state.current_screen           = 0;
        game_state.last_ticks_on_idle       = machine_tick_count();
        game_state.ticks_until_next_screen  = screen_data->duration;
        
        display_screen(screen_data->screen_base);
    }
    else
    {
        StatsManager::instance()->Finish();
        // No screens
        game_state.state                    = _quit_game;
        game_state.ticks_until_next_screen  = 0;
    }
}




void next_game_screen()
{
    const screen_data* data = get_screen_data(game_state.state);

    stop_ui_fade();
    show_cursor();
    
    if (++game_state.current_screen >= data->screen_count)
    {
        switch(game_state.state)
        {
            case _display_main_menu:
                exit(outOfMemory); // TODO: what is appropriate error
                break;
                
            case _display_quit_screens:
                StatsManager::instance()->Finish();

                hide_cursor();
                animate_ui_fade_out_blocking(true);
                game_state.state = _quit_game;
                break;
                
            default:
                display_main_menu();
                break;
        }
    }
    else
    {
        if (game_state.state == _display_intro_screens && game_state.current_screen == INTRO_SCREEN_TO_START_SONG_ON)
        {
            Music::instance()->RestartIntroMusic();
        }
        
        short pict_resource_id = data->screen_base + game_state.current_screen;
            
        SDL_Surface* tmp = nullptr;
        
        if (pict_resource_id == MAIN_MENU_BASE || pict_resource_id == MAIN_MENU_BASE + 1 || (tmp = get_pict_resource_from_images(pict_resource_id)))
        {
            SDL_FreeSurface(tmp);
            
            game_state.ticks_until_next_screen= data->duration;
            game_state.last_ticks_on_idle= machine_tick_count();
            
            display_screen(data->screen_base);
            
            if (game_state.state == _display_intro_screens)
            {
                if (introduction_sound) {
                    introduction_sound->AskStop();
                    introduction_sound.reset();
                }
                SoundRsrc.Unload();
                if (get_sound_resource_from_images(pict_resource_id, SoundRsrc))
                {
                    _fixed pitch = (shapes_file_is_m1() && game_state.state==_display_intro_screens) ? _m1_high_frequency : _normal_frequency;
                    SoundParameters parameters;
                    parameters.pitch = pitch * 1.f / _normal_frequency;
                    introduction_sound = SoundManager::instance()->PlaySound(SoundRsrc, parameters);
                }
            }
        }
        else
        {
            game_state.ticks_until_next_screen = 0;
            game_state.last_ticks_on_idle = machine_tick_count();
        }
    }
}




void display_screen(short base_pict_id) // TODO: FIX: caller should control whether cursor is shown or hidden; basically we want to push all UI transitions, including advance to next screen, up into app_event_loop's switch (once all in-game transitions are moved to game_event_loop)
{
    SDL_Surface* surface = get_pict_resource_from_images(base_pict_id + game_state.current_screen);
    
    if (surface)
    {
        /*
        stop_ui_fade();
        
        //show_cursor();
        
        animate_ui_fade_out_blocking();
        animate_ui_fade_in_blocking();
        */
        
        // these bookends are awkward
        if (ogl_is_active())
        {
            alephone::Screen::instance()->bound_screen(false); //OGL_Blitter::BoundScreen();
            OGL_ClearScreen();
        }
        
        int32_t w, h;
        MainScreenWindowSize(w, h);
        double scale = (double)h / surface->h;
        int32_t image_w = surface->w * scale;
        
        
        // TODO: this is quite annoying: something downstream (either in render_to_screen or OGL_RenderTexturedRect) is offsetting the image to automagically center it: if we pass non-zero x here, it ends up running off right of screen; we do need to pass the correct scaled w+h though otherwise the image gets stretched horizontally
        SDL_Rect dst_rect = {0, 0, image_w, h};//(w - image_w) / 2
        
        // TBH, it would be nice if we could specify a 'virtual screen' that's either 640x480 or 800x600 and have the math all done automatically
        
        Blitter* blitter = new_Blitter();
        blitter->take_surface(surface);
        blitter->render_to_screen(&dst_rect);
        delete blitter;
        //hide_cursor();

        
        if (ogl_is_active())
        {
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            //OGL_DoFades(dst_rect.x, dst_rect.y, dst_rect.x + dst_rect.w, dst_rect.y + dst_rect.h);
           // OGL_SwapBuffers();
        }

        // TODO: how will fades work now that we're mostly working with GPU textures? 1. How were (clut table-based) 8-bit SW fades tied into SDL rendering? How were 16/24-bit SW fades tied in? How do OGL fades do it?
        
       // assert_fail(current_picture_clut, "");
        //start_interface_fade(_long_cinematic_fade_in);
    }
    else
    {
        next_game_screen();
        return;
    }
}







#define SCROLLING_SPEED (MACHINE_TICKS_PER_SECOND / 20)

static void animate_scrolling_screen(Blitter* blitter, bool is_slow_text_scroll)
{
    // Find out in which direction to scroll // TODO: legacy importer should calculate direction, speed, etc
    int picture_width       = blitter->width();
    int picture_height      = blitter->height();
    int screen_width        = 640;
    int screen_height       = 480;
    bool scroll_horizontal  = picture_width > screen_width;
    bool scroll_vertical    = picture_height > screen_height;

    if (scroll_horizontal || scroll_vertical)
    {

        // Flush events
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

        // Prepare source and destination rectangles
        SDL_Rect src_rect = {0, 0, scroll_horizontal ? screen_width : picture_width, scroll_vertical ? screen_height : picture_height};
        SDL_Rect dst_rect = {0, 0, screen_width, screen_height};

        // Scroll loop
        bool done = false, aborted = false;
        uint64_t start_tick = machine_tick_count();
        
        // this function runs its own event loop, which isn't ideal
        do
        {
            int32_t delta = (int32_t)((machine_tick_count() - start_tick) / (is_slow_text_scroll ? (2 * SCROLLING_SPEED) : SCROLLING_SPEED));
            if (scroll_horizontal && delta > picture_width - screen_width)
            {
                delta = picture_width - screen_width;
                done = true;
            }
            if (scroll_vertical && delta > picture_height - screen_height)
            {
                delta = picture_height - screen_height;
                done = true;
            }

            // Blit part of picture
            src_rect.x = scroll_horizontal ? delta : 0;
            src_rect.y = scroll_vertical ? delta : 0;
            
            blitter->render_to_screen(&dst_rect, &src_rect);
            MainScreenSwap();
            
            // Give system time
            global_idle_proc();
            yield();

            // Check for events to abort
            SDL_Event event;
            if (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_KEYDOWN:
                    case SDL_CONTROLLERBUTTONDOWN:
                        aborted = true;
                        break;
                }
            }
        }
        while (!done && !aborted);
    }
}




// Note that this is modal. This sucks... // TODO: shouldn't need to be: it just needs the scrolling to be performed in main event loop updates
void try_and_display_chapter_screen(short level, bool interface_table_is_valid, bool is_slow_text_scroll)
{
    if (Movie::instance()->IsRecording() || !shell_options.replay_directory.empty())
        return;
    
    // hide_cursor(); // TODO: need to push all these up into main_event_loop.cpp's switch
    
    short pict_resource_number = get_screen_data(_display_chapter_heading)->screen_base + level;
    
    SDL_Surface* surface = get_pict_resource_from_map(pict_resource_number);

    if (surface)
    {
        Blitter* blitter = new_Blitter();
        blitter->take_surface(surface);
        
        short existing_state    = game_state.state; // this smells
        game_state.state        = _display_chapter_heading;

        Music::instance()->StopInGameMusic();
        SoundManager::instance()->StopAllSounds();
        
        /* This will NOT work if the initial level entered has a chapter screen, which is why */
        /*  we perform this check. (The interface_color_table is not valid...) */
        if (interface_table_is_valid)
        {
        //    animate_ui_fade_blocking(_cinematic_fade_out, interface_color_table);
        //    clear_screen();
        }

        change_screen_mode(_screentype_chapter);
        
        // Fade the screen to black
        assert_fail(!current_picture_clut, "");
        current_picture_clut        = calculate_picture_clut();
        current_picture_clut_depth  = interface_bit_depth;
        
        LoadedResource SoundRsrc;

        if (interface_bit_depth == 8) { assert_world_color_table(current_picture_clut, nullptr); } // slam the entire clut to black, now.
        
        // set (but don't start) the fade-in, so screen is black
       // animate_ui_fade_blocking(_start_cinematic_fade_in, current_picture_clut);
        
        blitter->render_to_screen();

        std::shared_ptr<SoundPlayer> soundPlayer;
        if (get_sound_resource_from_map(pict_resource_number,SoundRsrc))
        {
            _fixed pitch = (shapes_file_is_m1() && level == 101) ? _m1_high_frequency : _normal_frequency;
            SoundParameters parameters;
            parameters.pitch = pitch * 1.f / _normal_frequency;
            soundPlayer = SoundManager::instance()->PlaySound(SoundRsrc, parameters);
        }
        
        /* Fade in.... */
        assert_fail(current_picture_clut, "");
        animate_ui_fade_blocking(_long_cinematic_fade_in, current_picture_clut);
        
        animate_scrolling_screen(blitter, is_slow_text_scroll); // this is no-op if image is 640x480

        wait_for_click_or_keypress(is_slow_text_scroll ? -1 : 10 * MACHINE_TICKS_PER_SECOND);
        
        //animate_ui_fade_out_blocking(false);
        
        if (soundPlayer) soundPlayer->AskStop();
        
        game_state.state = existing_state;
    }
}



/* EES: draw_intro_screen was used throughout the display_SCREEN functions to transfer an intro/main menu/chapter screen SDL_Surface created from a scenario file 'pict' resource, via LP's stupid _port_wankery to draw_surface, and from there eventually arriving in Image_Blitter for transfer to GPU texture so SDL/OGL can at long last throw a simple picture onto the user's damned screen.
 
 Now display_SCREEN functions use Blitter->take_/borrow_surface and Blitter->render_to_screen to transfer chapter screen Surface to, eliminating a lot of AO's indirection. However, Blitter_OGL::render_to_screen currently lacks the extra OGL calls below so more thought is needed.
 
 The end goal:

 - a single standard 2D-drawing API (Canvas)

 - a single standard Surface-to-GPU-Texture API (Blitter)
 
 - a single standard fader API (Fader)
 
 Canvas and Blitter are getting there.
 
 Fader is to be started: existing fading logic is all very entangled and needs to be separated into 3 new Fader subclasses for performing 8-bit SW, 16/32-bit SW, and OGL fades behind a common API.
 
 The UI code is WIP and the code which composites the in-game screen is awful.
 
 void draw_intro_screen(void)
 {
     if (fade_blacked_screen())
         return;
     
     SDL_Rect src_rect = { 0, 0, Intro_Buffer->w, Intro_Buffer->h };
     SDL_Rect dst_rect = { 0, 0, src_rect.w, src_rect.h};
     
 #ifdef HAVE_OPENGL
     if (ogl_is_active()) {
         if (intro_buffer_changed) {
             SDL_SetSurfaceBlendMode(Intro_Buffer, SDL_BLENDMODE_NONE); <=============
             Intro_Blitter.Load(*Intro_Buffer);
             intro_buffer_changed = false;
         }
         OGL_Blitter::BoundScreen(); <=============
         OGL_ClearScreen(); <=============
         Intro_Blitter.Draw(dst_rect);
         
         glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); <=============
         glEnableClientState(GL_VERTEX_ARRAY); <=============
         glEnableClientState(GL_TEXTURE_COORD_ARRAY); <=============
         OGL_DoFades(dst_rect.x, dst_rect.y, dst_rect.x + dst_rect.w, dst_rect.y + dst_rect.h); <=============
         OGL_SwapBuffers(); <=============
     } else
 #endif
     {
         SDL_Surface *s = Intro_Buffer;
         if (!using_default_gamma) {
             apply_gamma(Intro_Buffer, Intro_Buffer_corrected);
             SDL_SetSurfaceBlendMode(Intro_Buffer_corrected, SDL_BLENDMODE_NONE);
             s = Intro_Buffer_corrected;
         }
         DrawSurface(s, dst_rect, src_rect);
         intro_buffer_changed = false;
     }
 }


 
 */


