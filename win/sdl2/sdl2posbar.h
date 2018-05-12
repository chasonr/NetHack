/* sdl2posbar.h */

#ifndef WIN_SDL2_SDL2POSBAR_H
#define WIN_SDL2_SDL2POSBAR_H

#include "sdl2.h"
#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_posbar_procs;
extern void FDECL(sdl2_posbar_set_map_window, (struct SDL2Window *window,
        struct SDL2Window *map_window));
extern int FDECL(sdl2_posbar_height_hint, (struct SDL2Window *window));
extern void FDECL(sdl2_posbar_resize, (struct SDL2Window *window,
        int left, int top, int right, int bottom));
extern void FDECL(sdl2_posbar_update, (struct SDL2Window *window,
        const char *features));

#endif
