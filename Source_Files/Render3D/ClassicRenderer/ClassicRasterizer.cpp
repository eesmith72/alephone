/*
 ClassicRasterizer.cpp -- originally SCOTTISH_TEXTURES.C
 
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


#include "ClassicRasterizer.h"

#include "low_level_textures.h"
#include "render.h"
#include "Screen.hpp"
#include "visual_effects.hpp"
#include "shapes.h" // gameworld_color_table_8 (used to construct gamma-adjusted indexed colors for 256-color rendering)


// boosted to cope with big displays
#define MAXIMUM_SCRATCH_TABLE_ENTRIES (8192)
#define MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE (MAX(sizeof(_vertical_polygon_data), sizeof(_horizontal_polygon_line_data)))

#define SHADE_TO_SHADING_TABLE_INDEX_8(shade)  ((shade) >> (FIXED_FRACTIONAL_BITS - shading_table_fractional_bits_8))
#define SHADE_TO_SHADING_TABLE_INDEX_16(shade) ((shade) >> (FIXED_FRACTIONAL_BITS - shading_table_fractional_bits_16))

#define DEPTH_TO_SHADE(d) (ao_fixed(d) << (FIXED_FRACTIONAL_BITS - WORLD_FRACTIONAL_BITS - 3))


/* Set aside temporary storage for two line tables (remember, we precalculate all the y-values
   for trapezoids and two lines worth of x-values for polygons before mapping them).
   These tables are used by the polygon rasterizer (to store the x-coordinates of the left and
   right lines of the current polygon), the trapezoid rasterizer (to store the y-coordinates
   of the top and bottom of the current trapezoid) and the rectangle mapper (for its
   vertical and, if necessary, horizontal distortion tables). These are not used to share data
   between functions, just as per-function storage. */
static short scratch_table0[MAXIMUM_SCRATCH_TABLE_ENTRIES];
static short scratch_table1[MAXIMUM_SCRATCH_TABLE_ENTRIES];
static uint8_t precalculation_table[MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE * MAXIMUM_SCRATCH_TABLE_ENTRIES];


static short* build_x_table(short* table, short x0, short y0, short x1, short y1);
static short* build_y_table(short* table, short x0, short y0, short x1, short y1);


//-----------------------------------------------------------------------------
// Color mapping tables for converting rendered 8/16-bit pixels to RGB32 for upload to GPU texture

// These incorporate gamma curve (added below), plus any liquid tint and/or hit effect fades mixed in by visual_effects.cpp

// Classic 8-bit indexed color to RGBA32:
static uint32_t color_map_i[256];

// Classic 16-bit RGB565 to RGBA32:
// (This also works for 24/32-bit color but we don't bother with a Classic 32 screen mode as it's not very interesting.)
// The channels are already bit-shifted into place so bitwise-OR them to get the final pixel color.
static uint32_t color_map_r[256];
static uint32_t color_map_g[256];
static uint32_t color_map_b[256];


SDL_PixelFormat output_pixel_format;


void initialize_classic_color_map()
{
    SDL_PixelFormat *pf;
    pf = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    output_pixel_format = *pf;
    SDL_FreeFormat(pf);
}


// Classic renderer base color tables

color_table_t gamma_8_gameworld_color_table; // used in Classic 8, the Shapes collections' indexed colors with any gamma applied
color_table_t gamma_16_curve_color_table; // used in Classic 16, the gamma curve to apply to gameworld's pixels


void set_classic_gamma(float gamma) // called by Screen::set_gameworld_gamma; // TODO: make this a method on ClassicRasterizer
{
    assert_fail(gameworld_color_table_8.color_count > 0, ""); // AO must initialize in order: preferences, shapes, rasterizer
    
    gamma_8_gameworld_color_table.make_copy_with_gamma(gameworld_color_table_8, gamma); // apply gamma directly to the clut colors
    gamma_16_curve_color_table.make_gamma(gamma); // build grayscale gamma curve
}


// Called by visual_effects.cpp (was fades.cpp) to get the SW rendering color table so it can apply any fades to it:
const color_table_t* get_classic_color_table() // TODO: make this a method on ClassicRasterizer and have `configure` set an ivar to the active table so we don't have to test bit_depth every single frame
{
    return main_screen.bit_depth() == 8 ? &gamma_8_gameworld_color_table : &gamma_16_curve_color_table;
}


// visual_effects.cpp then passes the updated color table back here and new color mapping tables are assembled.
// `convert_classic_xx_pixels_to_rgb32` will apply these tables as it converts the rendered gameworld's pixels to RGB32.
void set_classic_color_map(const color_table_t& color_table) // (replaces `animate_screen_clut`)
{
    if (main_screen.bit_depth() == 8)
    {
        for (int32_t i = 0; i < color_table.color_count; i++)
        {
            const ao_rgb& color = color_table.colors[i];
            
            color_map_i[i] = ((color.r >> 8) << pixel_format_32.Rshift)
                           | ((color.g >> 8) << pixel_format_32.Gshift)
                           | ((color.b >> 8) << pixel_format_32.Bshift)
                           | pixel_format_32.Amask;
        }
    }
    else
    {
        for (int32_t i = 0; i < color_table.color_count; i++)
        {
            const ao_rgb& color = color_table.colors[i];
            
            color_map_r[i] = ((color.r >> 8) << pixel_format_32.Rshift) | pixel_format_32.Amask;
            color_map_g[i] = ((color.g >> 8) << pixel_format_32.Gshift);
            color_map_b[i] = ((color.b >> 8) << pixel_format_32.Bshift);
        }
    }
}


void reset_classic_color_map()
{
    set_classic_color_map((main_screen.bit_depth() == 8) ? gamma_8_gameworld_color_table : gamma_16_curve_color_table);
}


//-----------------------------------------------------------------------------
// Transform the rendered 8/16-bit "virtual screen" pixel buffer to RGBA32 for uploading to GPU texture.

// These functions are a bit more complex than using SDL_ConvertSurfaceFormat, but they also apply the
// SW gamma+liquid tint+damage effects.

static void convert_classic_8_pixels_to_rgb32(bitmap_definition_t& bitmap_definition)
{
    uint8_t* pixels_src = (uint8_t*)bitmap_definition.bitmap.data();
    uint32_t* pixels_dst = (uint32_t*)bitmap_definition.bitmap.data();
    
    int32_t pixel_count = bitmap_definition.width * bitmap_definition.height;
    
    for (int32_t i = pixel_count - 1; i >= 0; i--) // iterate in reverse so we don't overwrite ourselves
    {
        pixels_dst[i] = color_map_i[pixels_src[i]];
    }
}


static void convert_classic_16_pixels_to_rgb32(bitmap_definition_t& bitmap_definition)
{
    uint16_t* pixels_src = (uint16_t*)bitmap_definition.bitmap.data();
    uint32_t* pixels_dst = (uint32_t*)bitmap_definition.bitmap.data();

    int32_t pixel_count = bitmap_definition.width * bitmap_definition.height;
    
    for (int32_t i = pixel_count - 1; i >= 0; i--)
    {
        // 16-bit uses RGB565, so bitshift them apart to get 3x 8-bit R,G,B values
        uint16_t pixel = pixels_src[i];
        pixels_dst[i] = color_map_r[(pixel & 0xf800) >> 8]
                      | color_map_g[(pixel & 0x07e0) >> 3]
                      | color_map_b[(pixel & 0x001f) << 3];
    }
}


//-----------------------------------------------------------------------------


void ClassicRasterizer::configure(const SDL_Point& size, int32_t bit_depth)
{
    // Classic 8-bit and 16-bit SW rendering has historical value; we omit 32-bit color as the Modern modes provide that
    assert_fail((size.x <= 800 && size.y <= 600) && (bit_depth == 8 || bit_depth == 16), "");
    
    // skip if we can reuse the existing surface
    if (this->size.x == size.x && this->size.y == size.y && this->bit_depth == bit_depth) return;
    
    this->size = size;
    this->bit_depth = bit_depth;
    
    set_classic_gamma(graphics_preferences.gamma_adjustment());
    
    switch (bit_depth)
    {
        case 8:
            convert_bitmap_to_rgb32 = convert_classic_8_pixels_to_rgb32;
            break;
        case 16:
            convert_bitmap_to_rgb32 = convert_classic_16_pixels_to_rgb32;
            break;
        default:
            throw_bug_report_f("Unsupported bit depth: %d", bit_depth);
    }
    
    m_bitmap_definition.width             = size.x; // 640/800 with M2 HUD
    m_bitmap_definition.height            = size.y; // 320/400 with M2 HUD; 480/600 if fullscreen
    m_bitmap_definition.bytes_per_row     = size.x * (bit_depth / 8);
    m_bitmap_definition.flags             = 0;
    m_bitmap_definition.bit_depth         = bit_depth;
    m_bitmap_definition.initialize_row_addresses(); // this doesn't care if the buffer is larger than needed
}


//-----------------------------------------------------------------------------


void ClassicRasterizer::End()
{
    convert_bitmap_to_rgb32(m_bitmap_definition);
    
    glEnable(GL_TEXTURE_2D);
    
    GLuint ref;
    glGenTextures(1, &ref);
    glBindTexture(GL_TEXTURE_2D, ref);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 m_bitmap_definition.width, m_bitmap_definition.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, m_bitmap_definition.bitmap.data());
    
    // disable everything but clipping
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    
    OGL_RenderTexturedRect(0, 0, size.x, size.y, 0, 0, 1, 1);
    
    glPopAttrib();
    
    glDeleteTextures(1, &ref);
    
    main_screen.request_swap();
}



//-----------------------------------------------------------------------------


template <class T>
void draw_dither(T* p, SDL_Point& size, int32_t offset)
{
    for (int32_t y = 0; y < size.y; y++)
    {
        for (int32_t x = y & 1; x < size.x; x += 2)
        {
            p[x] = 0;
        }
        p += offset;
    }
}


void ClassicRasterizer::darken()
{
    switch (bit_depth)
    {
        case 8:
            draw_dither((pixel8*)m_bitmap_definition.bitmap.data(), size, m_bitmap_definition.bytes_per_row);
            break;
        case 16:
            draw_dither((pixel16*)m_bitmap_definition.bitmap.data(), size, m_bitmap_definition.bytes_per_row / 2);
            break;
        default:
            throw_bug_report_f("Unsupported bit_depth: %d", bit_depth);
    }
}


//-----------------------------------------------------------------------------


// i0 + i1 == MAX(i0, i1) + MIN(i0, i1)/2
void ClassicRasterizer::calculate_shading_table(void*& result, void* shading_tables, short depth, ao_fixed ambient_shade)
{
    short table_index;
    if (ambient_shade < 0)
    {
        table_index = (bit_depth == 8) ? SHADE_TO_SHADING_TABLE_INDEX_8(-ambient_shade) : SHADE_TO_SHADING_TABLE_INDEX_16(-ambient_shade);
    }
    else
    {
        ao_fixed shade = PIN((view->maximum_depth_intensity - DEPTH_TO_SHADE(depth)), 0, FIXED_ONE);
        shade = (ambient_shade > shade) ? (ambient_shade + (shade >> 1)) : (shade + (ambient_shade >> 1));
        table_index = (bit_depth == 8) ? SHADE_TO_SHADING_TABLE_INDEX_8(shade) : SHADE_TO_SHADING_TABLE_INDEX_16(shade);
    }
    
    switch (bit_depth)
    {
        case 8:
            result = ((uint8_t*)shading_tables) + MAXIMUM_SHADING_TABLE_INDEXES * sizeof(pixel8) * CEILING(table_index, number_of_shading_tables_8 - 1);
            break;
        case 16:
            result = ((uint8_t*)shading_tables) + MAXIMUM_SHADING_TABLE_INDEXES * sizeof(pixel16) * CEILING(table_index, number_of_shading_tables_16 - 1);
            break;
        default:
            throw_bug_report_f("Unsupported bit_depth: %d", bit_depth);
    }
}


//-----------------------------------------------------------------------------


void ClassicRasterizer::texture_horizontal_polygon(polygon_definition& textured_polygon)
{
	polygon_definition *polygon = &textured_polygon;	// Reference to pointer
	short vertex, highest_vertex, lowest_vertex;
	point2d *vertices= polygon->vertices;

	assert_fail(polygon->vertex_count>=MINIMUM_VERTICES_PER_SCREEN_POLYGON&&polygon->vertex_count<MAXIMUM_VERTICES_PER_SCREEN_POLYGON, "");

	/* if we get static, tinted or landscaped transfer modes punt to the vertical polygon mapper */
	if (polygon->transfer_mode == _static_transfer) {
		texture_vertical_polygon(textured_polygon);
		return;
	}

	/* locate the vertically highest (closest to zero) and lowest (farthest from zero) vertices */
	highest_vertex= lowest_vertex= 0;
	for (vertex= 0; vertex<polygon->vertex_count; ++vertex)
	{
		if (!(vertices[vertex].x>=0&&vertices[vertex].x<=m_bitmap_definition.width&&vertices[vertex].y>=0&&vertices[vertex].y<=m_bitmap_definition.height))
		{
		//	ao__dprintf__("vertex #%d/#%d out of bounds:;dm %x %x;g;", vertex, polygon->vertex_count, polygon->vertices, polygon->vertex_count*sizeof(point2d));
			return;
		}
		if (vertices[vertex].y<vertices[highest_vertex].y) highest_vertex= vertex;
		else if (vertices[vertex].y>vertices[lowest_vertex].y) lowest_vertex= vertex;
	}

	/* if this polygon is not a horizontal line, draw it */
	if (highest_vertex!=lowest_vertex)
	{
		short left_line_count, right_line_count, total_line_count;
		short aggregate_left_line_count, aggregate_right_line_count, aggregate_total_line_count;
		short left_vertex, right_vertex;
		short *left_table= scratch_table0, *right_table= scratch_table1;

		left_line_count= right_line_count= 0; /* zero counts so the left and right lines get initialized */
		aggregate_left_line_count= aggregate_right_line_count= 0; /* we’ve precalculated nothing initially */
		left_vertex= right_vertex= highest_vertex; /* both sides start at the highest vertex */
		total_line_count= vertices[lowest_vertex].y-vertices[highest_vertex].y; /* calculate vertical line count */

		assert_fail(total_line_count<MAXIMUM_SCRATCH_TABLE_ENTRIES, ""); /* make sure we have enough scratch space */
		
		/* precalculate high and low y-coordinates for every x-coordinate */			
		aggregate_total_line_count= total_line_count;
		while (total_line_count>0)
		{
			
			/* if we’re out of scan lines on the left side, get a new vertex and build a table
				of x-coordinates so we can walk toward the new vertex */
			if (left_line_count<=0)
			{
				do /* counter-clockwise vertex search */
				{
					vertex= left_vertex ? (left_vertex-1) : (polygon->vertex_count-1);
					left_line_count= vertices[vertex].y-vertices[left_vertex].y;
					if (!build_x_table(left_table+aggregate_left_line_count, vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_left_line_count+= left_line_count;
					left_vertex= vertex;
//					ao__dprintf__("add %d left", left_line_count);
				}
				while (!left_line_count);
			}

			/* if we’re out of scan lines on the right side, get a new vertex and build a table
				of x-coordinates so we can walk toward the new vertex */
			if (right_line_count<=0)
			{
				do /* clockwise vertex search */
				{
					vertex= (right_vertex==polygon->vertex_count-1) ? 0 : (right_vertex+1);
					right_line_count= vertices[vertex].y-vertices[right_vertex].y;
					if (!build_x_table(right_table+aggregate_right_line_count, vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_right_line_count+= right_line_count;
					right_vertex= vertex;
//					ao__dprintf__("add %d right", right_line_count);
				}
				while (!right_line_count);
			}
              //AS: moving delta declaration up to where it's needed. Isn't C++ wonderful?
			/* advance by the minimum of left_line_count and right_line_count */
			short delta= MIN(left_line_count, right_line_count);
			assert_fail(delta, "");
//			ao__dprintf__("tc=%d lc=%d rc=%d delta=%d", total_line_count, left_line_count, right_line_count, delta);
			total_line_count-= delta;
			left_line_count-= delta;
			right_line_count-= delta;
			
			assert_fail(delta||!total_line_count, ""); /* if our delta is zero, we’d better be out of lines */
		}
		
		/* make sure every coordinate is accounted for in our tables */
		assert_fail(aggregate_right_line_count==aggregate_total_line_count, "");
		assert_fail(aggregate_left_line_count==aggregate_total_line_count, "");

		/* precalculate mode-specific data */
		switch (polygon->transfer_mode)
		{
			case _textured_transfer:
				TEXBITS_DISPATCH(polygon->texture, _pretexture_horizontal_polygon_lines, (polygon, vertices[highest_vertex].y,
                                                                                          left_table, right_table, aggregate_total_line_count));
				break;

			case _big_landscaped_transfer:
				_prelandscape_horizontal_polygon_lines(polygon,
                                                       vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
				break;
			
			default:
                throw_bug_report_f("horizontal_polygons dont support mode #%d", polygon->transfer_mode);
		}
		
		// render all lines
        switch (bit_depth)
        {
            case 8:
                switch (polygon->transfer_mode)
                {
                    case _textured_transfer:
                        TEXBITS_DISPATCH_2(polygon->texture, texture_horizontal_polygon_lines, pixel8, 0,
                                           (polygon->texture, &m_bitmap_definition, (_horizontal_polygon_line_data *)precalculation_table,
                                            vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count));
                        break;
                    case _big_landscaped_transfer:
                        landscape_horizontal_polygon_lines<pixel8>(polygon->texture, &m_bitmap_definition,
                                                                   (_horizontal_polygon_line_data*)precalculation_table,
                                                                   vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
                        break;
                    default:
                        throw_bug_report_f("Invalid transfer mode: %d", polygon->transfer_mode);
                }
                break;
                
            case 16:
                switch (polygon->transfer_mode)
                {
                    case _textured_transfer:
                        TEXBITS_DISPATCH_2(polygon->texture, texture_horizontal_polygon_lines, pixel16, 0,
                                           (polygon->texture, &m_bitmap_definition, (_horizontal_polygon_line_data*)precalculation_table,
                                            vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count));
                        break;
                    case _big_landscaped_transfer:
                        landscape_horizontal_polygon_lines<pixel16>(polygon->texture, &m_bitmap_definition,
                                                                    (_horizontal_polygon_line_data*)precalculation_table,
                                                                    vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
                        break;
                    default:
                        throw_bug_report_f("Invalid transfer mode: %d", polygon->transfer_mode);
                }
                break;
                                
            default:
                throw_bug_report_f("Unsupported bit depth: %d", bit_depth);
        }
	}
}


void ClassicRasterizer::texture_vertical_polygon(polygon_definition& textured_polygon)
{
	polygon_definition *polygon = &textured_polygon;	// Reference to pointer
	short vertex, highest_vertex, lowest_vertex;
	point2d *vertices= polygon->vertices;

	assert_fail(polygon->vertex_count>=MINIMUM_VERTICES_PER_SCREEN_POLYGON&&polygon->vertex_count<MAXIMUM_VERTICES_PER_SCREEN_POLYGON, "");

    if (polygon->transfer_mode == _big_landscaped_transfer) {
        texture_horizontal_polygon(textured_polygon);
        return;
    }
     
	/* locate the horizontally highest (closest to zero) and lowest (farthest from zero) vertices */
	highest_vertex= lowest_vertex= 0;
	for (vertex=1;vertex<polygon->vertex_count;++vertex)
	{
		if (vertices[vertex].x<vertices[highest_vertex].x) highest_vertex= vertex;
		if (vertices[vertex].x>vertices[lowest_vertex].x) lowest_vertex= vertex;
	}

	for (vertex=0;vertex<polygon->vertex_count;++vertex)
	{
		if (!(vertices[vertex].x>=0&&vertices[vertex].x<=m_bitmap_definition.width&&vertices[vertex].y>=0&&vertices[vertex].y<=m_bitmap_definition.height))
		{
//			ao__dprintf__("vertex #%d/#%d out of bounds:;dm %x %x;g;", vertex, polygon->vertex_count, polygon->vertices, polygon->vertex_count*sizeof(point2d));
			return;
		}
	}

	/* if this polygon is not a vertical line, draw it */
	if (highest_vertex!=lowest_vertex)
	{
		short left_line_count, right_line_count, total_line_count;
		short aggregate_left_line_count, aggregate_right_line_count, aggregate_total_line_count;
		short left_vertex, right_vertex;
		short *left_table= scratch_table0, *right_table= scratch_table1;

		left_line_count= right_line_count= 0; /* zero counts so the left and right lines get initialized */
		aggregate_left_line_count= aggregate_right_line_count= 0; /* we’ve precalculated nothing initially */
		left_vertex= right_vertex= highest_vertex; /* both sides start at the highest vertex */
		total_line_count= vertices[lowest_vertex].x-vertices[highest_vertex].x; /* calculate vertical line count */

		assert_fail(total_line_count<MAXIMUM_SCRATCH_TABLE_ENTRIES, ""); /* make sure we have enough scratch space */
		
		/* precalculate high and low y-coordinates for every x-coordinate */			
		aggregate_total_line_count= total_line_count;
		while (total_line_count>0)
		{			
			/* if we’re out of scan lines on the left side, get a new vertex and build a table
				of y-coordinates so we can walk toward the new vertex */
			if (left_line_count<=0)
			{
				do /* clockwise vertex search */
				{
					vertex= (left_vertex==polygon->vertex_count-1) ? 0 : (left_vertex+1);
					left_line_count= vertices[vertex].x-vertices[left_vertex].x;
//					ao__dprintf__("left line (%d,%d) to (%d,%d) for %d points", vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y, left_line_count);
					if (!build_y_table(left_table+aggregate_left_line_count, vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_left_line_count+= left_line_count;
					left_vertex= vertex;
				}
				while (!left_line_count);
			}

			/* if we’re out of scan lines on the right side, get a new vertex and build a table
				of y-coordinates so we can walk toward the new vertex */
			if (right_line_count<=0)
			{
				do /* counter-clockwise vertex search */
				{
					vertex= right_vertex ? (right_vertex-1) : (polygon->vertex_count-1);
					right_line_count= vertices[vertex].x-vertices[right_vertex].x;
//					ao__dprintf__("right line (%d,%d) to (%d,%d) for %d points", vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y, right_line_count);
					if (!build_y_table(right_table+aggregate_right_line_count, vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_right_line_count+= right_line_count;
					right_vertex= vertex;
				}
				while (!right_line_count);
			}
			
			/* advance by the minimum of left_line_count and right_line_count */
			short delta= MIN(left_line_count, right_line_count);
			assert_fail(delta, "");
			total_line_count-= delta;
			left_line_count-= delta;
			right_line_count-= delta;
			
			assert_fail(delta||!total_line_count, ""); /* if our delta is zero, we’d better be out of lines */
		}
		
		/* make sure every coordinate is accounted for in our tables */
		assert_fail(aggregate_right_line_count==aggregate_total_line_count, "");
		assert_fail(aggregate_left_line_count==aggregate_total_line_count, "");
        
        // precalculate mode-specific data
        if (polygon->transfer_mode != _textured_transfer && polygon->transfer_mode != _static_transfer)
        {
            throw_bug_report_f("vertical_polygons dont support mode #%d", polygon->transfer_mode);
        }
        
        TEXBITS_DISPATCH(polygon->texture, _pretexture_vertical_polygon_lines, (polygon, vertices[highest_vertex].x, left_table, right_table, aggregate_total_line_count));
          
		// render all lines
        switch (bit_depth)
        {
            case 8:
                switch (polygon->transfer_mode)
                {
                    case _textured_transfer:
                        if (polygon->texture->flags&_TRANSPARENT_BIT)
                            texture_vertical_polygon_lines<pixel8, 0, true>(&m_bitmap_definition, (_vertical_polygon_data*)precalculation_table, left_table, right_table);
                        else
                            texture_vertical_polygon_lines<pixel8, 0, false>(&m_bitmap_definition, (_vertical_polygon_data*)precalculation_table, left_table, right_table);
                        break;
                    case _static_transfer:
                        if (polygon->texture->flags&_TRANSPARENT_BIT)
                            randomize_vertical_polygon_lines<pixel8, true>(&m_bitmap_definition,
                                                                           (_vertical_polygon_data*)precalculation_table,
                                                                           left_table, right_table, polygon->transfer_data);
                        else
                            randomize_vertical_polygon_lines<pixel8, false>(&m_bitmap_definition,
                                                                            (_vertical_polygon_data*)precalculation_table,
                                                                            left_table, right_table, polygon->transfer_data);
                        break;
                        
                    default:
                        throw_bug_report_f("Invalid transfer mode: %d", polygon->transfer_mode);
                }
                break;
                
            case 16:
                switch (polygon->transfer_mode)
                {
                    case _textured_transfer:
                    {
                        if (polygon->texture->flags & _TRANSPARENT_BIT) {
                            texture_vertical_polygon_lines<pixel16, 0, true>(&m_bitmap_definition, (_vertical_polygon_data*)precalculation_table, left_table, right_table);
                        } else {
                            texture_vertical_polygon_lines<pixel16, 0, false>(&m_bitmap_definition, (_vertical_polygon_data*)precalculation_table, left_table, right_table);
                        }
                    }
                        break;
                    case _static_transfer:
                        if (polygon->texture->flags & _TRANSPARENT_BIT) {
                            randomize_vertical_polygon_lines<pixel16, true>(&m_bitmap_definition,
                                                                            (_vertical_polygon_data*)precalculation_table,
                                                                            left_table, right_table, polygon->transfer_data);
                        } else {
                            randomize_vertical_polygon_lines<pixel16, false>(&m_bitmap_definition,
                                                                             (_vertical_polygon_data*)precalculation_table,
                                                                             left_table, right_table, polygon->transfer_data);
                        }
                        break;
                    default:
                        throw_bug_report_f("Invalid transfer mode: %d", polygon->transfer_mode);
                }
                break;
                                
            default:
                throw_bug_report_f("Unsupported bit depth: %d", bit_depth);
        }
	}
}


// draw billboard sprite
void ClassicRasterizer::texture_rectangle(billboard_t& rectangle)
{
    if (rectangle.x0 < rectangle.x1 && rectangle.y0 < rectangle.y1)
    {
        // subsume screen boundaries into clipping parameters
        if (rectangle.clip_left < 0) { rectangle.clip_left = 0; }
        if (rectangle.clip_right > m_bitmap_definition.width) { rectangle.clip_right = m_bitmap_definition.width; }
        if (rectangle.clip_top < 0) { rectangle.clip_top = 0; }
        if (rectangle.clip_bottom > m_bitmap_definition.height) { rectangle.clip_bottom = m_bitmap_definition.height; }
        
        // subsume left and right sides of the rectangle into clipping parameters
        if (rectangle.clip_left < rectangle.x0) { rectangle.clip_left = rectangle.x0; }
        if (rectangle.clip_right > rectangle.x1) { rectangle.clip_right = rectangle.x1; }
        if (rectangle.clip_top < rectangle.y0) { rectangle.clip_top = rectangle.y0; }
        if (rectangle.clip_bottom > rectangle.y1) { rectangle.clip_bottom = rectangle.y1; }
	
		// only continue if we have a non-empty rectangle, at least some of which is on the screen
		if (rectangle.clip_left < rectangle.clip_right && rectangle.clip_top < rectangle.clip_bottom
            && rectangle.clip_right > 0 && rectangle.clip_left < m_bitmap_definition.width
            && rectangle.clip_bottom > 0 && rectangle.clip_top < m_bitmap_definition.height)
		{
			short delta; // scratch
			short screen_width  = rectangle.x1 - rectangle.x0;
			short screen_height = rectangle.y1 - rectangle.y0;
			short screen_x = rectangle.x0;
			bitmap_definition_t* texture = rectangle.texture;
	
            short* y0_table = scratch_table0;
            short* y1_table = scratch_table1;
			_vertical_polygon_data* header = (_vertical_polygon_data*)precalculation_table;
			_vertical_polygon_line_data* data = (_vertical_polygon_line_data*)(header + 1);
			
			ao_fixed texture_dx = INTEGER_TO_FIXED(texture->width) / screen_width;
			ao_fixed texture_x  = texture_dx>>1;
	
			ao_fixed texture_dy = INTEGER_TO_FIXED(texture->height) / screen_height;
			ao_fixed texture_y0 = 0;
			ao_fixed texture_y1;
			
			if (texture_dx && texture_dy)
			{
				// handle horizontal mirroring
				if (rectangle.flip_horizontal)
				{
					texture_dx = -texture_dx;
					texture_x = INTEGER_TO_FIXED(texture->width) + (texture_dx >> 1);
				}
				
				// left clipping
				if ((delta = rectangle.clip_left - rectangle.x0) > 0)
				{
					texture_x += delta * texture_dx;
					screen_width -= delta;
					screen_x = rectangle.clip_left;
				}
				// right clipping
				if ((delta = rectangle.x1 - rectangle.clip_right) > 0)
				{
					screen_width -= delta;
				}
				
				// top clipping
				if ((delta = rectangle.clip_top - rectangle.y0) > 0)
				{
					texture_y0+= delta*texture_dy;
					screen_height-= delta;
				}
				
				// bottom clipping
				if ((delta= rectangle.y1-rectangle.clip_bottom)>0)
				{
					screen_height-= delta;
				}
	
				texture_y1 = texture_y0 + screen_height * texture_dy;
				
				header->downshift = FIXED_FRACTIONAL_BITS;
				header->width = screen_width;
				header->x0 = screen_x;
				
				// calculate shading table, once
				void* shading_table = NULL;
				switch (rectangle.transfer_mode)
				{
					case _textured_transfer:
						if (!(rectangle.flags & _SHADELESS_BIT))
						{
							calculate_shading_table(shading_table, rectangle.shading_tables,
                                                    (short)MIN(rectangle.depth, SHRT_MAX), rectangle.ambient_shade);
							break;
						}
						// if shadeless, fall through to a single shading table, ignoring depth
					case _tinted_transfer:
					case _static_transfer:
						shading_table = rectangle.shading_tables;
						break;
					
					default:
                        throw_bug_report_f("rectangles dont support mode #%d", rectangle.transfer_mode);
				}
		
				for (; screen_width; --screen_width)
				{
					uint8_t* read = texture->row_addresses[FIXED_INTEGERAL_PART(texture_x)];
					// CB: first/last are stored in big-endian order
					uint16_t first = *read++ << 8;
					first         |= *read++;
					uint16_t  last = *read++ << 8;
					last          |= *read++;
					ao_fixed texture_y = texture_y0;
					short y0 = rectangle.clip_top, y1 = rectangle.clip_bottom;
					
					if (FIXED_INTEGERAL_PART(texture_y0) < first)
					{
						delta = (INTEGER_TO_FIXED(first) - texture_y0) / texture_dy + 1;
						assert_fail_f(delta >= 0, "[%x,%x] ∂=%x (#%d,#%d)", texture_y0, texture_y1, texture_dy, first, last);
						
						y0 = MIN(y1, y0 + delta);
						texture_y += delta * texture_dy;
					}
					
					if (FIXED_INTEGERAL_PART(texture_y1) > last)
					{
						delta = (texture_y1 - INTEGER_TO_FIXED(last)) / texture_dy + 1;
						assert_fail_f(delta >= 0, "[%x,%x] ∂=%x (#%d,#%d)", texture_y0, texture_y1, texture_dy, first, last);
						
						y1 = MAX(y0, y1 - delta);
					}
					
					data->texture_y = texture_y - INTEGER_TO_FIXED(first);
					data->texture_dy = texture_dy;
					data->shading_table = shading_table;
					data->texture = (uint8_t*)read;
					
					texture_x += texture_dx;
					data += 1;
					
					*y0_table++ = y0;
					*y1_table++ = y1;
					
					assert_fail(y0 <= y1, "");
					assert_fail(y0 >= 0 && y1 >= 0, "");
					assert_fail(y0 <= m_bitmap_definition.height, "");
					assert_fail(y1 <= m_bitmap_definition.height, "");
				}
		
                switch (bit_depth)
				{
					case 8:
						switch (rectangle.transfer_mode)
						{
							case _textured_transfer:
								texture_vertical_polygon_lines<pixel8, 0, true>(&m_bitmap_definition,
                                                                                (_vertical_polygon_data*)precalculation_table,
                                                                                scratch_table0, scratch_table1);
								break;
							
							case _static_transfer:
								randomize_vertical_polygon_lines<pixel8, true>(&m_bitmap_definition,
                                                                               (_vertical_polygon_data*)precalculation_table,
                                                                               scratch_table0, scratch_table1, rectangle.transfer_data);
								break;
							
							case _tinted_transfer:
								tint_vertical_polygon_lines<pixel8>(&m_bitmap_definition,
                                                                    (_vertical_polygon_data*)precalculation_table,
                                                                    &pixel_format_8, scratch_table0, scratch_table1, rectangle.transfer_data);
								break;
							
							default:
                                throw_bug_report_f("Invalid transfer mode: %d", rectangle.transfer_mode);
						}
						break;
		
					case 16:
						switch (rectangle.transfer_mode)
						{
							case _textured_transfer:
								texture_vertical_polygon_lines<pixel16, 0, true>(&m_bitmap_definition,
                                                                                 (_vertical_polygon_data*)precalculation_table,
                                                                                 scratch_table0, scratch_table1);
								break;
								
							case _static_transfer:
								randomize_vertical_polygon_lines<pixel16, true>(&m_bitmap_definition,
                                                                                (_vertical_polygon_data*)precalculation_table,
                                                                                scratch_table0, scratch_table1, rectangle.transfer_data);
								break;
							
							case _tinted_transfer:
								tint_vertical_polygon_lines<pixel16>(&m_bitmap_definition,
                                                                     (_vertical_polygon_data*)precalculation_table,
                                                                     &pixel_format_16, scratch_table0, scratch_table1, rectangle.transfer_data);
								break;
							
							default:
                                throw_bug_report_f("Invalid transfer mode: %d", rectangle.transfer_mode);
						}
						break;
				
					default:
                        throw_bug_report_f("Unsupported bit depth: %d", bit_depth);
				}
			}
		}
	}
}


/* ---------- private code */

/* starting at x0 and for line_count vertical lines between *y0 and *y1, precalculate all the
	information _texture_vertical_polygon_lines will need to work */
template<int TEXBITS>
void ClassicRasterizer::_pretexture_vertical_polygon_lines(polygon_definition *polygon, short x0, short *y0_table, short *y1_table, short line_count)
{
    _vertical_polygon_data* data = (_vertical_polygon_data*)precalculation_table;
	short screen_x= x0-view->half_screen_width;
	int32 dz0= view->world_to_screen_y*polygon->origin.z;
	int32 unadjusted_ty_denominator= view->world_to_screen_y*polygon->vector.k;
	int32 tx_numerator, tx_denominator, tx_numerator_delta, tx_denominator_delta;
	struct _vertical_polygon_line_data *line= (struct _vertical_polygon_line_data *) (data+1);

	assert_fail(sizeof(struct _vertical_polygon_line_data)<=MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE, "");

	data->downshift= VERTICAL_TEXTURE_DOWNSHIFT;
	data->x0= x0;
	data->width= line_count;

	/* calculate and rescale tx_numerator, tx_denominator, etc. */
	tx_numerator= view->world_to_screen_x*polygon->origin.y - screen_x*polygon->origin.x;
	tx_denominator= screen_x*polygon->vector.i - view->world_to_screen_x*polygon->vector.j;
	tx_numerator_delta= -polygon->origin.x;
	tx_denominator_delta= polygon->vector.i;

	while (--line_count>=0)
	{
		ao_fixed tx;
		// LP change: made this quantity more long-distance friendly;
		// have to avoid doing INTEGER_TO_FIXED on this one, however
		int32 world_x;
		short x0, y0= *y0_table++;
		short screen_y0= view->half_screen_height-y0+view->dtanpitch;
		int32 ty_numerator, ty_denominator;
		ao_fixed ty, ty_delta;

		/* would our precision be greater here if we shifted the numerator up to $7FFFFFFF and
			then downshifted only the numerator?  too bad we can’t use BFFFO in 68k */
		{
			int32 adjusted_tx_denominator = tx_denominator;
			int32 adjusted_tx_numerator = tx_numerator;
			
			while (adjusted_tx_numerator > ((1   << (31 - VERTICAL_TEXTURE_WIDTH_BITS)) - 1)
                || adjusted_tx_numerator < ((-1) << (31 - VERTICAL_TEXTURE_WIDTH_BITS))) // TODO: FIX: Left shift of negative value
			{
                adjusted_tx_numerator >>= 1;
                adjusted_tx_denominator >>= 1;
			}
            if (!adjusted_tx_denominator) { adjusted_tx_denominator = 1; } // -1 will still be -1
			x0 = ((adjusted_tx_numerator << VERTICAL_TEXTURE_WIDTH_BITS) / adjusted_tx_denominator) & (VERTICAL_TEXTURE_WIDTH - 1); // TODO: FIX: Left shift of negative value

			while (adjusted_tx_numerator > INT16_MAX || adjusted_tx_numerator < INT16_MIN)
			{
                adjusted_tx_numerator >>= 1;
                adjusted_tx_denominator >>= 1;
			}
            if (!adjusted_tx_denominator) { adjusted_tx_denominator = 1; } // -1 will still be -1
			tx = INTEGER_TO_FIXED(adjusted_tx_numerator) / adjusted_tx_denominator; // TODO: FIX: Left shift of negative value
		}
		
		world_x = polygon->origin.x + (int32(1LL*tx*polygon->vector.i) >> FIXED_FRACTIONAL_BITS);
		if (world_x<0) world_x= -world_x; /* it is mostly unclear what we’re supposed to do with negative x values */

		/* calculate and rescale ty_numerator, ty_denominator and calculate ty */
		ty_numerator= world_x*screen_y0 - dz0;
		ty_denominator= unadjusted_ty_denominator;
		while (ty_numerator>INT16_MAX||ty_numerator<INT16_MIN)
		{
            ty_numerator>>= 1;
            ty_denominator>>= 1;
		}
		if (!ty_denominator) ty_denominator= 1; /* -1 will still be -1 */
		ty= INTEGER_TO_FIXED(ty_numerator)/ty_denominator; // TODO: FIX: Left shift of negative value
		
		// LP change:
		// Use the same reduction hack used earlier,
		// because otherwise, INTEGER_TO_FIXED would cause world_x to wrap around.
		int32 adjusted_world_x = world_x;
		int32 adjusted_ty_denominator = unadjusted_ty_denominator>>8;
		
		// LP: remember that world_x is always >= 0
		while(adjusted_world_x > INT16_MAX)
		{
			adjusted_world_x >>= 1; adjusted_ty_denominator >>= 1;
		}
		if (!adjusted_ty_denominator) adjusted_ty_denominator= 1; /* -1 will still be -1 */
		ty_delta= - INTEGER_TO_FIXED(adjusted_world_x)/adjusted_ty_denominator;
		
		assert_fail_f(ty_delta >= 0, "ty_delta = W2F(%d) / %d = %d", world_x, unadjusted_ty_denominator, ty_delta);

		/* calculate the shading table for this column */
		if (polygon->flags&_SHADELESS_BIT)
		{
			line->shading_table= polygon->shading_tables;
		}
		else
		{
			calculate_shading_table(line->shading_table, polygon->shading_tables,
                                    (short)MIN(world_x, SHRT_MAX), polygon->ambient_shade);
		}

//		if (ty_delta)
		{
			/* calculate texture_y and texture_dy (floor-mapper style) */
//			data->n= VERTICAL_TEXTURE_DOWNSHIFT;
			line->texture_y= ty<<VERTICAL_TEXTURE_FREE_BITS; // TODO: FIX: Left shift of negative value
			line->texture_dy= ty_delta<<(VERTICAL_TEXTURE_FREE_BITS-8);
			line->texture= polygon->texture->row_addresses[x0];
			
			line+= 1;
		}
		
		tx_numerator+= tx_numerator_delta;
		tx_denominator+= tx_denominator_delta;
		
		screen_x+= 1;
	}
}


template<int TEXBITS>
void ClassicRasterizer::_pretexture_horizontal_polygon_lines(polygon_definition *polygon,
                                                             short y0, short *x0_table, short *x1_table, short line_count)
{
    _horizontal_polygon_line_data* data = (_horizontal_polygon_line_data*)precalculation_table;
    
	int32 hcosine, dhcosine;
	int32 hsine, dhsine;
	int32 hworld_to_screen;
	bool higher_precision= polygon->origin.z>-WORLD_ONE && polygon->origin.z<WORLD_ONE;
	
	/* precalculate a bunch of multiplies */
	hcosine= cosine_table[view->yaw];
	hsine= sine_table[view->yaw];
	if (higher_precision)
	{
		hcosine*= polygon->origin.z;
		hsine*= polygon->origin.z;
	}
	hworld_to_screen= polygon->origin.z*view->world_to_screen_y;
	dhcosine= view->world_to_screen_y*hcosine;
	dhsine= view->world_to_screen_y*hsine;

	while ((line_count-=1)>=0)
	{
		// LP change: made this more long-distance-friendly
		int32 depth;
		// world_distance depth;
		short screen_x, screen_y;
		short x0= *x0_table++;
		
		/* calculate screen_x,screen_y */
		screen_x= x0-view->half_screen_width;
		screen_y= view->half_screen_height-y0+view->dtanpitch;
		if (!screen_y) screen_y= 1; /* this will avoid division by zero and won't change rendering */
		
		/* calculate source_x, source_y, source_dx, source_dy */
		
			int32 source_x, source_y, source_dx, source_dy;
			
			/* calculate texture origins and deltas (source_x,source_dx,source_y,source_dy) */
			if (higher_precision)
			{
				source_x= (dhcosine - screen_x*hsine)/screen_y + (polygon->origin.x<<TRIG_SHIFT);
				source_dx= - hsine/screen_y;
				source_y= (screen_x*hcosine + dhsine)/screen_y + (polygon->origin.y<<TRIG_SHIFT);
				source_dy= hcosine/screen_y;
			}
			else
			{
				source_x= ((dhcosine - screen_x*hsine)/screen_y)*polygon->origin.z + (polygon->origin.x<<TRIG_SHIFT);
				source_dx= - (hsine*polygon->origin.z)/screen_y;
				source_y= ((screen_x*hcosine + dhsine)/screen_y)*polygon->origin.z + (polygon->origin.y<<TRIG_SHIFT);
				source_dy= (hcosine*polygon->origin.z)/screen_y;
			}

			auto bits = HORIZONTAL_FREE_BITS - (polygon->flags & _SCALE_BITS);
		
			/* voodoo so x,y texture wrapping is handled automatically by downshifting
				(subtract one from HORIZONTAL_FREE_BITS to double scale) */
        // EES: left-shifting ints into -ve values here and elsewhere is designed M2 behavior, not a bug, so ignore Xcode warnings about it
        data->source_x  = source_x  << bits;
        data->source_dx = source_dx << bits; // TODO: FIX: Left shift of negative value
        data->source_y  = source_y  << bits;
        data->source_dy = source_dy << bits; // TODO: FIX: Left shift of negative value
		

		/* get shading table (with absolute value of depth) */
		if ((depth= hworld_to_screen/screen_y)<0) depth= -depth;
		if (polygon->flags&_SHADELESS_BIT)
		{
			data->shading_table= polygon->shading_tables;
		}
		else
		{
			calculate_shading_table(data->shading_table, polygon->shading_tables, (short)MIN(depth, SHRT_MAX), polygon->ambient_shade);
		}
		
		data++;
		y0++;
	}
}


// height must be determined emperically (texture is vertically centered at 0°)
// #define LANDSCAPE_REPEAT_BITS 1
void ClassicRasterizer::_prelandscape_horizontal_polygon_lines(polygon_definition* polygon,
                                                               short y0, short* x0_table, short* x1_table, short line_count)
{
    _horizontal_polygon_line_data* data = (_horizontal_polygon_line_data*)precalculation_table;
	short landscape_width_bits= NextLowerExponent(polygon->texture->height);
	short texture_height= polygon->texture->width;
	ao_fixed ambient_shade= FIXED_ONE; // MPW C died if we passed the constant directly to the macro

	// Get the landscape-texturing options
	LandscapeOptions *LandOpts = View_GetLandscapeOptions(polygon->ShapeDesc);
	
	// LP change: separate horizontal and vertical pixel deltas:
	// LP change: using a "landscape yaw" that's at the left edge of the screen.
	ao_fixed first_horizontal_pixel= (view->landscape_yaw + LandOpts->Azimuth)<<(landscape_width_bits+(LandOpts->HorizExp)+FIXED_FRACTIONAL_BITS-ANGULAR_BITS);
	ao_fixed horizontal_pixel_delta= (view->half_cone<<(1+landscape_width_bits+(LandOpts->HorizExp)+FIXED_FRACTIONAL_BITS-ANGULAR_BITS))/view->standard_screen_width;
	ao_fixed vertical_pixel_delta= (view->half_cone<<(1+landscape_width_bits+(LandOpts->VertExp)+FIXED_FRACTIONAL_BITS-ANGULAR_BITS))/view->standard_screen_width;
	short landscape_free_bits= 32-FIXED_FRACTIONAL_BITS-landscape_width_bits;

	/* calculate the shading table */	
	void *shading_table = NULL;
	if (polygon->flags&_SHADELESS_BIT)
	{
		shading_table= polygon->shading_tables;
	}
	else
	{
		calculate_shading_table(shading_table, polygon->shading_tables, 0, ambient_shade);
	}
	
	// Find the height to repeat over; use value used for OpenGL texture setup
	short texture_width= polygon->texture->height;
	short repeat_texture_height = texture_width >> LandOpts->OGL_AspRatExp;
	
	short height_reduced = texture_height - 1;
	short height_shift = texture_height >> 1;
	short height_repeat_mask = repeat_texture_height - 1;
	short height_repeat_shift = repeat_texture_height >> 1;
	
	y0-= view->half_screen_height + view->dtanpitch; /* back to virtual screen coordinates */
	while ((line_count-= 1)>=0)
	{
		short x0= *x0_table++;
		
		data->shading_table= shading_table;
		// LP change: using vertical pixel delta
		// Also using vertical repeat if selected;
		// fold the height into the range (-repeat_height/2, repeat_height)
		short y_txtr_offset= FIXED_INTEGERAL_PART(y0*vertical_pixel_delta);
		if (LandOpts->VertRepeat)
			y_txtr_offset = ((y_txtr_offset + height_repeat_shift) & height_repeat_mask) -
				height_repeat_shift;
		data->source_y= texture_height - PIN(y_txtr_offset + height_shift, 0, height_reduced) - 1;
		// LP change: using horizontal pixel delta
		data->source_x= (first_horizontal_pixel + x0*horizontal_pixel_delta)<<landscape_free_bits;
		data->source_dx= horizontal_pixel_delta<<landscape_free_bits;
		
		data+= 1;
		y0+= 1;
	}
}


//-----------------------------------------------------------------------------


/* y0<y1; this is for vertical polygons */
static short *build_x_table(short *table, short x0, short y0, short x1, short y1)
{
	short dx, dy, adx, ady; /* 'a' prefix means absolute value */
	short x, y; /* x,y screen positions */
	short d, delta_d, d_max; /* descriminator, delta_descriminator, descriminator_maximum */
	short *record;

	/* calculate SGN(dx),SGN(dy) and the absolute values of dx,dy */	
    dx= x1-x0; adx= std::abs(dx); dx= SGN(dx);
    dy= y1-y0; ady= std::abs(dy); dy= SGN(dy);

	assert_fail(ady<MAXIMUM_SCRATCH_TABLE_ENTRIES, ""); /* can't overflow table */
	if (dy>0)
	{
		/* setup initial (x,y) location and initialize a pointer to our table */
        x= x0; y= y0;
		record= table;
	
		if (adx>=ady)
		{
			/* x-dominant line (we need to record x every time y changes) */
	
            d= adx-ady; delta_d= - 2*ady; d_max= 2*adx;
			while ((adx-=1)>=0)
			{
                if (d<0) { y+= 1; d+= d_max; *record++= x; ady-= 1; }
                x+= dx; d+= delta_d;
			}
			if (ady==1) *record++= x; else assert_fail(!ady, "");
		}
		else
		{
			/* y-dominant line (we need to record x every iteration) */
	
            d= ady-adx; delta_d= - 2*adx; d_max= 2*ady;
			while ((ady-=1)>=0)
			{
                if (d<0) { x+= dx; d+= d_max; }
				*record++= x;
                y+= 1;
                d+= delta_d;
			}
		}
	}
	else
	{
		/* can’t build a table for negative dy */
		if (dy<0) return NULL;
	}
	
	return table;
}


/* x0<x1; this is for horizontal polygons */
static short *build_y_table(short *table, short x0, short y0, short x1, short y1)
{
	short dx, dy, adx, ady; /* 'a' prefix means absolute value */
	short x, y; /* x,y screen positions */
	short d, delta_d, d_max; /* descriminator, delta_descriminator, descriminator_maximum */
	short *record;

	/* calculate SGN(dx),SGN(dy) and the absolute values of dx,dy */	
    dx= x1-x0; adx= std::abs(dx); dx= SGN(dx);
    dy= y1-y0; ady= std::abs(dy); dy= SGN(dy);

	assert_fail(adx < MAXIMUM_SCRATCH_TABLE_ENTRIES, ""); // can't overflow table
	if (dx >= 0) // vertical lines allowed
	{
		// setup initial (x,y) location and initialize a pointer to our table
		if (dy>=0)
		{
            x= x0;
            y= y0;
			record= table;
		}
		else
		{
            x= x1;
            y= y1;
			record= table+adx;
		}
	
		if (adx>=ady) // x-dominant line (we need to record y every iteration)
		{
            d= adx-ady;
            delta_d= - 2*ady;
            d_max= 2*adx;
			while ((adx-=1)>=0)
            {
                if (d<0)
                {
                    y+= 1;
                    d+= d_max;
                }
                if (dy>=0)
                {
                    *record++= y;
                }
                else
                {
                    *--record= y;
                }
                x+= dx;
                d+= delta_d;
			}
		}
		else
		{
			/* y-dominant line (we need to record y every time x changes) */
	
            d= ady-adx;
            delta_d= - 2*adx;
            d_max= 2*ady;
			while ((ady-=1)>=0)
			{
                if (d<0)
                {
                    x+= dx;
                    d+= d_max;
                    adx-= 1;
                    if (dy>=0)
                        *record++= y;
                    else
                        *--record= y;
                }
                y+= 1;
                d+= delta_d;
			}
			if (adx==1)
            {
                if (dy>=0)
                    *record++= y;
                else
                    *--record= y;
            }
            else
            {
                assert_fail(!adx, "");
            }
		}
	}
	else
	{
		/* can’t build a table for a negative dx */
		return NULL;
	}
	
	return table;
}
