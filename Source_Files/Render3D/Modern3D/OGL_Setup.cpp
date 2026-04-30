/*
 OpenGL Renderer -- set parameters for OpenGL rendering.
 by Loren Petrich, March 12, 2000
 
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

#include "cseries.h"

#include "shapes.h"
#include "OGL_Headers.h"
#include "OGL_Shader.h"
#include "OGL_Setup.h"

#include "preferences.h" // graphics_preferences

#include "InfoTree.h"


// move these onto graphics_preferences struct and start getting rid of stupid TEST_FLAG crap
bool Using_sRGB = false;
bool Bloom_sRGB = false;
bool npotTextures = false; // non-power-of-two

// Initializer
void OGL_Initialize()
{
    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    
    // TODO: tiling wall and landscape textures must be power-of-two, so that just leaves sprites (HUD and dialogs use ImageBlitter, which always uses PoT); sprites should move to 2048x2048 'sprite sheets'
    npotTextures = OGL_CheckExtension("GL_ARB_texture_non_power_of_two");
    
    // FBOs were already required (this check returned if it failed) so now we throw an exception
    if (!OGL_CheckExtension("GL_EXT_framebuffer_object"))
    {
        throw_ao_exception("Framebuffer Objects not available", 3); // what error code?
    }
    
    if (!(OGL_CheckExtension("GL_ARB_vertex_shader")  && OGL_CheckExtension("GL_ARB_fragment_shader") &&
          OGL_CheckExtension("GL_ARB_shader_objects") && OGL_CheckExtension("GL_ARB_shading_language_100")))
    {
        throw_ao_exception("Failed to initialize screen.", 2);
    }
    
    
    // TODO: is there any reason this should be a user preference?
    if (graphics_preferences->OGL_Configure.Use_sRGB)
    {
      if (!OGL_CheckExtension("GL_EXT_framebuffer_sRGB") || !OGL_CheckExtension("GL_EXT_texture_sRGB"))
      {
          graphics_preferences->OGL_Configure.Use_sRGB = false;
          log_warning("Gamma corrected blending is not available");
      }
    }
    
    Bloom_sRGB = true;
    if (TEST_FLAG(graphics_preferences->OGL_Configure.Flags, OGL_Flag_Bloom))
    {
      if (!OGL_CheckExtension("GL_EXT_framebuffer_sRGB") || !OGL_CheckExtension("GL_EXT_texture_sRGB"))
      {
          Bloom_sRGB = false;
          log_warning("sRGB framebuffer is not available for bloom effects");
      }
    }
    

}


bool OGL_CheckExtension(const std::string extension)
{
#ifdef __WIN32__
	return glewIsSupported(extension.c_str());
#else
	char *extensions = (char *) glGetString(GL_EXTENSIONS);
	if (!extensions) return false;

	while (*extensions)
	{
		size_t length = strcspn(extensions, " ");
		if (length == extension.size() && strncmp(extension.c_str(), extensions, length) == 0)
        {
			return true;
		}
		extensions += length + 1;
	}
#endif
	return false;
}


// Sensible defaults for the fog:
static OGL_FogData FogData[OGL_NUMBER_OF_FOG_TYPES] = 
{
	{{0x8000,0x8000,0x8000},8,0,false,true,OGL_Fog_Exp,1},
	{{0x8000,0x8000,0x8000},8,0,false,true,OGL_Fog_Exp,1}
};


// For flat landscapes: // TODO: If a user wants flat landscapes, they should use a Shapes plugin that overrides the original landscapes with flat textures (or anything else they want). Let's get rid of it: the less OGL code there is, the easier to convert to SDL_gpu; plus it gets rid of another dumb Preference control.
const rgb_color DefaultLscpColors[4][2] =
{
	{
		{0xffff, 0xffff, 0x6666},		// Day
		{0x3333, 0x9999, 0xffff},
	},
	{
		{0x1818, 0x1818, 0x1010},		// Night
		{0x0808, 0x0808, 0x1010},
	},
	{
		{0x6666, 0x6666, 0x6666},		// Moon
		{0x0000, 0x0000, 0x0000},
	},
	{
		{0x0000, 0x0000, 0x0000},		// Outer Space
		{0x0000, 0x0000, 0x0000},
	},
};


// Set defaults
void OGL_SetDefaults(OGL_ConfigureData& Data)
{
	for (int k=0; k<OGL_NUMBER_OF_TEXTURE_TYPES; k++)
	{
		OGL_Texture_Configure& TxtrData = Data.TxtrConfigList[k];
        
		TxtrData.NearFilter  = GL_LINEAR; // TODO: this needs to be determined automatically (or per-collection in MML if it can't be), based on bitmap dimensions and size it's being rendered at, i.e. is bitmap "HD" quality? only smooth it if pixel density is high enough as low-res textures look utter shit
        
        // always use these settings for Far (moving them into code can be done later)
        TxtrData.FarFilter   = (k == OGL_Txtr_Wall || k == OGL_Txtr_Inhabitant) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
		
        TxtrData.Resolution  = 0; // 1x
		TxtrData.ColorFormat = 0; // 32-bit color // TODO: this can go away
		TxtrData.MaxSize     = 0; // Unlimited
	}

    // TODO: as above
	Data.ModelConfig.NearFilter = 1;
	Data.ModelConfig.FarFilter = 5;
	Data.ModelConfig.Resolution = 0;
	Data.ModelConfig.ColorFormat = 0;
	Data.ModelConfig.MaxSize = 0;
	
	// Reasonable default flags
	Data.Flags = OGL_Flag_Fader | OGL_Flag_LiqSeeThru | OGL_Flag_Fog;

    Data.AnisotropyLevel = 0.0; // off
	Data.Multisamples = 0; // EES: TODO: AO being AO, there was no Preferences widget to set this value! So let's leave it at 0 for now, which is what it effectively was, and figure out what to do with it later.
	
	for (int il=0; il<4; il++)
		for (int ie=0; ie<2; ie++)
			Data.LscpColors[il][ie] = DefaultLscpColors[il][ie];

	Data.Use_sRGB = false;

	//Data.BillboardXY = false; // EES: the Modern renderer should always look its best, so I've permanently enabled perspective. Users who want an authentic 1995 look can use the Classic screen modes, which are now easy to select in Preferences and in-game.
}


inline bool StringPresent(std::vector<char>& String)
{
	return (String.size() > 1);
}


GLint glMaxTextureSize = 0;
bool hasS3TC = false;

void OGL_TextureOptionsBase::Load()
{
    ao_path File;

	GLint maxTextureSize = glMaxTextureSize;
	if (GetMaxSize())
	{
		maxTextureSize = MIN(maxTextureSize, GetMaxSize());
	}
	
	int flags = npotTextures ? 0 : ImageLoader_ResizeToPowersOfTwo;
		
	if (Type >= 0 && Type < OGL_NUMBER_OF_TEXTURE_TYPES && graphics_preferences->OGL_Configure.TxtrConfigList[Type].FarFilter > 1 /* GL_LINEAR */)
	{
			flags |= ImageLoader_LoadMipMaps;
	}

	if (hasS3TC) 
	{
		flags |= ImageLoader_CanUseDXTC;
	}

	// Load the normal image with alpha channel

	// Check to see if loading needs to be done;
	// it does not need to be if an image is present.
	if (NormalImg.IsPresent()) return;

	NormalImg.Clear();
	
	// Load the normal image if it has a filename specified for it
    if (std::filesystem::is_regular_file(NormalColors))
	{
		if (!NormalImg.LoadFromFile(NormalColors,ImageLoader_Colors, flags | (NormalIsPremultiplied ? ImageLoader_ImageIsAlreadyPremultiplied : 0), actual_width, actual_height, maxTextureSize))
		{
			// A texture must have a normal colored part
			return;
		}
	}
	else
	{
		return;
	}

	// load a heightmap
	if (TEST_FLAG(graphics_preferences->OGL_Configure.Flags, OGL_Flag_BumpMap) && std::filesystem::is_regular_file(OffsetMap)) {
		if(!OffsetImg.LoadFromFile(OffsetMap, ImageLoader_Colors, flags | (NormalIsPremultiplied ? ImageLoader_ImageIsAlreadyPremultiplied : 0), actual_width, actual_height, maxTextureSize)) {
			return;
		}
	}

	// Load the normal mask if it has a filename specified for it
	if (std::filesystem::is_regular_file(NormalMask))
	{
		NormalImg.LoadFromFile(NormalMask,ImageLoader_Opacity, flags, actual_width, actual_height, maxTextureSize);
	}

	if (maxTextureSize)
	{
		while (NormalImg.GetWidth() > maxTextureSize || NormalImg.GetHeight() > maxTextureSize)
		{
			if (!NormalImg.Minify()) break;
		}
		
		if(OffsetImg.IsPresent()) {
			while (OffsetImg.GetWidth() > maxTextureSize || OffsetImg.GetHeight() > maxTextureSize) {
				if(!OffsetImg.Minify()) { break; }
			}
		}
	}
	
	// Load the glow image with alpha channel
	if (!GlowImg.IsPresent())
	{
		GlowImg.Clear();
		
		// Load the glow image if it has a filename specified for it
		if (std::filesystem::is_regular_file(GlowColors))
		{
			if (GlowImg.LoadFromFile(GlowColors,ImageLoader_Colors, flags | (GlowIsPremultiplied ? ImageLoader_ImageIsAlreadyPremultiplied : 0), actual_width, actual_height, maxTextureSize))
			{
		
				// Load the glow mask if it has a
				// filename specified for it; only
				// loaded if an image has been loaded
				// for it
				if (std::filesystem::is_regular_file(GlowMask))
				{
					GlowImg.LoadFromFile(GlowMask,ImageLoader_Opacity, flags, actual_width, actual_height, maxTextureSize);
				}
			}
		}
	}
	
	if (GlowImg.IsPresent() && maxTextureSize)
	{
		while (GlowImg.GetWidth() > maxTextureSize || GlowImg.GetHeight() > maxTextureSize) 
		{
			if (!GlowImg.Minify()) break;
		}
	}

	// The rest of the code is made simpler by these constraints:
	// that the glow texture only be present if the normal texture is also present,
	// and that the normal and glow textures have the same dimensions
	if (NormalImg.IsPresent())
	{
		int W0 = NormalImg.GetWidth();
		int W1 = GlowImg.GetWidth();
		int H0 = NormalImg.GetHeight();
		int H1 = GlowImg.GetHeight();
		if ((W1 != W0) || (H1 != H0)) GlowImg.Clear();
	}
	else
	{
		GlowImg.Clear();
	}

}

void OGL_TextureOptionsBase::Unload()
{
	NormalImg.Clear();
	GlowImg.Clear();
	OffsetImg.Clear();
}

int OGL_TextureOptionsBase::GetMaxSize()
{
	if (Type >= 0 && Type < OGL_NUMBER_OF_TEXTURE_TYPES)
	{
		return graphics_preferences->OGL_Configure.TxtrConfigList[Type].MaxSize;
	}
	else
		return 0; // Unlimited
}


int OGL_CountModelsImages(short Collection)
{
	return OGL_CountTextures(Collection) + OGL_CountModels(Collection);
}


// for managing the model and image loading and unloading
void OGL_LoadModelsImages(short Collection)
{
	assert_fail(Collection >= 0 && Collection < MAXIMUM_COLLECTIONS, "");

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTextureSize);
	hasS3TC = OGL_CheckExtension("GL_ARB_texture_compression") && OGL_CheckExtension("GL_EXT_texture_compression_s3tc");
	
	// For wall/sprite images
	OGL_LoadTextures(Collection);
	
	// For models, skins
	OGL_LoadModels(Collection);
}


void OGL_UnloadModelsImages(short Collection)
{
	assert_fail(Collection >= 0 && Collection < MAXIMUM_COLLECTIONS, "");
	
	// For wall/sprite images
	OGL_UnloadTextures(Collection);
	
	// For models, skins
	OGL_UnloadModels(Collection);
}


OGL_FogData *OGL_GetFogData(int Type)
{
	return GetMemberWithBounds(FogData,Type,OGL_NUMBER_OF_FOG_TYPES);
}


// XML-parsing stuff
OGL_FogData *OriginalFogData = NULL;

void reset_mml_opengl()
{
	reset_mml_opengl_texture();
	reset_mml_opengl_model();
	reset_mml_opengl_shader();
	
	if (OriginalFogData) {
		for (unsigned i = 0; i < OGL_NUMBER_OF_FOG_TYPES; i++)
			FogData[i] = OriginalFogData[i];
		free(OriginalFogData);
		OriginalFogData = NULL;
	}
}


void parse_mml_opengl(const InfoTree& root)
{
	// back up old values first
	if (!OriginalFogData) {
		OriginalFogData = (OGL_FogData *) malloc(sizeof(OGL_FogData) * OGL_NUMBER_OF_FOG_TYPES);
		assert_fail(OriginalFogData, "");
		for (unsigned i = 0; i < OGL_NUMBER_OF_FOG_TYPES; i++)
			OriginalFogData[i] = FogData[i];
	}

	// texture options / clear, in order
	for (const InfoTree::value_type &v : root)
	{
		if (v.first == "texture")
			parse_mml_opengl_texture(v.second);
		else if (v.first == "txtr_clear")
			parse_mml_opengl_txtr_clear(v.second);
	}
	
	// model data / clear, in order
	for (const InfoTree::value_type &v : root)
	{
		if (v.first == "model")
			parse_mml_opengl_model(v.second);
		else if (v.first == "model_clear")
			parse_mml_opengl_model_clear(v.second);
	}
	
	for (const InfoTree &shader : root.children_named("shader"))
	{
		parse_mml_opengl_shader(shader);
	}
	
	for (const InfoTree &fog : root.children_named("fog"))
	{
		int16 type = 0;
		fog.read_indexed("type", type, OGL_NUMBER_OF_FOG_TYPES);
		OGL_FogData& def = FogData[type];
		
		fog.read_attr("on", def.IsPresent);
		fog.read_attr("depth", def.Depth);
		fog.read_attr("start", def.Start);
		fog.read_attr("landscapes", def.AffectsLandscapes);
		fog.read_attr("mode", def.Mode);
		fog.read_attr("landscape_mix", def.LandscapeMix);
		
		for (const InfoTree &color : fog.children_named("color"))
		{
			color.read_color(def.Color);
		}
	}
}


/* These don't belong here */
void SglColor3f(GLfloat r, GLfloat g, GLfloat b) {
  GLfloat ov[3] = {sRGB_frob(r), sRGB_frob(g), sRGB_frob(b)};
  glColor3fv(ov);
}

void SglColor3fv(const GLfloat* iv) {
  GLfloat ov[3] = {sRGB_frob(iv[0]), sRGB_frob(iv[1]), sRGB_frob(iv[2])};
  glColor3fv(ov);
}

void SglColor3ub(GLubyte r, GLubyte g, GLubyte b) {
  GLfloat ov[3] = {sRGB_frob(r*(1.f/255.f)), sRGB_frob(g*(1.f/255.f)), sRGB_frob(b*(1.f/255.f))};
  glColor3fv(ov);
}

void SglColor3us(GLushort r, GLushort g, GLushort b) {
  GLfloat ov[3] = {sRGB_frob(r*(1.f/65535.f)), sRGB_frob(g*(1.f/65535.f)), sRGB_frob(b*(1.f/65535.f))};
  glColor3fv(ov);
}

void SglColor3usv(const GLushort* iv) {
  GLfloat ov[3] = {sRGB_frob(iv[0]*(1.f/65535.f)), sRGB_frob(iv[1]*(1.f/65535.f)), sRGB_frob(iv[2]*(1.f/65535.f))};
  glColor3fv(ov);
}

void SglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  GLfloat ov[4] = {sRGB_frob(r), sRGB_frob(g), sRGB_frob(b), a};
  glColor4fv(ov);
}

void SglColor4fv(const GLfloat* iv) {
  GLfloat ov[4] = {sRGB_frob(iv[0]), sRGB_frob(iv[1]), sRGB_frob(iv[2]), iv[3]};
  glColor4fv(ov);
}

void SglColor4usv(const GLushort* iv) {
  GLfloat ov[4] = {sRGB_frob(iv[0]*(1.f/65535.f)), sRGB_frob(iv[1]*(1.f/65535.f)), sRGB_frob(iv[2]*(1.f/65535.f)), iv[3]*(1.f/65535.f)};
  glColor4fv(ov);
}

