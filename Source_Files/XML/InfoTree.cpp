/*
 Boost.PropertyTree-based structured-data reader and writer

	Copyright (C) 2015 and beyond by Jeremiah Morris
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

#include "InfoTree.h"

#include "shell.h"

#include "DataFile.hpp"

#include <boost/version.hpp>
#include <boost/range/adaptor/map.hpp>


class InfoTreeFileStream : public boost::iostreams::stream<OpenedFileDevice>
{
private:
	DataFile file;
    
public:
    
	explicit InfoTreeFileStream(const ao_path& path, bool write = false)
	{
        ao_err err = file.open(path, write ? DataFile::mode_text_write : DataFile::mode_text_read);
        if (err)
		{
            // TODO: use localizable string resources for all error messages
            const auto msg = std::string("couldn't open '") + path.generic_u8string() + "' for " + (write ? "writing" : "reading") + " (error " + std::to_string(err) + ")";
			throw boost::property_tree::ptree_error(msg); // not sure throwing a boost exception in our code is appropriate, but that's what the existing AO code did so leaving for now
		}
        else
        {
            //log_note_f("InfoTreeFileStream opened: '%s'", path.c_str());
        }
        open(file);
	}
    
    ~InfoTreeFileStream()
    {
        //log_note_f("InfoTreeFileStream closed: '%s'", file.get_path().c_str());
        close();
    }
};


InfoTree InfoTree::load_xml(const ao_path& path)
{
	InfoTreeFileStream stream(path);
	InfoTree xtree;
    boost::property_tree::read_xml<boost::property_tree::iptree>(stream, xtree);
	return xtree;
}


InfoTree InfoTree::load_xml(std::istringstream& stream)
{
	InfoTree xtree;
    boost::property_tree::read_xml<boost::property_tree::iptree>(stream, xtree);
	return xtree;
}


static void write_indented_xml(std::ostream& dest, const boost::property_tree::iptree& src)
{
	using settings_t = boost::property_tree::xml_writer_settings< std::conditional<BOOST_VERSION >= 105600, std::string, char>::type >;
    boost::property_tree::write_xml<boost::property_tree::iptree>(dest, src, settings_t(' ', 2));
}


void InfoTree::save_xml(const ao_path& path) const
{
	InfoTreeFileStream stream(path, /*write:*/ true);
	write_indented_xml(stream, *this);
}


void InfoTree::save_xml(std::ostringstream& stream) const
{
	write_indented_xml(stream, *this);
}


InfoTree InfoTree::load_ini(const ao_path& path)
{
	InfoTreeFileStream stream(path);
	InfoTree itree;
    boost::property_tree::read_ini<boost::property_tree::iptree>(stream, itree);
	return itree;
}


InfoTree InfoTree::load_ini(std::istringstream& stream)
{
	InfoTree itree;
    boost::property_tree::read_ini<boost::property_tree::iptree>(stream, itree);
	return itree;
}


void InfoTree::save_ini(const ao_path& path) const
{
	InfoTreeFileStream stream(path, /*write:*/ true);
    boost::property_tree::write_ini<boost::property_tree::iptree>(stream, *this);
}


void InfoTree::save_ini(std::ostringstream& stream) const
{
    boost::property_tree::write_ini<boost::property_tree::iptree>(stream, *this);
}



// -----------------------------------------------------------------------------------------
// read/write data


bool InfoTree::read_fixed(std::string path, _fixed& value, float min, float max) const
{
	float temp;
	if (read_attr_bounded(path, temp, min, max))
	{
		value = FIXED_ONE * temp + 0.5;
		return true;
	}
	return false;
}

bool InfoTree::read_wu(std::string path, short& value, float min, float max) const
{
	float temp;
	if (read_attr_bounded(path, temp, min, max))
	{
		value = WORLD_ONE * temp + 0.5;
		return true;
	}
	return false;
}

bool InfoTree::read_angle(std::string path, angle& value) const
{
	float temp;
	if (read_attr(path, temp))
	{
		temp = temp - 360*static_cast<int>(temp/360);
		while (temp < 0)
			temp += 360;
		while (temp >= 360)
			temp -= 360;
		value = FULL_CIRCLE*(temp/360) + 0.5;
		return true;
	}
	return false;
}

bool InfoTree::read_path(const std::string& key, ao_path& file) const // TODO: why isn't this expanding string vars? (should it?) what about expanding relative to absolute paths?
{
	std::string path;
	if (read_attr(key, path))
	{
		file = path;
		return true;
	}
	return false;
}

bool InfoTree::read_path(const std::string& key, std::string& path) const
{
	std::string tmp;
	if (read_attr(key, tmp))
	{
        path = expand_symbolic_path(tmp);
		return true;
	}
	return false;
}

void InfoTree::put_attr_path(const std::string& key, const std::string& path)
{
	put_attr(key, contract_symbolic_path(path));
}

bool InfoTree::read_cstr(const std::string& key, std::string& dest) const
{
    return read_attr(key, dest);
}


void InfoTree::put_cstr(const std::string& key, const std::string& cstr)
{
	put(key, cstr);
}

void InfoTree::put_attr_cstr(const std::string& key, const std::string& cstr)
{
	put_attr(key, cstr);
}



static const float ColorToTree = 1 / static_cast<float>(65535);
static const float TreeToColor = static_cast<float>(65535);

static bool _get_color_part(const InfoTree *tree, std::string key, uint16& part)
{
	float value;
	if (tree->read_attr(key, value))
	{
		part = value * TreeToColor;
		return true;
	}
	return false;
}

template<typename T> bool _get_color(const InfoTree *tree, T& color)
{
	bool found_r = _get_color_part(tree, "red", color.red);
	bool found_g = _get_color_part(tree, "green", color.green);
	bool found_b = _get_color_part(tree, "blue", color.blue);
	return found_r || found_g || found_b;
}

static void _set_color_part(InfoTree& tree, std::string key, uint16 part)
{
	tree.put(std::string("<xmlattr>.") + key, part * ColorToTree);
}

template<typename T> InfoTree _make_color(const T& color)
{
	InfoTree ctree;
	_set_color_part(ctree, "red", color.red);
	_set_color_part(ctree, "green", color.green);
	_set_color_part(ctree, "blue", color.blue);
	return ctree;
}
template<typename T> InfoTree _make_color(const T& color, size_t index)
{
	InfoTree ctree;
	ctree.put("<xmlattr>.index", index);
	_set_color_part(ctree, "red", color.red);
	_set_color_part(ctree, "green", color.green);
	_set_color_part(ctree, "blue", color.blue);
	return ctree;
}

InfoTree make_SDL_color(const SDL_Color& color)
{
    InfoTree ctree;
    _set_color_part(ctree, "red", color.r << 8);
    _set_color_part(ctree, "green", color.g << 8);
    _set_color_part(ctree, "blue", color.b << 8);
    return ctree;
}

InfoTree make_SDL_color(const SDL_Color& color, size_t index)
{
    InfoTree ctree;
    ctree.put("<xmlattr>.index", index);
    _set_color_part(ctree, "red", color.r << 8);
    _set_color_part(ctree, "green", color.g << 8);
    _set_color_part(ctree, "blue", color.b << 8);
    return ctree;
}


bool InfoTree::read_color(rgb_color& color) const
{
	return _get_color(this, color);
}

bool InfoTree::read_color(SDL_Color& color) const
{
    rgb_color c;
    if (!_get_color(this, c)) return false;
    color = {(uint8_t)(c.red >> 8), (uint8_t)(c.green >> 8), (uint8_t)(c.blue >> 8), 0xff};
    return true;
}

void InfoTree::add_color(std::string path, const rgb_color& color)
{
	add_child(path, _make_color(color));
}
void InfoTree::add_color(std::string path, const rgb_color& color, size_t index)
{
	add_child(path, _make_color(color, index));
}

void InfoTree::add_color(std::string path, const SDL_Color& color)
{
    add_child(path, make_SDL_color(color));
}
void InfoTree::add_color(std::string path, const SDL_Color& color, size_t index)
{
    add_child(path, make_SDL_color(color, index));
}

bool InfoTree::read_shape(shape_descriptor& descriptor, bool allow_empty) const
{
	uint16 seq = UNONE;
	bool seq_present = read_attr_bounded<uint16>("seq", seq, 0, MAXIMUM_SHAPES_PER_COLLECTION-1);
	if (!seq_present)
		seq_present = read_attr_bounded<uint16>("frame", seq, 0, MAXIMUM_SHAPES_PER_COLLECTION-1);
	
	uint16 coll = UNONE;
	bool coll_present = read_attr_bounded<uint16>("coll", coll, 0, MAXIMUM_COLLECTIONS-1);
	
	uint16 clut = 0;
	read_attr_bounded<uint16>("clut", clut, 0, MAXIMUM_CLUTS_PER_COLLECTION-1);
	
	if (coll_present && seq_present)
	{
		descriptor = BUILD_DESCRIPTOR(BUILD_COLLECTION(coll, clut), seq);
		return true;
	}
	else if (!coll_present && !seq_present && allow_empty)
	{
		descriptor = UNONE;
		return true;
	}
	return false;
}

bool InfoTree::read_damage(damage_definition& def) const
{
	bool status = false;
	if (read_indexed("type", def.type, NUMBER_OF_DAMAGE_TYPES, true))
		status = true;
	if (read_indexed("flags", def.flags, 2))
		status = true;
	if (read_attr("base", def.base))
		status = true;
	if (read_attr("random", def.random))
		status = true;
	if (read_fixed("scale", def.scale))
		status = true;
	return status;
}

bool InfoTree::read_font(font_key_t& font) const
{
	bool status = false;
	if (read_attr("size", font.size))
		status = true;
	if (read_attr("style", font.style))
		status = true;
	//if (read_attr("file", font.File)) // TO DO: FIX
	//	status = true;
	//if (status)
	//	font.Update();
	return status;
}


typedef boost::iterator_range<InfoTree::const_assoc_iterator> _match_range_type;

InfoTree::const_child_range InfoTree::children_named(std::string key) const
{
	std::pair<const_assoc_iterator, const_assoc_iterator> matches = equal_range(key);
	_match_range_type match_range = boost::make_iterator_range(matches.first, matches.second);
	return boost::adaptors::values(static_cast<const _match_range_type&>(match_range));
}
