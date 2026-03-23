

#include "chapter_screens.hpp"

#include "shapes.h" // shapes_file_is_m1
#include "fades.h"
#include "SoundManager.h"
#include "Music.h"
#include "images.h" // get_sound_resource_from_images
#include "screen.h" // clear_screen
//#include "main_menu.hpp" // display_main_menu (temporary till these functions are unknotted)
#include "mouse.h" // hide_cursor
#include "XML_LevelScript.h" // EndScreenIndex, NumEndScreens
#include "Statistics.h" // StatsManager, used in display_shutdown_screen
#include "screen_shared.h" // interface_bit_depth
#include "shell_options.h"

#include "image_blitter.hpp"

#include "MovieExporter.h"

#include "main_event_loop.hpp" // game_state (which should eventually move out of the functions below and into main_event_loop.cpp)


//************************************************************************************************
// sounds/music


#define M1_STARTUP_SOUND_ID (1240)

static std::shared_ptr<SoundPlayer> introduction_sound = nullptr;

static LoadedResource SoundRsrc;

static void play_optional_sound_resource(int32_t resource_id, _fixed pitch = _normal_frequency)
{
    if (introduction_sound)
    {
        introduction_sound->AskStop();
        introduction_sound.reset();
    }
    SoundRsrc.Unload();
    if (get_sound_resource_from_images(resource_id, SoundRsrc))
    {
        //_fixed pitch = (shapes_file_is_m1() && get_app_state() == app_state_t::startup_screen) ? _m1_high_frequency : _normal_frequency;
        SoundParameters parameters;
        parameters.pitch = pitch * 1.f / _normal_frequency;
        introduction_sound = SoundManager::instance()->PlaySound(SoundRsrc, parameters);
    }
}



typedef void (*start_audio_proc)(int32_t screen_id);


void play_m1_startup_sound(int32_t screen_id)
{
    switch (screen_id)
    {
        case M1_STARTUP_SCREEN_BASE:
        case M1_EPILOGUE_SCREEN_BASE:
            play_optional_sound_resource(M1_STARTUP_SOUND_ID);
            break;
            
        case 1114:
        case M1_EPILOGUE_SCREEN_BASE + 1:
            play_optional_sound_resource(M1_STARTUP_SOUND_ID, _m1_high_frequency);
            break;
            
        default:
        {}
    }
}


void play_m2_chapter_sound(int32_t screen_id)
{
    play_optional_sound_resource(screen_id);
}


void play_m2_startup_music(int32_t screen_id)
{
    Music::instance()->RestartIntroMusic();
}


void play_m2_epilogue_music(int32_t screen_id)
{
    Music::instance()->RestartIntroMusic();
    auto ticks = machine_tick_count();
    do { Music::instance()->Idle(); } while (machine_tick_count() - ticks < 10);
}


//************************************************************************************************
// screens config

// this could be inlined in display_ functions but going to leave it as table for now; TODO: how practical to move screen-specific behaviors, e.g. starting music, into table?

struct screen_data_t
{
    int32_t base_id;
    int32_t screen_count;
    uint32_t duration;
    bool slow_scroll;
    start_audio_proc sound;
};

struct screen_t
{
    app_state_t state;
    screen_data_t m1;
    screen_data_t m2;
};


#define STARTUP_SCREEN_DURATION     (215     * MACHINE_TICKS_PER_SECOND / TICKS_PER_SECOND) // fudge to align with sound
#define PROLOGUE_DURATION           (10      * MACHINE_TICKS_PER_SECOND)
#define CREDIT_SCREEN_DURATION      (15 * 60 * MACHINE_TICKS_PER_SECOND)
#define CHAPTER_SCREEN_DURATION     (7       * MACHINE_TICKS_PER_SECOND)


static const std::array<screen_t, 8> screens_std = {(screen_t)
    {app_state_t::startup_screen,   {M1_STARTUP_SCREEN_BASE,    4,   STARTUP_SCREEN_DURATION,   false,  play_m1_startup_sound},
                                    {M2_STARTUP_SCREEN_BASE,    3,   STARTUP_SCREEN_DURATION,   false,  play_m2_startup_music}},
    
    {app_state_t::prologue_screen,  {PROLOGUE_SCREEN_BASE,      1,   PROLOGUE_DURATION,         true,   nullptr},
                                    {PROLOGUE_SCREEN_BASE,      1,   PROLOGUE_DURATION,         true,   nullptr}},
    
    {app_state_t::chapter_screen,   {M1_CHAPTER_SCREEN_BASE,    1,   CHAPTER_SCREEN_DURATION,   false, play_m1_startup_sound},
                                    {M2_CHAPTER_SCREEN_BASE,    1,   CHAPTER_SCREEN_DURATION,   false, play_m2_chapter_sound}},
    
    {app_state_t::epilogue_screen,  {M1_EPILOGUE_SCREEN_BASE,   2,   INFINITE_TIME_DELAY,       true,   play_m1_startup_sound},
                                    {M2_EPILOGUE_SCREEN_BASE,   1,   INFINITE_TIME_DELAY,       true,   play_m2_epilogue_music}},
    
    {app_state_t::credit_screen,    {M1_CREDIT_SCREEN_BASE,     1,   CREDIT_SCREEN_DURATION,    false,  nullptr},
                                    {M2_CREDIT_SCREEN_BASE,     7,   CREDIT_SCREEN_DURATION,    false,  nullptr}},
    
    {app_state_t::shutdown_screen,  {SHUTDOWN_SCREEN_BASE,      1,   INFINITE_TIME_DELAY,       false,  nullptr},
                                    {SHUTDOWN_SCREEN_BASE,      1,   INFINITE_TIME_DELAY,       false,  nullptr}},
};


// MML allows scenarios to mess around with base (which is stupid) and number (which is unnecessary), so support for backwards compatibility
screen_data_t epilogue_screen_data;


const screen_data_t* screen_data;

static int32_t current_screen_id = 0;

SDL_Surface* screen_surface = nullptr;


const screen_data_t* get_data_for_screen_type(app_state_t screen_type)
{
    for (const auto& screen : screens_std)
    {
        if (screen_type == screen.state)
        {
            return shapes_file_is_m1() ? &screen.m1 : &screen.m2;
        }
    }
    throw_bug_report_f("invalid screen type: %d", screen_type);
}



//************************************************************************************************
// load and display

Blitter* screen_blitter = nullptr;


ao_err load_screen(app_state_t screen_type)
{
    screen_data = get_data_for_screen_type(screen_type);
    
    // scenarios may fiddle with epilogue screen in MML (which is annoyingly half-assed); we support for backwards compatibility
    if (screen_type == app_state_t::epilogue_screen)
    {
        epilogue_screen_data = *screen_data;
        get_epilogue_screen_base_id_and_count(epilogue_screen_data.base_id, epilogue_screen_data.screen_count);
        screen_data = &epilogue_screen_data;
    }
    
    current_screen_id = screen_data->base_id;
    
    // EES: how confident am I that all pict IDs are unique across scenario? not entirely, so hedging bets here for now // TODO: ideally this can be folded into a single `get_pict_resource_from_scenario(resource_id)` in future
    if (screen_type == app_state_t::chapter_screen)
    {
        screen_surface = get_pict_resource_from_map(current_screen_id);
        if (!screen_surface) { screen_surface = get_pict_resource_from_images(current_screen_id); }
    }
    else
    {
        screen_surface = get_pict_resource_from_images(current_screen_id);
        if (!screen_surface) { screen_surface = get_pict_resource_from_map(current_screen_id); }
    }
    
    return screen_surface ? no_err : STRID(strERRORS, missingFile);
}


uint32_t present_screen()
{
    // TODO: fades where?
    
    // EES: these should be okay here (originally chapter screen)
    Music::instance()->StopInGameMusic();
    SoundManager::instance()->StopAllSounds();
    
    /*
     stop_ui_fade();
     animate_ui_fade_out_blocking();
     animate_ui_fade_in_blocking();
     */
    
    // these bookends are awkward
    if (ogl_is_active())
    {
        alephone::Screen::instance()->bound_screen(false); //OGL_Blitter::BoundScreen();
        OGL_ClearScreen();
    }
    
    // TBH, it would be nice if we could specify a 'virtual screen' that's either 640x480, 800x600, or native and have all the math done automatically
    
    int32_t w, h;
    MainScreenWindowSize(w, h);
    int32_t sw = screen_surface->w * h / screen_surface->h;
    
    SDL_Rect dst_rect = {0, 0, sw, h};// x = ((w - sw) / 2) // TODO: FIX: something downstream (either in render_to_screen or OGL_RenderTexturedRect) is offsetting the image to automagically center it: if we pass non-zero x here, it ends up running off right of screen; we do need to pass the correct scaled w+h though otherwise the image gets stretched horizontally
    
    SDL_Rect src_rect = {0, 0, 640, 480};
    

    if (screen_blitter) { delete screen_blitter; }
    
    screen_blitter = new_Blitter();
    screen_blitter->render_to_screen(&dst_rect, &src_rect);
    MainScreenSwap();
    
    if (screen_data->sound) { screen_data->sound(current_screen_id); }
    
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
    
    return screen_data->duration;
}



ao_err advance_to_next_screen() // returns no_err/not found // TODO: FIX: need to implement this (including appropriate error codes)
{
    TODO("implement");
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
            update_audio_on_idle();
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




// TODO: this should FOAD, as soon as its functionality is fully relocated
// Note that this is modal. This sucks... // TODO: shouldn't need to be: it just needs the scrolling to be performed in main event loop updates
void try_and_display_chapter_screen(short level, bool interface_table_is_valid, bool is_slow_text_scroll)
{
    if (MovieExporter::instance()->IsRecording() || !shell_options.replay_directory.empty())
        return;
    
    // hide_cursor(); // TODO: need to push all these up into main_event_loop.cpp's switch
    
    short pict_resource_number = 0;//get_screen_data(app_state_t::chapter_screen)->base_id + level;
    
    SDL_Surface* surface = get_pict_resource_from_map(pict_resource_number);

    if (surface)
    {
        Blitter* blitter = new_Blitter();
        blitter->take_surface(surface);
        
        app_state_t existing_state    = get_app_state(); // this smells
        set_app_state(app_state_t::chapter_screen);

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
        
        set_app_state(existing_state);
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


