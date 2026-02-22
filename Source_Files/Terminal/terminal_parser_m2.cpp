/*
 terminal_parser_m2.cpp
 
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


#include "terminal_parser_m2.hpp"

#include "Packing.h"     // M2 terminals are in Map WAD
#include "Logging.h"     // logWarning



static void read_m2_string_for_terminal_text(TerminalText* text, const uint8_t* data, bool is_obfuscated)
{
    for (int32_t i = text->mr_start; i < text->mr_end; i++)
    {
        uint8_t c = data[i]; // append this char to current Text
        if (is_obfuscated)
        {
            switch (i % 4)
            {
                case 2:
                    c ^= 0xfe;
                    break;
                case 3:
                    c ^= 0xed;
                    break;
                default:
                    {}
            }
        }
        // The terminal's entire text is one long run of MacRoman-encoded data that may include NULs which AO treats as string endings.
        if (c == '\0') break;
        append_macroman_char_to_utf8(c, text->utf8_string);
    }
}


// The original didn't actually bounds-check the styled text's range.
inline TerminalPage* get_m2_terminal_page_with_checked_range(ComputerTerminal& terminal, int16_t page_index, size_t string_length)
{
    TerminalPage* current_page = &terminal.pages[page_index];
    if (current_page->mr_start < 0 || current_page->mr_end > string_length || current_page->mr_start > current_page->mr_end)
    {
        throw std::out_of_range("Invalid styled text range in terminal page.");
    }
    return current_page;
}


// Unpack the styled text runs as TerminalText instances and attach to the appropriate TerminalPage[s].
// This also unpacks the original one big MacRoman-encoded string as NUL-terminated UTF8 std::strings.
static void unpack_text_for_m2_terminal(ComputerTerminal &terminal, uint8_t*& data, uint8_t* data_end,
                                        uint16_t page_count, uint16_t text_count, bool is_obfuscated)
{
    if (page_count > 0 && text_count > 0) { return; } // (it is possible to have >0 Pages with no styled text, i.e. terminal has only #directives)
    
    uint8_t* mr_string_ptr = data + text_count * SIZEOF_m2_terminal_text;
    int64_t string_length = data_end - mr_string_ptr;
    assert(string_length >= 0);
    
    if (is_obfuscated)
    {
        // Everything up to last 0-3 bytes of string is encoded consistently (2 bytes as-is, 1 XORed 0xFE, 1 XORed 0xED)
        // but the last 0-3 bytes are all XORed 0xFE, which is quite annoying as they require special-case treatment.
        // "Luckily" data isn't const, so twiddle the 2 non-standard bits here so we don't have to deal with them in loop.
        size_t encoded_length = string_length & ~0x3;
        if (encoded_length + 1 < string_length) *(mr_string_ptr + encoded_length + 1) ^= 0xfe;
        if (encoded_length + 2 < string_length) *(mr_string_ptr + encoded_length + 2) ^= 0xfe;
    }
    
    TerminalText current_text;
    current_text.unpack_m2_data(data);
    TerminalText* prev_text = nullptr;
    
    int32_t page_index = 0, text_index = 1;
    TerminalPage* current_page = get_m2_terminal_page_with_checked_range(terminal, page_index++, string_length); // your periodic reminder that reference vars can't be rebound
    
    // Text objects are added to their Pages as each style run is deserialized
    while (text_index < text_count)
    {
        // the current Text belongs to a later Page, so advance to that Page
        while (current_text.mr_start >= current_page->mr_end && page_index < page_count)
        {
            current_page = get_m2_terminal_page_with_checked_range(terminal, page_index++, string_length);
        }
        
        //std::cout << "Populating Page: "; current_page->print_debug();
        
        // Add the current Text to the current Page, plus any subsequent Texts that start on this Page.
        // This should be entered at least once; but best to check again in case the data is bad.
        current_page->texts.reserve(text_count - text_index);
        while (current_text.mr_start < current_page->mr_end && text_index < text_count)
        {
            //std::cout << "With Text: "; current_text.print_debug();
            // emplace the current Text (its end_index will be added once we know the start of next Text)
            prev_text = &current_page->texts.emplace_back(current_text);
            
            // unpack the next Text (this gives us the newly added Text's end_index)
            current_text.unpack_m2_data(data);
            text_index++;
            
            // if the previous Text extends across one or more subsequent Pages, add it to those too
            while (current_text.mr_start > current_page->mr_end && page_index < page_count)
            {
                // finish the previous Page's Text
                prev_text->mr_end = current_page->mr_end;
                read_m2_string_for_terminal_text(prev_text, mr_string_ptr, is_obfuscated);
                
                // advance to next Page and copy previous Text to it
                current_page = get_m2_terminal_page_with_checked_range(terminal, page_index++, string_length);
                prev_text = &current_page->texts.emplace_back(*prev_text);
                prev_text->mr_start = current_page->mr_start;
            }
            prev_text->mr_end = current_text.mr_start;
            read_m2_string_for_terminal_text(prev_text, mr_string_ptr, is_obfuscated);
        }
        assert(prev_text);
        
        // TODO: fairly sure this should not be here:
        //prev_text->mr_end = text_index < text_count ? current_text.mr_start : string_length;
        //read_m2_string_for_text(string_start, prev_text, is_obfuscated);
    }
    /*
    terminal.print_debug();
    std::cout << "Tailings:\n";
    std::cout << "current_page: "; current_page->print_debug();
    std::cout << "current_text: "; current_text.print_debug();
    */
    
    // Any remaining Pages use their own MR string range with the last Text style:
    while (page_index < page_count)
    {
        current_page = get_m2_terminal_page_with_checked_range(terminal, page_index++, string_length);
        //std::cout << "Finishing page " << page_index << " of " << page_count << ": "; current_page->print_debug();
        TerminalText& text = current_page->texts.emplace_back(current_text);
        text.mr_start = current_page->mr_start;
        text.mr_end = current_page->mr_end;
        read_m2_string_for_terminal_text(&text, mr_string_ptr, is_obfuscated);
        //std::cout << "current_text: "; text.print_debug();
        text.print_debug();
    }
    //std::cout << "DONE.\npage " << page_index << " of " << page_count << ", text " << text_index << " of " << text_count << "\n";

    /*
     TODO: last terminal of Waterloo has a discrepancy between end of MR string range and what gets read (411 vs 409)
     
     Page #logoff (texts=3 range=377..411)
     Text range=377..403 style='' color=2 string={{ehhg.431.4122//<PFGR ZNE6 }}
     Text range=403..408 style='' color=6 string={{&49c2}}
     Text range=408..409 style='' color=2 string={{¿}} <----- ???
     */
    
    if (text_index < text_count) // ignore any trailing Text range with start_index >= string_length
    {
        logWarning("Ignoring malformed terminal text.");
    }
}


void unpack_m2_computer_terminal(uint8_t*& data, size_t& data_size, ComputerTerminal& terminal)
{
    uint8_t* data_start = data;
    uint16_t terminal_byte_size, page_count, text_count;
    uint16_t is_obfuscated; // _text_is_encoded_flag = 0x0001 (M2+ Map files obfuscate terminal text)
    
    StreamToValue(data, terminal_byte_size);
    StreamToValue(data, is_obfuscated);
    StreamToValue(data, terminal.lines_per_page);
    StreamToValue(data, page_count);
    StreamToValue(data, text_count);
    //assert((data - data_start) == static_cast<ptrdiff_t>(SIZEOF_static_preprocessed_terminal_state)); // TODO: the only thing this `assert` crap confirms is that AO's integer widths haven't changed since 1995; it really is quite useless except as a guard against AO code's own obfuscations and over-complexity. A sane data unpacking object would provide explicitly named methods, e.g. `myvar = data.read_uint16();`, avoiding opaque StreamToValue macros or CPP's overloaded `<<` overcleverness. This would cleanly, reliably, explicitly decouple file reading/writing logic from ancient 1995 data file formats to code implementation (specifically, what width of integer to use). Failure to decouple is why AO is still riddled with [u]int16s and their obvious capacity limitations, decades after [u]int32/64_t and gigabyte-RAM became the modern standard while 64-bit CPUs don't even want to deal with 16-bit ints any more.
    
    // TODO: replace dumb uint8_t* with an istream or WADReader or something that protects itself against overruns
    if (terminal_byte_size > data_size)
    {
        logWarning("Malformed terminal data.");
        data_size = 0;
        return;
    }
    uint8_t* data_end = data_start + terminal_byte_size;
    
    //        std::cout << "Total length=" << byte_count << "\n\n";
    
    // Read #unfinished, #success, and/or #failure groups' Pages. For historical reasons, all groups are stored in
    // one vector: PlayerTerminalState uses a simple array index to identify a particular group/page in it.
    uint8_t* p_start = data;
    terminal.pages.reserve(page_count);
    for (int32_t i = 0; i < page_count; i++)
    {
        terminal.pages.emplace_back().unpack_m2_data(data);
    }
    //assert((data - p_start) == static_cast<ptrdiff_t>(SIZEOF_m2_terminal_page) * page_count); // TODO: ditto
    
    std::cout << "\n\n\n=========================================================================\n";
    std::cout << "Reading terminal\n";
    std::cout << "=========================================================================\n";
    
    std::cout << "FOUND PAGES:\n\n";
    terminal.print_debug();
    std::cout << "\n\n";
    std::cout << "=========================================================================\n";
    std::cout << "READING TEXTS:\n\n";
    
    // Read styled Text runs into their corresponding Pages
    unpack_text_for_m2_terminal(terminal, data, data_end, page_count, text_count, is_obfuscated);
    
    std::cout << "\n\n";
    terminal.print_debug();
    
    // Continue with next terminal
    
    data = data_end;
    data_size = terminal_byte_size;
}
