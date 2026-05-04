/*
 *  network_dialog_widgets_sdl.cpp

	Copyright (C) 2001 and beyond by Woody Zenfell, III
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

 *  Implementation of network-dialog-specific widgets in the SDL dialog system.
 *
 *  Created by Woody Zenfell, III on Fri Sep 28 2001.
 *
 *  Mar 1, 2002 (Woody Zenfell): Added new w_entry_point_selector widget.
 */

#if !defined(DISABLE_NETWORKING)

#include	"network_dialog_widgets_sdl.h"

#include	"screen_drawing.h"
#include	"fonts.hpp"
#include	"interface.hpp"
#include	"network.h"

// these next are for playing with shape-drawing
#include	"player.h"
//#include	"HUDRenderer.h"
#include	"shell.h"
#include	"collection_definition.h"

// here are some for w_entry_point_selector
#include    "preferences.hpp"
#include    "Screen.hpp"

#include	"TextLayoutHelper.h"

#include "network_games.h" // calculate_ranking_text_for_post_game


void w_found_players::found_player(prospective_joiner_info &player)
{
    found_players.push_back(player);
    list_player(player);
}


void w_found_players::hide_player(const prospective_joiner_info &player)
{
    found_players.push_back(player);
    unlist_player(player);
}


void w_found_players::list_player(prospective_joiner_info &player)
{
    listed_players.push_back(player);
    new_items();
}


void w_found_players::update_player(prospective_joiner_info &player)
{
    unlist_player(player);
    list_player(player);
}


void w_found_players::unlist_player(const prospective_joiner_info &player)
{
    auto player_index = std::distance(listed_players.begin(), std::find(listed_players.begin(), listed_players.end(), player));
    if (player_index >= listed_players.size()) return;
    
    listed_players.erase(listed_players.begin() + player_index);
    
    int32_t old_top_item = top_item;
    
    int32_t num_items = (int32_t)listed_players.size();
    new_items();
    
    // If the element deleted was the top item or before the top item, shift view up an item to compensate (if there is anything "up").
    if (player_index <= old_top_item && old_top_item > 0) { old_top_item--; }
    
    // Reconcile overhang, if needed.
    set_top_item(old_top_item + shown_items > num_items && num_items >= shown_items ? (int32_t)listed_players.size() - shown_items
                                                                                    : old_top_item);
}


void w_found_players::item_selected()
{
    if (player_selected_callback) { player_selected_callback(this, listed_players[get_selection()]); }
}


// ZZZ: this is pretty ugly, it assumes that the callback will remove players from the widget.
// Fortunately, that's the case currently.  :)
void w_found_players::callback_on_all_items()
{
    if (player_selected_callback)
    {
        for (auto& it : listed_players)
        {
            player_selected_callback(this, it);
        }
    }
}


void w_found_players::draw_item(std::vector<prospective_joiner_info>::const_iterator i, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected)
{
    auto text = std::string(i->name) + (i->gathering ? " (gathering)" : "");
    int computed_x = x + (width - get_font()->measure_width(text)) / 2;
    int computed_y = y + get_font()->ascent;
    int text_state = i->gathering ? DISABLED_STATE : selected ? ACTIVE_STATE : DEFAULT_STATE;
    canvas->draw_text(text, get_font(), get_theme_color(ITEM_WIDGET, text_state), {computed_x, computed_y});
}


////// w_players_in_game2 //////

// I guess these should be computed more dynamically, but it wasn't obvious the best way to do that.
// These values work well for the standard player shapes, anyway.
enum {
    kWPIG2Width = 600,		// widget width
    kWPIG2Height = 142,		// widget height (actual height will differ if postgame_layout)
    kMaxHeadroom = 53,		// height above player origin (approx. navel) of tallest player shape
    kNameOffset = 80,		// how far below player origin baseline of player's name should appear
    kNumNameOffsets = MAXIMUM_NUMBER_OF_PLAYERS,	// used to resolve overlapping names
    kNameMargin = 6,		// names overlap if their edges are fewer than this many pixels apart
    kNormalPlayerOffset		= kMaxHeadroom,
    kNormalNameTotalOffset	= kNormalPlayerOffset + kNameOffset,

//    kPostgameTopMargin = 70,	// how much extra space is at the top of widget in postgame layout
    kPostgameTopMargin = 190,	// For postgame layout without chat window, we can use a lot more space.  (use 70 to coexist with full chat UI)
    kPostgameBottomMargin = 6,	// how much extra space is at the bottom of widget in postgame layout
    kBarBottomOffset = 80,	// how far below player origin score/kill bars should start
    kBarWidth = 10,		// how wide a kill/score bar should be
    kBarOffsetX = 20,		// how much to offset a bar so it won't draw directly on a player
    kBevelSize = 2,		// how much "depth effect" (in pixels around the border) bars have
    kUseLegendThreshhold = 5,	// with this many players or more, use legend for kills/deaths rather than print at bar labels
    kPostgamePlayerOffset	= kPostgameTopMargin + kMaxHeadroom,
    kPostgameNameTotalOffset	= kPostgamePlayerOffset + kNameOffset,
    kBarBottomTotalOffset	= kPostgamePlayerOffset + kBarBottomOffset,
    kPostgameHeight		= kPostgameTopMargin + kWPIG2Height + kPostgameBottomMargin
};

/*
// These can't see postgame_layout.  Duh.  And the idea here was to avoid having the constants above
// in a header file (as would be needed for making inline methods) where they would force extra
// recompilation... burrito.  Macros it is.
static inline int
get_player_y_offset() { return postgame_layout ? kPostgamePlayerOffset : kNormalPlayerOffset; }

static inline int
get_name_y_offset() { return postgame_layout ? kPostgameNameTotalOffset : kNormalNameTotalOffset; }
*/

#define get_player_y_offset() (postgame_layout ? kPostgamePlayerOffset : kNormalPlayerOffset)
#define get_name_y_offset() (postgame_layout ? kPostgameNameTotalOffset : kNormalNameTotalOffset)

// Here I divide each piece of space into N pieces (where N is the number of things to draw)
// each item is drawn in the center of its space.  This pitches them a little more widely than
// is used in the separately-drawn strategy.
// The computation used is (I from 0 to N-1, W is width) for the center:
// ((I + .5) / N) * W
// == WI + .5W / N
// == W*(2I + 1) / 2N
static inline int get_wide_spaced_center_offset(int left_x, int available_width, size_t index, size_t num_items)
{
    return left_x + (((2 * (int)index + 1) * available_width) / (2 * (int)num_items));
}


// for the left:
// I/N * W
// == WI/N
static inline int get_wide_spaced_left_offset(int left_x, int available_width, size_t index, size_t num_items)
{
    return left_x + (((int)index * available_width) / (int)num_items);
}


// width is easy...
// note though that the actual distances between left_offsets may vary slightly from this width due to rounding.
static inline int get_wide_spaced_width(int available_width, size_t num_items)
{
    return available_width / (int)num_items;
}


// Horizontal layout centers single player at 1/2 the width; two players at 1/3 and 2/3; three at 1/4, 2/4, 3/4....
// Doing (I * W) / N rather than the more natural (I/N) * W may give more accurate results with integer math.
static inline int get_close_spaced_center_offset(int left_x, int available_width, size_t index, size_t num_items)
{
    return left_x + ((((int)index + 1) * available_width) / ((int)num_items + 1));
}

static inline int get_close_spaced_width(int available_width, size_t num_items)
{
    return available_width / ((int)num_items + 1);
}


w_players_in_game2::w_players_in_game2(bool inPostgameLayout)
    : widget(MESSAGE_WIDGET), displaying_actual_information(false), postgame_layout(inPostgameLayout), draw_carnage_graph(false),
      num_valid_net_rankings(0), selected_player(NONE), clump_players_by_team(false), draw_scores_not_carnage(false)
{
    rect.w = kWPIG2Width;
    rect.h = postgame_layout ? kPostgameHeight : kWPIG2Height;

    saved_min_width = rect.w;
    saved_min_height = rect.h;
}


w_players_in_game2::~w_players_in_game2()
{
    clear_vector();
}


void w_players_in_game2::update_display(bool inFromDynamicWorld) // default=false
{
    // Start over - wipe out our local player-storage
    clear_vector();
    
    // Wipe out references to players through teams // ick
    for (int i = 0; i < NUMBER_OF_TEAM_COLORS; i++) { players_on_team[i].clear(); }
    
    // Find the number of players
    int num_players = inFromDynamicWorld ? get_number_of_players() : (displaying_actual_information ? NetGetNumberOfPlayers() : 0);
    
    // Fill in the entries
    for (int i = 0; i < num_players; i++)
    {
        player_entry2 thePlayerEntry;
        
        int	team_color;
        int	player_color;
        
        if (inFromDynamicWorld) // Get player information from dynamic_world
        {
            Player* player = get_player_data(i);
            thePlayerEntry.player_name = player->name;
            team_color   = player->team;
            player_color = player->color;
        }
        else // Get player information from topology
        {
            player_info* player = NetGetPlayerData(i);
            thePlayerEntry.player_name = player->name;
            team_color   = player->team;
            player_color = player->color;
        }
        
        // Set the size of the text
        const font_t* shadow_font = get_font()->shadowed();
        
        thePlayerEntry.name_width = shadow_font->measure_width(thePlayerEntry.player_name);
        
        // Get the pixel-color for the player's team (for drawing the name)
        thePlayerEntry.name_color = get_player_color(team_color);
        
        // Set up a player image for the player (funfun)
        thePlayerEntry.player_image = new PlayerImage;
        thePlayerEntry.player_image->setRandomFlatteringView();
        thePlayerEntry.player_image->setPlayerColor(player_color);
        thePlayerEntry.player_image->setTeamColor(team_color);
        
        // Add the player to our local storage area
        player_entries.push_back(thePlayerEntry);
        
        // Add a reference to the player through his team color
        players_on_team[team_color].push_back(i);
    }
    
    dirty = true;
}


void w_players_in_game2::click(int x, int) {
    if(draw_carnage_graph) {

        if(clump_players_by_team) {
            for(size_t i = 0; i < num_valid_net_rankings; i++) {
                if(std::abs(x - get_wide_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings))
                        < (get_wide_spaced_width(rect.w, num_valid_net_rankings) / 2))
                {
                    if(element_clicked_callback != NULL)
                        element_clicked_callback(this, clump_players_by_team, draw_carnage_graph, draw_scores_not_carnage,
                                                    i, net_rankings[i].color);

                    break;
                }
            }
        }
        
        else {
            for(size_t i = 0; i < num_valid_net_rankings; i++) {
                if(std::abs(x - get_close_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings))
                        < (get_close_spaced_width(rect.w, num_valid_net_rankings) / 2))
                {
                    if(element_clicked_callback != NULL)
                        element_clicked_callback(this, clump_players_by_team, draw_carnage_graph, draw_scores_not_carnage,
                                                    i, net_rankings[i].player_index);

                    break;
                }
            }
        }
    
    } // draw_carnage_graph
}

// enable carnage reporting mode and set the data needed to draw a graph.
void w_players_in_game2::set_graph_data(const net_rank* inRankings, int inNumRankings, int inSelectedPlayer,
                                        bool inClumpPlayersByTeam, bool inDrawScoresNotCarnage)
{
    draw_carnage_graph      = true;
    num_valid_net_rankings  = inNumRankings;
    selected_player         = inSelectedPlayer;
    clump_players_by_team   = inClumpPlayersByTeam;
    draw_scores_not_carnage = inDrawScoresNotCarnage;
    memcpy(net_rankings, inRankings, inNumRankings * sizeof(net_rank));

    dirty                   = true;
}


void w_players_in_game2::draw_player_icon(Canvas* canvas, size_t rank_index, int center_x)
{
    // Note, player images will not be re-fetched unless the brightness has *changed* since last draw.
    PlayerImage* theImage = player_entries[net_rankings[rank_index].player_index].player_image;
    theImage->setBrightness((selected_player != NONE && selected_player != rank_index) ? 0.4f : 1.0f);
    theImage->drawAt(canvas, center_x, rect.y + get_player_y_offset());
}


void w_players_in_game2::draw_player_icons_separately(Canvas* canvas)
{
    if (draw_carnage_graph) // Draw in sorted order (according to net_rankings)
    {
        for (int32_t i = 0; i < num_valid_net_rankings; i++)
        {
            int32_t center_x = get_close_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings);
            draw_player_icon(canvas, i, center_x);
        }
    }
    else // Draw in "natural order" (according to topology)
    {
        size_t number_of_players = player_entries.size();
        for (int32_t i = 0; i < number_of_players; i++)
        {
            int32_t center_x = get_close_spaced_center_offset(rect.x, rect.w, i, number_of_players);
            player_entries[i].player_image->drawAt(canvas, center_x, rect.y + get_player_y_offset());
        }
    }        
} // draw_player_icons_separately


void w_players_in_game2::draw_player_icons_clumped(Canvas* canvas)
{
    assert_fail(draw_carnage_graph, "");
    
    int	width_per_team = get_wide_spaced_width(rect.w, num_valid_net_rankings);
 
    // Walk through teams, drawing each batch.   
    for (int32_t i = 0; i < num_valid_net_rankings; i++)
    {
        int32_t team_left_x = get_wide_spaced_left_offset(rect.x, rect.w, i, num_valid_net_rankings);
        
        size_t theNumberOfPlayersOnThisTeam = players_on_team[net_rankings[i].color].size();

        assert_fail(theNumberOfPlayersOnThisTeam > 0, "");
        
        // Walk through players on a team to draw a batch.
        for (int32_t j = 0; j < theNumberOfPlayersOnThisTeam; j++)
        {
            int player_center_x = get_close_spaced_center_offset(team_left_x, width_per_team, j, theNumberOfPlayersOnThisTeam);
            
            // Note, player images will not be re-fetched unless the brightness has *changed* since last draw.
            // Though Marathon does not let one view team vs team carnage (just total team carnage), I'm leaving
            // the highlighting stuff here in case team view is later added.
            PlayerImage* theImage = player_entries[players_on_team[net_rankings[i].color][j]].player_image;
            theImage->setBrightness((selected_player != NONE && selected_player != i) ? 0.4f : 1.0f);
    
            theImage->drawAt(canvas, player_center_x, rect.y + get_player_y_offset());
        } // players
    } // teams
} // draw_player_icons_clumped


void w_players_in_game2::draw_player_names_separately(Canvas* canvas, TextLayoutHelper& ioTextLayoutHelper)
{
    // Now let's draw the names.  Let's take care to offset names vertically if they would
    // overlap (or come too close as defined by kNameMargin), so it's more readable.

    const font_t* shadow_font = get_font()->shadowed();

    size_t theNumPlayers = draw_carnage_graph ? num_valid_net_rankings : player_entries.size();
    
    for (size_t i = 0; i < theNumPlayers; i++)
    {
        int center_x = get_close_spaced_center_offset(rect.x, rect.w, i, theNumPlayers);
        const player_entry2* theEntry = draw_carnage_graph ? &player_entries[net_rankings[i].player_index] : &player_entries[i];
        int name_x = center_x - (theEntry->name_width / 2);
        int name_y = rect.y + get_name_y_offset();

        // Find a suitable vertical offset
        name_y = ioTextLayoutHelper.reserveSpaceFor(name_x - kNameMargin / 2, theEntry->name_width + kNameMargin, name_y, get_font()->line_height);
        
        canvas->draw_text(theEntry->player_name, shadow_font, theEntry->name_color, {name_x, name_y});
    }
}


void w_players_in_game2::draw_player_names_clumped(Canvas* canvas, TextLayoutHelper& ioTextLayoutHelper)
{
    // Now let's draw the names.  Let's take care to offset names vertically if they would
    // overlap (or come too close as defined by kNameMargin), so it's more readable.

    const font_t* shadow_font = get_font()->shadowed();
    
    // Walk through teams, drawing each batch.
    for (size_t i = 0; i < num_valid_net_rankings; i++)
    {
        int team_center_x = get_wide_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings);
        
        size_t theNumberOfPlayersOnThisTeam = players_on_team[net_rankings[i].color].size();

        assert_fail(theNumberOfPlayersOnThisTeam > 0, "");
        
        // Walk through players on a team to draw a batch.
        for(size_t j = 0; j < theNumberOfPlayersOnThisTeam; j++) {
    
            const player_entry2* theEntry = &(player_entries[players_on_team[net_rankings[i].color][j]]);
            int name_x = team_center_x - (theEntry->name_width / 2);
            int name_y = rect.y + get_name_y_offset();
    
            // Find a suitable vertical offset
            name_y = ioTextLayoutHelper.reserveSpaceFor(name_x - kNameMargin/2, theEntry->name_width + kNameMargin,
                                                            name_y, get_font()->line_height);
    
            canvas->draw_text(theEntry->player_name, shadow_font, theEntry->name_color, {name_x, name_y});
        }
    }
}


int w_players_in_game2::find_maximum_bar_value()
{
    int	theMaxValue = INT_MIN;

    // We track min also to handle games with negative scores.
    int theMinValue = INT_MAX;

    if(selected_player != NONE)
        // This way, all player vs player graphs use the same scale.
        theMaxValue = calculate_max_kills(num_valid_net_rankings);
    else {
        // Note this does the right thing for suicide bars as well.
        if(draw_scores_not_carnage) {
            for(size_t i = 0; i < num_valid_net_rankings; i++) {
                if(net_rankings[i].game_ranking > theMaxValue)
                    theMaxValue = net_rankings[i].game_ranking;
                if(net_rankings[i].game_ranking < theMinValue)
                    theMinValue = net_rankings[i].game_ranking;
            }
        } else {
            for(size_t i = 0; i < num_valid_net_rankings; i++) {
                if(net_rankings[i].kills > theMaxValue)
                    theMaxValue = net_rankings[i].kills;
                if(net_rankings[i].deaths > theMaxValue)
                    theMaxValue = net_rankings[i].deaths;
            }
        }
    }

    // If all values were nonpositive, and we had at least one negative, we
    // return the (negative) value furthest from 0.
    // The Mac version seems to do nothing of the sort - how can it possibly
    // display correct bars for games with negative scores like "Tag"??
    if(theMaxValue <= 0 && theMinValue < 0)
	{
        theMaxValue = theMinValue;
	}
	// if there is a mix of positive and negative, draw two different colored
	// bars
	else if (theMinValue < 0)
	{
		theMaxValue = std::max(std::abs(theMinValue), std::abs(theMaxValue));
	}

    return theMaxValue;
}


struct bar_info
{
    int32_t	    center_x;
    int32_t     top_y;
    SDL_Color   color;
    std::string label_text;
};


void w_players_in_game2::draw_bar_or_bars(Canvas* canvas, size_t rank_index, int32_t center_x,
                                          int32_t maximum_value, std::vector<bar_info>& results)
{
    if (draw_scores_not_carnage) // Draw score bar
    {
        int32_t score = net_rankings[rank_index].game_ranking;

        bar_info bar_info;
        bar_info.label_text = calculate_ranking_text_for_post_game(score);  // this makes a copy
        
        if ((score < 0) && ((score < 0) != (maximum_value <= 0))) { score = -score; }
        draw_bar(canvas, center_x, _score_color, score, maximum_value, bar_info);
        
        // Don't draw a "0" score label
        if (score != 0) { results.push_back(bar_info); }
    }
    else if (rank_index == selected_player) // Draw carnage bar(s)
    {
        // Draw suicides/friendly-fires
        bar_info bar_info;
        int32_t suicides = net_rankings[rank_index].kills;
        
        bar_info.label_text = get_string(STRID(strNET_STATS_STRINGS, strSUICIDES_STRING), {
            {"$count$", [suicides]{ return std::to_string(suicides); }},
        });
        
        draw_bar(canvas, center_x, _suicide_color, suicides, maximum_value, bar_info);
        
        // Don't push a "0" label.
        if (suicides > 0) { results.push_back(bar_info); }
    }
    else // Draw kills and deaths
    {
        int32_t kills  = net_rankings[rank_index].kills;
        int32_t deaths = net_rankings[rank_index].deaths;
        
        // Construct labels
        std::string kills_text  = std::to_string(kills);
        std::string deaths_text = std::to_string(deaths);
        
        // If more than threshhold bar-pairs to draw, use short form with legend rather than normal (long) form.
        if (num_valid_net_rankings < kUseLegendThreshhold)
        {
            kills_text  = get_string(STRID(strNET_STATS_STRINGS, strKILLS_STRING), {
                {"$count$", [kills_text]{ return kills_text; }},
            });
            deaths_text = get_string(STRID(strNET_STATS_STRINGS, strDEATHS_STRING), {
                {"$count$", [deaths_text]{ return deaths_text; }},
            });
        }
        
        // Set up bar_infos
        bar_info kills_info, deaths_info;
        kills_info.label_text  = kills_text;
        deaths_info.label_text = deaths_text;
        
        // Draw the shorter bar in front - looks nicer. If equal, draw kills in front.
        // Put shorter bar_info in vector first so its label doesn't "leapfrog" the taller bar label in case of conflict.
        // Don't put "0"s into the vector.
        if (kills > deaths) // Deaths bar is shorter - draw it in front
        {
            draw_bar(canvas, center_x - kBarWidth / 3, _kill_color,  kills,  maximum_value, kills_info);
            draw_bar(canvas, center_x + kBarWidth / 3, _death_color, deaths, maximum_value, deaths_info);
            
            if (deaths > 0) { results.push_back(deaths_info); }
            if (kills > 0)  { results.push_back(kills_info); }
        }
        else // Kills bar is shorter or equal - draw it in front
        {
            draw_bar(canvas, center_x + kBarWidth / 3, _death_color, deaths, maximum_value, deaths_info);
            draw_bar(canvas, center_x - kBarWidth / 3, _kill_color,  kills,  maximum_value, kills_info);
            
            if(kills > 0)  { results.push_back(kills_info); }
            if(deaths > 0) { results.push_back(deaths_info); }
        }
    }
}


void w_players_in_game2::draw_bars_separately(Canvas* canvas, std::vector<bar_info>& outBarInfos)
{
    // Find the largest value we'll be drawing, so we know how to scale our bars.
    int theMaxValue = find_maximum_bar_value();
    
    // Draw the bars.
    for(size_t i = 0; i < num_valid_net_rankings; i++) {
        int center_x = get_close_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings);

        draw_bar_or_bars(canvas, i, center_x + kBarOffsetX, theMaxValue, outBarInfos);
    }
}


void w_players_in_game2::draw_bars_clumped(Canvas* canvas, std::vector<bar_info>& outBarInfos)
{
    // Find the largest value we'll be drawing, so we know how to scale our bars.
    int theMaxValue = find_maximum_bar_value();
    
    // Walk through teams, drawing each batch.   
    for(size_t i = 0; i < num_valid_net_rankings; i++) {
        int team_center_x = (int)(rect.x + ((2*i + 1) * rect.w) / (2 * num_valid_net_rankings));
        
        size_t theNumberOfPlayersOnThisTeam = players_on_team[net_rankings[i].color].size();

        assert_fail(theNumberOfPlayersOnThisTeam > 0, "");

        // We will offset if we would draw on top of a player (i.e. if num players is odd), else
        // we will draw right smack in the middle.
        int center_x = team_center_x;
        
        if(theNumberOfPlayersOnThisTeam % 2 == 1)
            center_x += kBarOffsetX;

        draw_bar_or_bars(canvas, i, center_x, theMaxValue, outBarInfos);
    } // walk through rankings
} // draw_bars_clumped


void w_players_in_game2::draw_carnage_totals(Canvas* canvas)
{
    const font_t* shadow_font = get_theme_font(LABEL_WIDGET)->shadowed();
    
    for (size_t i = 0; i < num_valid_net_rankings; i++)
    {
        int center_x = clump_players_by_team ? get_wide_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings)
                                             : get_close_spaced_center_offset(rect.x, rect.w, i, num_valid_net_rankings);

        // Draw carnage score for player/team (list -N for N suicides)
        int	thePlayerCarnageScore = (selected_player == i) ? -net_rankings[i].kills : net_rankings[i].kills - net_rankings[i].deaths;
        std::string tmp;
        if (thePlayerCarnageScore < 0)
            tmp = "-" + std::to_string(thePlayerCarnageScore);
        if (thePlayerCarnageScore > 0)
            tmp = "+" + std::to_string(thePlayerCarnageScore);
        else
            tmp = "0";
        
        canvas->draw_text(tmp, shadow_font, {0xff, 0xff, 0xff, 0xff}, {center_x - shadow_font->measure_width(tmp) / 2, rect.y + rect.h - 1});
    }
}


const SDL_Color suicide_color = {0xff, 0xff, 0x00, 0xff};
const SDL_Color kill_color    = {0xff, 0x00, 0x00, 0xff};
const SDL_Color death_color   = {0xea, 0xea, 0xea, 0xff};
const SDL_Color score_color   = {0xea, 0xea, 0xea, 0xff};


SDL_Color get_net_color(int32_t index)
{
    switch (index)
    {
        case _suicide_color:
            return suicide_color;
        case _kill_color:
            return kill_color;
        default: // death/score
            return score_color;
    }
}


SDL_Color dim_color(SDL_Color color, int32_t percentage)
{
    return {(uint8_t)((color.r * percentage) / 100), (uint8_t)((color.g * percentage) / 100), (uint8_t)((color.b * percentage) / 100)};
}


void w_players_in_game2::draw_carnage_legend(Canvas* canvas)
{
    canvas->draw_text(get_string(STRID(strNET_STATS_STRINGS, strKILLS_LEGEND)),
                      get_font(), dim_color(kill_color, 70), {rect.x, rect.y + get_font()->line_height});
    
    canvas->draw_text(get_string(STRID(strNET_STATS_STRINGS, strDEATHS_LEGEND)),
                      get_font(), dim_color(death_color, 70), {rect.x, rect.y + 2 * get_font()->line_height});
}


void w_players_in_game2::draw_bar_labels(Canvas* canvas, const std::vector<bar_info>& inBarInfos, TextLayoutHelper& ioTextLayoutHelper)
{
    const font_t* shadow_font = get_font()->shadowed();
    
    for (const bar_info& theBarInfo : inBarInfos)
    {
        int theStringWidth = shadow_font->measure_width(theBarInfo.label_text);
        int theTextX = theBarInfo.center_x - theStringWidth / 2;
        int theBestY = ioTextLayoutHelper.reserveSpaceFor(theTextX - kNameMargin/2,
                            theStringWidth + kNameMargin, theBarInfo.top_y - 1, get_font()->line_height);

        canvas->draw_text(theBarInfo.label_text.c_str(), shadow_font, theBarInfo.color, {theTextX, theBestY});
    }
} // draw_bar_labels


void w_players_in_game2::draw(Canvas* canvas)
{
//    printf("widget top is %d, bottom is %d\n", rect.y, rect.y + rect.h);

    // Set clip rectangle so we don't color outside the lines
    canvas->set_clip(rect);

// Did some tests here - it seems that text drawing is clipped by this rectangle, but rect-filling
// and blitting are not.  (at least, on the Mac OS X version of SDL.  actually, in Win 98 too.)
// This means that little tiny chunks of pistol fire, feet, etc. can stick around if they are drawn
// outside the widget (because they won't be cleared away when the widget is redrawn).  I'm surrounding
// that with a "Somebody Else's Problem" field for the time being.
//    canvas->set_clip({300, 100, 400, 200});
//    printf("clipped at <%d %d %d %d>\n", rect.y, rect.x, rect.y + rect.h, rect.x + rect.w);

    // theTextLayoutHelper exists for the duration of the draw operation
    // helps us draw bits of text that do not overlap one another.
    TextLayoutHelper theTextLayoutHelper;
    
    // theBarInfos exists for the duration of the draw operation
    // helps us plan our bar label placement early (at draw_bar time)
    // but draw them late (at draw_bar_labels time).
    std::vector<bar_info> theBarInfos;

    // We draw in this order:
    // Player icons
    // Graph bars
    // Player names
    // Carnage totals (nobody should overlap, so timing is arbitrary)
    // Carnage legend
    // Bar labels

    // This order is largely for back-to-front ordering.  Bar labels and player names, since
    // they are placed using a text layout helper, will not overlap... but we draw bar labels
    // after player names anyway (which takes considerable extra effort, note) so that the names
    // have "first dibs" on the coveted low-in-the-widget screen area.  Bar labels that want space
    // occupied by player names will have to "float up"... which looks nicer than making the names
    // float up to give the bar labels space.

    // Draw actual content
    if (clump_players_by_team)
    {
        // draw player icons in clumps
        draw_player_icons_clumped(canvas);
        
        if (draw_carnage_graph) draw_bars_clumped(canvas, theBarInfos);
        
        draw_player_names_clumped(canvas, theTextLayoutHelper);
    }
    else
    {
        // Draw all the player icons first, so icons don't obscure names
        draw_player_icons_separately(canvas);
        
        if (draw_carnage_graph) draw_bars_separately(canvas, theBarInfos);

        draw_player_names_separately(canvas, theTextLayoutHelper);
    }
    
    if (draw_carnage_graph && !draw_scores_not_carnage)
    {
        draw_carnage_totals(canvas);
        if (num_valid_net_rankings >= kUseLegendThreshhold) draw_carnage_legend(canvas);
    }
    
    if (draw_carnage_graph) draw_bar_labels(canvas, theBarInfos, theTextLayoutHelper);

    // Reset clipping rectangle
    canvas->clear_clip();
}


void w_players_in_game2::clear_vector()
{
    for (auto it : player_entries)
    {
		// Free the name buffers associated with the elements.
		// I don't do this in a player_entry destructor because I'm afraid of freeing the name twice
		// (once here, when the vector's entry is destroyed, and another time when thePlayerEntry
		// above goes out of scope).
		if (it.player_image) delete it.player_image;
	}
	player_entries.clear();
}


void w_players_in_game2::draw_bar(Canvas* canvas, int inCenterX, int inBarColorIndex, int inBarValue, int inMaxValue, bar_info& outBarInfo)
{
    if (inBarValue != 0)
    {
        // Check that we'll draw a positive bar - value and max are either both positive or both negative.
        assert_fail(inBarValue > 0 == inMaxValue > 0, "");
        
        // "- 1" leaves room for shadow style.  Leave two line-heights so a kills and deaths at the top of widget resolve
        // (thanks to TextLayoutHelper) and still have space to live.
        int	theMaximumBarHeight = kBarBottomTotalOffset - get_font()->line_height * 2 - 1;
        int	theBarHeight = (theMaximumBarHeight * inBarValue) / inMaxValue;
        
        SDL_Rect theBarRect = {inCenterX - kBarWidth / 2, rect.y + kBarBottomTotalOffset - theBarHeight, kBarWidth, theBarHeight};
        
        // Draw the lightest part
        SDL_Color brightest_color = get_net_color(inBarColorIndex);
        canvas->draw_filled_rect(theBarRect, brightest_color);
        
        SDL_Rect theDarkRect;
        theDarkRect.x = theBarRect.x + theBarRect.w - kBevelSize;
        theDarkRect.y = theBarRect.y + kBevelSize;
        theDarkRect.w = kBevelSize;
        theDarkRect.h = theBarRect.h - kBevelSize;

        SDL_Color darkest_color = dim_color(brightest_color, 20);
        if(theBarRect.h > kBevelSize) { canvas->draw_filled_rect(theDarkRect, darkest_color); }
        
        // Draw the middle part
        SDL_Rect	theMiddleRect;
        theMiddleRect.x	= theBarRect.x + kBevelSize;
        theMiddleRect.w	= theBarRect.w - 2 * kBevelSize;
        theMiddleRect.y	= theBarRect.y + kBevelSize;
        theMiddleRect.h	= theBarRect.h - kBevelSize;
        
        SDL_Color middle_color = dim_color(brightest_color, 70);
        if(theBarRect.h > kBevelSize) { canvas->draw_filled_rect(theMiddleRect, middle_color); }
        
        // Capture bar information
        outBarInfo.center_x = inCenterX;
        outBarInfo.top_y    = theBarRect.y;
        outBarInfo.color    = middle_color;
    }
}





////// w_entry_point_selector //////

void
w_entry_point_selector::validateEntryPoint() {
    // Get the entry-point flags from the game type.
    uint32_t theAppropriateLevelTypeFlags = get_entry_point_flags_for_game_type(mGameType);

    mEntryPoints.clear();

    // OK, get the vector of entry points.
    get_all_levels_for_game_types(mEntryPoints, theAppropriateLevelTypeFlags);

    if(mEntryPoints.size() <= 0)
    {
        mEntryPoint.level_number = NONE;
        mEntryPoint.utf8_level_name = "(no valid options)";
        mCurrentIndex = NONE;
    }
    else {
		unsigned i;

        for(i = 0; i < mEntryPoints.size(); i++) {
            if(mEntryPoints[i].level_number == mEntryPoint.level_number)
                break;
        }

        if(i == mEntryPoints.size()) {
            mEntryPoint = mEntryPoints[0];
            mCurrentIndex = 0;
        }
        else {
            mEntryPoint = mEntryPoints[i];
            mCurrentIndex = i;
        }
    }

    dirty = true;
}

void
w_entry_point_selector::gotSelectedCallback(void* arg) {
    ((w_entry_point_selector*) arg)->gotSelected();
}

void
w_entry_point_selector::gotSelected() {
    if(mEntryPoints.size() > 1) {
        dialog theDialog;

	vertical_placer *placer = new vertical_placer;
        placer->dual_add(new w_title("SELECT LEVEL"), theDialog);

	placer->add(new w_spacer(), true);

        placer->dual_add(new w_static_text(environment_preferences.map_file.filename()), theDialog);

        placer->add(new w_spacer(), true);

        w_levels* levels_w = new w_levels(mEntryPoints, &theDialog, 480, 16, mCurrentIndex, false);
        placer->dual_add(levels_w, theDialog);

        placer->add(new w_spacer(), true);
        
        // TODO: FIX: this is a mess, obviously: a stringset needs an entry for "$count$ $gameType$ levels available", with singular and plural versions
        std::string tmp = std::to_string(mEntryPoints.size()) + " "
                        + get_string(STRID(kNetworkGameTypesStringSetID, mGameType)) + " levels available";

        placer->dual_add(new w_static_text(tmp), theDialog);

        placer->add(new w_spacer(), true);

        placer->dual_add(new w_button("CANCEL", dialog_cancel, &theDialog), theDialog);

	theDialog.set_widget_placer(placer);

        clear_screen();

        if(theDialog.run() == 0) {
            mCurrentIndex = levels_w->get_selection();
            mEntryPoint = mEntryPoints[mCurrentIndex];
            dirty = true;
        }
    }
}

void
w_entry_point_selector::event(SDL_Event &e) {
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_RIGHT) {
            size_t theNumberOfEntryPoints = mEntryPoints.size();

            if(theNumberOfEntryPoints > 1) {
                int theDesiredOffset = (e.key.keysym.sym == SDLK_LEFT) ? -1 : 1;

                mCurrentIndex = (mCurrentIndex + theNumberOfEntryPoints + theDesiredOffset)
                    % theNumberOfEntryPoints;

                mEntryPoint = mEntryPoints[mCurrentIndex];

                dirty = true;
                play_dialog_sound(DIALOG_CLICK_SOUND);
            }

            e.type = SDL_LASTEVENT;	// Swallow event
		}
	}
}

#endif // !defined(DISABLE_NETWORKING)
