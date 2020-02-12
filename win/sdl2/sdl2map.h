/* sdl2map.h */

#ifndef WIN_SDL2_SDL2MAP_H
#define WIN_SDL2_SDL2MAP_H

#include "sdl2.h"
#include "sdl2window.h"

extern struct SDL2Window_Methods const sdl2_map_procs;
extern int sdl2_map_height_hint(struct SDL2Window *window);
extern void sdl2_map_resize(struct SDL2Window *window,
        int x1, int y1, int x2, int y2);
extern void sdl2_map_cliparound(struct SDL2Window *window, int x, int y);
extern void sdl2_map_toggle_tile_mode(struct SDL2Window *window);
extern void sdl2_map_next_zoom_mode(struct SDL2Window *window);
extern boolean sdl2_map_mouse(struct SDL2Window *window, int x_in, int y_in,
        int *x_out, int *y_out);
extern int sdl2_map_xpos(struct SDL2Window *window);
extern int sdl2_map_width(struct SDL2Window *window);
extern void sdl2_map_update_cursor(struct SDL2Window *window,
            int index, genericptr_t ptr, int chg, int percent, int color,
            const unsigned long *colormasks);

#endif