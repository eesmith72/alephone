/*
 Decoder.h -- Decodes music and external sounds
 
 Copyright (C) 2007 Gregory Smith and the "Aleph One" developers.
 
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

#ifndef __DECODER_H
#define __DECODER_H


#include "cseries.h"

#include "DataFile.hpp"
#include "SoundManagerEnums.h"


class StreamDecoder
{
public:
    virtual bool Open(const ao_path& File) = 0;
	virtual int32 Decode(uint8* buffer, int32 max_length) = 0;
	virtual void Rewind() = 0;
	virtual void Close() = 0;

	virtual AudioFormat GetAudioFormat() = 0;
	virtual bool IsStereo() = 0;
	virtual int BytesPerFrame() = 0;
	virtual uint32_t Rate() = 0;
	virtual bool IsLittleEndian() = 0;
	virtual float Duration() = 0;
	virtual uint32_t Position() = 0;
	virtual void Position(uint32_t position) = 0;
	virtual uint32_t Size() = 0;

	StreamDecoder() { }
	virtual ~StreamDecoder() { }

	static std::unique_ptr<StreamDecoder> Get(const ao_path& File); // returns nullptr if file can't be read
};


class Decoder : public StreamDecoder
{
public:
	Decoder() : StreamDecoder() { }
	~Decoder() { }

	// total number of frames in the file
	virtual int32 Frames() = 0;

	static Decoder* Get(const ao_path& File); // returns nullptr if file can't be read
};

#endif
