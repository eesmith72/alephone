/*
 screen_overlay.h -- in-game on-screen messages (from screen_shared.h/.cpp)
 
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

#ifndef __screen_overlay_h__
#define __screen_overlay_h__

// TODO: this module may eventually be replaced by a Lua HUD plugin (note: Console should use C++ drawing APIs and there are widgets that use C++ APIs as well)

#include "cseries.h"



void screen_print(const std::string& s);



class FpsCounter
{
public:
	using clock = std::chrono::high_resolution_clock;
	static constexpr auto update_time = std::chrono::milliseconds(250);
	
	FpsCounter() :
		next_update_{clock::now() + update_time},
		sum_{0},
		count_{0},
		fps_{0}
		{
			
		}
	
	void update() {
		auto now = clock::now();
		sum_ += 1.0 / std::chrono::duration_cast<std::chrono::duration<float>>(now - prev_).count();
		++count_;
		prev_ = now;
		
		if (now >= next_update_)
		{
			next_update_ = now + update_time;
			if (count_) {
				fps_ = sum_ / count_;
			} else {
				fps_ = 0.f;
			}
			
			sum_ = 0;
			count_ = 0;
		}
	}
	
	float get() const { return fps_; }
	bool ready() const { return fps_ != 0; }

	void reset() {
		sum_ = 0;
		count_ = 0;
		prev_ = clock::now();

		fps_ = 0.f;
	}

private:
	clock::time_point prev_;
	clock::time_point next_update_;
	
	float sum_;
	int count_;

	bool ready_;
	float fps_;
};

constexpr std::chrono::milliseconds FpsCounter::update_time;

extern FpsCounter fps_counter;

extern bool ShowPosition;
extern bool ShowScores;


void reset_messages();

// Displays a message on the screen for a second or so; may be good for debugging
void ShowMessage(char *Text);

/* SB: Custom Blizzard-style overlays */
#define MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS 6
bool IsScriptHUDNonlocal();
void SetScriptHUDNonlocal(bool nonlocal = true);
/* color is a terminal color */
void SetScriptHUDColor(int player, int idx, int color);
/* text == NULL or "" removes that HUD element
   to turn HUD elements off, set all elements NULL or "" */
void SetScriptHUDText(int player, int idx, const char* text);
/* icon == NULL turns the icon off
   someday I'll document the format */
bool SetScriptHUDIcon(int player, int idx, const char* icon, size_t length);
/* sets the icon for that HUD to a colored square (same colors as SetScriptHUDColor) */
void SetScriptHUDSquare(int player, int idx, int color);



void update_fps_display(SDL_Surface* s);
void DisplayPosition(SDL_Surface* s);
void DisplayMessages(SDL_Surface* s);
void DisplayNetLoadingScreen(SDL_Surface* s);
void DisplayScores(SDL_Surface* s);
void DisplayInputLine(SDL_Surface* s);


#endif /*  __screen_overlay_h__ */
