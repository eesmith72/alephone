/*
 StyledFont.hpp - SDL font handling
 
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

#ifndef StyledFont_hpp
#define StyledFont_hpp

#include "Font.hpp"


// TODO: w_styled_text, w_games_in_room, w_players_in_room use this; consolidate with ComputerTerminal's styled text handling so there is one system and one markup scheme for drawing styled text and put this in its own StyledTextRenderer class so it isn't clogging up FontRenderer_

/*


class StyledFont
{
public:
    StyledFont() = default;
    
    virtual ~StyledFont() = default;
    
    virtual void unload() = 0; // should be protected friend called by `reset_fonts` below, which is called whenever a scenario/theme changes (it'd be nice to unload more granularly, but that'd require reference counting each loaded font since some will be shared dependencies; we can worry about that when overhauling Scenario/), but right now unload() is being called from sdl_dialogs.cpp and Font.cpp (which is ridiculous and almost certainly doesn't account for sharing),
    
    
    virtual uint16 get_ascent(void) const      = 0;
    virtual uint16 get_height(void) const      = 0;
    virtual uint16 get_line_height(void) const = 0;
    virtual uint16 get_descent(void) const     = 0;
    virtual int16  get_leading(void) const     = 0;
    
    
    virtual int draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const = 0;
    virtual uint16 text_width(const std::string& text, uint16 style) const = 0;
    virtual int trunc_text(const std::string& text, int max_width, uint16 style) const = 0;
    
    // these methods have moved to StyledTextRenderer:
    // int styled_text_width(const std::string& text, uint16 initial_style) const;
    // int trunc_styled_text(const std::string& text, int max_width, uint16 style) const;
    // std::string style_at(const std::string& text, std::string::const_iterator pos, uint16 style) const;
    // int draw_styled_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 initial_style) const;
    
protected:
   // virtual int _draw_text(SDL_Surface* s, const std::string& text, int x, int y, uint32 pixel, uint16 style) const = 0;
    
    virtual uint16 _text_width(const std::string& text, uint16 style) const = 0;
    
    virtual int _trunc_text_muckroman(const std::string& text, int max_width, uint16 style) const = 0;
    
private:
};

*/



#endif /* StyledFont_hpp */
