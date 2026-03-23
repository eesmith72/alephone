/*
 csalerts_sdl.cpp - Game alerts and debugging support
 
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
#include "sdl_widgets.h"

#include <SDL2/SDL_messagebox.h>


// -----------------------------------------------------------------------------------------
// user alerts (Mac/Win dialogs; Linux shell)


// TODO: can we use SDL_MessageBox on all platforms?
/*
#ifndef __MACOSX__
void notify_user_os_default(std::string& message, alert_level_t severity)
{
#if defined(__WIN32__)
    UINT type;
    if (severity == alert_level_t::info) {
        type = MB_ICONWARNING|MB_OK;
    } else {
        type = MB_ICONERROR|MB_OK;
    }
    MessageBoxW(NULL, utf8_to_wide(message).c_str(), severity == alert_level_t::info ? L"Warning" : L"Error", type);
#else
    fprintf(stderr, "%s: %s\n", severity == alert_level_t::info ? "INFO" : "FATAL", message);
#endif
}
*/


// KISS
void display_simple_dialog(ao_err code, const std::string& message)
{
    std::string title;
    SDL_MessageBoxFlags box_type;
    
    switch (get_alert_level_for_code(code))
    {
        case alert_level_t::info:
            title = "Information";
            box_type = SDL_MESSAGEBOX_INFORMATION;
            break;
            
        case alert_level_t::error:
            title = "Warning";
            box_type = SDL_MESSAGEBOX_WARNING;
            break;
            
        case alert_level_t::fatal:
            title = "Fatal Error";
            box_type = SDL_MESSAGEBOX_ERROR;
            break;
            
        default:
            title = "Bug"; // unknown level
            box_type = SDL_MESSAGEBOX_WARNING;
            break;
    }
    
    SDL_ShowSimpleMessageBox(box_type, title.c_str(), message.c_str(), NULL); // TODO: this doesn't allow 'Quit' button; need to use MessageBox+Data for that
}


const std::string format_user_alert_message(ao_err code, const std::string &extra_message, const string_vars_t &vars)
{
    std::string result;
    if (code)
    {
        result = get_string(code, vars);
        if (result.empty()) { result = "A bug occurred: " + std::to_string(code); }
        if (!extra_message.empty()) result += "\n";
    }
    
    return result + expand_string_vars(extra_message, vars);
}


// -----------------------------------------------------------------------------------------
// default procs for user_alert; display to simple dialog or print to stderr


void display_simple_dialog_notification(ao_err code, const std::string& extra_message, const string_vars_t vars)
{
    display_simple_dialog(code, format_user_alert_message(code, extra_message, vars));
}


void write_to_stderr_notification(ao_err code, const std::string& extra_message, const string_vars_t vars)
{
    std::string level;
    switch (get_alert_level_for_code(code))
    {
        case alert_level_t::info:
            level = "INFO  ";
            break;
            
        case alert_level_t::error:
            level = "ERROR ";
            break;
            
        case alert_level_t::fatal:
            level = "FATAL ";
            break;
            
        default:
            level = "?BUG? "; // unknown level
            break;
    }
    
    std::cerr << level;
    std::cerr << "(" << std::hex << code << std::dec << "): ";
    std::cerr << get_string(code, vars) << " // ";
    if (!extra_message.empty()) { std::cerr << expand_string_vars(extra_message, vars) << " "; }
    std::cerr << "[" << std::to_string((int16_t)(code >> 16)) << "|" << std::to_string((int16_t)code) << "]\n";
    std::cerr << "\n";
}


/*
 TODO:
 - `create_notification_filter_proc` constructor that takes std::vector<{resource_id,notify_user_proc_t}> and returns a closure that dispatches to each proc as specified, so there's an easy, general-purpose way to special-case certain error ranges (strDEBUG, stdERRORS, stdNETWORK_ERRORS), e.g. rerouting debug messages to print_to_screen in DEBUG builds and be suppressed in release
 - `create_notification_group_proc` constructor that takes std::vector<notify_user_proc_t> and returns a closure that dispatches to all procs, e.g. to show in dialog AND write to stderr
 - `ignore_notification_proc` proc that does nothing
 */


// -----------------------------------------------------------------------------------------
// The callback hook for notify_user(). Once all the SDL and MML stuff is active, they can install a new proc here to use their fancy w_widget- and theme-based dialogs.


// The simplest, lowest-level display that should always work, including during initialize application and normal/exception-triggered shutdown.
#ifdef A1_NETWORK_STANDALONE_HUB
static notify_user_proc_t notify_user_default_proc = write_to_stderr_notification;
static notify_user_proc_t notify_user_active_proc  = write_to_stderr_notification;
#else
static notify_user_proc_t notify_user_default_proc = display_simple_dialog_notification; // the proc to reset to
static notify_user_proc_t notify_user_active_proc  = display_simple_dialog_notification; // the proc that's currently being used
#endif


// TODO: insert these calls at appropriate points in AO initialize_application and shutdown_application
void set_notify_user_proc(notify_user_proc_t proc)
{
    notify_user_active_proc = proc;
}


void reset_notify_user_proc()
{
    notify_user_active_proc = notify_user_default_proc;
}


// TODO: most notify_user calls don't have proper error codes yet (best to put their strings into strERRORS/etc resource)

// display a message to the user
void notify_user(ao_err code, const std::string& extra_message, string_vars_t vars)
{
    notify_user_active_proc(code, extra_message, vars);
    /*
    if (get_alert_level_for_code(code) == alert_level_t::fatal)
    {
        // TODO: what is the appropriate shutdown call to make, and exactly who should make it (there are pros AND cons to doing it automatically here in `alert user`)? vhalt-like or halt to bail immediately? Or throw a AOFatalErrorOccurred exception which will propagate till it's caught in main.cpp? (Would be inclined to use an exception, unless there's a good reason to bail harder, caveat AO has a number of try blocks that call notify_user in catch, so those need amended or the damn thing will throw up multiple dialogs instead of one-and-done. Probably a good idea if fatal errors are all propagated as exceptions.)
    }
     */
}


// -----------------------------------------------------------------------------------------


#if defined(__MACOSX__)

// defined in csalerts.mm

#elif defined(__WIN32__)


// callback to set starting location for Win32 "choose scenario" dialog
static int CALLBACK scenario_chooser_callback(HWND hwnd, UINT msg, LPARAM lparam, LPARAM lpdata)
{
    WCHAR wcwd[MAX_PATH];
    switch (msg)
    {
        case BFFM_INITIALIZED:
            if (GetCurrentDirectoryW(MAX_PATH, wcwd))
            {
                SendMessageW(hwnd, BFFM_SETEXPANDED, TRUE, (LPARAM)wcwd);
                SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, (LPARAM)wcwd);
            }
    }
    return 0;
}


std::string display_load_scenario_dialog() // TODO: does this mean AO-Linux _can't_ display a dialog (i.e. cli only)?
{
    std::string chosen_dir;
    BROWSEINFOW bi = { 0 };
    wchar_t path[MAX_PATH];
    bi.lpszTitle = L"Select a scenario to play:";
    bi.pszDisplayName = path;
    bi.lpfn = scenario_chooser_callback;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | 0x00000200; // no "New Folder" button
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        SHGetPathFromIDListW(pidl, path);
        const int chars_written = WideCharToMultiByte(CP_UTF8, 0, path, -1, chosen_dir, 256, NULL, NULL);
        LPMALLOC pMalloc = NULL;
        SHGetMalloc(&pMalloc);
        pMalloc->Free(pidl);
        pMalloc->Release();
        return chars_written > 0;
    }
}


void open_url_in_browser(std::string& url)
{
    log_note_f("open_url_in_browser: %s\n", url.c_str());

    ShellExecuteW(NULL, L"open", utf8_to_wide(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
}


#else /* Linux, etc */


std::string display_load_scenario_dialog()
{
    return ""; // TODO: does this mean AO-Linux _can't_ display a dialog (i.e. cli only)?
}


void open_url_in_browser(std::string& url)
{
    log_note_f("open_url_in_browser: %s\n", url.c_str());

    pid_t pid = fork();
    if (pid == 0)
    {
        // try xdg-open first, fallback to sensible-browser
        execlp("xdg-open", "xdg-open", url, NULL);
        execlp("sensible-browser", "sensible-browser", url, NULL);
        exit(0);  // in case exec fails
    }
    else if (pid > 0)
    {
        int childstatus;
        wait(&childstatus);
    }
}


#endif


// dump this here for now

void display_loading_map_error(ao_err err)
{
    short string_id;
    
    switch (err)
    {
        case errServerDied:
            string_id = serverQuitInCooperativeNetGame;
            break;
            
        case errUnsyncOnLevelChange:
            string_id = unableToGracefullyChangeLevelsNet;
            break;
        
        case errMapFileNotSet:
        case errIndexOutOfRange:
        case errTooManyOpenFiles:
        case errUnknownWadVersion:
        case errWadIndexOutOfRange:
        default:
            string_id = badReadMapGameError;
            break;
    }
    notify_user(STRID(strERRORS, string_id));
}

