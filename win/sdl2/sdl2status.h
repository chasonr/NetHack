/* sdl2status.h */

#ifndef WIN_SDL2_SDL2STATUS_H
#define WIN_SDL2_SDL2STATUS_H

#include "sdl2.h"
#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_status_procs;
extern int sdl2_status_height_hint(struct SDL2Window *window);
extern int sdl2_status_width_hint(struct SDL2Window *window);
extern void sdl2_status_resize(struct SDL2Window *window,
        int left, int top, int right, int bottom);
extern void sdl2_status_do_enablefield(struct SDL2Window *win,
            int index, const char *name, const char *format,
            boolean enable);
extern void sdl2_status_do_update(struct SDL2Window *window,
            int index, genericptr_t ptr, int chg, int percent, int color,
            const unsigned long *colormasks);

#endif
