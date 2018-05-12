/* SDL2Font.h */

#ifndef WIN_SDL2_SDL2FONT_H
#define WIN_SDL2_SDL2FONT_H

#include "sdl2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Incomplete type describing an SDL2 font */
/* Implementation will vary according to the system API in use */
typedef struct SDL2Font_Impl SDL2Font;

extern SDL2Font *FDECL(sdl2_font_new, (const char *name, int ptsize));
extern void FDECL(sdl2_font_free, (SDL2Font *font));

/* Font metrics */
extern int FDECL(sdl2_font_ascent, (SDL2Font *font));
extern int FDECL(sdl2_font_descent, (SDL2Font *font));
extern int FDECL(sdl2_font_lineSkip, (SDL2Font *font));
extern int FDECL(sdl2_font_height, (SDL2Font *font));

/* Text rendering */
/* If no background is given, background is transparent */
extern SDL_Surface *FDECL(sdl2_font_renderChar, (SDL2Font *font, Uint32 ch, SDL_Color foreground));
extern SDL_Surface *FDECL(sdl2_font_renderStr, (SDL2Font *font, const char *text, SDL_Color foreground));
extern SDL_Surface *FDECL(sdl2_font_renderCharBG, (SDL2Font *font, Uint32 ch, SDL_Color foreground, SDL_Color background));
extern SDL_Surface *FDECL(sdl2_font_renderStrBG, (SDL2Font *font, const char *text, SDL_Color foreground, SDL_Color background));

/* Text extent */
extern SDL_Rect FDECL(sdl2_font_textSizeChar, (SDL2Font *font, Uint32 ch));
extern SDL_Rect FDECL(sdl2_font_textSizeStr, (SDL2Font *font, const char *text));

/* Default font names */
extern const char sdl2_font_defaultMonoFont[];
extern const char sdl2_font_defaultSerifFont[];
extern const char sdl2_font_defaultSansFont[];

#ifdef __cplusplus
}
#endif

#endif
