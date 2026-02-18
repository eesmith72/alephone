/*
 TerminalText.hpp
 
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

#include "TerminalText.hpp"

#include "FileHandler.h" // M1 terminals are App/Shapes resources
#include "Packing.h"     // M2 terminals are in Map WAD

#include "M1TerminalParser.hpp"


// -----------------------------------------------------------------------------------------
// terminal texts for current level

// terminal texts for the currently loaded level
static std::vector<TerminalText> terminal_texts;


/* Calculate the length the loaded terminal data would take up on disk (for saving):
 
         int16_t total_length;
         int16_t flags;
         int16_t lines_per_page; // Added for internationalization/sync problems
         int16_t grouping_count;
         int16_t font_changes_count;
     };
 */
static const int32_t SIZEOF_static_preprocessed_terminal_state = 10;

size_t TerminalText::get_bytesize()
{
    return SIZEOF_static_preprocessed_terminal_state
         + groupings.size() * SIZEOF_terminal_groupings
         + style_ranges.size() * SIZEOF_text_face_data
         + text.size();
}


int16_t number_of_terminal_texts() // ghs: for Lua
{
    return terminal_texts.size();
}


// euww
extern OpenedResourceFile M1ShapesFile;
extern OpenedResourceFile ExternalResources;


// TODO: this gets visited A LOT (every update_world when a player is in terminal), performing a lookup every time
TerminalText* get_terminal_text_for_terminal_id(int16_t terminal_id) // returns nullptr if not found
{
    if (terminal_id >= 0 && terminal_id < terminal_texts.size()) // M2 terminal ID
    {
        TerminalText* terminal_text = &terminal_texts.at(terminal_id);
        return terminal_text;
    }
    else // M1 terminal ID; each terminal is stored as a separate 'term' resource in the app/Shapes resource fork, with 0-9 terminals per level (and, IIRC, 1-99 levels?), e.g. 1125 is terminal 5 on level 12
    {
        // TODO: rework order of operations so all terms for current level are found and parsed when loading level, pushing this crap out of here and into Files where it belongs; this makes it much easier to import M1 terms and export in M2 format, plain text, or whatever
        terminal_id = 1000 + dynamic_world->current_level_number * 10 + terminal_id;
        
        static std::unique_ptr<TerminalText> resource_terminal;
        static int32_t resource_terminal_id = NONE;
        
        if (terminal_id == resource_terminal_id)
        {
            return resource_terminal.get();
        }
        else
        {
            LoadedResource rsrc;
            if (ExternalResources.IsOpen()) { ExternalResources.Get('t', 'e', 'r', 'm', terminal_id, rsrc); } // the M1 app's exported resource fork
            if (!rsrc.IsLoaded() && M1ShapesFile.IsOpen()) { M1ShapesFile.Get('t', 'e', 'r', 'm', terminal_id, rsrc); } // Trojan put terminal resources in its Shapes file
            if (rsrc.IsLoaded())
            {
                resource_terminal_id = terminal_id;
                resource_terminal.reset(compile_m1_terminal(reinterpret_cast<char*>(rsrc.GetPointer()), rsrc.GetLength()));
                return resource_terminal.get();
            }
            else
            {
                return nullptr;
            }
        }
    }
}


// -----------------------------------------------------------------------------------------
// M2 serialization

// whereas M1 bodged each terminal as a 'term' resource in the app's resource fork (M1 maps don't have a resource fork), M2 maps store terminals in the Maps WAD

static void deobfuscate_terminal_text(uint8_t* p, size_t length)
{
    for (size_t i = 0; i < length / 4; i++)
    {
        p += 2;
        *p++ ^= 0xfe;
        *p++ ^= 0xed;
    }
    for (size_t i = 0; i < length % 4; i++)
    {
        *p++ ^= 0xfe;
    }
}


size_t get_bytesize_of_packed_computer_terminals()
{
    size_t total = 0;
    for (TerminalText& text : terminal_texts) { total += text.get_bytesize(); }
    return total;
}


void unpack_computer_terminal_text(uint8_t* p, size_t count)
{
    terminal_texts.clear();
    
    while (count > 0)
    {
        TerminalText text;
        terminal_texts.push_back(text);
        TerminalText &data = terminal_texts.back();
        
        // Read header
        uint8_t* p_start = p;
        uint8_t* p_header = p;
        uint16_t total_length, grouping_count, font_changes_count;
        uint16_t is_obfuscated_flag; // _text_is_encoded_flag = 0x0001
        
        StreamToValue(p, total_length);
        StreamToValue(p, is_obfuscated_flag);
        StreamToValue(p, data.lines_per_page);
        StreamToValue(p, grouping_count);
        StreamToValue(p, font_changes_count);
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_static_preprocessed_terminal_state));
        
        // Read groupings
        data.groupings.reserve(grouping_count);
        data.style_ranges.reserve(font_changes_count);
        p_start = p;
        for (int32_t i = 0; i < grouping_count; i++)
        {
            TerminalTextGroup groupings;
            StreamToValue(p, groupings.flags);
            StreamToValue(p, groupings.type);
            StreamToValue(p, groupings.permutation);
            StreamToValue(p, groupings.start_index);
            StreamToValue(p, groupings.length);
            StreamToValue(p, groupings.maximum_line_count);
            data.groupings.push_back(groupings);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_terminal_groupings) * grouping_count);
        
        // Read font changes
        p_start = p;
        for (int32_t i = 0; i < font_changes_count; i++)
        {
            TerminalTextStyleRange f;
            f.unpack_m2_data(p);
            data.style_ranges.push_back(f);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_text_face_data) * font_changes_count);
        
        // Read text (no conversion) // TODO: what does 'no conversion' mean here?
        const int32_t text_length = total_length - static_cast<int32_t>(p - p_header);
        assert(text_length >= 0);
        data.text.resize(text_length);
        // TODO: there should be a single-pass parser for Classic M2 terminals, M1 terminals; translations will most likely be M2-format .txt files or possibly XML but let's not not obfuscate any more 30yo text that's been on Marathon's Story Page for last 29.99999yrs.
        StreamToBytes(p, data.text.data(), data.text.size());
        if (is_obfuscated_flag) // moving the de-obfuscation here means less crap for TerminalText class to deal with
        {
            deobfuscate_terminal_text(data.text.data(), data.text.size());
        }
        
        // Continue with next terminal
        count -= total_length;
    }
}


void pack_computer_terminal_text(uint8_t* p, size_t count)
{
    for (TerminalText& text : terminal_texts)
    {
        // Write header
        uint8_t* p_start = p;
        uint16_t total_length = static_cast<uint16_t>(text.get_bytesize());
        uint16_t grouping_count = static_cast<uint16_t>(text.groupings.size());
        uint16_t font_changes_count = static_cast<uint16_t>(text.style_ranges.size());
        ValueToStream(p, total_length);
        uint16_t is_obfuscated_flag = 0; // because life is too int16_t
        ValueToStream(p, is_obfuscated_flag);
        ValueToStream(p, text.lines_per_page);
        ValueToStream(p, grouping_count);
        ValueToStream(p, font_changes_count);
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_static_preprocessed_terminal_state));
        
        // Write groupings
        p_start = p;
        for (TerminalTextGroup& group : text.groupings)
        {
            ValueToStream(p, group.flags);
            ValueToStream(p, group.type);
            ValueToStream(p, group.permutation);
            ValueToStream(p, group.start_index);
            ValueToStream(p, group.length);
            ValueToStream(p, group.maximum_line_count);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_terminal_groupings) * grouping_count);
        
        // Write font changes
        p_start = p;
        for (TerminalTextStyleRange& face : text.style_ranges)
        {
            ValueToStream(p, face.start_index);
            ValueToStream(p, face.style);
            ValueToStream(p, face.color_id);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_text_face_data) * font_changes_count);
        
        // Write text (no conversion) // TODO: what does 'no conversion' mean here?
        BytesToStream(p, text.text.data(), text.text.size());
    }
}


