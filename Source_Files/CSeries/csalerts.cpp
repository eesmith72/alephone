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


// TODO: SDL3 has nice file chooser dialog API; dunno how much work to upgrade from SDL2 (and not doing it right now) but that should be on the radar



// -----------------------------------------------------------------------------------------
// user alerts (Mac/Win dialogs; Linux shell)


// TODO: can we use SDL_MessageBox on all platforms?
/*
#ifndef __MACOSX__
void alert_user_os_default(std::string& message, alert_level_t severity)
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
void show_alert_in_simple_dialog(aoerr code, const std::string& message)
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


const std::string format_user_alert_message(aoerr code, const std::string &extra_message, const string_vars_t &vars)
{
    std::string result;
    if (code)
    {
        result = get_resource_string(code, vars);
        if (result.empty()) { result = "A bug occurred: " + std::to_string(code); }
        if (!extra_message.empty()) result += "\n";
    }
    
    return result + expand_string_vars(extra_message, vars);
}


// the default proc for turning user_alert calls into simple/fancy dialogs, stderr logs, whatever


void write_alert_to_stderr(aoerr code, const std::string& extra_message, const string_vars_t vars)
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
    std::cerr << get_resource_string(code, vars) << " // ";
    if (!extra_message.empty()) { std::cerr << expand_string_vars(extra_message, vars) << " "; }
    std::cerr << "[" << std::to_string((int16_t)(code >> 16)) << "|" << std::to_string((int16_t)code) << "]\n";
    std::cerr << "\n";
}


void default_alert_simple_dialog(aoerr code, const std::string& extra_message, const string_vars_t vars)
{
    show_alert_in_simple_dialog(code, format_user_alert_message(code, extra_message, vars));
}




// -----------------------------------------------------------------------------------------
// The callback hook for alert_user(). Once all the SDL and MML stuff is active, they can install a new proc here to use their fancy w_widget- and theme-based dialogs.


// The simplest, lowest-level display that should always work, including during initialize application and normal/exception-triggered shutdown.
#ifdef A1_NETWORK_STANDALONE_HUB
static alert_user_proc_t alert_user_default_proc = default_alert_stderr;
static alert_user_proc_t alert_user_active_proc  = default_alert_stderr;
#else
static alert_user_proc_t alert_user_default_proc = default_alert_simple_dialog;
static alert_user_proc_t alert_user_active_proc  = default_alert_simple_dialog;
#endif


// TODO: insert these calls at appropriate points in AO initialize_application and shutdown_application
void set_alert_user_callback(alert_user_proc_t proc)
{
    alert_user_active_proc = proc;
}


void reset_alert_user_callback()
{
    alert_user_active_proc = alert_user_default_proc;
}


// TODO: most alert_user calls don't have proper error codes yet (best to put their strings into strERRORS/etc resource)

// display a message to the user
void alert_user(aoerr code, const std::string& extra_message, string_vars_t vars)
{
    alert_user_active_proc(code, extra_message, vars);
    /*
    if (get_alert_level_for_code(code) == alert_level_t::fatal)
    {
        // TODO: what is the appropriate shutdown call to make, and exactly who should make it (there are pros AND cons to doing it automatically here in `alert user`)? vhalt-like or halt to bail immediately? Or throw a AOFatalErrorOccurred exception which will propagate till it's caught in main.cpp? (Would be inclined to use an exception, unless there's a good reason to bail harder, caveat AO has a number of try blocks that call alert_user in catch, so those need amended or the damn thing will throw up multiple dialogs instead of one-and-done. Probably a good idea if fatal errors are all propagated as exceptions.)
    }
     */
}





#if defined(__WIN32__)
// callback to set starting location for Win32 "choose scenario" dialog
static int CALLBACK browse_callback_proc(HWND hwnd, UINT msg, LPARAM lparam, LPARAM lpdata)
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
#endif

#ifndef __MACOSX__
std::string show_choose_scenario_dialog() // TODO: does this mean AO-Linux _can't_ display a dialog (i.e. cli only)?
{
    std::string chosen_dir;
    
#if defined(__WIN32__)
    BROWSEINFOW bi = { 0 };
    wchar_t path[MAX_PATH];
    bi.lpszTitle = L"Select a scenario to play:";
    bi.pszDisplayName = path;
    bi.lpfn = browse_callback_proc;
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
#endif
    
    return chosen_dir;
}
#endif



#ifdef __MACOSX__
// defined in csalerts.mm
void open_url_in_browser(std::string& url);
#else
void open_url_in_browser(std::string& url)
{
#if defined(__WIN32__)
    ShellExecuteW(NULL, L"open", utf8_to_wide(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
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
#endif
}
#endif


void open_url_in_browser(std::string& url)
{
    fprintf(stderr, "System launch url: %s\n", url.c_str());
    open_url_in_browser(url);
}


// -----------------------------------------------------------------------------------------
// old halt, debug, assert // TODO: only here for reference; delete when done updating

/*
void vpause(std::string& message)
{
    logWaarning("vpause: %s", message.c_str());
    fprintf(stderr, "vpause %s\n", message.c_str());
}

void stop_recording();
void shutdown_application();

void vhalt(std::string& message)
{
    stop_recording();
    logFaatal_f("vhalt: %s", message.c_str());
    GetCurrentLogger()->flush();
    shutdown_application();
    //alert_user(STRING_KEY(strDEBUG, db_vhalt));
    abort();
}

void halt(void)
{
    logFaatal("halt called");
    fprintf(stderr, "halt\n");
    abort();
}
*/

