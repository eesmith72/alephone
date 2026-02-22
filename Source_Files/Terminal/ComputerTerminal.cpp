/*
 Computer.hpp
 
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

#include "ComputerTerminal.hpp"

#include "FileHandler.h" // M1 terminals are App/Shapes resources
#include "Packing.h"     // M2 terminals are in Map WAD
#include "Logging.h"     // logWarning

#include "terminal_parser_m1.hpp"
#include "terminal_parser_m2.hpp"


// M2 'term' resource describes N terminals, each term's data is 2 arrays (groups, style ranges) and a MacRoman-encoded string + size.
// Groups contain styles, and styles contain indexes into the string. This allowed M2 to avoid slow string copying when loading a map.
// However, AO now copies the entire string anyway, then modifies it in-place to insert NULs so each style run's char* behaves as a C
// string when passed to SDL APIs which expect that sort of thing.

// Now that M1+M2 terminals support UTF8 (variable-width encoding), fiddling the original MacRoman (single-byte) data in-place is
// no longer appropriate. (Plus, AO would copy and re-encode it downstream in order to pass to SDL APIs which take UTF8 C
// strings.) This new implementation handles the whole transform - split, deobfuscate, convert to UTF8, and NUL-terminate - during
// Map unpacking.
//
// Since the original data structures need altered, this is a good time to migrate to nice modern hierarchical C++ classes:
//
// ComputerTerminal -- a single terminal
//   |-TerminalPage -- a single 'screen' within that terminal (long text will scroll)
//       |-TerminalText -- a single styled (e.g. bold + red) string within that 'screen'
//           |-std::string -- the string for a single style run, as NUL-terminated UTF8
//
//
// TODO: a modern, portable, modder-friendly UTF8-encoded plain-text format, to be stored in WAD as 'utrm' or as .txt file in plugin, based on the decompiled M2 terminal format. If practical, the same format should be used for ALL parameterized strings: this allows us to use the same text reading, writing, and drawing code (e.g. for strErrors, the #directives would be the numeric/symbolic error codes). This may come in handy when preparing translations (e.g. font style or RTL direction changes may be needed). A sensible, safe expansion syntax is also needed: printf codes (e.g. "%d") are not acceptable


// -----------------------------------------------------------------------------------------
// terminal texts for current level

// terminal texts for the currently loaded level
// note: whereas M2 terminal IDs appear to be contiguous, allowing array to be resized exactly,
// M1 terminal IDs may not be so array will always be padded to 10 for those
static std::vector<ComputerTerminal> computer_terminals;


/* Calculate the length the loaded terminal data would take up on disk (for saving):
 
         int16_t total_length;
         int16_t flags;
         int16_t lines_per_page; // Added for internationalization/sync problems
         int16_t grouping_count;
         int16_t font_changes_count;
     };
 */
static const int32_t SIZEOF_static_preprocessed_terminal_state = 10;

size_t ComputerTerminal::get_bytesize() // old M2 format
{
    return 0; //SIZEOF_static_preprocessed_terminal_state + groups.size() * SIZEOF_terminal_page + texts.size() * SIZEOF_text_face_data + text.size();
}


ComputerTerminal* get_terminal_for_id(int16_t terminal_id)
{
    return terminal_id >= 0 && terminal_id < computer_terminals.size() ? &computer_terminals[terminal_id] : nullptr;
}


int16_t number_of_terminals() // ghs: for Lua
{
    return computer_terminals.size();
}


TerminalPage* ComputerTerminal::get_page_at_index(int16_t page_id)
{
    return (page_id >= 0 && page_id < pages.size()) ? &pages[page_id] : nullptr;
}


void ComputerTerminal::print_debug()
{
    std::cout << "==================== BEGIN TERMINAL ====================\n";
    for (TerminalPage& page : pages)
    {
        page.print_debug();
        std::cout << "\n";
    }
    std::cout << "==================== ENDED TERMINAL ====================\n";
}



// -----------------------------------------------------------------------------------------
// M1 deserialization

// Whereas M1 stored each terminal as a plaintext 'term' resource in the App's resource fork (presumably because M1 maps don't have a resource fork), M2 stores each level's terminals data in the Map WAD.

#define M1_TERMINAL_COUNT (10)


// TODO: not sure where this should end up
extern OpenedResourceFile M1ShapesFile;
extern OpenedResourceFile ExternalResources;


void load_m1_computer_terminals_for_level(int16_t level_number)
{
    int16_t base_resource_id = 1000 + level_number * 10;
    
    computer_terminals.clear();
    computer_terminals.resize(M1_TERMINAL_COUNT); // M1 maps can have 0-10 terminals stored in resource fork, e.g. 1125 is terminal 5 of level 12
    
    for (int16_t terminal_id = 0; terminal_id < M1_TERMINAL_COUNT; terminal_id++)
    {
        int16_t resource_id = base_resource_id + terminal_id;
        LoadedResource rsrc;
        if (ExternalResources.IsOpen()) // the M1 app's resource fork (M1 maps don't have a resource fork)
        {
            ExternalResources.Get('t', 'e', 'r', 'm', resource_id, rsrc);
        }
        if (!rsrc.IsLoaded() && M1ShapesFile.IsOpen()) // Trojan's terminal resources are in its Shapes file
        {
            M1ShapesFile.Get('t', 'e', 'r', 'm', resource_id, rsrc);
        }
        if (rsrc.IsLoaded())
        {
            bool success = unpack_m1_computer_terminal((uint8_t*)rsrc.GetPointer(), rsrc.GetLength(), computer_terminals[terminal_id]);
            if (!success) logWarning("Can't read M1 terminal %i due to syntax error.", resource_id);
            computer_terminals[terminal_id].print_debug();
        }
    }
}


// -----------------------------------------------------------------------------------------
// M2 deserialization


void unpack_m2_computer_terminals(uint8_t* data, size_t data_size) // unpack $count terminals for the current level
{
    computer_terminals.clear();
    
    while (data_size > 0)
    {
        // parse one M2 terminal into ComputerTerminal object
        ComputerTerminal& terminal = computer_terminals.emplace_back();
        
        // Read terminal header
        unpack_m2_computer_terminal(data, data_size, terminal);
    }
}


void pack_computer_terminals(uint8_t* p, size_t count)
{
    /*
    // TODO: pack as plain text 'utrm'? yes, and add a new FilmProfile flag so the new App version is required to read these saved/net maps
     
    for (ComputerTerminal& text : computer_terminals)
    {
        // Write header
        uint8_t* p_start = p;
        uint16_t total_length = static_cast<uint16_t>(text.get_bytesize());
        uint16_t grouping_count = static_cast<uint16_t>(text.groups.size());
        uint16_t font_changes_count = static_cast<uint16_t>(text.texts.size());
        ValueToStream(p, total_length);
        uint16_t is_obfuscated_flag = 0; // because life is too int16_t
        ValueToStream(p, is_obfuscated_flag);
        ValueToStream(p, text.lines_per_page);
        ValueToStream(p, grouping_count);
        ValueToStream(p, font_changes_count);
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_static_preprocessed_terminal_state));
        
        // Write groupings
        p_start = p;
        for (TerminalPage& group : text.groups)
        {
            ValueToStream(p, group.flags);
            ValueToStream(p, group.type);
            ValueToStream(p, group.permutation);
            ValueToStream(p, group.start_index);
            ValueToStream(p, group.length);
            ValueToStream(p, group.maximum_line_count);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_terminal_pageings) * grouping_count);
        
        // Write font changes
        p_start = p;
        for (TerminalText& face : text.texts)
        {
            ValueToStream(p, face.start_index);
            ValueToStream(p, face.style);
            ValueToStream(p, face.color_id);
        }
        assert((p - p_start) == static_cast<ptrdiff_t>(SIZEOF_text_face_data) * font_changes_count);
        
        // Write text (no conversion) // TODO: what does 'no conversion' mean here?
        BytesToStream(p, text.text.data(), text.text.size());
    }
     */
}


size_t get_bytesize_of_packed_computer_terminals()
{
    return 0;
    // TODO: pretty sure we want a plaintext format, similar to tha
    /*
    size_t total = 0;
    for (ComputerTerminal& text : computer_terminals) { total += text.get_bytesize(); }
    return total;
     */
}

