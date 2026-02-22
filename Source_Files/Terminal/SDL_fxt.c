/*
 SDL_fxt.cpp modified from SDL_fox.c
 
 Original copyright below
 */


/******************************************************************************
 *    SDL_fox
 *******************
 * Font rendering library for Simple Direct Media Layer 2.x (SDL2).
 * Copyright (c) 2016-2022 Niklas Benfer <https://github.com/palomena>
 *
 * Dependencies:
 *    freetype2 (https://freetype.org/)
 *    SDL2 (https://www.libsdl.org/)
 *
 * License: MIT License (see ../LICENSE.txt)
 *****************************************************************************/

#include "SDL_fxt.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#include <ctype.h>
#include <stdbool.h>


/******************************************************************************
 * SDL_fxt library state and initialization
 *****************************************************************************/

static enum FXT_LibraryState FXT_state = FXT_UNINITIALIZED;

static FT_Library libfreetype = NULL;

enum FXT_LibraryState FXT_WasInit(void) {
	return FXT_state;
}

enum FXT_LibraryState FXT_Init(void) {
	if(!FXT_WasInit()) {
		if(!SDL_WasInit(0)) return FXT_state;
		if(FT_Init_FreeType(&libfreetype)) return FXT_state;
		FXT_state = FXT_INITIALIZED;
	}

	return FXT_state;
}

void FXT_Exit(void) {
	if(FXT_WasInit()) {
		FT_Done_FreeType(libfreetype);
		FXT_state = FXT_UNINITIALIZED;
	}
}

/******************************************************************************
 * UTF-8 handling
 *****************************************************************************/

typedef struct {
	unsigned char mask;
	unsigned char lead;
	int bits_stored;
} FXT_Utf8;

static FXT_Utf8 *FXT_utf[] = {
	&(FXT_Utf8){0x3f, 0x80, 6},
	&(FXT_Utf8){0x7f,    0, 7},
	&(FXT_Utf8){0x1f, 0xc0, 5},
	&(FXT_Utf8){0xf,  0xe0, 4},
	&(FXT_Utf8){0x7,  0xf0, 3},
	NULL
};

static int FXT_Utf8Length(const unsigned char ch) {
	int len = 0;

	for(FXT_Utf8 **u = FXT_utf; *u; ++u) {
		if((ch & ~(*u)->mask) == (*u)->lead) {
			break;
		}
		++len;
	}
	if(len > 4) { /* Malformed leading byte */
		return -1;
	}
	return len;
}

static Uint32 FXT_Utf8Decode(const Uint8* sequence, const Uint8** endptr) {
	int bytes = FXT_Utf8Length(*sequence);
	int shift = FXT_utf[0]->bits_stored * (bytes - 1);
	Uint32 codep = (*sequence++ & FXT_utf[bytes]->mask) << shift;

	for(int i = 1; i < bytes; ++i, ++sequence) {
		shift -= FXT_utf[0]->bits_stored;
		codep |= ((char)*sequence & FXT_utf[0]->mask) << shift;
	}

	*endptr = sequence-1;
	return(codep);
}

/******************************************************************************
 * Font definition and open/close
 *****************************************************************************/

struct FXT_Font {
	//SDL_Renderer *renderer;
	//SDL_Texture *atlas;
    SDL_Surface *atlas;
	FXT_GlyphMetrics *metrics;
	FT_Face face;	/* freetype font face */
	int length;		/* side length of the atlas texture (sqrt(width^2)) */
	FXT_FontMetrics size;
	SDL_bool use_kerning;
};


static SDL_Surface* FXT_RenderFontToSurface(FXT_Font *font);

FXT_Font* FXT_OpenFont(const char *path, int size) {
	FXT_Font *font = SDL_calloc(1, sizeof(*font));

	/* Open the font file using libfreetype */
	if(FT_New_Face(libfreetype, path, 0, &font->face)) {
		goto abort0;
	}

	/* Set the pixel size for rendering characters */
	if(FT_Set_Pixel_Sizes(font->face, size, size)) {
		goto abort1;
	}

	/* Calculate atlas surface dimensions */
	int length = (int)SDL_ceil(SDL_sqrt(font->face->num_glyphs));

	/* Set font parameters */
	font->length = length;
	font->size.ptsize = size;
	font->size.height = (int)font->face->size->metrics.height >> 6;
	font->use_kerning = FT_HAS_KERNING(font->face);

	/* Render characters to surface */
    font->atlas = FXT_RenderFontToSurface(font);
	if(!font->atlas) goto abort1;

	return font;

	/* Premature error handling */
	abort1:
		FT_Done_Face(font->face);
	abort0:
		SDL_free(font);
		return NULL;
}

void FXT_CloseFont(FXT_Font *font) {
	SDL_FreeSurface(font->atlas);
	FT_Done_Face(font->face);
	SDL_free(font->metrics);
	SDL_free(font);
}

/*****************************************************************************/

static void FXT_SetMetrics(FXT_Font *font, Uint32 index, int xpos, int ypos) {
	font->metrics[index].rect.x = xpos * font->size.ptsize;
	font->metrics[index].rect.y = ypos * font->size.ptsize;
	font->metrics[index].rect.w = (int)font->face->glyph->metrics.width >> 6;
	font->metrics[index].rect.h = (int)font->face->glyph->metrics.height >> 6;
	font->metrics[index].bearing.x = (int)font->face->glyph->metrics.horiBearingX >> 6;
	font->metrics[index].bearing.y = (int)font->face->glyph->metrics.horiBearingY >> 6;
	font->metrics[index].advance = (int)font->face->glyph->metrics.horiAdvance >> 6;
	if(font->size.max_width < font->metrics[index].rect.w) {
		font->size.max_width = font->metrics[index].rect.w;
	}
	if(font->size.max_height < font->metrics[index].rect.h) {
		font->size.max_height = font->metrics[index].rect.h;
	}
	if(font->size.max_advance < font->metrics[index].advance) {
		font->size.max_advance = font->metrics[index].advance;
	}
}

SDL_Surface* FXT_RenderFontToSurface(FXT_Font *font) {
	/* Allocate SDL surface */
	int width = font->length * font->size.ptsize;
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, width,
												32, SDL_PIXELFORMAT_RGBA32);
	if(!surface) return NULL;

	/* Allocate glyph metrics array */
	font->metrics = SDL_malloc(sizeof(*font->metrics) * font->face->num_glyphs);
	if(!font->metrics) {
		SDL_FreeSurface(surface);
		return NULL;
	}

	FT_UInt index;
	int xpos = 0;
	int ypos = 0;

	for(FT_ULong charcode = FT_Get_First_Char(font->face, &index);
		index != 0;
		charcode = FT_Get_Next_Char(font->face, charcode, &index)
	) {
		if(xpos < (font->length - 1)) {
			xpos++;
		} else {
			xpos = 0;
			ypos++;
		}

		FT_Load_Char(font->face, charcode, FT_LOAD_RENDER);
		FT_Bitmap *bitmap = &font->face->glyph->bitmap;
		if(bitmap->pixel_mode != ft_pixel_mode_grays) {
			break;
		}

		FXT_SetMetrics(font, index, xpos, ypos);

		int xreal = xpos * font->size.ptsize;
		int yreal = ypos * font->size.ptsize;
		for(int y = 0; y < bitmap->rows; y++) {
			for(int x = 0; x < bitmap->width; x++) {
				int index = (yreal + y) * surface->w + xreal + x;
				Uint32 *pixel = &((Uint32*)surface->pixels)[index];
				Uint8 alpha = bitmap->buffer[y * bitmap->pitch + x];
				*pixel = SDL_MapRGBA(surface->format, 255, 255, 255, alpha);
			}
		}
	}

	return surface;
}

/******************************************************************************
 * Font rendering
 *****************************************************************************/

static const Uint8* skip_whitespace(const Uint8 *text) {
	while(SDL_isspace(*text)) text++;
	return text;
}


static bool can_break_after(const uint8_t* ch)
{
    switch (ch[0]) // determined empirically on my PowerBook
    {
        case '&':
        case '*':
        case '+':
        case '-':
        case '\\':
        case '<':
        case '=':
        case '>':
        case '/':
        case '^':
        case '|':
            return true;
        default:
            /*
             * from libtextwrap - Text-Wrapping Library with I18N
             * Copyright (C) 2003 by Tomohiro KUBOTA <kubota@debian.org>
             */
            switch (mblen((char*)ch, MB_CUR_MAX))
            {
                case 3: // U+0800 - U+FFFF
                {
                    uint32_t u = (*ch&0x0f)*0x1000 + (*(ch+1)&0x3f)*0x40 + (*(ch+2)&0x3f);
                    if (u >= 0x3000 && u <= 0x312f)
                    {
                        return !(u == 0x300a || u == 0x300c || u == 0x300e || u == 0x3010
                                 || u == 0x3014 || u == 0x3016 || u == 0x3018 || u == 0x301a);
                    }
                    else // CJK punctuations, Hiragana, Katakana, Bopomofo
                    {
                        return ((u >= 0x31a0 && u <= 0x31bf)      // Bopomofo */
                                || (u >= 0x31f0 && u <= 0x31ff)   // Katakana extension */
                                || (u >= 0x3400 && u <= 0x9fff)   // Han Ideogram */
                                || (u >= 0xf900 && u <= 0xfaff)); // Han Ideogram */
                    }
                }
                case 4: // U+10000 - U+1FFFFF
                {
                    uint32_t u = (*ch&7)*0x40000 + (*(ch+1)&0x3f)*0x1000 + (*(ch+2)&0x3f)*0x40 + (*(ch+3)&0x3f);
                    return (u >= 0x20000 && u <= 0x2ffff);  /* Han Ideogram */
                }
                default:
                {}
            }
            return false;
    }
}


static SDL_bool FXT_NextWordFitsOnLine(FXT_Font *font, const Uint8 *text, const SDL_Point *position, int maxX)
{
	SDL_Point cursor = *position;
	Uint32 previous_ch = 0;
	for(text = skip_whitespace(text); *text; text++) {
		Uint32 ch = FXT_Utf8Decode(text, &text);
        
        
		if(ch == '\n' || ch == '\t' || ch == ' ' || ch == '\r') {
			break;
		} else {
			cursor.x += FXT_GetAdvance(font, ch, previous_ch);
			previous_ch = ch;
		}

		if(cursor.x >= maxX) return SDL_FALSE;
        
        if (can_break_after(text)) break;
	}

	return SDL_TRUE;
}

static int FXT_RenderLine(SDL_Surface *dst_surface, FXT_Font *font, const Uint8 *text, const Uint8 **endptr, const SDL_Point *position, int width, int n)
{
	int maxX = position->x + width;
	SDL_Point cursor = *position;
	Uint32 previous_ch = 0;
	SDL_bool unsafe = SDL_TRUE;

	/* Skip any whitespace at the beginning of a new line */
	for(text = skip_whitespace(text); *text; text++) {
		if(n == 0) break;

		/* We shall not exceed the line width by printing the next char. */
		if(unsafe && ((cursor.x + font->size.ptsize) >= maxX)) {
			*endptr = text;
			break;
		}

		Uint32 ch = FXT_Utf8Decode(text, &text);
		if(ch == '\n') {
			continue;
		} else {
			cursor.x += FXT_RenderChar(dst_surface, font, ch, previous_ch, &cursor);
			previous_ch = ch;
			if(ch == ' ') {
				if(!FXT_NextWordFitsOnLine(font, text, &cursor, maxX)) {
					unsafe = SDL_TRUE;
					break;
				} else unsafe = SDL_FALSE;
			}
		}

		if(n > 0) n--;
	}

	*endptr = text;
	return n;
}

/*****************************************************************************/

void FXT_SetColor(FXT_Font *font, SDL_Color color)
{
    SDL_SetSurfaceColorMod(font->atlas, color.r, color.g, color.b);
}


int FXT_RenderChar(SDL_Surface *dst_surface, FXT_Font *font, Uint32 ch, Uint32 previous_ch, const SDL_Point *position)
{
	int advance = 0;
	const FXT_GlyphMetrics *metrics = FXT_QueryGlyphMetrics(font, ch);
	if(metrics) {
		SDL_Rect dstrect;
		SDL_Color color;

		dstrect.x = position->x;
		dstrect.y = position->y - metrics->bearing.y + font->size.height;
		dstrect.w = metrics->rect.w;
		dstrect.h = metrics->rect.h;

		if(previous_ch) {
			advance += FXT_GetKerningOffset(font, ch, previous_ch);
			dstrect.x += advance;
		}

        SDL_BlitSurface(font->atlas, &metrics->rect, dst_surface, &dstrect);
		advance += metrics->advance;
	}

	return advance;
}


int FXT_RenderTextInside(SDL_Surface *dst_surface, FXT_Font *font, const Uint8 *text, const Uint8 **endptr, const SDL_Rect *rect, int n)
{
	int state = 0;
	unsigned linesAvailable = rect->h / font->size.height;
	if(linesAvailable == 0) {
		return -1;
	}

	#ifdef FXT_DEBUG
	{
		SDL_Color rc;
		SDL_GetRenderDrawColor(font->renderer, &rc.r, &rc.g, &rc.b, &rc.a);
		SDL_SetRenderDrawColor(font->renderer, 255, 255, 255, 255);
		SDL_RenderDrawRect(font->renderer, rect);
		SDL_SetRenderDrawColor(font->renderer, rc.r, rc.g, rc.b, rc.a);
	}
	#endif

	SDL_Point cursor = {rect->x, rect->y};
	for(unsigned line = 0; line < linesAvailable; line++) {
		n = FXT_RenderLine(dst_surface, font, text, &text, &cursor, rect->w, n);
		if(n == 0) {
			state = 1;
			break;
		}

		cursor.x = rect->x;
		cursor.y += font->size.height;
	}

	if(*text != '\0') {
		*endptr = text;
		return state + 1;
	} else {
		*endptr = NULL;
		return 0;
	}
}


/******************************************************************************
 * Font metrics and glyph dimensions interface
 *****************************************************************************/

const FXT_GlyphMetrics* FXT_QueryGlyphMetrics(FXT_Font *font, Uint32 ch) {
	const FXT_GlyphMetrics *metrics = NULL;
	FT_UInt glyph_index = FT_Get_Char_Index(font->face, ch);
	if(glyph_index != 0) {
		metrics = &font->metrics[glyph_index];
	}

	return metrics;
}

int FXT_GetKerningOffset(FXT_Font *font, Uint32 ch, Uint32 previous_ch) {
	int offset = 0;

	if(font->use_kerning) {
		FT_UInt glyph_index = FT_Get_Char_Index(font->face, ch);
		FT_UInt previous_glyph_index = FT_Get_Char_Index(font->face,
														previous_ch);
		if(glyph_index && previous_glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(font->face, previous_glyph_index, glyph_index,
											FT_KERNING_DEFAULT, &delta);
			offset = (int)delta.x >> 6;
		}
	}

	return offset;
}

int FXT_GetAdvance(FXT_Font *font, Uint32 ch, Uint32 previous_ch) {
	int advance = 0;
	const FXT_GlyphMetrics *metrics = FXT_QueryGlyphMetrics(font, ch);
	if(metrics) {
		advance += metrics->advance;
		advance += FXT_GetKerningOffset(font, ch, previous_ch);
	}
	return advance;
}

void FXT_EnableKerning(FXT_Font *font, SDL_bool enable) {
	font->use_kerning = enable && FT_HAS_KERNING(font->face);
}

const FXT_FontMetrics* FXT_QueryFontMetrics(FXT_Font *font) {
	return &font->size;
}
