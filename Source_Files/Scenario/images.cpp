/*
	images.cpp -- access pict, snd, text resources (resource fork or wad file)

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

#include "cseries.hpp"
#include "DataFile.hpp"

#include "interface.hpp"
#include "shell.h"
#include "images.h"
#include "Screen.hpp"
#include "wad.h"
#include "screen_drawing.h"

#include "render.h"
#include "OGL_Render.h"
#include "ImageBlitter.hpp"
#include "Plugins.h"
 

enum // add these to 8-bit pict resource ids to get 16- and 32-bit pict ids
{
	_images_file_delta16   =  1000,
	_images_file_delta32   =  2000,
	_scenario_file_delta16 = 10000,
	_scenario_file_delta32 = 20000,
};


// EES: TODO: this is nasty; extracting this to a LegacyFileConverter/ and having it extract resources to a directory/zipfile of plain old files would be progress


// Structure for open image file
class image_file_t
{
public:
	image_file_t() {}
	~image_file_t() {close();}

	bool open(const ao_path &path);
	void close();
	bool is_open();

	int find_best_pict_resource_id(int base_id, int delta16, int delta32);

	bool has_pict(int id);

	bool get_pict(int id, LoadedResource &rsrc);
	bool get_snd(int id, LoadedResource &rsrc);
	bool get_text(int id, LoadedResource &rsrc);

private:
	bool has_rsrc(uint32 rsrc_type, uint32 wad_type, int id);
	bool get_rsrc(uint32 rsrc_type, uint32 wad_type, int id, LoadedResource &rsrc);
    
    bool is_data, is_resource;
    
    ao_path path;
    
	ResourceFile rsrc_file;
	DataFile wad_file;
	wad_header_t wad_header;
};

// Global variables
static image_file_t ImagesFile;
static image_file_t MapFile;
static image_file_t ExternalResourcesFile;
static image_file_t ShapesImagesFile;
static image_file_t SoundsImagesFile;

// Prototypes
static void shutdown_images_handler(void);






extern bool shapes_file_is_m1();

/*
 *  Uncompress picture data, returns size of compressed image data that was read
 */

// Uncompress (and endian-correct) scan line compressed by PackBits RLE algorithm
template <class T>
static const uint8 *unpack_bits(const uint8 *src, int row_bytes, T *dst)
{
	// Read source count
	int src_count;
	if (row_bytes > 250) {
		src_count = (src[0] << 8) | src[1];
		src += 2;
	} else
		src_count = *src++;

	while (src_count > 0) {

		// Read flag/count byte
		int c = (int8)*src++;
		src_count--;
		if (c < 0) {

			// RLE compressed run
			int size = -c + 1;
			T data;
			if (sizeof(T) == 1) {
				data = *src++;
				src_count--;
			} else {
				data = (src[0] << 8) | src[1];
				src += 2;
				src_count -= 2;
			}
			for (int i=0; i<size; i++)
				*dst++ = data;

		} else {

			// Uncompressed run
			int size = c + 1;
			for (int i=0; i<size; i++) {
				T data;
				if (sizeof(T) == 1) {
					data = *src++;
					src_count--;
				} else {
					data = (src[0] << 8) | src[1];
					src += 2;
					src_count -= 2;
				}
				*dst++ = data;
			}
		}
	}
	return src;
}

// 8-bit picture, one scan line at a time
static int uncompress_rle8(const uint8 *src, int row_bytes, uint8 *dst, int dst_pitch, int height)
{
	const uint8 *start = src;
	for (int y=0; y<height; y++) {
		src = unpack_bits(src, row_bytes, dst);
		dst += dst_pitch;
	}
	return static_cast<int>(src - start);
}

// 16-bit picture, one scan line at a time, 16-bit chunks
static int uncompress_rle16(const uint8 *src, int row_bytes, uint8 *dst, int dst_pitch, int height)
{
	const uint8 *start = src;
	for (int y=0; y<height; y++) {
		src = unpack_bits(src, row_bytes, (uint16 *)dst);
		dst += dst_pitch;
	}
	return static_cast<int>(src - start);
}

static void copy_component_into_surface(const uint8 *src, uint8 *dst, int count, int component)
{
#ifdef ALEPHONE_LITTLE_ENDIAN
		dst += 2 - component;
#else
		dst += component + 1;
#endif
	while (count--) {
		*dst = *src++;
		dst += 4;
	}
}

// 32-bit picture, one scan line, one component at a time
static int uncompress_rle32(const uint8 *src, int row_bytes, uint8 *dst, int dst_pitch, int height)
{
	uint8 *tmp = (uint8 *)malloc(row_bytes);
	if (tmp == NULL)
		return -1;
	memset(tmp, 0, row_bytes);

	const uint8 *start = src;

	int width = row_bytes / 4; 
	for (int y=0; y<height; y++) {
		src = unpack_bits(src, row_bytes, tmp);

		// "tmp" now contains "width" bytes of red, followed by "width"
		// bytes of green and "width" bytes of blue, so we have to copy them
		// into the surface in the right order
		copy_component_into_surface(tmp, dst, width, 0);
		copy_component_into_surface(tmp + width, dst, width, 1);
		copy_component_into_surface(tmp + width * 2, dst, width, 2);

		dst += dst_pitch;
	}

	free(tmp);

	return static_cast<int>(src - start);
}

static int uncompress_picture(const uint8 *src, int row_bytes, uint8 *dst, int dst_pitch, int depth, int height, int pack_type)
{
	// Depths <8 have to be color expanded to depth 8 after uncompressing,
	// so we uncompress into a temporary buffer
	uint8 *orig_dst = dst;
	int orig_dst_pitch = dst_pitch;
	if (depth < 8) {
		dst = (uint8 *)malloc(row_bytes * height);
		dst_pitch = row_bytes;
		if (dst == NULL)
			return -1;
	}

	int data_size = 0;

	if (row_bytes < 8) {

		// Uncompressed data
		const uint8 *p = src;
		uint8 *q = dst;
		for (int y=0; y<height; y++) {
			memcpy(q, p, MIN(row_bytes, dst_pitch));
			p += row_bytes;
			q += dst_pitch;
		}
		data_size = row_bytes * height;

	} else {

		// Compressed data
		if (depth <= 8) {

			// Indexed color
			if (pack_type == 1)
				goto no_packing;
			data_size = uncompress_rle8(src, row_bytes, dst, dst_pitch, height);

		} else {

			// Direct color
			if (pack_type == 0) {
				if (depth == 16)
					pack_type = 3;
				else if (depth == 32)
					pack_type = 4;

			}
			switch (pack_type) {
				case 1: {	// No packing
no_packing:			const uint8 *p = src;
					uint8 *q = dst;
					for (int y=0; y<height; y++) {
						memcpy(q, p, MIN(row_bytes, dst_pitch));
						p += row_bytes;
						q += dst_pitch;
					}
					data_size = row_bytes * height;
					if (depth == 16)
                        swap_array_BE16((uint16_t*)dst, dst_pitch * height / 2);
					else if (depth == 32)
                        swap_array_BE32((uint32_t*)dst, dst_pitch * height / 4);
					break;
				}
				case 3:		// Run-length encoding by 16-bit chunks
					data_size = uncompress_rle16(src, row_bytes, dst, dst_pitch, height);
					break;
				case 4:		// Run-length encoding one component at a time
					data_size = uncompress_rle32(src, row_bytes, dst, dst_pitch, height);
					break;
				default:
					fprintf(stderr, "Unimplemented packing type %d (depth %d) in PICT resource\n", pack_type, depth);
					data_size = -1;
					break;
			}
		}
	}

	// Color expansion 1/2/4->8 bits
	if (depth < 8) {
		const uint8 *p = dst;
		uint8 *q = orig_dst;

		// Source and destination may have different alignment restrictions,
		// don't run off the right of either
		int x_max = row_bytes;
		while (x_max * 8 / depth > orig_dst_pitch)
			x_max--;

		switch (depth) {
			case 1:
				for (int y=0; y<height; y++) {
					for (int x=0; x<x_max; x++) {
						uint8 b = p[x];
						q[x*8+0] = (b & 0x80) ? 0x01 : 0x00;
						q[x*8+1] = (b & 0x40) ? 0x01 : 0x00;
						q[x*8+2] = (b & 0x20) ? 0x01 : 0x00;
						q[x*8+3] = (b & 0x10) ? 0x01 : 0x00;
						q[x*8+4] = (b & 0x08) ? 0x01 : 0x00;
						q[x*8+5] = (b & 0x04) ? 0x01 : 0x00;
						q[x*8+6] = (b & 0x02) ? 0x01 : 0x00;
						q[x*8+7] = (b & 0x01) ? 0x01 : 0x00;
					}
					p += row_bytes;
					q += orig_dst_pitch;
				}
				break;
			case 2:
				for (int y=0; y<height; y++) {
					for (int x=0; x<x_max; x++) {
						uint8 b = p[x];
						q[x*4+0] = (b >> 6) & 0x03;
						q[x*4+1] = (b >> 4) & 0x03;
						q[x*4+2] = (b >> 2) & 0x03;
						q[x*4+3] = b & 0x03;
					}
					p += row_bytes;
					q += orig_dst_pitch;
				}
				break;
			case 4:
				for (int y=0; y<height; y++) {
					for (int x=0; x<x_max; x++) {
						uint8 b = p[x];
						q[x*2+0] = (b >> 4) & 0x0f;
						q[x*2+1] = b & 0x0f;
					}
					p += row_bytes;
					q += orig_dst_pitch;
				}
				break;
		}
		free(dst);
	}

	return data_size;
}

int get_pict_header_width(LoadedResource &rsrc)
{
	SDL_RWops *p = SDL_RWFromMem(rsrc.GetPointer(), (int) rsrc.get_length());
	if (p)
	{
		SDL_RWseek(p, 8, SEEK_CUR);
		int width = SDL_ReadBE16(p);
		SDL_RWclose(p);
		return width;
	}
	return -1;
}




// Convert MacOS PICT resource to SDL surface
static SDL_Surface* picture_to_surface(LoadedResource &rsrc)
{
	SDL_Surface* s = nullptr;

	if (!rsrc.IsLoaded()) return s;

	// Open stream to picture resource
	SDL_RWops *p = SDL_RWFromMem(rsrc.GetPointer(), (int)rsrc.get_length());
	if (p == NULL) return s;
	SDL_RWseek(p, 6, SEEK_CUR);		// picSize/top/left
	int pic_height = SDL_ReadBE16(p);
	int pic_width = SDL_ReadBE16(p);
	//printf("pic_width %d, pic_height %d\n", pic_width, pic_height);

	// Read and parse picture opcodes
	bool done = false;
	while (!done) {
		uint16 opcode = SDL_ReadBE16(p);
		//printf("%04x\n", opcode);
		switch (opcode) {

			case 0x0000:	// NOP
			case 0x0011:	// VersionOp
			case 0x001c:	// HiliteMode
			case 0x001e:	// DefHilite
			case 0x0038:	// FrameSameRect
			case 0x0039:	// PaintSameRect
			case 0x003a:	// EraseSameRect
			case 0x003b:	// InvertSameRect
			case 0x003c:	// FillSameRect
			case 0x02ff:	// Version
				break;

			case 0x00ff:	// OpEndPic
				done = true;
				break;

			case 0x0001: {	// Clipping region
				uint16 size = SDL_ReadBE16(p);
				if (size & 1)
					size++;
				SDL_RWseek(p, size - 2, SEEK_CUR);
				break;
			}

			case 0x0003:	// TxFont
			case 0x0004:	// TxFace
			case 0x0005:	// TxMode
			case 0x0008:	// PnMode
			case 0x000d:	// TxSize
			case 0x0015:	// PnLocHFrac
			case 0x0016:	// ChExtra
			case 0x0023:	// ShortLineFrom
			case 0x00a0:	// ShortComment
				SDL_RWseek(p, 2, SEEK_CUR);
				break;

			case 0x0006:	// SpExtra
			case 0x0007:	// PnSize
			case 0x000b:	// OvSize
			case 0x000c:	// Origin
			case 0x000e:	// FgColor
			case 0x000f:	// BgColor
			case 0x0021:	// LineFrom
				SDL_RWseek(p, 4, SEEK_CUR);
				break;

			case 0x001a:	// RGBFgCol
			case 0x001b:	// RGBBkCol
			case 0x001d:	// HiliteColor
			case 0x001f:	// OpColor
			case 0x0022:	// ShortLine
				SDL_RWseek(p, 6, SEEK_CUR);
				break;

			case 0x0002:	// BkPat
			case 0x0009:	// PnPat
			case 0x000a:	// FillPat
			case 0x0010:	// TxRatio
			case 0x0020:	// Line
			case 0x0030:	// FrameRect
			case 0x0031:	// PaintRect
			case 0x0032:	// EraseRect
			case 0x0033:	// InvertRect
			case 0x0034:	// FillRect
				SDL_RWseek(p, 8, SEEK_CUR);
				break;

			case 0x0c00:	// HeaderOp
				SDL_RWseek(p, 24, SEEK_CUR);
				break;

			case 0x00a1: {	// LongComment
				SDL_RWseek(p, 2, SEEK_CUR);
				int size = SDL_ReadBE16(p);
				if (size & 1)
					size++;
				SDL_RWseek(p, size, SEEK_CUR);
				break;
			}

			case 0x0098:	// Packed CopyBits
			case 0x0099:	// Packed CopyBits with clipping region
			case 0x009a:	// Direct CopyBits
			case 0x009b: {	// Direct CopyBits with clipping region
				// 1. PixMap
				if (opcode == 0x009a || opcode == 0x009b)
					SDL_RWseek(p, 4, SEEK_CUR);		// pmBaseAddr
				uint16 row_bytes = SDL_ReadBE16(p);	// the upper 2 bits are flags
				//printf(" row_bytes %04x\n", row_bytes);
				bool is_pixmap = ((row_bytes & 0x8000) != 0);
				row_bytes &= 0x3fff;
				uint16 top = SDL_ReadBE16(p);
				uint16 left = SDL_ReadBE16(p);
				uint16 height = SDL_ReadBE16(p) - top;
				uint16 width = SDL_ReadBE16(p) - left;
				uint16 pack_type, pixel_size;
				if (is_pixmap) {
					SDL_RWseek(p, 2, SEEK_CUR);			// pmVersion
					pack_type = SDL_ReadBE16(p);
					SDL_RWseek(p, 14, SEEK_CUR);		// packSize/hRes/vRes/pixelType
					pixel_size = SDL_ReadBE16(p);
					SDL_RWseek(p, 16, SEEK_CUR);		// cmpCount/cmpSize/planeBytes/pmTable/pmReserved
				} else {
					pack_type = 0;
					pixel_size = 1;
				}
				//printf(" width %d, height %d, row_bytes %d, depth %d, pack_type %d\n", width, height, row_bytes, pixel_size, pack_type);

				// Allocate surface for picture
				uint32 Rmask = 0, Gmask = 0, Bmask = 0;
				int surface_depth = 8;
				switch (pixel_size) {
					case 1:
					case 2:
					case 4:
					case 8:
						Rmask = Gmask = Bmask = 0;
						surface_depth = 8;	// SDL surfaces must be at least 8 bits depth, so we expand 1/2/4-bit pictures to 8-bit
						break;
					case 16:
						Rmask = 0x7c00;
						Gmask = 0x03e0;
						Bmask = 0x001f;
						surface_depth = 16;
						break;
					case 32:
						Rmask = 0x00ff0000;
						Gmask = 0x0000ff00;
						Bmask = 0x000000ff;
						surface_depth = 32;
						break;
					default:
						fprintf(stderr, "Unsupported PICT depth %d\n", pixel_size);
						done = true;
						break;
				}
				if (done)
					break;
				SDL_Surface *bm = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, surface_depth, Rmask, Gmask, Bmask, 0);
				if (bm == NULL) {
					done = true;
					break;
				}

				// 2. ColorTable
				if (is_pixmap && (opcode == 0x0098 || opcode == 0x0099)) {
					SDL_Color colors[256];
					SDL_RWseek(p, 4, SEEK_CUR);			// ctSeed
					uint16 flags = SDL_ReadBE16(p);
					int num_colors = SDL_ReadBE16(p) + 1;
					for (int i=0; i<num_colors; i++) {
						uint8 value = SDL_ReadBE16(p) & 0xff;
						if (flags & 0x8000)
							value = i;
						colors[value].r = SDL_ReadBE16(p) >> 8;
						colors[value].g = SDL_ReadBE16(p) >> 8;
						colors[value].b = SDL_ReadBE16(p) >> 8;
						colors[value].a = 0xff;
					}
					SDL_SetPaletteColors(bm->format->palette, colors, 0, 256);
				}

				// 3. source/destination screen_rectangle and transfer mode
				SDL_RWseek(p, 18, SEEK_CUR);

				// 4. clipping region
				if (opcode == 0x0099 || opcode == 0x009b) {
					uint16 rgn_size = SDL_ReadBE16(p);
					SDL_RWseek(p, rgn_size - 2, SEEK_CUR);
				}

				// 5. graphics data
				int data_size = uncompress_picture((uint8 *)rsrc.GetPointer() + SDL_RWtell(p), row_bytes, (uint8 *)bm->pixels, bm->pitch, pixel_size, height, pack_type);
				if (data_size < 0) {
					done = true;
					break;
				}
				if (data_size & 1)
					data_size++;
				SDL_RWseek(p, data_size, SEEK_CUR);

				// If there's already a surface, throw away the decoded image
				// (actually, we could have skipped this entire opcode, but the
				// only way to do this is to decode the image data).
				// So we only draw the first image we encounter.
				if (s) {
					SDL_FreeSurface(bm);
				}
				else {
					s = bm;
				}

				break;
			}

#ifdef HAVE_SDL_IMAGE
			case 0x8200: {	// Compressed QuickTime image (we only handle JPEG compression)
				// 1. Header
				uint32 opcode_size = SDL_ReadBE32(p);
				if (opcode_size & 1)
					opcode_size++;
				uint32 opcode_start = (uint32)SDL_RWtell(p);
				SDL_RWseek(p, 26, SEEK_CUR);	// version/matrix (hom. part)
				int offset_x = SDL_ReadBE16(p);
				SDL_RWseek(p, 2, SEEK_CUR);
				int offset_y = SDL_ReadBE16(p);
				SDL_RWseek(p, 6, SEEK_CUR);	// matrix (remaining part)
				uint32 matte_size = SDL_ReadBE32(p);
				SDL_RWseek(p, 22, SEEK_CUR);	// matteRec/mode/srcRect/accuracy
				uint32 mask_size = SDL_ReadBE32(p);

				// 2. Matte image description
				if (matte_size) {
					uint32 matte_id_size = SDL_ReadBE32(p);
					SDL_RWseek(p, matte_id_size - 4, SEEK_CUR);
				}

				// 3. Matte data
				SDL_RWseek(p, matte_size, SEEK_CUR);

				// 4. Mask region
				SDL_RWseek(p, mask_size, SEEK_CUR);

				// 5. Image description
				uint32 id_start = (uint32)SDL_RWtell(p);
				uint32 id_size = SDL_ReadBE32(p);
				uint32 codec_type = SDL_ReadBE32(p);
				if (codec_type != FOUR_CHARS_TO_INT('j','p','e','g')) {
					fprintf(stderr, "Unsupported codec type %c%c%c%c\n", codec_type >> 24, codec_type >> 16, codec_type >> 8, codec_type);
					done = true;
					break;
				}
				SDL_RWseek(p, 36, SEEK_CUR);	// resvd1/resvd2/dataRefIndex/version/revisionLevel/vendor/temporalQuality/spatialQuality/width/height/hRes/vRes
				uint32 data_size = SDL_ReadBE32(p);
				SDL_RWseek(p, id_start + id_size, SEEK_SET);

				// Allocate surface for complete (but possibly banded) picture
				if (!s) { s = create_sdl_surface_32(pic_width, pic_height); }

				// 6. Compressed image data
				SDL_RWops *img = SDL_RWFromMem((uint8 *)rsrc.GetPointer() + SDL_RWtell(p), data_size);
				if (img == NULL) {
					done = true;
					break;
				}
				SDL_Surface *bm = IMG_LoadTyped_RW(img, true, const_cast<char*>("JPG"));

				// Copy image (band) into surface
				if (bm) {
					SDL_Rect dst_rect = {offset_x, offset_y, bm->w, bm->h};
					SDL_BlitSurface(bm, NULL, s, &dst_rect);
					SDL_FreeSurface(bm);
				}

				SDL_RWseek(p, opcode_start + opcode_size, SEEK_SET);
				break;
			}
#endif

			default:
				if (opcode >= 0x0300 && opcode < 0x8000)
					SDL_RWseek(p, (opcode >> 8) * 2, SEEK_CUR);
				else if (opcode >= 0x8000 && opcode < 0x8100)
					break;
				else {
					fprintf(stderr, "Unimplemented opcode %04x in PICT resource\n", opcode);
					done = true;
				}
				break;
		}
	}

	// Close stream, return surface
	SDL_RWclose(p);
	return s;
}



/*
template <class T>
static void rescale(T *src_pixels, int src_pitch, T *dst_pixels, int dst_pitch, int width, int height, uint32 dx, uint32 dy)
{
	// Brute-force rescaling, no interpolation
	uint32 sy = 0;
	for (int y=0; y<height; y++) {
		T *p = src_pixels + src_pitch / sizeof(T) * (sy >> 16);
		uint32 sx = 0;
		for (int x=0; x<width; x++) {
			dst_pixels[x] = p[sx >> 16];
			sx += dx;
		}
		dst_pixels += dst_pitch / sizeof(T);
		sy += dy;
	}
}


SDL_Surface *rescale_surface(SDL_Surface *s, int width, int height)
{
    assert_fail(s != NULL, "");

	SDL_Surface *s2 = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    assert_fail(s2 != NULL, "");

	uint32 dx = (s->w << 16) / width;
	uint32 dy = (s->h << 16) / height;

	switch (s->format->BytesPerPixel) {
		case 1:
			rescale((pixel8 *)s->pixels, s->pitch, (pixel8 *)s2->pixels, s2->pitch, width, height, dx, dy);
			break;
		case 2:
			rescale((pixel16 *)s->pixels, s->pitch, (pixel16 *)s2->pixels, s2->pitch, width, height, dx, dy);
			break;
		case 4:
			rescale((pixel32 *)s->pixels, s->pitch, (pixel32 *)s2->pixels, s2->pitch, width, height, dx, dy);
			break;
	}

	if (s->format->palette)
		SDL_SetPaletteColors(s2->format->palette, s->format->palette->colors, 0, s->format->palette->ncolors);

	return s2;
}
*/


// Initialize image manager, open Images file
void initialize_images_manager()
{
    ao_path path = find_file_at_subpath(get_string(STRID(strFILENAMES, filenameIMAGES))); // _typecode_images
    
    log_note_f("loading Images: %s", path.c_str());
    
    if (!std::filesystem::is_regular_file(path))
    {
        log_error("Images file not found");
    }
    if (ImagesFile.open(path))
    {
        log_error("Images file could not be opened");
    }
    atexit(shutdown_images_handler);
}


/*
 *  Shutdown image manager
 */

static void shutdown_images_handler(void)
{
	SoundsImagesFile.close();
	ExternalResourcesFile.close();
	ShapesImagesFile.close();
	MapFile.close();
	ImagesFile.close();
}


/*
 *  Set map file to load images from
 */

void open_map_file_resources(const ao_path &file)
{
	MapFile.open(file);
}

void close_map_file_resources()
{
	MapFile.close();
}

void open_shapes_file_resources(const ao_path &file)
{
	ShapesImagesFile.open(file);
}

void open_m2_external_resources_file(const ao_path &file)
{
    // fail here, instead of above, if Images is missing
    if (!std::filesystem::is_regular_file(file) || !ExternalResourcesFile.open(file))
    {
        ao_path default_path = find_file_at_subpath(get_string(STRID(strFILENAMES, filenameEXTERNAL_RESOURCES)));
        if ((!std::filesystem::is_regular_file(default_path) || !ExternalResourcesFile.open(default_path))
            && !ImagesFile.is_open()) { exit(badExtraFileLocations); }
    }
}

void open_sounds_file_resources(const ao_path &file)
{
	SoundsImagesFile.open(file);
}


/*
 *  Open/close image file
 */

bool image_file_t::open(const ao_path &path)
{
	close();
    this->path.clear();
    wad_header.version = -1;
    is_data = is_resource = false;
    
    // TODO: would be a lot more reassuring if resource files were all auto-converted to standard WAD files
    // Try to open as a MacOS resource file...
    is_resource = rsrc_file.open(path) == no_err;
    
    if (!is_resource)
    {
        // Try to open wad file, too; TODO: struggling to understand: why both? can M2 wad files (Map) have resource fork as well as data fork?
        is_data = wad_file.open(path) == no_err; // TODO: seems problematic to keep it open here when it may be opened elsewhere
        
        if (!read_wad_header(wad_file, &wad_header))
        {
            wad_file.close();
            is_data = false;
        }
    }
    this->path = path;
	return is_data || is_resource;
}

void image_file_t::close(void)
{
	rsrc_file.Close();
	wad_file.close();
}

bool image_file_t::is_open(void)
{
	return rsrc_file.IsOpen() || wad_file.is_open();
}


/*
 *  Get resource from file
 */

bool image_file_t::has_rsrc(uint32 rsrc_type, uint32 wad_type, int id)
{
	// Check for resource in resource file
	if (rsrc_file.IsOpen())
	{
		if (rsrc_file.Check(rsrc_type, id))
			return true;
	}
	
	// Check for resource in wad file
	if (wad_file.is_open())
    {
        wad_data* data;
        ao_err err = read_indexed_wad_from_file(wad_file, &wad_header, id, true, data);
		if (!err)
        {
			size_t len;
            bool success = get_wad_resource_for_tag(data, wad_type, &len);
			free_wad(data);
			return success;
		}
	}
	
	return false;
}

bool image_file_t::has_pict(int id)
{
	return has_rsrc(FOUR_CHARS_TO_INT('P','I','C','T'), FOUR_CHARS_TO_INT('P','I','C','T'), id) || has_rsrc(FOUR_CHARS_TO_INT('P','I','C','T'), FOUR_CHARS_TO_INT('p','i','c','t'), id);
}


int image_file_t::find_best_pict_resource_id(int base_id, int delta16, int delta32)
{
   int actual_id = base_id;
   bool done = false;
    int bit_depth = main_screen.bit_depth();

   while (!done)
   {
       int next_bit_depth;
   
       actual_id = base_id;
       switch (bit_depth)
       {
           case 8:
               next_bit_depth = 0;
               break;
               
           case 16:
               next_bit_depth = 8;
               actual_id += delta16;
               break;
               
           case 32:
               next_bit_depth = 16;
               actual_id += delta32;
               break;
               
           default:
               assert_fail(false, "");
               break;
       }
       
       if (has_pict(actual_id))
       {
           done = true;
       }
       else if (next_bit_depth)
       {
           bit_depth = next_bit_depth;
       }
       else // Didn't find it. Return the 8 bit version and bail.
       {
           done = true;
       }
   }
   return actual_id;
}


bool image_file_t::get_rsrc(uint32 rsrc_type, uint32 wad_type, int id, LoadedResource &rsrc) // wad_type is the wad resource's tag
{
	// Get resource from resource file
	if (rsrc_file.IsOpen())
	{
		if (rsrc_file.Get(rsrc_type, id, rsrc))
			return true;
	}
	
	// Get resource from wad file
    if (!wad_file.is_open()) return false;
    
    wad_data* wad;
    ao_err err = read_indexed_wad_from_file(wad_file, &wad_header, id, true, wad);
    if (!err) {
        bool success = false;
        size_t raw_length;
        uint8_t* raw = get_wad_resource_for_tag(wad, wad_type, &raw_length); // returns borrowed data from wad, or nullptr if tag not found
        if (raw)
        {
            switch (rsrc_type)
            {
                case FOUR_CHARS_TO_INT('P','I','C','T'):
                    if (wad_type == FOUR_CHARS_TO_INT('P','I','C','T'))
                    {
                        void *pict_data = malloc(raw_length);
                        memcpy(pict_data, raw, raw_length);
                        rsrc.SetData(pict_data, raw_length);
                        success = true;
                    }
                    // ignore 'clut' resources as those are no longer used by AO
                    break;
                    
                case FOUR_CHARS_TO_INT('s','n','d',' '):
                {
                    void *snd_data = malloc(raw_length);
                    memcpy(snd_data, raw, raw_length);
                    rsrc.SetData(snd_data, raw_length);
                    success = true;
                    break;
                }
                    
                case FOUR_CHARS_TO_INT('T','E','X','T'):
                {
                    void *text_data = malloc(raw_length);
                    memcpy(text_data, raw, raw_length);
                    rsrc.SetData(text_data, raw_length);
                    success = true;
                    break;
                }
            }
        }
        free_wad(wad);
        return success;
    }
    
	return false;
}


bool image_file_t::get_pict(int id, LoadedResource &rsrc)
{
	return get_rsrc(FOUR_CHARS_TO_INT('P','I','C','T'), FOUR_CHARS_TO_INT('P','I','C','T'), id, rsrc)
        || get_rsrc(FOUR_CHARS_TO_INT('P','I','C','T'), FOUR_CHARS_TO_INT('p','i','c','t'), id, rsrc);
}


bool image_file_t::get_snd(int id, LoadedResource &rsrc)
{
	return get_rsrc(FOUR_CHARS_TO_INT('s','n','d',' '), FOUR_CHARS_TO_INT('s','n','d',' '), id, rsrc);
}


bool image_file_t::get_text(int id, LoadedResource &rsrc)
{
	return get_rsrc(FOUR_CHARS_TO_INT('T','E','X','T'), FOUR_CHARS_TO_INT('t','e','x','t'), id, rsrc);
}



// -----------------------------------------------------------------------------------------
// Get image/sound/text resources from scenario files


// TODO: this searches all files except Map, but if pict ID ranges are unique (e.g. terminal picts can't collide with chapter/splash screen or M2 HUD picts), which I think they are, then we should merge MapImagesFile into this as well and optimize search order so best hit is first (e.g. search Map file first when id is in terminal picts range); we can then see about exporting any old resource-fork resources either to .png or to wadfile, so that stuff can be moved to legacy exporter

SDL_Surface* get_pict_resource_from_images(int pict_resource_id)
{
    bool found = false;
    LoadedResource PictRsrc;
    // search order: Images file, M1 Application resources (.appl) file, Shapes file
    if (!found && ImagesFile.is_open())
    {
        // TODO: streamline get_pict so it automatically returns the pict with highest-available bit depth compatible with screen's current bit depth (Q. what if a pict isn't available at 8-bit?)
        found = ImagesFile.get_pict(ImagesFile.find_best_pict_resource_id(pict_resource_id, _images_file_delta16, _images_file_delta32), PictRsrc);
    }
    if (!found && ExternalResourcesFile.is_open())
    {
        found = ExternalResourcesFile.get_pict(pict_resource_id, PictRsrc);
    }
    if (!found && ShapesImagesFile.is_open())
    {
        found = ShapesImagesFile.get_pict(pict_resource_id, PictRsrc);
    }
    return found ? picture_to_surface(PictRsrc) : nullptr;
}


SDL_Surface* get_pict_resource_from_map(int base_resource)
{
    bool found = false;
    LoadedResource PictRsrc;

    if (!found && MapFile.is_open())
    {
        auto id = MapFile.find_best_pict_resource_id(base_resource, _scenario_file_delta16, _scenario_file_delta32);
        found = Plugins::instance()->get_resource(FOUR_CHARS_TO_INT('P','I','C','T'), id, PictRsrc);
        if (!found)
        {
            found = MapFile.get_pict(MapFile.find_best_pict_resource_id(base_resource, _scenario_file_delta16, _scenario_file_delta32), PictRsrc);
        }
    }
    
    if (!found && ShapesImagesFile.is_open())
    {
        found = Plugins::instance()->get_resource(FOUR_CHARS_TO_INT('P','I','C','T'), base_resource, PictRsrc);

        if (!found)
        {
            found = ShapesImagesFile.get_pict(base_resource, PictRsrc);
        }
    }
    
    return found ? picture_to_surface(PictRsrc) : nullptr;
}


bool get_sound_resource_from_map(int resource_number, LoadedResource &SoundRsrc)
{
    bool found = false;
    
    if (!found && MapFile.is_open())
    {
        found = Plugins::instance()->get_resource(FOUR_CHARS_TO_INT('s','n','d',' '), resource_number, SoundRsrc);
        if (!found)
        {
            found = MapFile.get_snd(resource_number, SoundRsrc);
        }
    }
    
    if (!found && SoundsImagesFile.is_open())
    {
        // Marathon 1 case: only one sound used for chapter screens
        found = Plugins::instance()->get_resource(FOUR_CHARS_TO_INT('s','n','d', ' '), 1240, SoundRsrc);

        if (!found)
        {
            found = SoundsImagesFile.get_snd(1240, SoundRsrc);
        }
    }
    
    return found;
}



bool get_sound_resource_from_images(int resource_number, LoadedResource &SoundRsrc)
{
    return ImagesFile.is_open() && ImagesFile.get_snd(resource_number, SoundRsrc);
}


bool get_sound_resource_from_sounds(int resource_number, LoadedResource &SoundRsrc)
{
    return SoundsImagesFile.is_open() && SoundsImagesFile.get_snd(resource_number, SoundRsrc);
}


bool get_text_resource_from_map(int resource_number, LoadedResource &TextRsrc)
{
    if (!MapFile.is_open())
        return false;

    auto success = Plugins::instance()->get_resource(FOUR_CHARS_TO_INT('T','E','X','T'), resource_number, TextRsrc);

    if (!success)
    {
        success = MapFile.get_text(resource_number, TextRsrc);
    }
    
    return success;
}


// -----------------------------------------------------------------------------------------
// used by scenario chooser


SDL_Surface* find_m2_title_screen(const ao_path& file)
{
	image_file_t image_file;
	if (image_file.open(file))
	{
		for (auto i = 2; i >= 0; --i)
		{
			LoadedResource title_screen;
			if (image_file.get_pict(M2_STARTUP_SCREEN_BASE + i + _images_file_delta32, title_screen))
			{
				return picture_to_surface(title_screen);
			}
			if (image_file.get_pict(M2_STARTUP_SCREEN_BASE + i + _images_file_delta16, title_screen))
			{
				return picture_to_surface(title_screen);
			}
			if (image_file.get_pict(M2_STARTUP_SCREEN_BASE + i, title_screen))
			{
				return picture_to_surface(title_screen);
			}
		}
	}
	return nullptr;
}


SDL_Surface* find_m1_title_screen(const ao_path& file)
{
	image_file_t shapes_file;
	if (shapes_file.open(file))
	{
		LoadedResource title_screen;
		if (shapes_file.get_pict(1114, title_screen))
		{
			return picture_to_surface(title_screen);
		}
	}
	return nullptr;
}
