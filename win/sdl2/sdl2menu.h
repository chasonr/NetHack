/* sdl2menu.h */

#ifndef WIN_SDL2_SDL2MENU_H
#define WIN_SDL2_SDL2MENU_H

#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_menu_procs;

extern void FDECL(sdl2_menu_startMenu, (struct SDL2Window *win));
extern void FDECL(sdl2_menu_addMenu, (struct SDL2Window *win, int glyph,
        const anything* identifier, CHAR_P ch, CHAR_P gch, int attr,
        const char *str, BOOLEAN_P preselected));
extern void FDECL(sdl2_menu_endMenu, (struct SDL2Window *win,
        const char *prompt));
extern int  FDECL(sdl2_menu_selectMenu, (struct SDL2Window *win, int how,
        menu_item ** menu_list));

#endif
