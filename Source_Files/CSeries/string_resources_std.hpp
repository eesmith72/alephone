/*
 string_resources_std.hpp -- M2's original resource-fork strings, plus some AO ones
 
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

#ifndef string_resources_std_hpp
#define string_resources_std_hpp


#include "string_resources.hpp"


// TODO: standard naming conventions, e.g. using "s_" as prefix on all enums and define convenience macros for each set: `get_app_error_string(bad_processor)`, `get_netgame_stats_string(::flag_pulls_format, custom_string_vars)`, etc


// -----------------------------------------------------------------------------------------
// Load the original Bungie strings.

// This MUST be done at startup and before loading a scenario's string sets, returning all standard strings to their default values.

// These strings can then be partially/wholly replaced by MML (scenario customizations, language translations), so long as their meanings stay the same.

void load_standard_strings();


// Used by notify_user to determine severity and display accordingly.
alert_level_t get_alert_level_for_code(ao_err code);


// -----------------------------------------------------------------------------------------
// The strings
//
// Notes:
//
// While dynamically looking up strings by key is a bit less efficient than putting them all
// in `extern std::string NAME;` globals, it has the benefit that we can expand any "$NAME$"
// variables in those strings so that the caller receives the finished string ready to use.
//
// The new `ao_err` type (uint32) is intended to be interchangeable with string_ids_t, so
// error messages, info strings, and anything else that needs to find meaning in numbers
// has exactly one standard API to look them all up.
//
// The resource ID macros and string ID enums are messy wrt naming convention, but that can
// be tidied up later. It wouldn't hurt to convert Metaserver exceptions to simple codes
// too (see strMetaserverExceptions below).


// in cserr.hpp
#define strAOERROR (errAOERROR)

#define errDataFileCorrupt  (corruptedMap)
#define errDataFileTooNew   (replayVersionTooNew)


// app and scenario errors
#define strERRORS (128)
enum
{
    // TODO: error enums should start with a string description of the resource id, e.g. "application error", "scenario error", "network error" so that can be included in alert message
    badProcessor = 0,
    
    // TODO: where best to define standard FS error codes?
    missingFile,   // badQuickDraw
    fileIsNotOpen, // badSystem
    cantReadFile,  // badMemory
    cantWriteFile, // badMonitor
    
    badExtraFileLocations,
    badSoundChannels,
    fileError,
    copyHasBeenModified, // bad serial number
    copyHasExpired,
    keyIsUsedForSound,
    keyIsUsedForMapZooming,
    keyIsUsedForScrolling,
    keyIsUsedAlready,
    outOfMemory,
    warningExternalPhysicsModel,
    warningExternalMapsFile,
    badReadMapGameError,
    badReadMapSystemError,
    badWriteMap,
    badSerialNumber,
    duplicateSerialNumbers,
    networkOnlySerialNumber,
    corruptedMap,
    checkpointNotFound, // "Checkpoint %d was not found!"
    pictureNotFound,    // "Picture %d was not found!"
    networkNotSupportedForDemo,
    serverQuitInCooperativeNetGame,
    unableToGracefullyChangeLevelsNet,
    cantFindMap,        // Called when the save game can't find the map.  Reverts to default map.
    cantFindReplayMap,  // Called when you can't find the map that the replay references.
    notEnoughNetworkMemory,
    luascriptconflict,
    replayVersionTooNew,
    keyScrollWheelDoesntWork,
    cantParsePreferences,
};


// scenario file names
#define strFILENAMES (129)
enum
{
    filenameSHAPES8 = 0,
    filenameSHAPES16, // unused
    filenameSOUNDS8,
    filenameSOUNDS16, // unused
    filenamePREFERENCES,
    filenameDEFAULT_MAP,
    filenameDEFAULT_SAVE_GAME,
    filenameMARATHON_NAME,
    filenameMARATHON_RECORDING,
    filenamePHYSICS_MODEL,
    filenameMUSIC,
    filenameIMAGES,
    filenameMOVIE,
    filenameDEFAULT_THEME,
    filenameEXTERNAL_RESOURCES,
};

const std::string get_default_filename_at_index(string_index_t index);


// file chooser dialogs
#define strPROMPTS (131)
enum {
    _save_game_prompt = 0,
    _save_replay_prompt,
    _select_replay_prompt,
    _default_prompt,
};


// network connection errors
#define strNETWORK_ERRORS (132)
enum
{
    netErrCantAddPlayer = 0,
    netErrCouldntDistribute,
    netErrCouldntJoin,
    netErrServerCanceled,
    netErrMapDistribFailed,
    netErrWaitedTooLongForMap,
    netErrSyncFailed,
    netErrJoinFailed,
    netErrCantContinue,
    netErrIncompatibleVersion,
    netErrGatheredPlayerUnacceptable,
    netErrUngatheredPlayerUnacceptable,
    netErrJoinerCantFindScenario,
    netErrLostConnection,
    netErrCouldntResolve,
    netErrCouldntReceiveMap,
    netWarnJoinerHasNoStar,
    netWarnJoinerHasNoRing,
    netWarnJoinerNoLua,
    netErrMetaserverConnectionFailure,
    netWarnCouldNotAdvertiseOnMetaserver,
    netWarnUPnPConfigureFailed,
    netWarnRemoteHubServerNotAvailable,
};


// computer terminal header+footer texts, plus M1 logon/logoff title
#define strCOMPUTER_TERMINAL_LABELS (135)
enum
{
    _m1_marathon_name = 0,
    _computer_starting_up,
    _computer_manufacturer,
    _computer_address,
    _computer_terminal,
    _scrolling_message,
    _acknowledgement_message,
    _disconnecting_message,
    _connection_terminated_message,
    _date_format, // "%H%M %m.%d.%Y"
};


// join netgame dialog
#define strJOIN_DIALOG_MESSAGES (136)
enum
{
    _join_dialog_welcome_string = 0,
    _join_dialog_waiting_string,
    _join_dialog_accepted_string
};


// weapon-in-hand names as displayed in HUD
#define strWEAPON_NAME_LIST (137)
// physics-defined ints, no enums


// #define strPATHS (138)
// "Marathon Trilogy:Marathon Infinity ƒ:Marathon Infinity:"


// netgame stats
#define strNETWORK_GAME_STRINGS (140)
enum {
    flagPullsFormatString = 0,    // "%d flags"
    minutesPossessedFormatString, // "%d:%02d"
    pointsFormatString,           // "%d points"
    teamString,
    timeWithBallString,
    flagsCapturedString,
    timeItString,
    goalsString,
    reignString,
    // Benad
    timeOnBaseString,
    // SB
    pointsString,
    timeString,
};


#define strSETUP_NET_GAME_MESSAGES (141)
enum {
    killLimitString = 0,
    killsString, // unused
    flagPullsString,
    flagsString, // unused
    pointLimitString,
    pointsScoringString,
    // START Benad
    timeOnBaseString_, // unused
    minutesString // unused
    // END Benad
};


#define strJOIN_NETWORK_STRINGS (142)
enum {
    _standard_format = 0, // "You have been accepted into a game of ‘%s’. [...]"
    
    // "Every Man for Himself", "Cooperative Play", etc. (inserts into `_standard_format` string as %s/$gameType$):
    _carnage_word,
    _cooperative_string,
    _capture_the_flag,
    _king_of_the_hill,
    _kill_the_man_with_the_ball,
    _defender_offender,
    _rugby,
    _tag,
    _custom_string
};


// difficulty level names
#define kDifficultyLevelsStringSetID (145)

#define kNetworkGameTypesStringSetID (146)
#define kEndConditionTypeStringSetID (147)
//#define kScoreLimitTypeStringSetID   (148)
#define strSoloOrCoop  (149)


// HUD inventory items
#define strITEM_NAME_LIST (150)
// physics-defined ints, no enums


// HUD inventory section titles
#define strHEADER_NAME_LIST (151)
// physics-defined ints, no enums


// Netgame player color names; see NUMBER_OF_TEAM_COLORS in player.h for enum
// matches STR# for colors in original Marathon (m2 and inf moved it to a menu)
#define kTeamColorsStringSetID (152)


#define strNET_STATS_STRINGS (153)
enum {
    strKILLS_STRING = 0,            // "%d kills"
    strDEATHS_STRING,               // "%d deaths"
    strSUICIDES_STRING,             // "%d suicides"
    strTOTALS_STRING,
    strMONSTERS_STRING,
    strTOTAL_KILLS_STRING,          // "Total Kills: %d (%.2f kpm)"
    strTOTAL_DEATHS_STRING,         // "Total Deaths: %d (%.2f dpm)"
    strINCLUDING_SUICIDES_STRING,   // " including %d suicides"
    strTEAM_TOTALS_STRING,
    strFRIENDLY_FIRE_STRING,        // " including %d friendly-fire kills"
    strTOTAL_SCORES,
    strTOTAL_TEAM_SCORES,
    strTEAM_CARNAGE_STRING,         // "%s’s team"
    strKILLS_LEGEND,
    strDEATHS_LEGEND,
    strSUICIDES_LEGEND,
    strFRIENDLY_FIRE_LEGEND
};



#define strMetaserverExceptions (241)
// enums and strings are defined in MetaserverClient::LoginDeniedException // TODO: why they are Exceptions instead of plain old error codes is unclear, but they should probably be codes since they indicate expected behaviors (while CPP makes some effort to clean up memory as it unrolls the stack, exceptions are best limited to "oh shit, something seriously broke")


// EES: Allowing MML to choose a strings ID is a fine way to break things (e.g. if a scenario MML specifies an existing strings's ID, it will load its strings all over that), so we are going to hardcode it as 255 for now. TODO: find out later 1. which scenarios define a <vidmaster> MML, and 2. what ID they most commonly use so we can standardize on that.
#define vidmasterStringSetID (255)
enum {
    strVidmasterEnterLevel,
    strVidmasterIntroduction,
    strVidmasterOath,
};


#endif /* string_resources_std_hpp */
