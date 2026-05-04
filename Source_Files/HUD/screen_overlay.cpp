/*
 screen_overlay.cpp
 
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


#include "screen_overlay.h"

#include "ImageBlitter.hpp"

#include "app_state.hpp" // game_is_networked

#include "Screen.hpp" // Screen

#include "screen_drawing.h" // _computer_interface_text_color

#include "network.h" // MAXIMUM_NUMBER_OF_NETWORK_PLAYERS
#include "network_games.h" // player_rankings_t
#include "camera.hpp" // standard_camera_settings

#include "computer_interface.h" // player_in_terminal_mode

#include "preferences.hpp" // graphics_preferences


extern screen_rectangle lua_text_margins; // in lua_screen_objects.cpp


// TODO: aside from FpsCounter and DisplayNetLoadingScreen, the rest of this code draws the HUD overlay and it's seriously tempting to yeet it and turning over that job to a Lua HUD plugin; wrapping the Canvas class in Lua should provide sufficient drawing capabilities for HUDs, messaging, Console, computer terminal, automap customization; this in turn should allow 2D map editor to be implemented as Lua plugin to complement the existing Vasara for 3D texturing



// EES: just gonna hardcode these here; once we support multiple LuaHUD plugins, the screen overlay will be plugin-based and the code below will DIAF
static const font_key_t screen_messages_font_key = {kFontIDMono, styleNormal, 12};

static const font_t* LoadedOnScreenFont = nullptr;


FpsCounter fps_counter;

bool ShowPosition = false;
bool ShowScores = false;



// Current screen messages:

struct ScreenMessage
{
    enum {
        Len = 256
    };

    uint64_t ExpirationTime; // machine ticks the screen message expires at
    std::string Text;        // Text to display
    
    ScreenMessage(): ExpirationTime(machine_tick_count()) {Text[0] = 0;}
};

const int NumScreenMessages = 7;

static int MostRecentMessage = NumScreenMessages - 1;

static ScreenMessage Messages[NumScreenMessages];



void screen_print(const std::string& s)
{
    MostRecentMessage = (MostRecentMessage + 1) % NumScreenMessages;
    while (MostRecentMessage < 0) MostRecentMessage += NumScreenMessages;
    
    ScreenMessage& Message = Messages[MostRecentMessage];
    Message.ExpirationTime = machine_tick_count() + 7 * MACHINE_TICKS_PER_SECOND;
    Message.Text = s;
}


struct ScriptHUDElement
{
    /* this needs optimized (sorry, making fun of my grandmother...) */
    /* it's char[4] instead of int32 to make the OpenGL support simpler to implement */
    uint8_t icon[1024];
    int32_t color;
    std::string text;
    ImageBlitter* blitter;
};

static ScriptHUDElement script_hud_elements[MAXIMUM_NUMBER_OF_NETWORK_PLAYERS][MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS];





namespace icon {
    
  static inline char nextc(const char*& p, size_t& rem) {
    if(rem == 0) throw "end of string";
    --rem;
    return *(p++);
  }
    
  // we can't use ctype for this because of locales and hexadecimal
  static inline bool isadigit(char p) {
    if(p >= '0' && p <= '9') return true;
    else if(p >= 'A' && p <= 'F') return true;
    else if(p >= 'a' && p <= 'f') return true;
    else return false;
  }
    
  static inline unsigned char digit(char p) {
    if(p >= '0' && p <= '9') return p - '0';
    else if(p >= 'A' && p <= 'F') return p - 'A' + 0xA;
    else if(p >= 'a' && p <= 'f') return p - 'a' + 0xA;
    else throw "invalid digit";
  }
    
  static inline unsigned char readuc(const char*& p, size_t& rem) {
    char a = nextc(p, rem), b;
    b = nextc(p, rem);
    return (digit(a) << 4) | digit(b);
  }
    
  static bool parseicon(const char* p, size_t rem, uint8_t palette[1024], int& numcolors, uint8_t graphic[256]) {
    char chars[256];
    try {
      char oc, c;
      size_t n, m;
      numcolors = 0;
      while(1) {
    c = nextc(p, rem);
    if(c >= '0' && c <= '9')
      numcolors = numcolors * 10 + (c - '0');
    else break;
      }
      if(numcolors == 0) return 1;
      oc = c;
      do {
    c = nextc(p, rem);
      } while(c == oc);
      n = 0;
      while(n < numcolors) {
    chars[n] = c;
    palette[n * 4] = readuc(p, rem);
    palette[n * 4 + 1] = readuc(p, rem);
    palette[n * 4 + 2] = readuc(p, rem);
    c = nextc(p, rem); /* ignore a char, UNLESS... */
    if(isadigit(c)) {  /* ...it's a digit */
      --p; ++rem; /* let readuc see it */
      palette[n * 4 + 3] = readuc(p, rem);
      nextc(p, rem); /* remember to ignore another char */
    }
    else
      palette[n * 4 + 3] = 255;
    ++n;
    c = nextc(p, rem);
      }
      n = 0;
      while(n < 256) {
    for(m = 0; m < numcolors; ++m) {
      if(chars[m] == c) {
        graphic[n++] = m;
        break;
      }
    }
    c = nextc(p, rem);
      }
    } catch(...) {
      return false;
    }
    return true;
  }
    
  void seticon(int player, int idx, unsigned char palette[1024], unsigned char graphic[256]) {
    unsigned char* p1, *p2, px;
    int n;
    p1 = script_hud_elements[player][idx].icon;
    p2 = graphic;
    for(n = 0; n < 256; ++n) {
      px = *(p2++);
      *(p1++) = palette[px * 4 + 3];
      *(p1++) = palette[px * 4];
      *(p1++) = palette[px * 4 + 1];
      *(p1++) = palette[px * 4 + 2];
    }
    if (script_hud_elements[player][idx].blitter)
    {
      delete script_hud_elements[player][idx].blitter;
    }
    SDL_Surface* srf = SDL_CreateRGBSurfaceFrom(script_hud_elements[player][idx].icon, 16, 16, 32, 64, SDLRGBSurfaceBitmask);
    
    script_hud_elements[player][idx].blitter = new ImageBlitter();
    script_hud_elements[player][idx].blitter->take_surface(srf);
  }
    
}

static bool nonlocal_script_hud = false;

bool IsScriptHUDNonlocal() {
  return nonlocal_script_hud;
}

void SetScriptHUDNonlocal(bool nonlocal) {
  nonlocal_script_hud = nonlocal;
}

void SetScriptHUDColor(int player, int idx, int color) {
  player %= MAXIMUM_NUMBER_OF_NETWORK_PLAYERS;
  idx %= MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS; /* o_o */
  script_hud_elements[player][idx].color = color % 8; /* O_O */
}

void SetScriptHUDText(int player, int idx, const char* text) {
  player %= MAXIMUM_NUMBER_OF_NETWORK_PLAYERS;
  idx %= MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS;
  if(!text) script_hud_elements[player][idx].text.clear();
  else script_hud_elements[player][idx].text = text;
}

bool SetScriptHUDIcon(int player, int idx, const char* text, size_t rem) {
  unsigned char palette[1024], graphic[256];
  int numcolors;
  player %= MAXIMUM_NUMBER_OF_NETWORK_PLAYERS;
  idx %= MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS;
  if (text)
  {
    if(!icon::parseicon(text, rem, palette, numcolors, graphic)) return false;
    icon::seticon(player, idx, palette, graphic);
  }
  else
  {
    if (script_hud_elements[player][idx].blitter)
    {
      delete script_hud_elements[player][idx].blitter;
      script_hud_elements[player][idx].blitter = nullptr;
    }
  }
  return true;
}

void SetScriptHUDSquare(int player, int idx, int _color) {
  unsigned char palette[4]; /* short, I KNOW. */
  unsigned char graphic[256];
  player %= MAXIMUM_NUMBER_OF_NETWORK_PLAYERS;
  idx %= MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS;
  script_hud_elements[player][idx].color = _color % 8;
  memset(graphic, 0, 256);
  SDL_Color color = get_interface_color(_color + _computer_interface_text_color);
  palette[0] = color.r;
  palette[1] = color.g;
  palette[2] = color.b;
  palette[3] = 0xff;
  icon::seticon(player, idx, palette, graphic);
}
/* /SB */

void reset_messages()
{
    // ZZZ: reset screen_printf's
    for(int i = 0; i < NumScreenMessages; i++)
        Messages[i].ExpirationTime = machine_tick_count();
    /* SB: reset HUD elements */
    for(int p = 0; p < MAXIMUM_NUMBER_OF_NETWORK_PLAYERS; p++) {
        for(int i = 0; i < MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS; i++) {
            script_hud_elements[p][i].color = 1;
            script_hud_elements[p][i].text.clear();
            if (script_hud_elements[p][i].blitter)
            {
              delete script_hud_elements[p][i].blitter;
              script_hud_elements[p][i].blitter = nullptr;
            }
        }
    }
        nonlocal_script_hud = false;
}


// Globals for communicating with the SDL contents of DisplayText // fuck off
static SDL_Surface *DisplayTextDest = NULL;
static const font_t* DisplayTextFont = NULL;
static short DisplayTextStyle = 0;


// this is only called here
void DisplayText(short BaseX, short BaseY, const std::string& Text, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff)
{
    /*
    // OpenGL version:
    // activate only in the main view, and also if OpenGL is being used for the overhead map
    if((OGL_MapActive || !automap_is_visible()) && !computer_terminal_is_visible())
        if (OGL_RenderText(BaseX, BaseY, Text, r, g, b)) return;

    draw_text(DisplayTextDest, Text, BaseX+1, BaseY+1, SDL_MapRGB(world_pixels->format, 0x00, 0x00, 0x00), DisplayTextFont, DisplayTextStyle);
    draw_text(DisplayTextDest, Text, BaseX, BaseY, SDL_MapRGB(world_pixels->format, r, g, b), DisplayTextFont, DisplayTextStyle);
    */
}

void DisplayTextCursor(SDL_Surface *s, short BaseX, short BaseY, const std::string& Text, short Offset, unsigned char r = 0xff, unsigned char g = 0xff, unsigned char b = 0xff)
{
    /*
    SDL_Rect cursor_rect;
    int w = DisplayTextFont->measure_width(Text.substr(Offset)); // DisplayTextStyle);

    cursor_rect.x = BaseX + w;
    cursor_rect.w = 1;
    cursor_rect.y = BaseY - DisplayTextFont->ascent;
    cursor_rect.h = DisplayTextFont->height;
    
    SDL_Rect shadow_rect = cursor_rect;
    shadow_rect.x += 1;
    shadow_rect.y += 1;
    
    // OpenGL version:
    // activate only in the main view, and also if OpenGL is being used for the overhead map
    if((OGL_MapActive || !automap_is_visible()) && !computer_terminal_is_visible())
        if (OGL_RenderTextCursor(cursor_rect, r, g, b)) return;
    
    SDL_FillRect(s, &shadow_rect, SDL_MapRGB(world_pixels->format, 0x00, 0x00, 0x00));
    SDL_FillRect(s, &cursor_rect, SDL_MapRGB(world_pixels->format, r, g, b));
     */
}





const font_t* GetOnScreenFont()
{
    TODO("sort out fonts for display");
    /*
    // EES: goddamn artless shit, ridiculous polling; the font should be [re-]set any time the screen changes; this can be sorted once screen.cpp is ripped and rebuilt
    
    short NeededSize = screen_messages_font_key.size;
    
    int w, h;
    main_screen.get_window_coordinates_size(&w, &h);
    
    switch (graphics_preferences.hud_size)
    {
    case 1:
        if(h > 960) NeededSize *= 2;
        break;
    case 2:
        if(h > 480) NeededSize = NeededSize * h / 480;
        break;
    }
    if (!LoadedOnScreenFont || LoadedOnScreenFont->key.size != NeededSize)
    {
        font_key_t key = {screen_messages_font_key.font_id, screen_messages_font_key.style, NeededSize};
        LoadedOnScreenFont = get_font_for_key(key);
    }
    return LoadedOnScreenFont;
     */
}




void DisplayPosition(SDL_Surface *s)
{
    if (!ShowPosition) return;
        
    DisplayTextDest = s;
    DisplayTextFont = GetOnScreenFont();

    auto text_margins = lua_text_margins;
    short X0 = text_margins.left;
    short Y0 = text_margins.top;
    
    short LineSpacing = DisplayTextFont->line_height;
    short X = X0 + LineSpacing / 3;
    short Y = Y0 + LineSpacing;
    const float FLOAT_WORLD_ONE = float(WORLD_ONE);
    const float AngleConvert = 360/float(FULL_CIRCLE);
    
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "X       = %8.3f", main_camera_settings.origin.x/FLOAT_WORLD_ONE);
    DisplayText(X,Y,tmp);
    Y += LineSpacing;
    snprintf(tmp, sizeof(tmp), "Y       = %8.3f", main_camera_settings.origin.y/FLOAT_WORLD_ONE);
    DisplayText(X,Y,tmp);
    Y += LineSpacing;
    snprintf(tmp, sizeof(tmp), "Z       = %8.3f", main_camera_settings.origin.z/FLOAT_WORLD_ONE);
    DisplayText(X,Y,tmp);
    Y += LineSpacing;
    snprintf(tmp, sizeof(tmp), "Polygon = %8d", main_camera_settings.origin_polygon_index);
    DisplayText(X,Y,tmp);
    Y += LineSpacing;
    short Angle = main_camera_settings.yaw;
    if (Angle > HALF_CIRCLE) Angle -= FULL_CIRCLE;
    snprintf(tmp, sizeof(tmp), "Yaw     = %8.3f", AngleConvert*Angle);
    DisplayText(X,Y,tmp);
    Y += LineSpacing;
    Angle = main_camera_settings.pitch;
    if (Angle > HALF_CIRCLE) Angle -= FULL_CIRCLE;
    snprintf(tmp, sizeof(tmp), "Pitch   = %8.3f", AngleConvert*Angle);
    DisplayText(X,Y,tmp);
    
}


void DisplayInputLine(SDL_Surface *s)
{
    if (Console::instance()->input_active() && !Console::instance()->displayBuffer().empty())
    {
        DisplayTextDest = s;
        DisplayTextFont = GetOnScreenFont();
        
        auto text_margins = lua_text_margins;
        short X0 = text_margins.left;
        short Y0 = s->h - text_margins.bottom;
        
        short Offset = DisplayTextFont->line_height / 3;
        short X = X0 + Offset;
        short Y = Y0 - Offset;
        const std::string buf = Console::instance()->displayBuffer();
        DisplayText(X, Y, buf);
        DisplayTextCursor(s, X, Y, buf, Console::instance()->cursor_position());
    }
}


void DisplayMessages(SDL_Surface *s)
{
    DisplayTextDest = s;
    DisplayTextFont = GetOnScreenFont();

    auto text_margins = lua_text_margins;
    short X0 = text_margins.left;
    short Y0 = text_margins.top;
    
    short LineSpacing = DisplayTextFont->line_height;
    short X = X0 + LineSpacing / 3;
    short Y = Y0 + LineSpacing;
    if (ShowPosition) Y += 6 * LineSpacing;    // Make room for the position data
    short view = nonlocal_script_hud ? local_player_index : current_player_index;
    
    int logical_width, logical_height;
    main_screen.get_window_coordinates_size(logical_width, logical_height);
    
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS; ++i)
    {
        if (!script_hud_elements[view][i].text.empty())
        {
            short x2 = X, sk = DisplayTextFont->measure_width("AAAAAAAAAAAAAA"), icon_skip = 0, icon_drop = 0;
            
            switch (graphics_preferences.hud_size)
            {
            case 0:
                icon_drop = 2;
                break;
            case 1:
                icon_drop = (logical_height >= 960) ? 4 : 2;
                break;
            case 2:
                icon_drop = (logical_height >= 480) ? logical_height * 2 / 480 : 2;
                break;
            default:
                throw_bug_report_f("Invalid hud scale level: %d", graphics_preferences.hud_size);
            }
            bool had_icon = false;
            /* Yes, I KNOW this is the same i as above. I know what I'm doing. */
            for (i = 0; i < MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS; ++i)
            {
                if(script_hud_elements[view][i].text.empty()) continue;
                if(script_hud_elements[view][i].blitter)
                {
                    had_icon = true;

                    SDL_Rect rect;
                    rect.x = x2;
                    rect.y = Y - DisplayTextFont->ascent + DisplayTextFont->leading;
                    rect.w = rect.h = 16;
                    icon_skip = 20;
                    
                    switch (graphics_preferences.hud_size)
                    {
                    case 1:
                        if(logical_height >= 960)
                        {
                            rect.w *= 2;
                            rect.h *= 2;
                            icon_skip *= 2;
                        }
                        break;
                    case 2:
                        if(logical_height > 480)
                        {
                            rect.w = rect.w * logical_height / 480;
                            rect.h = rect.h * logical_height / 480;
                            icon_skip = icon_skip * logical_height / 480;
                        }
                        break;
                    }
                                        
                    script_hud_elements[view][i].blitter->render_to_screen(&rect);
                    
                    x2 += icon_skip;
                }
                SDL_Color color = get_interface_color(script_hud_elements[view][i].color+_computer_interface_text_color);
                DisplayText(x2, Y + (script_hud_elements[view][i].blitter ? icon_drop : 0), script_hud_elements[view][i].text, color.r, color.g, color.b);
                x2 += sk;
                if (script_hud_elements[view][i].blitter) x2 -= icon_skip;
            }
            Y += LineSpacing;
            if (had_icon) Y += icon_drop;
            break;
        }
    }
    
    for (int k = NumScreenMessages - 1; k >= 0; k--)
    {
      int Which = (MostRecentMessage+NumScreenMessages-k) % NumScreenMessages;
        while (Which < 0)
            Which += NumScreenMessages;
        ScreenMessage& Message = Messages[Which];
        if (static_cast<int32_t>(Message.ExpirationTime - machine_tick_count()) < 0)
        {
            continue;
        }
        
        DisplayText(X,Y,Message.Text);
        Y += LineSpacing;
    }

}


extern short local_player_index;

static const SDL_Color Green  = { 0x00, 0xff, 0x00, 0xff };
static const SDL_Color Yellow = { 0xff, 0xff, 0x00, 0xff };
static const SDL_Color Red    = { 0xff, 0x00, 0x00, 0xff };
static const SDL_Color Gray   = { 0x7f, 0x7f, 0x7f, 0xff };


void DisplayScores(SDL_Surface *s)
{
    if (!game_is_networked() || !ShowScores) return;

    // assume a proportional font
    int CWidth = DisplayTextFont->measure_width("W");

    // field widths
    static const int kNameWidth = 20;
    int WName = CWidth * kNameWidth;
    static const int kScoreWidth = 5;
    int WScore = CWidth * kScoreWidth;
    static const int kPingWidth = 7;
    int WPing = CWidth * kPingWidth;
    int WJitter = CWidth * kPingWidth;
    int WErrors = CWidth * kPingWidth;
    static const int kIdWidth = 2;
    int WId = CWidth * kIdWidth;

    DisplayTextDest = s;
    DisplayTextFont = GetOnScreenFont();

    int H = DisplayTextFont->line_height * (get_number_of_players() + 1);
    int W = WName + WScore + WPing + WJitter + WErrors + WId;

    auto text_margins = lua_text_margins;
    int X = text_margins.left + (s->w - text_margins.right - W) / 2;
    int Y = std::max(text_margins.top + (s->h - text_margins.bottom - H) / 2, DisplayTextFont->line_height * (NumScreenMessages + 1));

    int XName = X;
    int XScore = XName + WName + CWidth;
    int XPing = XScore + WScore + CWidth;
    int XJitter = XPing + WPing + CWidth;
    int XErrors = XJitter + WPing + CWidth;
    int XId = XErrors + WPing + CWidth;

    // draw headers
    DisplayText(XName, Y, "Name", 0xbf, 0xbf, 0xbf);
    DisplayText(XScore + WScore - DisplayTextFont->measure_width("Score"), Y, "Score", 0xbf, 0xbf, 0xbf);
    DisplayText(XPing + WPing - DisplayTextFont->measure_width("Delay"), Y, "Delay", 0xbf, 0xbf, 0xbf);
    DisplayText(XJitter + WPing - DisplayTextFont->measure_width("Jitter"), Y, "Jitter", 0xbf, 0xbf, 0xbf);
    DisplayText(XErrors + WPing - DisplayTextFont->measure_width("Errors"), Y, "Errors", 0xbf, 0xbf, 0xbf);
    DisplayText(XId + WId - DisplayTextFont->measure_width("ID"), Y, "ID", 0xbf, 0xbf, 0xbf);
    Y += DisplayTextFont->line_height;
    player_rankings_t rankings;
    calculate_player_rankings(rankings);
    for (int i = 0; i < get_number_of_players(); ++i)
    {
        Player *player = get_player_data(rankings[i].player_index);

        SDL_Color color = get_interface_color(PLAYER_COLOR_BASE_INDEX + player->color);
        
        std::string name(player->name);
        DisplayText(XName, Y, name.c_str(), color.r, color.g, color.b);
        
        std::string ranking_text = calculate_ranking_text(rankings[i].ranking);
        DisplayText(XScore + WScore - DisplayTextFont->measure_width(ranking_text.c_str()), Y, ranking_text.c_str(), color.r, color.g, color.b);
        
        const NetworkStats& stats = NetGetStats(rankings[i].player_index);
        
        std::string latency_text;
        SDL_Color color2 = Gray;
        switch (stats.latency)
        {
            case NetworkStats::invalid:
                latency_text = " ";
                break;
            case NetworkStats::disconnected:
                latency_text = "DC";
                break;
            default:
                latency_text = std::to_string(stats.latency) + " ms";
                
                if (stats.latency < 150)
                    color2 = Green;
                else if (stats.latency < 350)
                    color2 = Yellow;
                else
                    color2 = Red;
        }
        std::string tmp;
        // TODO: FIX: no idea what these 2 lines are up to; Dog knows who wrote to the global buffer last
        //temporary[kPingWidth + 1] = '\0';
        //DisplayText(XPing + WPing - DisplayTextFont->measure_width(temporary), Y, temporary, color2.r, color2.g, color2.b);
        
        if (stats.jitter == NetworkStats::invalid)
        {
            tmp = " ";
        }
        else if (stats.jitter == NetworkStats::disconnected)
        {
            tmp = "DC";
        }
        else
        {
            tmp = std::to_string(stats.jitter) + " ms";
        }
        if (stats.jitter == NetworkStats::invalid || stats.jitter == NetworkStats::disconnected)
        {
            color2 = Gray;
        }
        else if (stats.jitter < 75)
        {
            color2 = Green;
        }
        else if (stats.jitter < 150)
        {
            color2 = Yellow;
        }
        else
        {
            color2 = Red;
        }
        DisplayText(XJitter + WPing - DisplayTextFont->measure_width(tmp), Y, tmp, color2.r, color2.g, color2.b);

        tmp = std::to_string(stats.errors);
        //temporary[kPingWidth + 1] = '\0';
        if (stats.errors > 0)
            color2 = Yellow;
        else
            color2 = Green;
        DisplayText(XErrors + WPing - DisplayTextFont->measure_width(tmp), Y, tmp, color2.r, color2.g, color2.b);

        tmp = std::to_string(rankings[i].player_index);
        DisplayText(XId + WId - DisplayTextFont->measure_width(tmp), Y, tmp, color.r, color.g, color.b);

        Y += DisplayTextFont->line_height;
    }
}


void DisplayNetLoadingScreen(SDL_Surface* s)
{
    // assume a proportional font
    int CWidth = DisplayTextFont->measure_width("W");

    // field widths
    static const int kNameWidth = 20;
    int WName = CWidth * kNameWidth;
    static const int kStatusWidth = 20;
    int WStatus = CWidth * kStatusWidth;

    DisplayTextDest = s;
    DisplayTextFont = GetOnScreenFont();

    int H = DisplayTextFont->line_height * (get_number_of_players() + 1);
    int W = WName + WStatus;

    int X = (s->w - W) / 2;
    int Y = std::max((s->h - H) / 2, DisplayTextFont->line_height * (NumScreenMessages + 1));

    int XName = X;
    int XStatus = XName + WName + CWidth;

    // draw headers
    DisplayText(XName, Y, "Name", 0xbf, 0xbf, 0xbf);
    DisplayText(XStatus + WStatus - DisplayTextFont->measure_width("Status"), Y, "Status", 0xbf, 0xbf, 0xbf);

    Y += DisplayTextFont->line_height;

    auto nb_loading_dots = ((uint64_t)(machine_tick_count() / (2000.f / 3)) % 4);

    for (int i = 0; i < get_number_of_players(); ++i)
    {
        const auto& player = get_player_data(i);
        const auto& stats = NetGetStats(i);

        SDL_Color color = get_interface_color(PLAYER_COLOR_BASE_INDEX + player->color);

        std::string name(player->name);
        DisplayText(XName, Y, name.c_str(), color.r, color.g, color.b);

        std::string player_status;

        switch (stats.pregame_state)
        {
            case NetworkStats::valid:
                color = Green;
                player_status = "Ready";
                break;
            case NetworkStats::disconnected:
                color = Red;
                player_status = "Disconnected";
                break;
            case NetworkStats::invalid:
            default:
                color = Yellow;
                player_status = "Loading" + std::string(nb_loading_dots, '.') + std::string(3 - nb_loading_dots, ' ');
                break;
        }
        
        DisplayText(XStatus + WStatus - DisplayTextFont->measure_width(player_status), Y, player_status, color.r, color.g, color.b);

        Y += DisplayTextFont->line_height;
    }
}





void update_fps_display(SDL_Surface *s)
{
    if (graphics_preferences.show_fps && !player_in_terminal_mode(current_player_index))
    {
        char fps[sizeof("1000 fps (10000 ms)")];
        char ms[sizeof("(10000 ms)")];

        fps_counter.update();

        if (!fps_counter.ready())
        {
            strcpy(fps, "--");
        }
        else
        {
            
            int latency = NetGetLatency();
            if (latency > -1)
                snprintf(ms, sizeof(ms), "(%i ms)", std::min(latency, 10000));
            else
                ms[0] = '\0';
            
            snprintf(fps, sizeof(fps), "%0.f fps %s", fps_counter.get(), ms);
        }

        DisplayTextDest = s;
        DisplayTextFont = GetOnScreenFont();

        auto text_margins = lua_text_margins;
        short X0 = text_margins.left;
        short Y0 = s->h - text_margins.bottom;

        // The line spacing is a generalization of "5" for larger fonts
        short Offset = DisplayTextFont->line_height / 3; // EES: TODO: this was Font.LineSpacing, which I'm guessing is line_height
        short X = X0 + Offset;
        short Y = Y0 - Offset;
        if (Console::instance()->input_active())
        {
            Y -= DisplayTextFont->line_height;
        }
        DisplayText(X,Y,fps);
        
    }
    else
    {
        fps_counter.reset();
    }
}

