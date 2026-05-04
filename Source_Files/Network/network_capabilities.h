/*
 *  network_capabilities.h -- a versioning system for gatherers and joiners

	Copyright (C) 2005 and beyond by Gregory Smith
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

#ifndef NETWORK_CAPABILITIES_H
#define NETWORK_CAPABILITIES_H

#include "cseries.hpp"


using std::string;

typedef std::map<string, uint32> capabilities_t;

class Capabilities : public capabilities_t
{
 public:
  enum { kMaxKeySize = 1024 };

  static const int kGameworldVersion = 6;
  static const int kGameworldM1Version = 4;
  static const int kStarVersion = 6;
  static const int kLuaVersion = 2;
  static const int kGatherableVersion = 1;
  static const int kZippedDataVersion = 1; // map, lua, physics
  static const int kNetworkStatsVersion = 1; // latency, jitter, errors
  static const int kRugbyVersion = 1; // sane score limit

  static const std::string kGameworld;    // the PRNG, physics, etc.
  static const std::string kGameworldM1;  // like gameworld, but for Marathon 1 compatibility
  static const std::string kStar;         // the star network protocol
  static const std::string kLua;          // Lua script support
  static const std::string kGatherable;   // joiner's response indicating he can be
                                     // gathered
  static const std::string kZippedData;   // can receive zipped data
  static const std::string kNetworkStats; // can receive network stats
  static const std::string kRugby;        // rugby version
  
  uint32& operator[](const std::string& k) { 
    assert_fail(k.length() < kMaxKeySize, "");
    return capabilities_t::operator[](k);
  }
};

#endif

