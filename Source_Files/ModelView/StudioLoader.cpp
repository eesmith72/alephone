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

	3D Studio Max Object Loader
	
	By Loren Petrich, Sept 1, 2001
	
	Derived from the work of
	
	Jeff Lewis (werewolf@worldgate.com)
	Martin van Velsen (vvelsen@ronix.ptf.hro.nl)
	Robin Fercoq (robin@msrwww.fc-net.fr)
	Jim Pitts (jim@micronetics.com)
	Albert Szilvasy (szilvasy@almos.vein.hu)
	Terry Caton (tcaton@umr.edu)
	Matthew Fairfax
	
*/

#include "cseries.h"

#ifdef HAVE_OPENGL


#include "StudioLoader.h"

// Use pack/unpack, but with little-endian data
#undef PACKED_DATA_IS_BIG_ENDIAN
#define PACKED_DATA_IS_LITTLE_ENDIAN
#include "Packing.h"

/*
	The various authors have different conventions for the chunk names;
	the nomenclature here is a hybrid of these.
	Only the chunks actually read will be listed here.
*/

const uint16 MASTER =				0x4d4d;
const uint16   EDITOR =				0x3d3d;
const uint16     OBJECT =			0x4000;
const uint16       TRIMESH =		0x4100;
const uint16         VERTICES =		0x4110;
const uint16         TXTR_COORDS =	0x4140;
const uint16         FACE_DATA =	0x4120;

static ao_path path_to_model_file; // think this is only used for logging

struct ChunkHeaderData
{
	uint16 ID;
	uint32 Size;
};
const int SIZEOF_ChunkHeaderData = 6;

// For read-in chunks
std::vector<uint8> ChunkBuffer;
inline uint8 *ChunkBufferBase() {return &ChunkBuffer[0];}
inline size_t ChunkBufferSize() {return ChunkBuffer.size();}
inline void SetChunkBufferSize(int Size) {ChunkBuffer.resize(Size);}

// Local prototypes;
// these functions return "false" if there was a read or reposition error
static bool ReadChunkHeader(DataFile& OFile, ChunkHeaderData& ChunkHeader);
static bool LoadChunk(DataFile& OFile, ChunkHeaderData& ChunkHeader);
static bool SkipChunk(DataFile& OFile, ChunkHeaderData& ChunkHeader);

// These functions read the contents of these container chunks
// to the file location "ChunkEnd".
// The generic reader takes specific readers as the last argument
static bool ReadContainer(DataFile& OFile, ChunkHeaderData& ChunkHeader,
	bool (*ContainerCallback)(DataFile&,int32));
static bool ReadMaster(DataFile& OFile, int32 ParentChunkEnd);
static bool ReadEditor(DataFile& OFile, int32 ParentChunkEnd);
static bool ReadObject(DataFile& OFile, int32 ParentChunkEnd);
static bool ReadTrimesh(DataFile& OFile, int32 ParentChunkEnd);
static bool ReadFaceData(DataFile& OFile, int32 ParentChunkEnd);

// For processing the raw chunk data into appropriate forms:
static void LoadVertices();
static void LoadTextureCoordinates();

static void LoadFloats(int NVals, uint8 *Stream, GLfloat *Floats);


// For feeding into the read-in routines
static Model3D *ModelPtr = NULL;


bool LoadModel_Studio(const ao_path& Spec, Model3D& Model)
{
	ModelPtr = &Model;
	Model.Clear();
	
	path_to_model_file = Spec;
    log_note_f("Loading 3D Studio Max model file %s",path_to_model_file.c_str());
	
	DataFile OFile;
	if (OFile.open(Spec))
	{
        log_error_f("failed to open %s",path_to_model_file.c_str());
		return false;
	}
	
	ChunkHeaderData ChunkHeader;
	if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
	if (ChunkHeader.ID != MASTER)
	{
        log_error_f("not a 3DS Max model file: %s",path_to_model_file.c_str());
		return false;
	}
	
	if (!ReadContainer(OFile,ChunkHeader,ReadMaster)) return false;
	
	if (Model.Positions.empty())
	{
        log_error_f("no vertices found in %s",path_to_model_file.c_str());
		return false;
	}
	if (Model.VertIndices.empty())
	{
        log_error_f("no faces found in %s",path_to_model_file.c_str());
		return false;
	}
	return true;
}

bool ReadChunkHeader(DataFile& OFile, ChunkHeaderData& ChunkHeader)
{
	uint8 Buffer[SIZEOF_ChunkHeaderData];
    OFile.read(SIZEOF_ChunkHeaderData, Buffer);
	uint8 *S = Buffer;
	StreamToValue(S, ChunkHeader.ID);
	StreamToValue(S, ChunkHeader.Size);
	return true;
}

bool LoadChunk(DataFile& OFile, ChunkHeaderData& ChunkHeader)
{
    log_trace_f("Loading chunk 0x%04hx size %u",ChunkHeader.ID,ChunkHeader.Size);
	int32 DataSize = ChunkHeader.Size - SIZEOF_ChunkHeaderData;
	SetChunkBufferSize(DataSize);
    OFile.read(DataSize, ChunkBufferBase());
	return true;
}

bool SkipChunk(DataFile& OFile, ChunkHeaderData& ChunkHeader)
{
    log_trace_f("Skipping chunk 0x%04hx size %u",ChunkHeader.ID,ChunkHeader.Size);
	int64_t DataSize = ChunkHeader.Size - SIZEOF_ChunkHeaderData;
	
	int64_t Location = OFile.get_position();
    OFile.set_position(Location + DataSize);
	return true;
}

// Generic container-chunk reader
bool ReadContainer(DataFile& OFile, ChunkHeaderData& ChunkHeader,
	bool (*ContainerCallback)(DataFile&,int32))
{
    log_trace_f("Entering chunk 0x%04hx size %u",ChunkHeader.ID,ChunkHeader.Size);
	
	int64_t ChunkEnd = OFile.get_position();
	ChunkEnd += ChunkHeader.Size - SIZEOF_ChunkHeaderData;
	
	if (!ContainerCallback(OFile,(int32_t)ChunkEnd)) return false;
	
    log_trace_f("Exiting chunk 0x%04hx size %u",ChunkHeader.ID,ChunkHeader.Size);
	return true;
}


// For reading the master chunk (ideally, whole file)
static bool ReadMaster(DataFile& OFile, int32 ParentChunkEnd)
{
	int64_t Location = OFile.get_position();
	
	while(Location < ParentChunkEnd)
	{
		ChunkHeaderData ChunkHeader;
		if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
		
		switch(ChunkHeader.ID)
		{
		case EDITOR:
			if (!ReadContainer(OFile,ChunkHeader,ReadEditor)) return false;
			break;
				
		default:
			if (!SkipChunk(OFile,ChunkHeader)) return false;
		}
		
		// Where are we now?
        Location = OFile.get_position();
	}
	
	if (Location > ParentChunkEnd)
	{
        log_error_f("Overran parent chunk: %d > %lld in %s", Location, ParentChunkEnd, path_to_model_file.c_str());
		return false;
	}
	return true;
}

// For reading the editor-data chunk
static bool ReadEditor(DataFile& OFile, int32 ParentChunkEnd)
{
	int64_t Location = OFile.get_position();
	
	while(Location < ParentChunkEnd)
	{
		ChunkHeaderData ChunkHeader;
		if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
		
		switch(ChunkHeader.ID)
		{
		case OBJECT:
			if (!ReadContainer(OFile,ChunkHeader,ReadObject)) return false;
			break;
			
		default:
			if (!SkipChunk(OFile,ChunkHeader)) return false;
		}
		
		// Where are we now?
        Location = OFile.get_position();
	}
	
	if (Location > ParentChunkEnd)
	{
        log_error_f("Overran parent chunk: %d > %d in %s", Location, ParentChunkEnd, path_to_model_file.c_str());
		return false;
	}
	return true;
}

// For reading the object-data chunk
static bool ReadObject(DataFile& OFile, int32 ParentChunkEnd)
{
	// Read the name
	char c;
	do
	{
        OFile.read(1,&c);
	}
	while(c != 0);
	
	int64_t Location = OFile.get_position();
	
	while(Location < ParentChunkEnd)
	{
		ChunkHeaderData ChunkHeader;
		if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
		
		switch(ChunkHeader.ID)
		{
		case TRIMESH:
			if (!ReadContainer(OFile,ChunkHeader,ReadTrimesh)) return false;
			break;
			
		default:
			if (!SkipChunk(OFile,ChunkHeader)) return false;
		}
		
		// Where are we now?
        Location = OFile.get_position();
	}
	
	if (Location > ParentChunkEnd)
	{
        log_error_f("ERROR: Overran parent chunk: %d > %d in %s", Location, ParentChunkEnd, path_to_model_file.c_str());
		return false;
	}
	return true;
}

// For reading the triangle-mesh-data chunk
static bool ReadTrimesh(DataFile& OFile, int32 ParentChunkEnd)
{
	int64_t Location = OFile.get_position();
	
	assert_fail(ModelPtr, "");
	
	while(Location < ParentChunkEnd)
	{
		ChunkHeaderData ChunkHeader;
		if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
		
		switch(ChunkHeader.ID)
		{
		case VERTICES:
			if (!LoadChunk(OFile,ChunkHeader)) return false;
			LoadVertices();
			break;
			
		case TXTR_COORDS:
			if (!LoadChunk(OFile,ChunkHeader)) return false;
			LoadTextureCoordinates();
			break;
			
		case FACE_DATA:
			if (!ReadContainer(OFile,ChunkHeader,ReadFaceData)) return false;
			break;
			
		default:
			if (!SkipChunk(OFile,ChunkHeader)) return false;
		}
		
		// Where are we now?
        Location = OFile.get_position();
	}
	
	if (Location > ParentChunkEnd)
	{
        log_error_f("ERROR: Overran parent chunk: %d > %d in %s", Location, ParentChunkEnd, path_to_model_file.c_str());
		return false;
	}
	return true;
}


// For reading the face-data chunk
static bool ReadFaceData(DataFile& OFile, int32 ParentChunkEnd)
{
	uint8 NFBuffer[2];
	uint16 NumFaces;
    OFile.read(2,NFBuffer);
	uint8 *S = NFBuffer;
	StreamToValue(S,NumFaces);
	
	int32 DataSize = 4*sizeof(uint16)*int(NumFaces);
	SetChunkBufferSize(DataSize);
    OFile.read(DataSize,ChunkBufferBase());
	
	S = ChunkBufferBase();
	ModelPtr->VertIndices.resize(3*NumFaces);
	for (int k=0; k<NumFaces; k++)
	{
		uint16 *CurrPoly = ModelPtr->VIBase() + 3*k;
		uint16 Flags;
		StreamToList(S,CurrPoly,3);
		StreamToValue(S,Flags);
	}
	
	int64_t Location = OFile.get_position();
	
	while(Location < ParentChunkEnd)
	{
		ChunkHeaderData ChunkHeader;
		if (!ReadChunkHeader(OFile,ChunkHeader)) return false;
		
		switch(ChunkHeader.ID)
		{
		/*
		case OBJECT:
			if (!ReadContainer(OFile,ChunkHeader,ReadObject)) return false;
			break;
		*/
		default:
			if (!SkipChunk(OFile,ChunkHeader)) return false;
		}
		
		// Where are we now?
        Location = OFile.get_position();
	}
	
	if (Location > ParentChunkEnd)
	{
        log_error_f("ERROR: Overran parent chunk: %d > %d in %s", Location, ParentChunkEnd, path_to_model_file.c_str());
		return false;
	}
	return true;
}


static void LoadVertices()
{
	uint8 *S = ChunkBufferBase();
	uint16 Size;
	StreamToValue(S,Size);
	
	int NVals = 3*int(Size);
	
	ModelPtr->Positions.resize(NVals);
	
	LoadFloats(NVals,S,ModelPtr->PosBase());
}

static void LoadTextureCoordinates()
{
	uint8 *S = ChunkBufferBase();
	uint16 Size;
	StreamToValue(S,Size);
	
	int NVals = 2*int(Size);
	
	ModelPtr->TxtrCoords.resize(NVals);
	
	LoadFloats(NVals,S,ModelPtr->TCBase());
}


void LoadFloats(int NVals, uint8 *Stream, GLfloat *Floats)
{
	// Test to see whether the destination floating-point values are the right size:
	assert_fail(sizeof(GLfloat) == 4, "");
	
	uint32 IntVal;
	GLfloat *FloatPtr = Floats;
	for (int k=0; k<NVals; k++, FloatPtr++)
	{
		// Intermediate step: 4-byte integer
		// (won't have the right value if interpreted as an integer!)
		StreamToValue(Stream,IntVal);
		
		// This will work on any platform where
		// GLfloat is an IEEE 754 4-byte float.
		// Otherwise, use whatever appropriate conversion tricks are appropriate
		uint8 *SrcPtr = (uint8 *)(&IntVal);
		uint8 *DestPtr = (uint8 *)FloatPtr;
		for (int c=0; c<4; c++)
			DestPtr[c] = SrcPtr[c];
	}
}

// Load a 3D Studio MAX model and convert its vertex and texture coordinates
// from 3DS' right-handed coordinate system to Aleph One's left-handed system.
bool LoadModel_Studio_RightHand(const ao_path& Spec, Model3D& Model)
{
	bool Result = LoadModel_Studio(Spec, Model);
	if (!Result) return Result;

    log_trace("Converting handedness.");

	// Wings 3d and Blender produce 3DS models with a z-up orientation,
	// and for Blender models y increases towards the back. In Aleph One,
	// z increases upward, and items that have been placed with 0 degrees
	// of rotation face in the positive-x direction.
	// Preserve the orientation and switch handedness by swapping and
	// flipping x and y.
	for (unsigned XPos = 0; XPos < Model.Positions.size(); XPos += 3)
	{
		GLfloat X = Model.Positions[XPos];
		Model.Positions[XPos] = -Model.Positions[XPos + 1];
		Model.Positions[XPos + 1] = -X;
	}

	// Put the vertices of faces back into clockwise order.
	for (unsigned IPos = 0; IPos < Model.VertIndices.size(); IPos += 3)
	{
		int Index = Model.VertIndices[IPos + 1];
		Model.VertIndices[IPos + 1] = Model.VertIndices[IPos];
		Model.VertIndices[IPos] = Index;
	}

	// Switch texture coordinates from right-handed (x,y) to
	// left-handed (row,column).
	for (unsigned YPos = 1; YPos < Model.TxtrCoords.size(); YPos += 2)
	{
		Model.TxtrCoords[YPos] = 1.0 - Model.TxtrCoords[YPos];
	}

	return true;
}

#endif // def HAVE_OPENGL
