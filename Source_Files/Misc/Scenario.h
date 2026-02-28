#ifndef _SCENARIO
#define _SCENARIO
/*

	Copyright (C) 2006 and beyond by Bungie Studios, Inc.
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

	Scenario tag parser
	by Gregory Smith 2006

	This is for handling scenario compatibility info
*/


#include <string>
#include <vector>

using std::string;
using std::vector;

class Scenario
{
public:
	static Scenario *instance();
	
	const std::string GetName() { return m_name; }
	void SetName(const std::string name) { m_name = std::string(name, 0, 31); }
	
	const std::string GetVersion() { return m_version; }
	void SetVersion(const std::string version) { m_version = std::string(version, 0, 7); }

	const string GetID() { return m_id; }
	void SetID(const std::string id) { m_id = std::string(id, 0, 23); }
	
	bool IsCompatible(const std::string);
	void AddCompatible(const std::string);

	void SetAllowsClassicGameplay(bool allow) { m_allows_classic_gameplay = allow; }
	bool AllowsClassicGameplay() const { return m_allows_classic_gameplay; }
	
private:
	Scenario() : m_allows_classic_gameplay{false} { }
	
    std::string m_name;
    std::string m_version;
    std::string m_id;
	
    std::vector<string> m_compatibleVersions;

	bool m_allows_classic_gameplay;
};

class InfoTree;
void parse_mml_scenario(const InfoTree& root);
void reset_mml_scenario();

#endif
