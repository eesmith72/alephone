/*

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
/*
 *  network_dummy.cpp - Dummy network functions
 */

#include "cseries.h"
#include "map.h"
#include "network.h"
#include "network_games.h"


void NetExit()
{
}

bool NetSync()
{
	return true;
}

void NetUnSync()
{
}

short NetGetLocalPlayerIndex()
{
	return 0;
}

short NetGetPlayerIdentifier(short player_index)
{
	return 0;
}

short NetGetNumberOfPlayers()
{
	return 1;
}

player_info* NetGetPlayerData(short player_index)
{
	return NULL;
}

game_info* NetGetGameData()
{
	return NULL;
}

ao_err NetChangeMap(int16_t level_number)
{
	return false;
}

int32 NetGetNetTime()
{
	return 0;
}

void display_net_game_stats()
{
}

bool display_network_gather_dialog(bool resuming_coop_game)
{
	return false;
}

ao_err display_network_join_dialog(bool& resume_game)
{
	return STRID(strNETWORK_ERRORS, netErrCouldntJoin);
}

bool current_game_has_balls()
{
	return false;
}

bool NetAllowBehindview()
{
	return false;
}

bool NetAllowCrosshair()
{
	return false;
}

bool NetAllowTunnelVision()
{
	return false;
}
