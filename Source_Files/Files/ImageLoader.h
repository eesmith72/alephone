#ifndef _IMAGE_LOADER_
#define _IMAGE_LOADER_
/*

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
	
	Image-Loader Interface File,
	by Loren Petrich,
	October 21, 2000

	This file contains an image-descriptor object and a function for loading the image
	from a file.
	
*/

#include "cseries.hpp"

#include "DataFile.hpp"


#ifndef __DDRAW_INCLUDED__

// Written in 2005 by Gregory Smith
// From the Directx9 reference
// http://msdn.microsoft.com/archive/default.asp?url=/archive/en-us/directx9_c/directx/graphics/reference/ddsfilereference/ddsfileformat.asp

#define DDSD_CAPS          0x00000001
#define DDSD_HEIGHT        0x00000002
#define DDSD_WIDTH         0x00000004
#define DDSD_PITCH         0x00000008
#define DDSD_PIXELFORMAT   0x00001000
#define DDSD_MIPMAPCOUNT   0x00002000
#define DDSD_LINEARSIZE    0x00008000
#define DDSD_DEPTH         0x00800000

#define DDPF_ALPHAPIXELS   0x00000001
#define DDPF_FOURCC        0x00000004
#define DDPF_RGB           0x00000040

#define DDSCAPS_COMPLEX    0x00000008
#define DDSCAPS_TEXTURE    0x00001000
#define DDSCAPS_MIPMAP     0x00400000

#define DDSCAPS2_CUBEMAP   0x00000200
#define DDSCAPS2_VOLUME    0x00200000

struct DDSURFACEDESC2
{
    uint32 dwSize;
    uint32 dwFlags;
    uint32 dwHeight;
    uint32 dwWidth;
    uint32 dwPitchOrLinearSize;
    uint32 dwDepth;
    uint32 dwMipMapCount;
    uint32 dwReserved1[11];

    struct {
        uint32 dwSize;
        uint32 dwFlags;
        uint32 dwFourCC;
        uint32 dwRGBBitCount;
        uint32 dwRBitMask;
        uint32 dwGBitMask;
        uint32 dwBBitMask;
        uint32 dwRGBAlphaBitMask;
    } ddpfPixelFormat;

    struct {
        uint32 dwCaps1;
        uint32 dwCaps2;
        uint32 Reserved[2];
    } ddsCaps;

    uint32 dwReserved2;
};
    
#endif




// the read-in image
class ImageDescriptor
{
	int Width;	    // along scanlines
	int Height;	    // scanline to scanline

	double VScale;
	double UScale;

	uint32 *Pixels;
	int Size;

	int MipMapCount;
	
public:
	
	bool IsPresent() const {return (Pixels != NULL); }
	bool IsPremultiplied() const { return (IsPresent() ? PremultipliedAlpha : false); }

	bool LoadFromFile(const ao_path& File, int ImgMode, int flags, int actual_width = 0, int actual_height = 0, int maxSize = 0);

	// Size of level 0 image
	int GetWidth() const {return Width;}
	int GetHeight() const {return Height;}
	int GetNumPixels() const {return Width*Height;}

	int GetMipMapCount() const { return MipMapCount; }
	int GetTotalBytes() const { return Size; }
	int GetBufferSize() const { return Size; }
	int GetFormat() const { return Format; }

	double GetVScale() const { return VScale; }
	double GetUScale() const { return UScale; }

	// Pixel accessors
	uint32& GetPixel(int Horiz, int Vert) {return Pixels[Width*(Vert%Height) + (Horiz%Width)];}
	uint32 *GetPixelBasePtr() {return Pixels;}
	const uint32 *GetBuffer() const { return Pixels; }
	uint32 *GetBuffer() { return Pixels; }

	uint32 *GetMipMapPtr(int Level);
	const uint32 *GetMipMapPtr(int Level) const;
	int GetMipMapSize(int level) const;
	
	// Reallocation
	void Resize(int _Width, int _Height);

	// mipmappy operations
	void Resize(int _Width, int _Height, int _TotalBytes);

	bool Minify();

	bool MakeRGBA();
	bool MakeDXTC3();

	void PremultiplyAlpha();
	bool PremultipliedAlpha; // public so find silhouette version can unset

	// Clearing
	void Clear()
		{Width = Height = Size = 0; delete []Pixels; Pixels = NULL;}

	ImageDescriptor(const ImageDescriptor &CopyFrom);
	
ImageDescriptor(): Width(0), Height(0), VScale(1.0), UScale(1.0), Pixels(NULL), Size(0), PremultipliedAlpha(false) {}

	// asumes RGBA8
	ImageDescriptor(int width, int height, uint32 *pixels);

	enum ImageFormat {
		RGBA8,
		DXTC1,
		DXTC3,
		DXTC5,
		Unknown
	};

	~ImageDescriptor()
	{
		delete []Pixels;
		Pixels = NULL;
	}
    
    SDL_Surface* copy_to_surface()
    {
        // TODO: going to assume this never fails in practice
        return SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32 *>(GetBuffer()), GetWidth(), GetHeight(),
                                                  32, GetWidth() * 4, AO_PIXEL_FORMAT_32);
    }
			
private:
	bool LoadDDSFromFile(const ao_path& File, int flags, int actual_width = 0, int actual_height = 0, int maxSize = 0);
	bool LoadMipMapFromFile(DataFile &File, int flags, int level, DDSURFACEDESC2 &ddsd, int skip);
	bool SkipMipMapFromFile(DataFile &File, int flags, int level, DDSURFACEDESC2 &ddsd);

	ImageFormat Format;
};

template <typename T>
class copy_on_edit
{
public:
	copy_on_edit() : _original(NULL), _copy(NULL) { };

	void set(const T* original) {
		if (_copy) {
			delete _copy;
			_copy = NULL;
		}
		_original = original;
	}

	void set(T* original) {
		if (_copy) {
			delete _copy;
			_copy= NULL;
		}
		_original = original;
	}


	const T* get() {
		if (_copy)
			return (const T*) _copy;
		else
			return (const T*) _original;
	}

	T* edit() {
		if (!_original) {
			return _copy;
		} else {
			if (!_copy) {
				_copy = new T(*_original);
			}
			return _copy;
		}
	}

	// takes possession of copy
	T* edit(T* copy) {
		if (_copy) {
			delete _copy;
		}
		_original = NULL;
		_copy = copy;
        return _copy;
	}

	~copy_on_edit() {
		if (_copy) {
			delete _copy;
			_copy = NULL;
		}
	}

private:
	T* _original;
	T* _copy;
};

typedef copy_on_edit<ImageDescriptor> ImageDescriptorManager;
		

// What to load: image colors (must be loaded first)
// or image opacity (replaces the default, which is 100% opaque everywhere).
// The image-opacity image must have the same size as the color image;
// it is interpreted as a grayscale image.
enum {
	ImageLoader_Colors,
	ImageLoader_Opacity
};

enum {
	ImageLoader_ResizeToPowersOfTwo = 0x1,
	ImageLoader_CanUseDXTC = 0x2,
	ImageLoader_LoadMipMaps = 0x4,
	ImageLoader_LoadDXTC1AsDXTC3 = 0x8,
	ImageLoader_ImageIsAlreadyPremultiplied = 0x10
};
// Returns whether or not the loading was successful
//bool LoadImageFromFile(ImageDescriptor& Img, const ao_path& File, int ImgMode, int flags, int maxSize = 0);

uint32 *GetMipMapPtr(uint32 *pixels, int size, int level, int width, int height, int format);

#endif
