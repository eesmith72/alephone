/*
 choose_file_dialogs_os.cpp
 
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

#include "choose_file_dialogs_os.hpp"


// non-optional for now
//#ifdef HAVE_NFD
#include "nfd.h"
#include <SDL2/SDL_syswm.h>
//#endif

// zip really needs to be non-optional too, though that's a separate discussion
#ifdef HAVE_ZZIP
#include "SDL_rwops_zzip.h"
#endif

#include "preferences.h" // environment_preferences
#include "screen.h" // MainScreenWindow
#include "sdl_widgets.h" // used in show_confirm_overwrite_file_dialog


static /*const*/ std::map<filetype_t, std::vector<nfdu8filteritem_t>> typecode_filters = { // nfdsavedialogu8args_t doesn't like const
    {_typecode_map, { {"Map file",        "sceA"}        }},
    {_typecode_savegame, { {"Saved game file", "sgaA"}        }},
    {_typecode_film,     { {"Recording file",  "filA"}        }},
    {_typecode_physics,  { {"Physics file",    "phyA"}        }},
    {_typecode_shapes,   { {"Shapes file",     "shpA"}        }},
    {_typecode_sounds,   { {"Sounds file",     "sndA"}        }},
    {_typecode_patch,    { {"Patch file",      "ShPa"}        }},
    {_typecode_images,   { {"Images file",     "imgA"}        }},
    {_typecode_music,    { {"Music file",      "aif,ogg,wav"} }},
    {_typecode_movie,    { {"Video file",      "webm"}        }},
};



// from nativefiledialog-extended nfd_sdl2.h
// we copied it because nfd_sdl2.h is including SDL2 headers directly and we would have to adjust the include paths
static bool GetNativeWindowFromSDLWindowForNFD(SDL_Window* sdlWindow, nfdwindowhandle_t* nativeWindow)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(sdlWindow, &info)) {
        return false;
    }
    switch (info.subsystem) {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    case SDL_SYSWM_WINDOWS:
        nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
        nativeWindow->handle = (void*)info.info.win.window;
        return true;
#elif defined(SDL_VIDEO_DRIVER_COCOA)
    case SDL_SYSWM_COCOA:
        nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
        nativeWindow->handle = (void*)info.info.cocoa.window;
        return true;
#elif defined(SDL_VIDEO_DRIVER_X11)
    case SDL_SYSWM_X11:
        nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_X11;
        nativeWindow->handle = (void*)info.info.x11.window;
        return true;
#endif
    default:
        return false;
    }
}


// -----------------------------------------------------------------------------------------
// choose file/directory dialogs


ao_path show_read_directory_dialog_os(const ao_path& start_path)
{
#if defined(_WIN32)
    bool is_full_screen = get_screen_mode()->fullscreen;
    if (is_full_screen) { set_full_screen_enabled(false); }
#endif
    
    // set up dialog params
    std::string path = (start_path.empty() ? get_local_storage_dir() : start_path).generic_u8string();
    nfdpickfolderu8args_t params = {path.c_str()};
    if (GetNativeWindowFromSDLWindowForNFD(MainScreenWindow(), &params.parentWindow))
    {
        // we ignore the "window focus lost + gained" events to prevent pausing the game on "focus lost"
        // otherwise, a user input would be necessary to resume the game
        SDL_EventState(SDL_WINDOWEVENT, SDL_DISABLE);
    }
    
    ao_path result;
    nfdchar_t* outpath; // TODO: not immediately clear, but I'm assuming this is a POSIX-style path? need to confirm it can't be Windows-style backslashes
    if (NFD_PickFolderU8_With(&outpath, &params) == NFD_OKAY)
    {
        result = outpath;
        NFD_FreePathU8(outpath);
    }
    SDL_EventState(SDL_WINDOWEVENT, SDL_ENABLE);
    
#if defined(_WIN32)
    if (is_full_screen) { set_full_screen_enabled(true); }
#endif
    return result;
}


ao_path show_read_file_dialog_os(filetype_t type, const std::string& prompt, const ao_path& start_path)
{
#if defined(_WIN32)
    bool is_full_screen = get_screen_mode()->fullscreen;
    if (is_full_screen) { set_full_screen_enabled(false); }
#endif
    
    // setup dialog params
#ifdef __MACOSX__
    // NFD doesn't append a wildcard filter on mac, so if you set ANY
    // filter here, anything without that extension gets grayed out. So, I
    // guess just accept any files
    std::vector<nfdu8filteritem_t> filters = {};
#else
    auto& filters = typecode_filters[type];
#endif
    std::string path = (start_path.empty() ? get_local_storage_dir() : start_path).generic_u8string();
    nfdopendialogu8args_t params = {
        filters.data(),
        static_cast<nfdfiltersize_t>(filters.size()),
        path.c_str(),
    };
    
    if (GetNativeWindowFromSDLWindowForNFD(MainScreenWindow(), &params.parentWindow))
    {
        // we ignore the "window focus lost + gained" events to prevent pausing the game on "focus lost"
        // otherwise, a user input would be necessary to resume the game
        SDL_EventState(SDL_WINDOWEVENT, SDL_DISABLE);
    }
    ao_path result;
    nfdchar_t* outpath;
    if (NFD_OpenDialogU8_With(&outpath, &params) == NFD_OKAY)
    {
        result = std::string(outpath);
        NFD_FreePathU8(outpath);
    }
    SDL_EventState(SDL_WINDOWEVENT, SDL_ENABLE);

#ifdef __WIN32__
    if (is_full_screen) { set_full_screen_enabled(true); }
#endif
    return result;
}


ao_path show_write_file_dialog_os(filetype_t file_type, const std::string& prompt,
                                    const ao_path& start_path, const std::string& default_filename)
{
#if defined(_WIN32)
    bool is_full_screen = get_screen_mode()->fullscreen;
    if (is_full_screen) { set_full_screen_enabled(false); }
#endif
    // TODO: if start_path's a file, delete last path component; also check that dir exists; fall back to local data dir if start_path not given/doesn't exist
    std::string path = (start_path.empty() ? get_local_storage_dir() : start_path).generic_u8string();
    nfdsavedialogu8args_t params = {
        typecode_filters[file_type].data(),
        static_cast<nfdfiltersize_t>(typecode_filters[file_type].size()),
        path.c_str(),
        default_filename.c_str()
    };
    if (GetNativeWindowFromSDLWindowForNFD(MainScreenWindow(), &params.parentWindow))
    {
        // we ignore the "window focus lost + gained" events to prevent pausing the game on "focus lost"
        // otherwise, a user input would be necessary to resume the game
        SDL_EventState(SDL_WINDOWEVENT, SDL_DISABLE);
    }
    
    ao_path result;
    nfdchar_t* outpath;
    if (NFD_SaveDialogU8_With(&outpath, &params) == NFD_OKAY)
    {
        result = outpath;
        NFD_FreePathU8(outpath);
    }
    SDL_EventState(SDL_WINDOWEVENT, SDL_ENABLE);
    
#if defined(_WIN32)
    if (is_full_screen) { set_full_screen_enabled(true); }
#endif
    
    // TODO: ensure the file has the correct extension, e.g. ".filA" for _typecode_film; typecode_filters
    
    return result;
}



// -----------------------------------------------------------------------------------------


bool show_confirm_overwrite_file_dialog(const std::string& filename)
{
    std::string text = "“";
    text += environment_preferences.hide_extensions ? hide_ao_filename_extension(filename) : filename;
    text += "” already exists.";

    dialog d;
    vertical_placer *placer = new vertical_placer;
    placer->dual_add(new w_static_text(text.c_str()), d);
    placer->dual_add(new w_static_text("Ok to overwrite?"), d);
    placer->add(new w_spacer(), true);

    horizontal_placer *button_placer = new horizontal_placer;
    w_button *default_button = new w_button("YES", dialog_ok, &d);
    button_placer->dual_add(default_button, d);
    button_placer->dual_add(new w_button("NO", dialog_cancel, &d), d);

    placer->add(button_placer, true);

    d.activate_widget(default_button);

    d.set_widget_placer(placer);

    return d.run() == 0;
}

