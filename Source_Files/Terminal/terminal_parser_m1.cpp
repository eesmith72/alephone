/*
 terminal_parser_m1.cpp
 
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

#include "terminal_parser_m1.hpp"

#include "ComputerTerminal.hpp"

#include "screen_drawing.h" // _terminal_full_text_rect
#include "Logging.h" // logWarning


// TODO: check parsed line breaks are correct in quantity and positions

// M2 terminal data is laid out in a single array consisting of 1-3 complete page sequences: unfinished, success, and failure.
// These sequences are pre-composed in M2 Maps, whereas M1 'term' resources use plain text with directives which must be
// assembled into M2-style sequences.


// TODO: may want to use this format for UTF8 terms, in which case it needs to support M2/AO directives as well (will need strncmp for that, or maybe nested switches, or a std::map)


// note: TerminalPage/Text.print_debug() will show all MR string ranges as "0..0" as M1 parser doesn't use those ivars. Styles and colors are supported (though cannot remember offhand if M1 terminal text was ever any color except green). They should show a string (unless it was entirely non-printing chars which are stripped).


/*
 An M2 Map's pre-built terminal contains 1-3 directive groups: #unfinished, #success, #failure.
 (#success is required; #unfinished is for mission levels, #failure for rescue mission only.)
 Each group is a full sequence of directives for terminal to perform, from #logon to #logoff.

 M1, terminal resources have a simpler, unprepared plaintext format which must be parsed and
 converted to M2-derived data structures.
 
 An M2 terminal's text content was originally serialized as a single MacRoman-encoded string in the WAD.
 Page and Text structs held only start index and length into that string, avoiding excess string copying.
 AO substituted NULs into that string at each style/page break so each segment could be passed to SDL APIs
 which expect a NUL-terminated C string.
 
 As an UTF8-encoded string may be wider than the MacRoman string, there is no benefit to preserving the
 original arrangement. Each segment of the MacRoman data is now copied into its Text object while being
 transcoded to UTF8 and NUL-terminated and stored in std::string for easy management.
 
 Now we have a nice modern ComputerTerminal + TerminalPage + TerminalText hierarchy that uses UTF8 as a
 solid foundation for implementing HD terminal rendering with full[1] localization support.
 
 [1] Caveat localized terminal text files will need to use correct codepoints for any ligatures, and SDL2's
     ability to render RTL scripts (Arabic, Hebrew) is TBD. Optionally hyphenating long words is also TBC.
 */


/*
 logon and logoff screens have 2 lines of text:
 
 one line of config-defined "U.E.S.C. Marathon" in bold, followed by 1 line of terminal-specific text
 
 */

/*
 
 ;L000.WELCOME.ENTRY
 #logon
 Airlock 34-a Terminal Access <Port 19.1.2.128>
 ;
 #information
 <Message to All Marathon Terminals>
 
 $BMarathon Emergency Systems Broadcast$b
 
 Today at 0820 hours, the Marathon came under surprise attack from unknown hostile forces.  The Marathon has sustained serious damage.
 
 At 0830 hours, alien forces boarded the Marathon.  The current situation is dire.  All personnel are required to arm themselves and fight for their lives.
 <Posted 2794.7.3.14.08.39>
 
 
 
 ***INCOMING MESSAGE FROM LEELA***
 
 Welcome to the Marathon.  I am Leela, one of the two surviving Artificial Intelligences aboard the Marathon.  I have been severely damaged, and am working to understand the current situation.
 
 Find the teleport terminal located in the Hangar’s control room.  By that time, I should have a better idea of what is going on.
 
 ***END MESSAGE***
 
 #checkpoint 0
 This is where you are now.  From here you can explore the rest of the Hangar area, although not all of the doors on the level are functioning.
 
 #checkpoint 3
 There is a pattern buffer at this location.
 
 #checkpoint 1
 There is a jump pad at this location.  Activate the terminal to leave the Hangar area.
 
 */


// -----------------------------------------------------------------------------------------
// M1 plaintext terminal parser

// (For historical reasons this is a class that gets instantiated, used, and discarded once per-terminal, but most
// of its ivars are only needed in parse. Refactoring it into simple functions can be done another time.)

class M1TerminalParser
{
public:
    
    M1TerminalParser(uint8_t* data, int16_t length) : data(data)
    {
        // The M1 logon/logoff screen has one line of config-defined text (e.g. "U.E.S.C. Marathon"),
        // followed by another line of custom text defined by the terminal resource's #logon directive.
        end_data = data + length;
        logon_page = {_logon_page, _draw_object_on_center | _terminal_is_m1};
        
        std::string logon_first_line;
        char message[256];
        getcstr(message, strCOMPUTER_LABELS, _m1_marathon_name); // TODO: this assumes (unsafely!) that strings will always be max 255 chars + NUL terminator; fixing it is for later
        for (int16_t i = 0; i < sizeof(message); i++)
        {
            char c = message[i];
            if (c == '\0') break;
            append_macroman_char_to_utf8(c, logon_first_line);
        }
        logon_first_line += "\n";
        logon_page.texts.emplace_back(styleBold, 0, 0, 0, logon_first_line);
        // the terminal data's #logon directive will append a new Text object containing the second line
    }
    
    bool parse(ComputerTerminal& terminal);
    
private:
    
    uint8_t* data;
    uint8_t* end_data;
    
    int16_t teleport_to_level = NONE;
    
    // TerminalPage objects are collected here, to be merged into complete M2-style unfinished, success,
    // and/or failure page groups within ComputerTerminal.pages at end of ::parse() method
    TerminalPage logon_page;
    TerminalPages briefing_pages;
    TerminalPages unfinished_pages;
    TerminalPages success_pages;
    TerminalPages failure_pages;
    
    
    void advance_to_linebreak()
    {
        while (data < end_data && !is_line_break(*data)) { data++; } // scan to end of paragraph (or end of terminal)
    }
    
    void parse_paragraph(TerminalPage* group);
    
    void add_unfinished_group(TerminalPages& result);
    
    void add_finished_group(TerminalPages& result, TerminalPages& group, int16_t group_type);

};


// -----------------------------------------------------------------------------------------


static void calculate_maximum_lines_for_pages(TerminalPage* groups, int16_t group_count, char* text_base)
{
    for (int16_t index = 0; index < group_count; ++index)
    {
        switch (groups[index].type)
        {
                // single-screen; these do not scroll so any keypress will advance to next group
                // TO DO: half of these aren't even supported in M1 terms
                // simplest to look at text Surface's height: if it's taller than its screen rect, keys will scroll it and only go to next/previous page on reaching bottom/top; if it fits in screen rect, keys will go to next/previous page immediately
            case _logon_page:
            case _logoff_page:
            case _interlevel_teleport_page:
            case _intralevel_teleport_page:
            case _sound_page:
            case _tag_page:
            case _movie_page:
            case _track_page:
            case _camera_page:
            case _static_page:
            case _end_page:
                groups[index].maximum_line_count = 1; // any click or keypress gets us out.
                break;
                
                // TODO: not sure about these (comment is confusing)
            case _unfinished_page:
            case _success_page:
            case _failure_page:
                groups[index].maximum_line_count = 0; // should never get to one of these groups.
                break;
                
                //
            case _checkpoint_page:
            case _pict_page:
            {
                Rect text_bounds = groups[index].calculate_bounds_for_text_box(); // The only thing we care about is the width.
                groups[index].maximum_line_count = count_total_lines(text_base,
                                                                     RECT_WIDTH(text_bounds),
                                                                     groups[index].mr_start,
                                                                     groups[index].mr_end);
                break;
            }
            case _information_page:
            {
                Rect text_bounds = get_term_rectangle(_terminal_full_text_rect);
                groups[index].maximum_line_count = count_total_lines(text_base,
                                                                     RECT_WIDTH(text_bounds),
                                                                     groups[index].mr_start,
                                                                     groups[index].mr_end);
                break;
            }
            default:
                break;
        }
    }
}


// -----------------------------------------------------------------------------------------
// ComputerTerminal methods


bool M1TerminalParser::parse(ComputerTerminal& terminal)
{
    TerminalPage tmp; // note: any text before first directive will be discarded
    TerminalPage* current_page = &tmp; // #logon *should* be first directive, possibly preceded by comments
    
    TerminalPages& group = unfinished_pages;
    
    while (data < end_data) // this loop always starts on the first character of a new paragraph
    {
        uint8_t* d = data;
        switch (*data) // is this paragraph a comment/directive/text?
        {
            case ';': // ';' at start of paragraph = comment to be ignored
                advance_to_linebreak();
                data++; // step over CR
                break;
                
            case '#': // '#' at start of paragraph = directive (aka group type), e.g. "#logon"
                // start a new directive
                
                //std::cout << "Read directive '" << '#' << data[1] << data[2] << "'\n";
                
                switch (*(++data)) // step over '#'; we'll be lazy and switch on first char of word (licbusf)
                {
                    case 'l': // #logon // used for both logon and logoff screens
                        current_page = &logon_page; // the second line of text will be added
                        // TODO: the first line ends in "\n\000"
                        break;
                        
                    case 'i': // #information
                        // there may be any number of information pages
                        group = unfinished_pages;
                        current_page = &group.emplace_back(_information_page, _terminal_is_m1);
                        
                    case 'c': // #checkpoint
                    {
                        // There can be any number of checkpoint pages; they do not need to be in order of checkpoint ID.
                        // This cleaned up class assumes a terminal resource's #checkpoint directives already appear in
                        // correct order relative to #information and other directives, whereas the MarathonTerminalCompiler
                        // implementation would extract any checkpoints into separate vectors, reinserting them at end of the
                        // M2-style #success (was #briefing) and #unfinished groups. It is not clear if this was necessary to
                        // replicate M1 terminal behavior or just unnecessary extra complexity/poor code design (AO code has
                        // a LOT of that). TODO: Let's assume the latter for now and fully check later to confirm no regressions.
                        current_page = &group.emplace_back(_checkpoint_page, _terminal_is_m1);
                        data += strlen("checkpoint"); // step over keyword
                        if (data + 2 > end_data) return false; // ensure there's a (presumably) space+digit[s] after it
                        current_page->permutation = atoi((char*)data); // TODO: strtol would be more robust
                        break;
                    }
                    case 'u': // #unfinished // level completion status < 100%
                        group = unfinished_pages;
                        current_page = &group.emplace_back(_information_page, _terminal_is_m1);
                        break;
                        
                    case 's': // #success // rescue level completed with most/all Bobs alive
                        group = success_pages;
                        current_page = &group.emplace_back(_information_page, _terminal_is_m1);
                        break;
                        
                    case 'f': // #failure // rescue level completed with most/all Bobs dead
                        group = failure_pages;
                        current_page = &group.emplace_back(_information_page, _terminal_is_m1);
                        break;
                        
                    case 'b': // #briefing // level completed; M1's level jump
                    {
                        group = briefing_pages;
                        current_page = &group.emplace_back(_information_page, _terminal_is_m1);
                        data += strlen("briefing");
                        if (data + 2 > end_data) return false;
                        teleport_to_level = atoi((char*)data);
                        break;
                    }
                    default:
                        return false;
                }
                advance_to_linebreak(); // go to end of line
                data++; // step over \r
                
            default:
                //std::cout << "read paragraph\n";
                parse_paragraph(current_page);
        }
        
        assert (data > d);
    }
    
    // there is always an information/uninished group, even if it's only {logon,logoff,end}
    add_unfinished_group(terminal.pages);
    
    // there is always a success group on a level-teleporting terminal
    if (!success_pages.empty() || !briefing_pages.empty()) { add_finished_group(terminal.pages, success_pages, _success_page); }
    
    // there is a failure group on rescue mission levels only
    if (!failure_pages.empty()) { add_finished_group(terminal.pages, failure_pages, _failure_page); }
    
    // TODO: these should go away, and be calculated when the Surface is rendered
   // terminal.lines_per_page = calculate_lines_per_page();
    //calculate_maximum_lines_for_pages(&terminal.groups[0], terminal.groups.size(), reinterpret_cast<char*>(terminal.text.data()));
    
    return true;
}


// TODO: this assumes CR linebreaks, never LF (or CRLF, though that's very unlikely); confirm, or use either CR OR LF

void M1TerminalParser::parse_paragraph(TerminalPage* current_page)
{
    // M1 formatting resets at the beginning of a paragraph
    font_style_t current_style = styleNormal;
    int16_t current_color_id = 0;
    TerminalText* text = &current_page->texts.emplace_back(current_style, current_color_id);
    
    bool done = false;
    while (data < end_data && !done)
    {
        uint8_t* d = data;
        
        switch (*data)
        {
            case '$': // escape sequence for style modifier, e.g. "some $Bword$b"
                if (++data < end_data) // step over '$'
                {
                    switch (*data) // process the modifier, e.g. 'b' = unset bold
                    {
                        case 'B':
                            current_style |= styleBold;
                            break;
                        case 'b':
                            current_style &= ~styleBold;
                            break;
                        case 'I':
                            current_style |= styleItalic;
                            break;
                        case 'i':
                            current_style &= ~styleItalic;
                            break;
                        case 'U':
                            current_style |= styleUnderline;
                            break;
                        case 'u':
                            current_style &= ~styleUnderline;
                            break;
                        case 'C': // e.g. "$C4"
                            if (++data < end_data) // step over 'C' and look for digit 0-7
                            {
                                char c = *data;
                                if (c >= '0' && c < '8')
                                {
                                    current_color_id = c - '0';
                                }
                                else // unknown color
                                {
                                    text->utf8_string += "$C";
                                    append_macroman_char_to_utf8(c, text->utf8_string);
                                    done = is_line_break(*data); // TODO: incomplete modifier "$C\n" currently appends as-is; should it discard?
                                }
                            }
                            else // missing color (unexpected end of string)
                            {
                                text->utf8_string += "$C";
                            }
                            break;
                        default: // unknown modifier
                            text->utf8_string += "$";
                            append_macroman_char_to_utf8(*data, text->utf8_string);
                            done = is_line_break(*data); // TODO: ditto
                    }
                    
                    data++; // step over the last char of modifier (BbIiUu, or digit if 'C' modifier)
                }
                else // missing modifier (unexpected end of string)
                {
                    text->utf8_string += '$'; // trailing '$' at end of 'term' resource
                }
                
                if (!text->utf8_string.empty()) // finish the current styled text run and start a new one
                {
                    text = &current_page->texts.emplace_back(current_style, current_color_id);
                }
                break;
        
            case '%': // TODO: seems like a second escape char, supporting "%r", "%%"
                if (++data < end_data) // step over '%'
                {
                    switch (*data)
                    {
                        case 'r':
                            //out_text.insert(out_text.end(), replacement, replacement + strlen(replacement));
                            text->utf8_string += "The colony has been wiped out. Phhht! Just like that."; // is this an Easter egg?
                            break;
                        case '%':
                            text->utf8_string += '%';
                            break;
                        default:
                            text->utf8_string += '%'; // TODO: check this; it might be skipped
                            append_macroman_char_to_utf8(*data, text->utf8_string);
                            done = is_line_break(*data); // TODO: not sure about "%\n"? preserve? discard?
                           
                    }
                }
                else // unexpected end of string, so append escape char as-is
                {
                    text->utf8_string += '%';
                }
                data++; // step over escaped char
                break;
            default:
                append_macroman_char_to_utf8(*data, text->utf8_string);
                done = is_line_break(*data);
                data++; // step over char
                
                // concatenating paragraphs is fine as long as CR isn't also acting as implicit style/color reset
                if (done && text->style == styleNormal && text->color_id == 0 && data < end_data && *data != '#')
                {
                    done = false;
                }
        }
        

        assert(data > d);
    }
    
    //text->print_debug();
}


void M1TerminalParser::add_unfinished_group(TerminalPages& result)
{
    // {logon, [information/unfinished,] logoff, end}
    result.emplace_back(_unfinished_page, _terminal_is_m1); // marker for the start of this sequence in M2
    result.push_back(logon_page);
    result.insert(result.end(), unfinished_pages.begin(), unfinished_pages.end());
    result.push_back(logon_page);
    result.back().type = _logoff_page;
    result.emplace_back(_end_page, _terminal_is_m1);
}


void M1TerminalParser::add_finished_group(TerminalPages& result, TerminalPages& rescue, int16_t group_type)
{
    // {logon, [success/failure,] [briefing,] logoff, [interlevel,] end}
    result.emplace_back(group_type, _terminal_is_m1); // marker for the start of this sequence in M2
    result.push_back(logon_page);
    if (!rescue.empty())   { result.insert(result.end(), rescue.begin(), rescue.end()); }
    if (!briefing_pages.empty()) { result.insert(result.end(), briefing_pages.begin(), briefing_pages.end()); }
    result.push_back(logon_page);
    result.back().type = _logoff_page;
    if (teleport_to_level != NONE)
    {
        result.emplace_back(_interlevel_teleport_page, _terminal_is_m1, teleport_to_level);
    }
    result.emplace_back(_end_page, _terminal_is_m1);
}


// -----------------------------------------------------------------------------------------
// public API

// TODO: is length argument needed as text may contain NULs/isn't NUL-terminated? or is it just a guard?
bool unpack_m1_computer_terminal(uint8_t* text, int16_t length, ComputerTerminal& terminal)
{
    M1TerminalParser parser(text, length);
    bool success = parser.parse(terminal);
    if (!success) { terminal = ComputerTerminal(); } // clear incomplete data on failure
    return success;
}

