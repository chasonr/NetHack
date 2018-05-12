/* sdl2message.h */

#ifndef WIN_SDL2_SDL2MESSAGE_H
#define WIN_SDL2_SDL2MESSAGE_H

#include "sdl2.h"
#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_message_procs;
extern int sdl2_message_width_hint(struct SDL2Window *window);
extern int sdl2_message_height_hint(struct SDL2Window *window);
extern void sdl2_message_resize(struct SDL2Window *window,
        int x1, int y1, int x2, int y2);
extern void sdl2_message_more(struct SDL2Window *window);
extern void sdl2_message_new_turn(struct SDL2Window *window);
extern void sdl2_message_previous(struct SDL2Window *window);

#endif
