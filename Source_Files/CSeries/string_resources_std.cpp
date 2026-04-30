/*
 string_resources_std.cpp
 
 Copyright (C) 2002 and beyond by the "Aleph One" developers.
 
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

#include "string_resources_std.hpp"


// TODO: test the legacy '%' codes work (since there will be existing MML strings that use them), then update all the hardcoded strings below to use $NAME$


// TODO: All user-visible strings currently hardcoded throughout AO need to be migrated into existing or new strings below and assigned their own string IDs. This will allow UI localization in addition to translating computer terminals.

// TODO: strongly recommend using JSON for all new strings files, plus legacy support for existing MMLs (need to check how MML reader deals with out-of-range strings/string ids in supplied MML files; it _should_ ignore unrecognized strings gracefully but who knows with that old slop)


// TODO: clean up most of the dead strings (use "" to indicate a string is unused, so the remaining strings' IDs remain the same)


// BTW, if LP could have designed for shit, a sane MML format would be:
//
// <errorStrings>
//   <badProcessor>Sorry, $appName$ requires a 68040 processor or higher.</badProcessor>
//   <badQuickDraw>Sorry, $appName$ requires Color QuickDraw.</badQuickDraw>
//   ...
// </errorStrings>
// <filenameStrings>
//   ...
//
// AO's design is oft both perplexingly over-complicated and cripplingly stupid. For now, the first priority is
// to make these APIs safe; no more wildly dangerous `vnprintf` with parameterized format strings! Addressing
// MML's poor usability is left to another day and, with luck, mod-friendly JSON.



// -----------------------------------------------------------------------------------------


alert_level_t get_alert_level_for_code(ao_err code)
{
    string_index_t key = code; // discard top 16 bits (resource ID) to get string ID
    switch (code >> 16) // bitshift 16 right to get resource ID
    {
        case strDEBUG:
            switch (key)
            {
                case db_hello_bob:
                    return alert_level_t::info;
                case db_found_a_bug:
                case db_todo:
                    return alert_level_t::fatal;
                case db_insecure_lua:
                    return alert_level_t::info;
                default:
                    return alert_level_t::error; // bug, but figure out how best to represent later
            }
            
        case strERRORS:
            switch (key)
            {
                case badProcessor:
                case missingFile:
                case fileIsNotOpen:
                case cantReadFile:
                case cantWriteFile:
                    //case badExtraFileLocations:
                case badSoundChannels:
                    //case fileError:
                case copyHasBeenModified:
                case copyHasExpired:
                    //case keyIsUsedForSound:
                    //case keyIsUsedForMapZooming:
                    //case keyIsUsedForScrolling:
                    //case keyIsUsedAlready:
                case outOfMemory:
                    //case warningExternalPhysicsModel:
                    //case warningExternalMapsFile:
                case badReadMapGameError:
                case badReadMapSystemError:
                case badWriteMap:
                case badSerialNumber:
                case duplicateSerialNumbers:
                case networkOnlySerialNumber:
                    //case corruptedMap:
                    //case checkpointNotFound:
                    //case pictureNotFound:
                    //case networkNotSupportedForDemo:
                    //case serverQuitInCooperativeNetGame:
                    //case unableToGracefullyChangeLevelsNet:
                    //case cantFindMap:
                    //case cantFindReplayMap:
                case notEnoughNetworkMemory:
                    //case luascriptconflict:
                    //case replayVersionTooNew:
                    //case keyScrollWheelDoesntWork:
                    return alert_level_t::fatal;
                default:
                    return alert_level_t::error;
            };
            
        case strNETWORK_ERRORS:
            return alert_level_t::error;
            
        default: // all other resources (including non-existent ones) are assumed to be info
            // TODO: Lua plugins should be able to define alert levels for any custom string sets they might define, at which point we'll need to look (but we're not going to think about that until there's a system in place to ensure each plugin adds its own string sets without risk of stomping on another's). When that happens, we'll need an old priest and a young priest^H^H^H^H^H a std::map somewhere that holds all the dynamic string set info, so we can look up their levels from here.
            return alert_level_t::info;
    }
}


// -----------------------------------------------------------------------------------------
//

static const strings_t strings_66_debug = {
    "They’re Everywhere!",
    "Sorry. Aleph One broke. Quitting now.",
    "Stopped at a TODO.",
    "Insecure Lua has been manually enabled. Malicious Lua scripts can use Insecure Lua to take over your computer. Unless you specifically trust every single Lua script that will be running, you should quit $appName$ IMMEDIATELY.",
};


// -----------------------------------------------------------------------------------------
// StringSets from original Bungie resource forks


static const strings_t strings_128_app_errors = { // mostly app errors with a few scenario errors mixed in
    "Sorry, $appName$ requires a 68040 processor or higher.",
    
    "File was not found.",
    "File is not open.",
    "Can't read file.",
    "Can't write file.",
    
    //     "Sorry, $appName$ requires a 13\" monitor (640x480) or larger which can be set to at least 256 colors or grays.",

    
    "Please be sure the files “Map”, “Shapes”, “Images” and “Sounds” are correctly installed and try again.",
    "$appName$ couldn’t initialize the sound.",
    "$appName$ has encountered a file system error.  Check to make sure you have enough disk space and that you are not trying to save to a locked volume.",
    "This copy of $appName$ has been modified, perhaps by a virus.  Please re-install it from the original disks.",
    "This beta copy of $appName$ has expired and is no longer functional. Call Bungie at (312) 563-6200 ext. 21 for more information.",
    "Sorry, that key is already used to adjust the sound volume.",
    "Sorry, that key is already used for zooming in the overhead map view.",
    "Sorry, that key is already used for scrolling the inventory.",
    "Sorry, that key is already used for a special game function.",
    "$appName$ has used up all available RAM and cannot continue.  Trying giving it more memory, switching to a lower bit-depth or turning off sounds and try again.",
    "$appName$ is about to load an external physics model.  This could result in erratic performance, inexplicable crashes, corrupted saved games, and inconsistent network games (just a warning).",
    "$appName$ is using maps which were not created with Bungie tools.  This could result in poor performance, inexplicable crashes and corrupted saved games (proceed at your own risk).",
    "A game related error occurred while attempting to read from your map or saved game file.",
    "A system error occurred while attempting to read from your map or saved game file.",
    "An error occurred while saving your game (maybe your hard drive is full, or you tried to save to a locked volume?)",
    "That serial number is invalid.  Please try again.",
    "Sorry, you can’t start a network game with more than one copy of the same serial number.  Call 1-800-295-0060 to order more network serial numbers by fax.",
    "Sorry, this copy of $appName$ has been serialized with a network-only serial number.  You cannot play the single-player game with a network-only serial number.",
    "The map you are trying to load has been corrupted.  Please reinstall from a backup copy.",
    "Checkpoint %d was not found!", // TODO: use "$objectID$", etc
    "Picture %d was not found!",
    "This preview copy of $appName$ does not support networking.  A full demo will be available on-line or from Bungie shortly which includes networking (and a whole lot of other cool features).",
    "The gathering computer has quit the game, leaving everyone stranded without the next level.  Perhaps you should tar and feather him.",
    "Sorry, $appName$ was unable to gracefully exit from the network game.  As a result, your romp through the levels has been prematurely halted.",
    "The original Map file from which this game was saved cannot be found.",
    "$appName$ was unable to find the map that this film was recorded on, so the film cannot be replayed.",
    "Sorry, $appName$ needs 6000k free to play in a networked game.  Give $appName$ more memory and try again.",
    "There appears to be a script conflict.  Perhaps mml and netscript are having differences over who gets to control lua.  Don’t be surprised if you get unexpected script behavior or out of sync.",
    "This replay was created with a newer version of $appName$ and cannot be played with this version. Upgrade $appName$ and try again.",
    "Sorry, the scroll wheel can only be used for switching weapons.",
    "Sorry, can't make sense of Preferences file.",
};


static const strings_t strings_129_filenames = {
    "Shapes.shpA",
    "",
    "Sounds.sndA",
    "",
    "$appName$ Preferences",
    "Map.sceA",
    "Untitled Game.sgaA",
    "Marathon",
    "$appName$ Recording",
    "Physics.phyA",
    "Music.ogg", // (M2 scenario has .ogg file so let's roll with that as extension)
    "Images.imgA",
    "Movie.mpg",
    "Default Theme",
    "Marathon.appl",
};


const std::string get_default_filename_at_index(string_index_t index)
{
    if (index < filenameSHAPES8 || index > filenameEXTERNAL_RESOURCES) { return "[bad index]"; }
    if (index == filenameDEFAULT_SAVE_GAME) { return "Saved Game"; }
    return expand_string_vars(strings_129_filenames[index]);
}


static const strings_t strings_130_ui_main_screen_options = {
    "BEGIN GAME",
    "OPEN SAVED GAME",
    "",
    "REPLAY LAST GAME",
    "REPLAY SAVED RECORDING",
    "SAVE LAST RECORDING",
    "",
    "GATHER NETWORK GAME",
    "JOIN NETWORK GAME",
    "",
    "PREFERENCES",
    "START DEMO",
    "QUIT",
};


static const strings_t strings_131_ui_dialogs = {
    "SAVE GAME",
    "SAVE RECORDING",
    "Select recording as:",
    "Default",
};


static const strings_t strings_132_network_errors = {
    "Sorry, that player could not be found on the network.  He may have cancelled his Join Game dialog.",
    "One or more of the players in the game could not be found to receive the map.  The game has been canceled.",
    "The gatherering computer is not responding. It may be behind a firewall, or you may have mistyped the address.",
    "Sorry, the gathering computer has cancelled the game (you should all gang up on him next game).",
    "The map was not received in its entirety, so the game has been canceled.",
    "The gathering computer never sent the map, so the game has been cancelled.  Maybe one of the other machines in the game went down.",
    "$appName$ was unable to start the game.  Maybe one of the other machines in the game went down.",
    "An error ocurred while trying to join a game (an incompatible version of $appName$ may have tried to gather you).  Try again.",
    "Sorry, a network error ocurred and $appName$ is unable to continue.",
    "An error occurred while trying to join a game (the gatherer is using an incompatible version).",
    "The player you just added is using an older version of $appName$ that does not support some advanced features required by the game you’re trying to gather.  You will not be allowed to start the game.",
    "The player you just attempted to add is using a version of $appName$ that does not support some advanced features required by the game you’re trying to gather.",
    "$appName$ was unable to locate the Map file this level came from.  Some terminals may not display properly, and saving this game on this computer is not recommended.",
    "The connection to the gatherer was lost.",
    "Unable to look up the gatherer. Maybe you typed the address in wrong.",
    "An error occurred receiving the map from the server. Game over.", // Game over, man. Game over.
    "The gatherer is using the star protocol, but your configuration is set to ring. You will not appear in the list of available players.",
    "The gatherer is using the ring protocol, but your configuration is set to star. You will not appear in the list of available players.",
    "The gatherer is using a Lua netscript, but this version was built without Lua support. You will not appear in the list of available players.",
    
    //"There was a problem connecting to the server that tracks Internet games ($error$). Please try again later.",
	"There was a problem connecting to the server that tracks Internet games.  Please try again later.",
	"Your game could not be advertised on the Internet.  Please distribute your public IP address by other means or try again later.",
	"$appName$ failed to configure the firewall/router. You may be unable to gather.",
    "Impossible to establish a connection with an available remote server. Please try again later, or try to increase your latency tolerance, or try to gather yourself.",
};


static const strings_t strings_133_key_code_names = {
    "A",
    "S",
    "D",
    "F",
    "H",
    "G",
    "Z",
    "X",
    "C",
    "V",
    "0x0A",
    "B",
    "Q",
    "W",
    "E",
    "R",
    "Y",
    "T",
    "1",
    "2",
    "3",
    "4",
    "6",
    "5",
    "=",
    "9",
    "7",
    "-",
    "8",
    "0",
    "]",
    "O",
    "U",
    "[",
    "I",
    "P",
    "Return",
    "L",
    "J",
    "'",
    "K",
    ";",
    "\\",
    ",",
    "/",
    "N",
    "M",
    ".",
    "Tab",
    "Space",
    "`",
    "Delete",
    "0x34",
    "Escape",
    "0x35",
    "Command",
    "Shift",
    "Caps Lock",
    "Option",
    "Control",
    "0x3c",
    "0x3d",
    "0x3e",
    "0x3f",
    "0x40",
    "Keypad .",
    "0x42",
    "Keypad *",
    "0x44",
    "Keypad +",
    "0x46",
    "Clear",
    "0x48",
    "0x49",
    "0x4a",
    "Keypad /",
    "Enter",
    "0x4d",
    "Keypad -",
    "0x4f",
    "0x50",
    "Keypad =",
    "Keypad 0",
    "Keypad 1",
    "Keypad 2",
    "Keypad 3",
    "Keypad 4",
    "Keypad 5",
    "Keypad 6",
    "Keypad 7",
    "0x5a",
    "Keypad 8",
    "Keypad 9",
    "0x5d",
    "0x5e",
    "0x5f",
    "F5",
    "F6",
    "F7",
    "F3",
    "F8",
    "F9",
    "0x66",
    "F11",
    "0x68",
    "F13",
    "0x6a",
    "F14",
    "0x6c",
    "F10",
    "0x6e",
    "F12",
    "0x70",
    "F15",
    "Help",
    "Home",
    "Page Up",
    "Forw. Del.",
    "F4",
    "End",
    "F2",
    "Page Down",
    "F1",
    "Left Arrow",
    "Right Arrow",
    "Down Arrow",
    "Up Arrow",
    "Power",
};


static const strings_t strings_134_ui_hardware_configuration_messages = {
    "Be sure your external speakers or headphones are connected properly, and that you have enabled stereo output from the Sound Control Panel.",
    "Be sure that your Cybermaxx helmet is properly hooked up to the serial port and turned on.",
    "Please check to be sure you have the file “QuickTime[TM] Musical Instruments” in your “Extensions” folder, because $appName$’s background music will sound really, really stupid without it.",
};


static const strings_t strings_135_computer_terminal_labels = {
    "U.E.S.C. Marathon",
    "Opening Connection to b.4.5-23",
    "CAS.qterm//CyberAcme Systems Inc.",
    "<931.461.60231.14.vt920>",
    "UESCTerm 802.11 (remote override)",
    "PgUp/PgDown/Arrows To Scroll",
    "Return/Enter To Acknowledge",
    "Disconnecting...",
    "Connection Terminated.",
    "%H%M %m.%d.%Y",
};


static const strings_t strings_136_ui_netgame_joining_dialog = {
    "Click ‘Join’ to wait for an invitation into a network game of $appName$.",
    "Now waiting to be gathered into a network game by a server.  Click ‘Cancel’ to give up.",
    "You have been accepted into a game.  Now waiting for the server to add the remaining players... ",
};


static const strings_t strings_137_current_weapon_names = {
    "FISTS",
    ".44 MAGNUM MEGA CLASS A1",
    "ZEUS-CLASS FUSION PISTOL",
    "MA-75B ASSAULT RIFLE/ GRENADE LAUNCHER",
    "SPNKR-X18 SSM LAUNCHER",
    "TOZT-7 BACKPACK NAPALM UNIT",
    "UNKNOWN WEAPON CLASS system error 0xfded",
    "WSTE-M COMBAT SHOTGUN",
    "(somehow related to time of applicability)",
    "KKV-7 10MM FLECHETTE SMG",
};


// static const strings_t strings_138_minf_scenario_directory = { // Minf's original data file directory
//     "Marathon Trilogy:Marathon Infinity ƒ:Marathon Infinity:",
// };


static const strings_t strings_139_ui_preference_sections = {
    "Graphics",
    "Player",
    "Sound",
    "Controls",
    "Environment",
};


static const strings_t strings_140_netgame_stats = {
    "%d flags",
    "%d:%02d",
    "%d points",
    "Team",
    "Time With Ball",
    "Flags Captured",
    "Time ‘It’",
    "Goals",
    "Time On Hill",
    "Time On Hill",
    "Points",
    "Time",
};


static const strings_t strings_141_netgame_options = {
    "Kill Limit",
    "kills",
    "Capture Limit",
    "flags",
    "Point Limit",
    "points",
    "Time On Hill:",
    "minutes",
    "Points",
    "Time",
};


static const strings_t strings_142_netgame_joined_dialog = {
    "You have been accepted into a game of ‘%s’.  Now waiting for the server to add the remaining players...",
    "Every Man For Himself",
    "You have been accepted into a cooperative game.  Now waiting for the server to add the remaining players...",
    "Capture the Flag",
    "King of the Hill",
    "Kill the Man With the Ball",
    "Defense",
    "Rugby",
    "Tag",
    "You have been accepted into a custom game. Now waiting for the server to add the remaining players...",
};


static const strings_t strings_143_netgame_connecting_progress = {
    "Sending map to remote player.",
    "Sending map to remote players.",
    "Receiving map from server.",
    "Waiting for server to send map.",
    "Sending environment information to the other player.",
    "Sending environment information to the other players.",
    "Receiving environment information from server.",
    "Loading...",
    "Submitting item content to Steam...",
    "Preparing upload to Steam...",
    "Uploading content to Steam...",
    "Attempting to open router ports...",
    "Closing router ports...",
    "Checking for updates...",
    "Connecting to a remote hub server...",
};


static const strings_t strings_145_difficulty_levels = {
    "Kindergarten",
    "Easy",
    "Normal",
    "Major Damage",
    "Total Carnage"
};


static const strings_t strings_146_netgame_types = {
    "Every Man for Himself",
    "Cooperative Play",
    "Capture the Flag",
    "King of the Hill",
    "Kill the Man With the Ball",
    "Rugby",
    "Tag",
    "Netscript",
};


static const strings_t strings_147_netgame_end_condition_types = {
//    "No Limit (Alt+Q to quit)",
	"Unlimited",
    "Time Limit",
    "Score Limit",
};


static const strings_t strings_149_game_mode = {
    "Solo Player",
    "Co-Op Players",
};


// HUD items
static const strings_t strings_150_inventory_names = {
    "FISTS",
    ".44 MAGNUM MEGA CLASS",
    ".44 MAGNUM MEGA CLASS",
    ".44 CLIP (x8)",
    ".44 CLIPS (x8)",
    "ZEUS-CLASS FUSION PISTOL",
    "FUSION BATTERY",
    "FUSION BATTERIES",
    "MA-75B ASSAULT RIFLE",
    "MA-75B CLIP (x52)",
    "MA-75B CLIPS (x52)",
    "MA-75B GRENADES (x7)",
    "MA-75B GRENADES (x7)",
    "SPNKR-X18 SSM LAUNCHER",
    "SSM MISSILE (x2)",
    "SSM MISSILES (x2)",
    "ALIEN WEAPON",
    "SHOTGUN SHELL (x2)",
    "SHOTGUN SHELLS (x2)",
    "TOZT-7 NAPALM UNIT",
    "NAPALM CANISTER",
    "NAPALM CANISTERS",
    "POWERPC 620 CHIP",
    "POWERPC 620 CHIPS",
    "ALIEN ENERGY CONVERTER",
    "WAVE MOTION CANNON",
    "THE PLANS",
    "WSTE-M COMBAT SHOTGUN",
    "WSTE-M COMBAT SHOTGUNS",
    "S’PHT CARD KEY",
    "S’PHT CARD KEYS",
    "UPLINK CHIP",
    "UPLINK CHIPS",
    "Ryan’s Light Blue Ball",
    "SKULL",
    "Violet Ball",
    "Yellow Ball",
    "Brown Ball",
    "Orange Ball",
    "Blue Ball",
    "Green Ball",
    "KKV-7 10MM FLECHETTE SMG",
    "10MM FLECHETTE MAGAZINE",
    "10MM FLECHETTE MAGAZINES",
};


// HUD labels
static const strings_t strings_151_inventory_sections = {
    "WEAPONS",
    "AMMUNITION",
    "POWERUPS",
    "ITEMS",
    "WEAPON POWERUPS",
    "BALLS",
    "NETWORK STATISTICS",
};


// netgame stats (so great it has multiple resources dedicated to it!)
static const strings_t strings_153_more_netgame_stats = {
    "%d kills",
    "%d deaths",
    "%d suicides",
    "Total Carnage",
    "Monsters",
    "Total Kills: %d (%.2f kpm)",
    "Total Deaths: %d (%.2f dpm)",
    " including %d suicides",
    "Total Team Carnage",
    " including %d friendly-fire kills",
    "Total Scores",
    "Total Team Scores",
    // ZZZ: added for team vs. team carnage in postgame report
    "%s’s team",
    // ZZZ: added for legend/key in SDL postgame report
    "kills",
    "deaths",
    "suicides",
    "friendly-fire kills",
};


// EES: Does any user ever use these?
static const strings_t strings_200_ogl_color_dialogs = {
    "What color for the Void?",
    "What day ground color?",
    "What day sky color?",
    "What night ground color?",
    "What night sky color?",
    "What moon ground color?",
    "What moon sky color?",
    "What space ground color?",
    "What space sky color?",
    "What fog color?",
};


// RvB's daddy
static const strings_t strings_152_netgame_team_colors = {
    "Slate",
    "Red",
    "Violet",
    "Yellow",
    "White",
    "Orange",
    "Blue",
    "Green",
};


static const strings_t strings_255_vidmaster_dialog = {
    "Start at level:",
    "Before proceeding any further, you\nmust take the oath of the vidmaster:",
    "“I pledge to punch all switches,\nto never shoot where I could use grenades,\nto admit the existence of no level\nexcept Total Carnage,\nto never use Caps Lock as my ‘run’ key,\nand to never, ever, leave a single Bob alive.”",
};


// -----------------------------------------------------------------------------------------


static const strings_t strings_1024_screen_size = {
    "640x480 (Classic 8-bit)", // TODO: "640×480" printfs as "640480" (the Unicode glyph is missing); why? (see log_note_f in set_size of screen.cpp)
    "800x600 (Classic 16-bit)",
    "Standard",
    "High-Definition",
    "Ultrawide",
    "Ultrawide HD",

};


// -----------------------------------------------------------------------------------------

void load_standard_strings()
{
    set_strings_for_resource(strDEBUG, strings_66_debug);
    set_strings_for_resource(strERRORS, strings_128_app_errors);
    set_strings_for_resource(strFILENAMES, strings_129_filenames);
    set_strings_for_resource(130, strings_130_ui_main_screen_options);
    set_strings_for_resource(131, strings_131_ui_dialogs);
    set_strings_for_resource(132, strings_132_network_errors);
    set_strings_for_resource(133, strings_133_key_code_names);
    set_strings_for_resource(134, strings_134_ui_hardware_configuration_messages);
    set_strings_for_resource(135, strings_135_computer_terminal_labels);
    set_strings_for_resource(136, strings_136_ui_netgame_joining_dialog);
    set_strings_for_resource(137, strings_137_current_weapon_names);
  //set_strings_for_resource(138, strings_138_minf_scenario_directory); // obsolete, unused
    set_strings_for_resource(139, strings_139_ui_preference_sections);
    set_strings_for_resource(140, strings_140_netgame_stats);
    set_strings_for_resource(141, strings_141_netgame_options);
    set_strings_for_resource(142, strings_142_netgame_joined_dialog);
    set_strings_for_resource(143, strings_143_netgame_connecting_progress);
    // 144 is unused
    set_strings_for_resource(kDifficultyLevelsStringSetID, strings_145_difficulty_levels);
    set_strings_for_resource(kNetworkGameTypesStringSetID, strings_146_netgame_types);
    set_strings_for_resource(kEndConditionTypeStringSetID, strings_147_netgame_end_condition_types);
    // kScoreLimitTypeStringSetID // unused
    set_strings_for_resource(strSoloOrCoop,	strings_149_game_mode);
    set_strings_for_resource(150, strings_150_inventory_names);
    set_strings_for_resource(151, strings_151_inventory_sections);
    set_strings_for_resource(kTeamColorsStringSetID, strings_152_netgame_team_colors);
    set_strings_for_resource(153, strings_153_more_netgame_stats);
    set_strings_for_resource(200, strings_200_ogl_color_dialogs);
    set_strings_for_resource(255, strings_255_vidmaster_dialog);
    
    
    set_strings_for_resource(strScreenSize, strings_1024_screen_size);
}
