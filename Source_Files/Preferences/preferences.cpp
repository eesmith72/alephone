/*
 preferences.cpp
 
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


#include "preferences_support.hpp"


//*****************************************************************************
// PREFERENCES
//*****************************************************************************


static void reset_preferences()
{
    graphics_preferences.reset();
    network_preferences.reset();
    player_preferences.reset();
    input_preferences.reset();
    sound_preferences.reset();
    environment_preferences.reset();
}


void read_preferences()
{
    reset_preferences();

    ao_path prefs_path = get_preferences_dir();

	std::string name = get_string(STRID(strFILENAMES, filenamePREFERENCES));
	if (shell_options.editor) { name += " Editor"; } // check for editor prefs
	prefs_path /= name;

	bool defaults = false;
    DataFile OFile;
	ao_err err = OFile.open(prefs_path);

	if (err && shell_options.editor)
	{
		// copy non-editor prefs
		prefs_path = get_preferences_dir() / (get_string(STRID(strFILENAMES, filenamePREFERENCES)));
		err = OFile.open(prefs_path); // TODO: these don't do anything except check the file can be opened
	}

	if (err)
    {
		defaults = true;
        prefs_path = find_file_at_subpath("Scripts/Default Preferences.xml");
		err = OFile.open(prefs_path);
	}

	// legacy default prefs // EES: presumably still needed as older scenarios will contain it; TODO: need to confirm and keep the code for reading legacy prefs files if so (yuck, but unavoidable)
	if (err)
    {
		defaults = true;
        prefs_path = find_file_at_subpath(get_string(STRID(strFILENAMES, filenamePREFERENCES)));
		err = OFile.open(prefs_path);
	}
	
	if (!err)
	{
		OFile.close();
		try {
			InfoTree prefs = InfoTree::load_xml(prefs_path);
			InfoTree root = prefs.get_child("mara_prefs");
			
			std::string version = "";
			root.read_attr("version", version);
			if (!version.length())
                log_warning_f("Reading older preferences of unknown version. Preferences will be upgraded to version %s when saved. (%s)", A1_DATE_VERSION, prefs_path.c_str());
			else if (version < A1_DATE_VERSION)
                log_warning_f("Reading older preferences of version %s. Preferences will be upgraded to version %s when saved. (%s)", version.c_str(), A1_DATE_VERSION, prefs_path.c_str());
			else if (version > A1_DATE_VERSION)
                log_warning_f("Reading newer preferences of version %s. Preferences will be downgraded to version %s when saved. (%s)", version.c_str(), A1_DATE_VERSION, prefs_path.c_str());
			
			for (const InfoTree &child : root.children_named("graphics"))
				graphics_preferences.read(child, version);
			for (const InfoTree &child : root.children_named("player"))
				player_preferences.read(child, version);
			for (const InfoTree &child : root.children_named("input"))
				input_preferences.read(child, version);
			for (const InfoTree &child : root.children_named("sound"))
				sound_preferences.read(child, version);
#if !defined(DISABLE_NETWORKING)
			for (const InfoTree &child : root.children_named("network"))
				network_preferences.read(child, version);
#endif
			for (const InfoTree &child : root.children_named("environment"))
				environment_preferences.read(child, version);
			
		}
        catch (const InfoTree::Exception& ex)
        {
            log_error_f("Error parsing preferences file (%s): %s", prefs_path.c_str(), ex.what());
            err = STRID(strERRORS, cantParsePreferences);
		}
	}

	if (err)
	{
        notify_user(err, defaults ? "Error reading the default Preferences file (see $appLogFile$ for details)"
                                  : "Error reading the Preferences file (see $appLogFile$ for details)");
	}
}







void write_preferences()
{
	InfoTree root;
	root.put_attr("version", A1_DATE_VERSION);
	
	root.put_child("graphics", graphics_preferences.write());
	root.put_child("player", player_preferences.write());
	root.put_child("input", input_preferences.write());
	root.put_child("sound", sound_preferences.write());
#if !defined(DISABLE_NETWORKING)
	root.put_child("network", network_preferences.write());
#endif
	root.put_child("environment", environment_preferences.write());
	
	InfoTree fileroot;
	fileroot.put_child("mara_prefs", root);
	
    ao_path FileSpec = get_preferences_dir();

	std::string name = get_string(STRID(strFILENAMES, filenamePREFERENCES));
	if (shell_options.editor) { name += " Editor"; }
	FileSpec /= name;
	
	try
    {
		fileroot.save_xml(FileSpec);
	}
    catch (const InfoTree::Exception& ex)
    {
        log_error_f("Error saving preferences file (%s): %s", FileSpec.c_str(), ex.what());
    }
}



//*****************************************************************************
// DIALOGS
//*****************************************************************************


void display_main_preferences_dialog()
{
    // Save the existing preferences, in case we have to reload them
    //write_preferences(); // TODO: there's a lot of saving going on

    // Create top-level dialog
    dialog d;
    vertical_placer *placer = new vertical_placer;
    w_title *w_header = new w_title("PREFERENCES");
    d.add(w_header);
    w_button *w_player = new w_button("PLAYER", player_dialog, &d);
    d.add(w_player);
    w_button *w_online = new w_button("INTERNET", online_dialog, &d);
    d.add(w_online);
    w_button *w_graphics = new w_button("GRAPHICS", graphics_dialog, &d);
    d.add(w_graphics);
    w_button *w_sound = new w_button("SOUND", sound_dialog, &d);
    d.add(w_sound);
    w_button *w_controls = new w_button("CONTROLS", controls_dialog, &d);
    d.add(w_controls);
    w_button *w_environment = new w_button("ENVIRONMENT", environment_dialog, &d);
    d.add(w_environment);
    w_button *w_plugins = new w_button("PLUGINS", plugins_dialog, &d);
    d.add(w_plugins);

    w_button *w_return = new w_button("RETURN", dialog_cancel, &d);
    d.add(w_return);

    placer->add(w_header);
    placer->add(new w_spacer, true);
    placer->add(w_player);
    placer->add(w_online);
    placer->add(w_graphics);
    placer->add(w_sound);
    placer->add(w_controls);
    placer->add(w_environment);
    placer->add(w_plugins);
    placer->add(new w_spacer, true);
    placer->add(w_return);

    d.set_widget_placer(placer);

    main_screen.clear();
    d.run();
}

