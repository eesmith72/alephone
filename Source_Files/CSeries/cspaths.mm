/*
 cspaths.mm -- macOS-specific implementations
 
 Copyright (C) 2017 and beyond by Jeremiah Morris
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

#import <Cocoa/Cocoa.h>

#include "cspaths.hpp"


// TODO: this was snaky before (and still is), so review to confirm the original behavior is unchanged

// -----------------------------------------------------------------------------------------
// app's display name for use in string vars and dialogs


#define A1_PREFER_APP_NAME_TO_BUNDLE_ID (@"A1_PREFER_APP_NAME_TO_BUNDLE_ID")


std::string get_application_name()
{
    static std::string app_name = "";
    if (app_name.empty())
    {
        NSDictionary* bundleInfo = [[NSBundle mainBundle] localizedInfoDictionary];
        app_name = [[bundleInfo valueForKey: (NSString*)kCFBundleNameKey] UTF8String];
    }
    return app_name;
}


// -----------------------------------------------------------------------------------------
// used below as directory names


static std::string get_bundle_id()
{
    static std::string bundle_id;
    if (bundle_id.empty()) { bundle_id = NSBundle.mainBundle.bundleIdentifier.UTF8String; }
    return bundle_id;
}


static std::string get_app_name_for_path()
{
    static std::string name = "";
    if (name.empty())
    {
        bool useAppName = [[NSBundle.mainBundle.localizedInfoDictionary valueForKey: A1_PREFER_APP_NAME_TO_BUNDLE_ID] boolValue];
        name = useAppName ? get_application_name() : "AlephOne";
    }
    return name;
}


// -----------------------------------------------------------------------------------------
// standard AO directories


ao_path get_macos_app_bundle_game_data_dir()
{
    static ao_path dir;
    if (dir.empty()) { dir = ao_path(std::string([NSBundle.mainBundle.resourcePath UTF8String])) / "DataFiles"; }
    return dir;
}


ao_path get_local_storage_dir() // was local_data_dir
{
	static ao_path dir;
	if (dir.empty())
	{
		NSArray* arr = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
		NSString* path = [arr objectAtIndex: 0];
        if (path) { dir = ao_path(path.UTF8String) / get_app_name_for_path(); }
	}
	return dir;
}


ao_path get_default_game_data_dir() // was default_data_dir
{
	static ao_path dir;
	if (dir.empty())
	{
		char path[MAXPATHLEN];
        NSURL* url = [NSBundle.mainBundle.bundleURL URLByDeletingLastPathComponent];
        if ([url getFileSystemRepresentation: path maxLength: sizeof(path)]) { dir = path; }
	}
	return dir;
}


static ao_path get_library_dir()
{
	static ao_path dir;
	if (dir.empty())
	{
		NSArray* arr = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
		NSString* path = [arr objectAtIndex: 0];
        if (path) { dir = path.UTF8String; }
	}
	return dir;
}


ao_path get_preferences_dir()
{
    static ao_path dir;
    // Apple really wants everyone to use NSUserDefaults but AO does its own thing,
    // so this is a bodge to get "~/Library/Preferences/APPNAME"
    if (dir.empty()) { dir = get_library_dir() / "Preferences" / get_app_name_for_path(); }
    return dir;
}


ao_path get_logs_dir()
{
    static ao_path dir;
    if (dir.empty()) { dir = get_library_dir() / "Logs" / get_app_name_for_path(); }
    return dir;
}


// TODO: is there a reason the non-AppStore builds aren't using "~/Pictures/APPNAME/Screenshots/" for screenshots?
/*
#ifdef MAC_APP_STORE
static std::string _get_pictures_path()
{
	static std::string pictures_dir = "";
	if (pictures_dir.empty())
	{
		NSArray *arr = NSSearchPathForDirectoriesInDomains(NSPicturesDirectory, NSUserDomainMask, YES);
		NSString *picturesPath = [arr objectAtIndex:0];
		if (picturesPath != nil)
			pictures_dir = [picturesPath UTF8String];
	}
	return pictures_dir;
}
#endif
 */
