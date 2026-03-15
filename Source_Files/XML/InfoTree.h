#ifndef _INFO_TREE_
#define _INFO_TREE_

/*

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

	Boost.PropertyTree-based structured-data reader and writer
*/


#include "cseries.h"
#include "DataFile.hpp"
#include "fonts.hpp"
#include "map.h"
#include "world.h"
#include <type_traits>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/any_range.hpp>

class InfoTree : public boost::property_tree::iptree
{
public:
    
    typedef boost::property_tree::ptree_error Exception; // EES: AO really doesn't care about the exact exception type so let's simplify try-catch blocks
    
	InfoTree() {}
	explicit InfoTree(const data_type &data) : boost::property_tree::iptree(data) {}
	InfoTree(const boost::property_tree::iptree &rhs) : boost::property_tree::iptree(rhs) {}
	
    static InfoTree load_xml(const ao_path& path);
	static InfoTree load_xml(std::istringstream& stream);
	void save_xml(const ao_path& path) const;
	void save_xml(std::ostringstream& stream) const;
	
	static InfoTree load_ini(const ao_path& path);
	static InfoTree load_ini(std::istringstream& stream);
	void save_ini(const ao_path& path) const;
	void save_ini(std::ostringstream& stream) const;

	template<typename T> bool read(std::string path, T& value) const
	{
		try
        {
			value = get_child(path).get_value<T>();
			return true;
		}
        catch (const Exception& ep)
        {
            return false;
        }
	}

	template<typename T> bool read_attr(std::string path, T& value) const
	{
		return read(std::string("<xmlattr>.") + path, value);
	}

	template<typename T, typename std::enable_if_t<!std::is_enum<T>::value, bool>* = nullptr> bool read_attr_bounded(std::string path, T& value, const T min, const T max) const
	{
		T temp;
		if (read_attr(path, temp) && temp >= min && temp <= max)
		{
			value = temp;
			return true;
		}
		return false;
	}

	template<typename T, typename std::enable_if_t<std::is_enum<T>::value, bool>* = nullptr> bool read_attr_bounded(std::string path, T& value, const T min, const T max) const
	{
		std::underlying_type_t<T> temp;
		if (read_attr(path, temp) &&
			temp >= static_cast<std::underlying_type_t<T>>(min) &&
			temp <= static_cast<std::underlying_type_t<T>>(max))
		{
			value = static_cast<T>(temp);
			return true;
		}
		return false;
	}
	
	bool read_indexed(std::string path, int16& value, int num_slots, bool allow_none = false) const
	{
		return read_attr_bounded<int16>(path, value, (allow_none ? NONE : 0), num_slots - 1);
	}

	template<typename T> void put_attr(std::string path, const T value)
	{
		put(std::string("<xmlattr>.") + path, value);
	}
	
	bool read_color(RGBColor& color) const;
	bool read_color(rgb_color& color) const;
    bool read_color(SDL_Color& color) const;
	bool read_shape(shape_descriptor& descriptor, bool allow_empty = true) const;
	bool read_damage(damage_definition& definition) const;
	bool read_font(font_key_t& font) const;
	
	bool read_path(const std::string& key, ao_path& file) const;
	bool read_path(const std::string& key, std::string& dest) const;
    
    bool read_string(const std::string& key, std::string& dest) const;
	bool read_cstr(const std::string& key, std::string& dest) const;
	
    bool read_fixed(std::string key, _fixed& value, float min = -SHRT_MAX, float max = SHRT_MAX) const;
	bool read_wu(std::string key, short& value, float min = -64, float max = 64) const;
	bool read_angle(std::string key, angle& value) const;
	
	void add_color(std::string path, const RGBColor& color);
	void add_color(std::string path, const RGBColor& color, size_t index);
	void add_color(std::string path, const rgb_color& color);
	void add_color(std::string path, const rgb_color& color, size_t index);
    void add_color(std::string path, const SDL_Color& color);
    void add_color(std::string path, const SDL_Color& color, size_t index);
	
	void put_attr_path(const std::string& path, const std::string& filepath);
	void put_cstr(const std::string& path, const std::string& cstr);
	void put_attr_cstr(const std::string& path, const std::string& cstr);
	
	typedef boost::any_range<const InfoTree, boost::forward_traversal_tag, const InfoTree, std::ptrdiff_t> const_child_range;
	const_child_range children_named(std::string key) const;
};

#endif
