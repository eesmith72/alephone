/*
 Console.cpp - console utilities for Aleph One

 Copyright (C) 2005 and beyond by Gregory Smith
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

// TODO: FIX: this is not UTF8-aware; w_text_entry contains duplicate editing functionality so merge them

#include "Console.h"

#include "preferences.hpp"

#include "network.h"

// for carnage reporting:
#include "player.h"
#include "projectiles.h"

// for saving
#include "DataFile.hpp"
#include "map_wad.h"

#include "InfoTree.h"




void handle_console_key(const SDL_Event &event)
{
    if (event.key.keysym.mod & KMOD_CTRL) // Ctrl-KEY = Unix shell editing/navigation controls
    {
        switch(event.key.keysym.sym)
        {
            case SDLK_a:
                Console::instance()->line_home();
                break;
            case SDLK_b:
                Console::instance()->left_arrow();
                break;
            case SDLK_d:
                Console::instance()->del();
                break;
            case SDLK_e:
                Console::instance()->line_end();
                break;
            case SDLK_f:
                Console::instance()->right_arrow();
                break;
            case SDLK_h:
                Console::instance()->backspace();
                break;
            case SDLK_k:
                Console::instance()->forward_clear();
                break;
            case SDLK_n:
                Console::instance()->down_arrow();
                break;
            case SDLK_p:
                Console::instance()->up_arrow();
                break;
            case SDLK_t:
                Console::instance()->transpose();
                break;
            case SDLK_u:
                Console::instance()->clear();
                break;
            case SDLK_w:
                Console::instance()->delete_word();
                break;
        }
    }
    else
    {
        switch(event.key.keysym.sym)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                Console::instance()->enter();
                break;
            case SDLK_ESCAPE:
                Console::instance()->abort();
                break;
            case SDLK_BACKSPACE:
                Console::instance()->backspace();
                break;
            case SDLK_DELETE:
                Console::instance()->del();
                break;
            case SDLK_UP:
                Console::instance()->up_arrow();
                break;
            case SDLK_DOWN:
                Console::instance()->down_arrow();
                break;
            case SDLK_LEFT:
                Console::instance()->left_arrow();
                break;
            case SDLK_RIGHT:
                Console::instance()->right_arrow();
                break;
            case SDLK_HOME:
                Console::instance()->line_home();
                break;
            case SDLK_END:
                Console::instance()->line_end();
                break;
        }
    }
}




// from Network/
extern void hub_set_minimum_send_period(int32);
extern int32& hub_get_minimum_send_period();

struct set_latency_tolerance
{
    void operator() (const std::string& arg) const
    {
        hub_set_minimum_send_period(atoi(arg.c_str()));
        screen_print_f("latency tolerance is now %s", arg.c_str());
        write_preferences();
    }
};

struct get_latency_tolerance
{
    void operator() (const std::string&) const
    {
        screen_print_f("latency tolerance is %d",hub_get_minimum_send_period());
    }
};





Console::Console() : m_active(false), m_carnage_messages_exist(false), m_use_lua_console(true)
{
	m_command_iter = m_prev_commands.end();
	m_carnage_messages.resize(NUMBER_OF_PROJECTILE_TYPES);
	register_save_commands();
    
    // pulled these out of initialize_preferences(!)
    CommandParser PreferenceSetCommandParser;
    PreferenceSetCommandParser.register_command("latency_tolerance", set_latency_tolerance());
    CommandParser PreferenceGetCommandParser;
    PreferenceGetCommandParser.register_command("latency_tolerance", get_latency_tolerance());
    
    CommandParser PreferenceCommandParser;
    PreferenceCommandParser.register_command("set", PreferenceSetCommandParser);
    PreferenceCommandParser.register_command("get", PreferenceGetCommandParser);
    register_command("preferences", PreferenceCommandParser);
}





Console *Console::instance() {
	static Console *m_instance = nullptr;
	if (!m_instance) {
		m_instance = new Console;
	}
	return m_instance;
}




static inline void lowercase(string& s) // inasmuch as Console commands are in US English, this is probably sufficient
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}


static std::pair<std::string, std::string> split(string buffer)
{
    std::string command;
    std::string remainder;

    std::string::size_type pos = buffer.find(' ');
	if (pos != std::string::npos && pos < buffer.size())
	{
		remainder = buffer.substr(pos + 1);
		command = buffer.substr(0, pos);
	}
	else
	{
		command = buffer;
	}

	return std::pair<std::string, std::string>(command, remainder);
}


void CommandParser::register_command(std::string command, std::function<void(const std::string&)> f)
{
	lowercase(command);
	m_commands[command] = f;
}

void CommandParser::register_command(std::string command, const CommandParser& command_parser)
{
	lowercase(command);
	m_commands[command] = std::bind(&CommandParser::parse_and_execute, command_parser, std::placeholders::_1);
}

void CommandParser::unregister_command(std::string command)
{
	lowercase(command);
	m_commands.erase(command);
}

void CommandParser::parse_and_execute(const std::string& command_string)
{
	std::pair<std::string, std::string> cr = split(command_string);

    std::string command = cr.first;
    std::string remainder = cr.second;

	lowercase(command);
	
	command_map::iterator it = m_commands.find(command);
	if (it != m_commands.end())
	{
		it->second(remainder);
	}
}


void Console::enter() {
	// store entered command if not duplicate of previous command
	if (m_prev_commands.size() == 0 ||
	    m_buffer != *(m_prev_commands.end()-1)) {
		m_prev_commands.push_back(m_buffer);
	}
	m_command_iter = m_prev_commands.end();
	
	// macros are processed first
	if (m_buffer[0] == '.')
	{
		std::pair<std::string, std::string> mr = split(m_buffer.substr(1));

        std::string input = mr.first;
        std::string output = mr.second;

		lowercase(input);

		std::map<string, std::string>::iterator it = m_macros.find(input);
		if (it != m_macros.end())
		{
			if (output != "")
				m_buffer = it->second + " " + output;
			else
				m_buffer = it->second;
		}
	}

	// commands are processed before callbacks
	if (m_buffer[0] == '.')
	{
		parse_and_execute(m_buffer.substr(1));
	} else if (!m_callback) {
        log_anomaly("console enter activated, but no callback set");
	} else {
		m_callback(m_buffer);
	}
	
	m_callback = nullptr;
	m_buffer.clear();
	m_displayBuffer.clear();
	m_active = false;
	SDL_StopTextInput();
}

void Console::abort() {
	m_buffer.clear();
	m_displayBuffer.clear();
	if (!m_callback) {
        log_anomaly("console abort activated, but no callback set");
	} else {
		m_callback(m_buffer);
	}

	m_callback = nullptr;
	m_active = false;
	SDL_StopTextInput();
}

void Console::backspace() {
	if (m_cursor_position > 0) {
		m_cursor_position--;
		m_buffer.erase(m_cursor_position, 1);
		m_displayBuffer.erase(cursor_position(), 1);
	}
}

void Console::del() {
	if (m_cursor_position < m_buffer.length()) {
		m_buffer.erase(m_cursor_position, 1);
		m_displayBuffer.erase(cursor_position(), 1);
	}
}

void Console::clear() {
	if (m_cursor_position > 0) {
		m_buffer.erase(0, m_cursor_position);
		m_displayBuffer.erase(m_prompt.length() + 1, cursor_position());
		m_cursor_position = 0;
	}
}

void Console::forward_clear() {
	if (m_cursor_position < m_buffer.length()) {
		m_buffer.erase(m_cursor_position);
		m_displayBuffer.erase(cursor_position());
	}
}

void Console::transpose() {
	if (m_cursor_position > 0) {
		if (m_cursor_position == m_buffer.length())
			--m_cursor_position;
		--m_cursor_position;
		char tmp = m_buffer[m_cursor_position];
		m_buffer.erase(m_cursor_position, 1);
		m_displayBuffer.erase(cursor_position(), 1);
		++m_cursor_position;
		m_buffer.insert(m_cursor_position, 1, tmp);
		m_displayBuffer.insert(cursor_position(), 1, tmp);
		++m_cursor_position;
	}
}

void Console::delete_word() {
	int erase_position = m_cursor_position;
	while (erase_position && m_buffer[erase_position - 1] == ' ')
		--erase_position;
	while (erase_position && m_buffer[erase_position - 1] != ' ')
		--erase_position;
	int erase_length = m_cursor_position - erase_position;
	if (erase_length > 0) {
		m_buffer.erase(erase_position, erase_length);
		m_displayBuffer.erase(cursor_position() - erase_length, erase_length);
		m_cursor_position = erase_position;
	}
}

void Console::textEvent(const SDL_Event &e) {
	std::string s = e.text.text;
	m_buffer.insert(m_cursor_position, s);
	m_displayBuffer.insert(cursor_position(), s);
	m_cursor_position += s.size(); // TODO: check this
}

// up and down arrows display previously entered commands at current prompt
void Console::up_arrow() {
	if (m_command_iter == m_prev_commands.begin()) return;
	m_command_iter--;
	set_command(*m_command_iter);
}

void Console::down_arrow() {
	if (m_command_iter == m_prev_commands.end()) return;
	m_command_iter++;
	if (m_command_iter == m_prev_commands.end()) {
		set_command("");
	} else {
		set_command(*m_command_iter);
	}
}

void Console::set_command(std::string command) {
	m_buffer = command;
	m_displayBuffer = m_prompt + " " + m_buffer;
	m_cursor_position = (int32_t)command.length();
}

void Console::left_arrow() {
	if (m_cursor_position > 0) {
		m_cursor_position--;
	}
}

void Console::right_arrow() {
	if (m_cursor_position < m_buffer.length()) {
		m_cursor_position++;
	}
}

void Console::line_home() {
	m_cursor_position = 0;
}

void Console::line_end() {
	m_cursor_position = (int32_t)m_buffer.length();
}

void Console::activate_input(std::function<void (const std::string&)> callback,
			     const std::string& prompt)
{
	assert_fail(!m_active, "");
	m_callback = callback;
	m_buffer.clear();
	m_displayBuffer = m_prompt = prompt;
	m_displayBuffer += " ";
	m_active = true;
	m_cursor_position = 0;
	
	SDL_StartTextInput();
	SDL_FlushEvent(SDL_TEXTINPUT);
}

void Console::deactivate_input() {
	m_buffer.clear();
	m_displayBuffer.clear();
	
	m_callback = nullptr;
	m_active = false;
	SDL_StopTextInput();
}

int Console::cursor_position() {
	return (int32_t)m_prompt.length() + 1 + m_cursor_position;
}

void Console::register_macro(string input, std::string output)
{
	lowercase(input);
	m_macros[input] = output;
}

void Console::unregister_macro(std::string input)
{
	lowercase(input);
	m_macros.erase(input);
}

void Console::clear_macros()
{
	m_macros.clear();
}

void Console::set_carnage_message(int16 projectile_type, const std::string& on_kill, const std::string& on_suicide)
{
	m_carnage_messages_exist = true;
	m_carnage_messages[projectile_type] = std::pair<std::string, std::string>(on_kill, on_suicide);
}

static std::string replace_first(std::string &result, const std::string& from, const std::string& to)
{
	const int pos = (int32_t)result.find(from);
	if (pos != string::npos)
	{
		result.replace(pos, from.size(), to);
	}
	return result;
}


void Console::report_kill(int16 player_index, int16 aggressor_player_index, int16 projectile_index)
{
	if (!game_is_networked() || !NetAllowCarnageMessages() || !m_carnage_messages_exist || projectile_index == -1) return;

	// do some lookups
	projectile_data *projectile = 0;
	if (projectile_index != NONE) 
	{
		projectile = get_projectile_data(projectile_index);
	}

	const std::string player_key = "%player%";
	const std::string aggressor_key = "%aggressor%";
	if (projectile)
	{
		std::string display_string;
		std::string player_name = get_player_data(player_index)->name;
		if (player_index != aggressor_player_index)
		{
			display_string = m_carnage_messages[projectile->type].first;
			if (display_string == "") return;
			std::string aggressor_player_name = get_player_data(aggressor_player_index)->name;

			const int ppos = (int32_t)display_string.find(player_key);
			const int apos = (int32_t)display_string.find(aggressor_key);
			if (ppos == std::string::npos || apos == std::string::npos || ppos > apos)
			{
				replace_first(display_string, player_key, player_name);
				replace_first(display_string, aggressor_key, aggressor_player_name);
			}
			else
			{
				replace_first(display_string, aggressor_key, aggressor_player_name);
				replace_first(display_string, player_key, player_name);
			}
			
		}
		else
		{
			display_string = m_carnage_messages[projectile->type].second;
			if (display_string == "") return;
			replace_first(display_string, player_key, player_name);
		}

		screen_print(display_string);
	}
}

void Console::clear_carnage_messages()
{
	m_carnage_messages.clear();
	m_carnage_messages.resize(NUMBER_OF_PROJECTILE_TYPES);
	m_carnage_messages_exist = false;
}

bool Console::use_lua_console()
{
    return m_use_lua_console || environment_preferences.use_solo_lua;
};





struct save_level
{
	void operator() (const std::string& arg) const
    {
		if (!NetAllowSavingLevel())
		{
			screen_print("Level saving disabled");
			return;
		}
        
        static std::string last_level_name = "";
        static std::string last_file_name = "";
        
        if (last_level_name != static_world.level_name)
        {
            last_level_name = static_world.level_name;
            last_file_name.clear();
        }
        
		std::string filename = arg;
		if (!arg.empty())
        {
            last_level_name = filename;
        }
        else if (last_level_name.empty()) // generate new name
        {
            filename = last_level_name = static_world.level_name;
        }
        else // reuse previous name
        {
            filename = last_level_name;
        }
        // TODO: FIX: filename needs any non-FS-safe characters removed (including leading period) and, if empty, use "Untitled"
        
        if (!boost::algorithm::ends_with(filename, ".sceA")) { filename += ".sceA"; }
		
        ao_path path = get_local_storage_dir() / filename;
        ao_err err = export_level(path);
		if (err)
        {
            // TODO: Console should be doing its own screen display
            screen_print_f("Error %d occurred while saving level as \"%s\"", err, path.c_str());
        }
		else
        {
            screen_print_f("Saved level as \"%s\"", path.c_str());
        }
	}
};


void Console::register_save_commands()
{
	CommandParser saveParser;
	saveParser.register_command("level", save_level());
	register_command("save", saveParser);
}








void reset_mml_console()
{
	Console *console = Console::instance();
	console->use_lua_console(true);
	console->clear_macros();
	console->clear_carnage_messages();
}


void parse_mml_console(const InfoTree& root)
{
	Console *console = Console::instance();

	bool use_lua_console = true;
	if (root.read_attr("use_lua_console", use_lua_console))
    {
        console->use_lua_console(use_lua_console);
    }
	for (const InfoTree &macro : root.children_named("macro"))
	{
		std::string input, output;
		if (!macro.read_attr("input", input) || !input.size()) continue;
		
		macro.read_attr("output", output);
		console->register_macro(input, output);
	}
	for (const InfoTree &message : root.children_named("carnage_message"))
	{
		int16 projectile_type;
		if (!message.read_indexed("projectile_type", projectile_type, NUMBER_OF_PROJECTILE_TYPES)) continue;
		
		std::string on_kill, on_suicide;
		message.read_attr("on_kill", on_kill);
		message.read_attr("on_suicide", on_suicide);
		console->set_carnage_message(projectile_type, on_kill, on_suicide);
	}
}

