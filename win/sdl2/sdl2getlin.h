/* sdl2getlin.h */

#ifndef WIN_SDL2_SDL2GETLIN_H
#define WIN_SDL2_SDL2GETLIN_H

#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_getline_procs;
extern boolean sdl2_getline_run(struct SDL2Window *window,
        const char *prompt, char *buf, size_t bufsize);

extern int sdl2_extcmd_run(struct SDL2Window *window);

#endif
