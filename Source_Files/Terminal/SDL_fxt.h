/******************************************************************************
 *    SDL_fxt
 *******************
 * Font rendering library for Simple Direct Media Layer 2.x (SDL2).
 * Copyright (c) 2016-2022 Niklas Benfer <https://github.com/palomena>
 *
 * Dependencies:
 *    freetype2 (https://freetype.org/)
 *    fontconfig (https://www.freedesktop.org/wiki/Software/fontconfig/)
 *    SDL2 (https://www.libsdl.org/)
 *
 * License: MIT License (see ../LICENSE.txt)
 *****************************************************************************/

#ifndef SDL_FXT_H
#define SDL_FXT_H

#include <SDL2/SDL.h>
#include <SDL2/begin_code.h>

/* set up for c function definitions, even when using c++ */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * SDL_fxt library state and initialization
 *****************************************************************************/

/* SDL_fxt library initialization state */
enum FXT_LibraryState {
	FXT_UNINITIALIZED,
	FXT_INITIALIZED
};

/* Initializes the SDL_fxt library 
 * If the library was initialized previously, then nothing is done. */
extern DECLSPEC enum FXT_LibraryState SDLCALL FXT_Init(void);

/* Deinitializes the SDL_fxt library
 * Nothing is done if SDL_fxt was not previously initialized.
 */
extern DECLSPEC void SDLCALL FXT_Exit(void);

/* Returns whether SDL_fxt has been initialzed. */
extern DECLSPEC enum FXT_LibraryState SDLCALL FXT_WasInit(void);

/******************************************************************************
 * Font definition and open/close
 *****************************************************************************/

/* Internal font representation */
typedef struct FXT_Font FXT_Font;

/* Opens a font via a file-path and specified font parameters. */
extern DECLSPEC FXT_Font* SDLCALL FXT_OpenFont(const char *path, int size);

/* build option to enable fontconfig */
#ifdef FXT_USE_FONTCONFIG

/* Opens a font using a fontconfig string. */
extern DECLSPEC FXT_Font* SDLCALL FXT_OpenFontFc(const unsigned char *fontstr);

#endif /* FXT_USE_FONTCONFIG */

/* Closes a previously opened font via its handle. */
extern DECLSPEC void SDLCALL FXT_CloseFont(FXT_Font *font);

/******************************************************************************
 * Font rendering
 *****************************************************************************/

void FXT_SetColor(FXT_Font *font, SDL_Color color);


/* Renders a character at the given position. */
extern DECLSPEC int SDLCALL FXT_RenderChar(SDL_Surface *draw_surface, FXT_Font *font, Uint32 ch, Uint32 previous_ch, const SDL_Point *position);


enum FXT_PrintState {
	FXT_ERROR = -1,
	FXT_DONE_PRINTING,
	FXT_NEXT_PAGE,
	FXT_MORE_TEXT
};

/* Renders a utf-8 string of text inside the given rect. */
extern DECLSPEC int SDLCALL FXT_RenderTextInside(SDL_Surface *dst_surface, FXT_Font *font, const Uint8 *text, const Uint8 **endptr, const SDL_Rect *rect, int n);


/******************************************************************************
 * Font metrics and glyph dimensions interface
 *****************************************************************************/

/* Specifies the metrics of a character glyph. */
typedef struct {
	SDL_Rect rect;
	SDL_Point bearing;
	int advance;
} FXT_GlyphMetrics;

/* Queries the glyph metrics for a given character. */
extern DECLSPEC const FXT_GlyphMetrics* SDLCALL
FXT_QueryGlyphMetrics(FXT_Font *font, Uint32 ch);

/* Get the x-axis kerning offset for a given character combination. */
extern DECLSPEC int SDLCALL FXT_GetKerningOffset(FXT_Font *font,
								Uint32 ch, Uint32 previous_ch);

/* Get the total x-axis advance spacing for a given character combination. */
extern DECLSPEC int SDLCALL FXT_GetAdvance(FXT_Font *font,
							Uint32 ch, Uint32 previous_ch);

/* Enable/Disable kerning for the specified font. */
extern DECLSPEC void SDLCALL FXT_EnableKerning(FXT_Font *font,
												SDL_bool enable);

/* Specifies the metrics of the font */
typedef struct {
	int height;
	int ptsize;
	int max_width;
	int max_height;
	int max_advance;
} FXT_FontMetrics;

/* Queries the font metrics */
extern DECLSPEC const FXT_FontMetrics* SDLCALL
FXT_QueryFontMetrics(FXT_Font *font);

/* end c function definitions when using c++ */
#ifdef __cplusplus
}
#endif

#include <SDL2/close_code.h>

#endif /* SDL_FXT_H */
