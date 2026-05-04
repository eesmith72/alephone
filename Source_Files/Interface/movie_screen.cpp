

#include "movie_screen.hpp"


// TODO: delete unused includes

#ifdef HAVE_LIBYUV
#include <libyuv/convert.h>
#include <libyuv/scale.h>
#endif

#include "choose_file_dialogs_os.hpp"

//#include "Canvas_SDL.hpp"

#include "map.h"
#include "shell.h"
#include "interface.hpp"
#include "player.h"
#include "network.h"
#include "screen_drawing.h"
#include "SoundManager.h"
#include "fades.h"
//#include "hud_manager.h"
#include "Music.h"
#include "images.h"
#include "Screen.hpp"
#include "vbl.h"
#include "preferences.hpp"
#include "DataFile.hpp"
#include "lua_script.h" // PostIdle
#include "XML_LevelScript.h"
#include "FilmExporter.h"
#include "QuickSave.h"
#include "Plugins.h"
#include "Statistics.h"
#include "shell_options.h"
#include "OpenALManager.h"

#include "InfoTree.h"


#include "render.h"
#include "OGL_Render.h"
#include "ImageBlitter.hpp"
#include "alephversion.h"

// To tell it to stop playing, and also to run the end-game script
#include "XML_LevelScript.h"

// ZZZ: should the function that uses these (join_resumed_coop_game()) go elsewhere?
#include "wad.h"
#include "map_wad.h"

#include "motion_sensor.hpp" // for reset_motion_sensor() // this is also called in map_wad.cpp and, all over the place, really

#include "lua_hud_script.h"

#include "Canvas.hpp"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "network_dialog_widgets_sdl.h"



/*
 *  Show movie
 */

static void audio_samples_decoder_callback(plm_t* mpeg, plm_samples_t* samples, void* userdata)
{
    auto& [mutex, audio_buffer] = *static_cast<std::tuple<SDL_mutex*, std::deque<float>*>*>(userdata);

    if (SDL_LockMutex(mutex) == 0)
    {
        audio_buffer->insert(audio_buffer->end(), samples->interleaved, samples->interleaved + samples->count * 2); //always work on stereo in interleaved mode
        SDL_UnlockMutex(mutex);
    }
}

static int audio_player_callback(uint8_t* data, uint32_t length, void* userdata)
{
    auto& [mutex, audio_buffer] = *static_cast<std::tuple<SDL_mutex*, std::deque<float>*>*>(userdata);

    if (audio_buffer->size() && SDL_LockMutex(mutex) == 0)
    {
        const auto samples_length = std::min(audio_buffer->size(), (size_t)length / sizeof(float));
        auto data_out = reinterpret_cast<float*>(data);

        for (auto i = 0; i < samples_length; i++)
            data_out[i] = (*audio_buffer)[i];

        audio_buffer->erase(audio_buffer->begin(), audio_buffer->begin() + samples_length);
        SDL_UnlockMutex(mutex);
        return (int32_t)(samples_length * sizeof(float));
    }

    return 0;
}

static void video_frame_decoder_callback(plm_t* mpeg, plm_frame_t* frame, void* userdata)
{
    auto& [dimensions, surface, buffer, out_new_frame] = *static_cast<std::tuple<SDL_Rect, SDL_Surface*, std::vector<uint8>*, bool*>*>(userdata);

#ifdef HAVE_LIBYUV
    libyuv::I420Scale(frame->y.data, frame->y.width, frame->cb.data, frame->cb.width, frame->cr.data, frame->cr.width, frame->width, frame->height,
        buffer[0].data(), dimensions.w, buffer[1].data(), dimensions.w / 2, buffer[2].data(), dimensions.w / 2, dimensions.w, dimensions.h, libyuv::FilterMode::kFilterNone);

#ifdef ALEPHONE_LITTLE_ENDIAN
        libyuv::I420ToABGR(buffer[0].data(), dimensions.w, buffer[1].data(), dimensions.w / 2, buffer[2].data(), dimensions.w / 2, (uint8_t*)surface->pixels, surface->pitch, dimensions.w, dimensions.h);
#else
        libyuv::I420ToRGBA(buffer[0].data(), dimensions.w, buffer[1].data(), dimensions.w / 2, buffer[2].data(), dimensions.w / 2, (uint8_t*)surface->pixels, surface->pitch, dimensions.w, dimensions.h);
#endif
#else
    plm_frame_to_rgba(frame, (uint8_t*)surface->pixels, surface->pitch);
#endif

    (*out_new_frame) = true;
}




void show_movie(short level_number)
{
    if (FilmExporter::instance()->IsExporting() || !shell_options.replay_directory.empty()) return;
    
    ao_path File = get_movie_path_for_level(level_number);
    if (File.empty() && level_number == 0)
    {
        File = find_file_at_subpath(get_string(STRID(strFILENAMES, filenameMOVIE)));
    }
    if (File.empty()) return;

    //change_screen_mode(_screentype_chapter);

    sound_manager.SetStatus(false);
    auto plm_context = plm_create_with_filename(File.c_str());
    if (!plm_context)
    {
        sound_manager.SetStatus(true); // TODO: it is unclear if plm_create_with_filename interacts with SoundManager; if it doesn't, just make the initial SetStatus(false) after this conditional
        return;
    }

#ifdef HAVE_LIBYUV
    SDL_Rect dst_rect = { 0, 0, 640, 480 };
#else
    SDL_Rect dst_rect = { 0, 0, plm_context->video_decoder->width, plm_context->video_decoder->height };
#endif

    SDL_Surface* vframe = CreateSDLSurface(dst_rect.w, dst_rect.h);

    bool got_new_frame = false;
    std::vector<uint8> frame_buffers[3];
    frame_buffers[0].resize(dst_rect.w * dst_rect.h);
    frame_buffers[1].resize(dst_rect.w * dst_rect.h / 2);
    frame_buffers[2].resize(dst_rect.w * dst_rect.h / 2);

    auto callback_video_userdata = std::make_tuple(dst_rect, vframe, frame_buffers, &got_new_frame);
    plm_set_video_decode_callback(plm_context, video_frame_decoder_callback, &callback_video_userdata);

    SDL_mutex* audio_mutex = nullptr;
    std::deque<float> shared_audio_buffer;
    std::tuple<SDL_mutex*, std::deque<float>*> callback_audio_userdata;
    bool audio_playback = OpenALManager::Get();

    if (audio_playback)
    {
        if (plm_get_num_audio_streams(plm_context) == 0)
        {
            plm_probe(plm_context, 5000 * 1024); //on some video formats like VCDs, number of audio streams is not present in header
            audio_playback = plm_get_num_audio_streams(plm_context) > 0;
        }

        if (audio_playback)
        {
            audio_mutex = SDL_CreateMutex();
            callback_audio_userdata = std::make_tuple(audio_mutex, &shared_audio_buffer);
            plm_set_audio_lead_time(plm_context, 0.1); // we don't set as (sample size / sample rate) as recommended but set a fixed value to give a little bit more time to be sure audio does not underrun
            plm_set_audio_decode_callback(plm_context, audio_samples_decoder_callback, &callback_audio_userdata);
        }
    }

    plm_set_audio_enabled(plm_context, audio_playback);

    clear_screen(false);

    ImageBlitter* movie_blitter = new ImageBlitter();

    if (audio_playback) OpenALManager::Get()->Start();

    bool done = false;
    auto last_rendered_time = machine_tick_count();
    std::shared_ptr<StreamPlayer> movie_audio_player;
    const auto framerate = plm_get_framerate(plm_context);

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_KEYDOWN:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_CONTROLLERBUTTONDOWN:
                done = true;
                break;
            default:
                break;
            }
        }

        auto current_time = machine_tick_count();
        auto elapsed_time = current_time - last_rendered_time;
        last_rendered_time = current_time;
        plm_decode(plm_context, std::min(elapsed_time / 1000.0, 1.0 / framerate));
            
        if (audio_playback && (!movie_audio_player || !movie_audio_player->IsActive()))
        {
            movie_audio_player = OpenALManager::Get()->PlayStream(audio_player_callback, plm_get_samplerate(plm_context), true, AudioFormat::_32_float, &callback_audio_userdata);
        }

        if (got_new_frame)
        {
            movie_blitter->borrow_surface(vframe);
            movie_blitter->render_to_screen(&dst_rect);
            main_screen.swap();
            got_new_frame = false;
        }
        else if (plm_has_ended(plm_context))
        {
            done = true;
        }
        else
        {
            sleep_for_machine_ticks(1);
        }
    }

    if (audio_playback)
    {
        while (movie_audio_player && movie_audio_player->IsActive()) {
            sleep_for_machine_ticks(MACHINE_TICKS_PER_SECOND / 100);
        }

        OpenALManager::Get()->Stop();
        SDL_DestroyMutex(audio_mutex);
    }
    
    delete movie_blitter;
    SDL_FreeSurface(vframe);
    plm_destroy(plm_context);
    
    sound_manager.SetStatus(true);
}

