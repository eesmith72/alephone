/*
 M1TerminalParser.cpp
 
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

#include "M1TerminalParser.hpp"

#include "TerminalText.hpp"

#include "Logging.h"


// -----------------------------------------------------------------------------------------
// parse an M1 terminal text into M2 TerminalText data structure
// the implementation is stinky but it 1. does the job, and 2. is completely hidden, so STET

class M1TerminalParser
{
public:
    M1TerminalParser(char* text, int16_t length) : terminal_text(new TerminalText), in_buffer(text, length), in(&in_buffer) {
        information_group.type = 0;
        briefing_group.type = 0;
        success_group.type = 0;
        failure_group.type = 0;
        unfinished_group.type = 0;
        group.type = NONE;
    }
    TerminalText* Compile();
    
private:
    void FinishGroup();
    void CompileLine(const std::string& line);
    
    void BuildUnfinishedGroup();
    void BuildSuccessGroup();
    void BuildFailureGroup();
    
    std::unique_ptr<TerminalText> terminal_text;
    
    boost::iostreams::stream_buffer<boost::iostreams::array_source> in_buffer;
    std::istream in;
    
    std::vector<uint8_t> out;
    
    int16_t current_group_type;
    TerminalTextGroup group;
    
    TerminalTextGroup logon_group;
    TerminalTextGroup information_group;
    TerminalTextGroup briefing_group;
    TerminalTextGroup unfinished_group;
    TerminalTextGroup failure_group;
    TerminalTextGroup success_group;
    
    std::vector<TerminalTextGroup> information_checkpoints;
    std::vector<TerminalTextGroup> unfinished_checkpoints;
    // does Marathon allow #checkpoints inside success/failure/briefing?
    std::vector<TerminalTextGroup>* checkpoints;
};


// -----------------------------------------------------------------------------------------
// public API

TerminalText* compile_m1_terminal(char* text, int16_t length)
{
    M1TerminalParser compiler(text, length);
    return compiler.Compile();
}


// -----------------------------------------------------------------------------------------
// TerminalText methods


TerminalText* M1TerminalParser::Compile()
{
    std::string line;
    while (std::getline(in, line, static_cast<char>(MAC_LINE_END)))
    {
        if (line[0] == '#')
        {
            FinishGroup();
            
            group.flags = _group_is_marathon_1;
            group.permutation = 0;
            
            if (boost::algorithm::istarts_with(line, "#logon"))
            {
                group.type = _logon_group;
            }
            else if (boost::algorithm::istarts_with(line, "#information"))
            {
                group.type = _information_group;
            }
            else if (boost::algorithm::istarts_with(line, "#checkpoint"))
            {
                group.type = _checkpoint_group;
                std::istringstream permutation(line.substr(1 + strlen("#checkpoint")));
                permutation >> group.permutation;
            }
            else if (boost::algorithm::istarts_with(line, "#briefing"))
            {
                group.type = _interlevel_teleport_group;
                std::istringstream permutation(line.substr(1 + strlen("#briefing")));
                permutation >> group.permutation;
            }
            else if (boost::algorithm::istarts_with(line, "#unfinished"))
            {
                group.type = _unfinished_group;
            }
            else if (boost::algorithm::istarts_with(line, "#success"))
            {
                group.type = _success_group;
            }
            else if (boost::algorithm::istarts_with(line, "#failure"))
            {
                group.type = _failure_group;
            }
            else
            {
                logWarning("Unrecognized group");
                terminal_text.reset(0);
                return 0;
            }
            
            group.start_index = out.size();
        }
        else if (line[0] != ';')
        {
            CompileLine(line);
        }
    }
    
    FinishGroup();
    
    BuildUnfinishedGroup();
    BuildSuccessGroup();
    BuildFailureGroup();
    
    terminal_text->text = out;
    
    terminal_text->lines_per_page = calculate_lines_per_page();
    calculate_maximum_lines_for_groups(&terminal_text->groupings[0], terminal_text->groupings.size(), reinterpret_cast<char*>(terminal_text->text.data()));
    
    return terminal_text.release();
}


void M1TerminalParser::FinishGroup()
{
    group.length = out.size() - group.start_index;
    
    out.push_back('\0');
    
    switch (group.type)
    {
        case _logon_group:
            checkpoints = nullptr;
            logon_group = group;
            break;
        case _information_group:
            information_group = group;
            information_checkpoints.clear();
            checkpoints = &information_checkpoints;
            break;
        case _checkpoint_group:
            if (checkpoints)
            {
                checkpoints->push_back(group);
            }
            break;
        case _interlevel_teleport_group:
            checkpoints = nullptr;
            briefing_group = group;
            break;
        case _success_group:
            checkpoints = nullptr;
            success_group = group;
            break;
        case _failure_group:
            checkpoints = nullptr;
            failure_group = group;
            break;
        case _unfinished_group:
            unfinished_group = group;
            unfinished_checkpoints.clear();
            checkpoints = &unfinished_checkpoints;
            break;
    }
}


void M1TerminalParser::CompileLine(const std::string& line)
{
    // Marathon formatting resets at the beginning of the line
    TerminalTextStyleRange style_range;
    style_range.start_index = out.size();
    terminal_text->style_ranges.push_back(style_range);
    
    for (std::string::const_iterator it = line.begin(); it != line.end(); ++it)
    {
        if (*it == '$' && (it + 1 != line.end())) // escape sequence, e.g. "some $Bword$b"
        {
            style_range.start_index = out.size();
            char c = *(it + 1);
            switch (c)
            {
                case 'B':
                    style_range.style |= styleBold;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'b':
                    style_range.style &= ~styleBold;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'I':
                    style_range.style |= styleItalic;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'i':
                    style_range.style &= ~styleItalic;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'U':
                    style_range.style |= styleUnderline;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'u':
                    style_range.style &= ~styleUnderline;
                    terminal_text->style_ranges.push_back(style_range);
                    ++it;
                    break;
                case 'C':
                    if (it + 2 != line.end())
                    {
                        char cc = *(it + 2);
                        if (cc >= '0' && cc <= '7')
                        {
                            style_range.color_id = cc - '0';
                            terminal_text->style_ranges.push_back(style_range);
                            it += 2;
                        }
                        else
                        {
                            out.push_back(*it);
                        }
                    }
                    else
                    {
                        out.push_back(*it);
                    }
                    break;
                default:
                    ++it;
            }
        }
        else if (*it == '%' && (it + 1 != line.end()))
        {
            static char replacement[] = "The colony has been wiped out. Phhht! Just like that.";
            
            char c = *(it + 1);
            switch (c)
            {
                case 'r':
                    out.insert(out.end(), replacement, replacement + strlen(replacement));
                    ++it;
                    break;
                case '%':
                    out.push_back(*it);
                    ++it;
                    break;
                default:
                    out.push_back(*it);
            }
        }
        else
        {
            out.push_back(*it);
        }
    }
    
    out.push_back(MAC_LINE_END); // old-school!
}


void M1TerminalParser::BuildUnfinishedGroup()
{
    TerminalTextGroup group;
    group.flags = 0;
    group.type = _unfinished_group;
    group.permutation = 0;
    group.start_index = 0;
    group.length = 0;
    terminal_text->groupings.push_back(group);
    
    terminal_text->groupings.push_back(logon_group);
    
    if (information_group.type)
    {
        terminal_text->groupings.push_back(information_group);
        terminal_text->groupings.insert(terminal_text->groupings.end(),
                                        information_checkpoints.begin(),
                                        information_checkpoints.end());
    }
    
    if (unfinished_group.type)
    {
        group = unfinished_group;
        group.type = _information_group;
        terminal_text->groupings.push_back(group);
        
        terminal_text->groupings.insert(terminal_text->groupings.end(),
                                        unfinished_checkpoints.begin(),
                                        unfinished_checkpoints.end());
    }
    
    group = logon_group;
    group.type = _logoff_group;
    terminal_text->groupings.push_back(group);
    
    group.type = _end_group;
    group.flags = 0;
    group.permutation = 0;
    group.start_index = 0;
    group.length = 0;
    terminal_text->groupings.push_back(group);
}


void M1TerminalParser::BuildSuccessGroup()
{
    if (success_group.type || briefing_group.type)
    {
        TerminalTextGroup group;
        group.flags = 0;
        group.type = _success_group;
        group.permutation = 0;
        group.start_index = 0;
        group.length = 0;
        terminal_text->groupings.push_back(group);
        
        terminal_text->groupings.push_back(logon_group);
        
        if (success_group.type == _success_group)
        {
            group = success_group;
            group.type = _information_group;
            terminal_text->groupings.push_back(group);
        }
        
        if (briefing_group.type)
        {
            group = briefing_group;
            group.type = _information_group;
            group.permutation = 0;
            terminal_text->groupings.push_back(group);
        }
        
        group = logon_group;
        group.type = _logoff_group;
        terminal_text->groupings.push_back(group);
        
        if (briefing_group.type)
        {
            group.type = _interlevel_teleport_group;
            group.permutation = briefing_group.permutation;
            group.start_index = 0;
            group.length = 0;
            terminal_text->groupings.push_back(group);
        }
        
        group.type = _end_group;
        group.flags = 0;
        group.permutation = 0;
        group.start_index = 0;
        group.length = 0;
        terminal_text->groupings.push_back(group);
    }
}


void M1TerminalParser::BuildFailureGroup()
{
    if (failure_group.type)
    {
        TerminalTextGroup group;
        group.flags = 0;
        group.type = _failure_group;
        group.permutation = 0;
        group.start_index = 0;
        group.length = 0;
        terminal_text->groupings.push_back(group);
        
        terminal_text->groupings.push_back(logon_group);
        
        group = failure_group;
        group.type = _information_group;
        terminal_text->groupings.push_back(group);
        
        if (briefing_group.type)
        {
            group = briefing_group;
            group.type = _information_group;
            group.permutation = 0;
            terminal_text->groupings.push_back(group);
        }
        
        group = logon_group;
        group.type = _logoff_group;
        terminal_text->groupings.push_back(group);
        
        if (briefing_group.type)
        {
            group.type = _interlevel_teleport_group;
            group.permutation = briefing_group.permutation;
            group.start_index = 0;
            group.length = 0;
            terminal_text->groupings.push_back(group);
        }
        
        group.type = _end_group;
        group.flags = 0;
        group.permutation = 0;
        group.start_index = 0;
        group.length = 0;
        terminal_text->groupings.push_back(group);
    }
}
