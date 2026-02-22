/* csfonts.h

	Copyright (C) 1991-2001 and beyond by Bo Lindbergh
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


    Sept-Nov 2001 (Woody Zenfell): new styleOutline helps put a "halo" (heh) around text.
*/

#ifndef _CSERIES_FONTS_
#define _CSERIES_FONTS_

#include "cstypes.h"
#include <string>




typedef int16_t font_style_t; // TODO: proper typing will have to wait
enum
{
    styleNormal     = 0,
    styleBold       = 1,
    styleItalic     = 2,
    styleUnderline  = 4,
    // styleOutline = 8, // can't be used with TTF
    styleShadow     = 16,
};

struct TextSpec {
	int16 font;

	uint16 style;
	int16 size;
	int16 adjust_height;

	// paths to fonts
	std::string normal;
	std::string oblique;
	std::string bold;
	std::string bold_oblique;
};

#endif

