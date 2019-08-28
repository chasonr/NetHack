/* sdl2posbar.c */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2posbar.h"
#include "sdl2font.h"
#include "sdl2map.h"
#include "sdl2window.h"

struct PosbarPrivate {
    char *m_data;
    struct SDL2Window *m_map_window;
};

static void sdl2_posbar_create(struct SDL2Window *win);
static void sdl2_posbar_destroy(struct SDL2Window *win);
static void sdl2_posbar_redraw(struct SDL2Window *win);

struct SDL2Window_Methods const sdl2_posbar_procs = {
    sdl2_posbar_create,
    sdl2_posbar_destroy,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_posbar_redraw
};

static void
sdl2_posbar_create(struct SDL2Window *win)
{
    struct PosbarPrivate *data = (struct PosbarPrivate *) alloc(sizeof(*data));
    win->data = data;

    data->m_map_window = NULL;
    data->m_data = NULL;
    win->m_visible = TRUE;

    /* Map window font */
    sdl2_window_set_font(win, iflags.wc_font_map, iflags.wc_fontsiz_map,
            sdl2_font_defaultMonoFont, 20);
}

void
sdl2_posbar_set_map_window(struct SDL2Window *win,
                           struct SDL2Window *map_window)
{
    struct PosbarPrivate *data = (struct PosbarPrivate *) win->data;

    data->m_map_window = map_window;
}

static void
sdl2_posbar_destroy(struct SDL2Window *win)
{
    struct PosbarPrivate *data = (struct PosbarPrivate *) win->data;

    free(data->m_data);
    free(data);
}

static void
sdl2_posbar_redraw(struct SDL2Window *win)
{
    static const SDL_Color black = {   0,   0,   0, 255 };
    static const SDL_Color brown = {  96,  32,   0, 255 };
    static const SDL_Color white = { 255, 255, 255, 255 };
    static const SDL_Color clear = {   0,   0,   0,   0 };

    struct PosbarPrivate *data = (struct PosbarPrivate *) win->data;
    int xpos, xsize, xleft, xright;
    SDL_Rect rect;
    unsigned i;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;

    if (data->m_data == NULL) { return; }

    /* Position of window with respect to map */
    xpos = -sdl2_map_xpos(data->m_map_window);
    xsize = sdl2_map_width(data->m_map_window);
    if (xpos < 0) {
        /* Screen is wider than map */
        xleft = 0;
        xright = width;
    } else {
        /* Map is at least as wide as screen */
        xleft = xpos * width / xsize;
        xright = (xpos + width) * width / xsize;
    }
    rect.y = 0;
    rect.h = height;
    if (xleft > 0) {
        rect.x = 0;
        rect.w = xleft;
        sdl2_window_fill(win, &rect, black);
    }
    rect.x = xleft;
    rect.w = xright - xleft;
    sdl2_window_fill(win, &rect, brown);
    if (xright < width) {
        rect.x = xright;
        rect.w = width - xright;
        sdl2_window_fill(win, &rect, black);
    }

    for (i = 0; data->m_data[i + 0] != '\0' && data->m_data[i + 1] != '\0';
            i += 2) {
        Uint32 glyph;
        int pos;
        int x;

        glyph = (unsigned char) (data->m_data[i + 0]);
        pos = (unsigned char) (data->m_data[i + 1]);
        if (pos < 1 || pos >= COLNO) { continue; }
        x = pos * width / COLNO;
        sdl2_window_renderCharBG(win, glyph, x, 0, white, clear);
    }
}

int
sdl2_posbar_height_hint(struct SDL2Window *win)
{
    return win->m_line_height;
}

void
sdl2_posbar_update(struct SDL2Window *win, const char *features)
{
    struct PosbarPrivate *data = (struct PosbarPrivate *) win->data;

    free(data->m_data);
    data->m_data = dupstr(features);
}

void
sdl2_posbar_resize(struct SDL2Window *win, int x1, int y1, int x2, int y2)
{
    sdl2_window_resize(win, x1, y1, x2, y2);
}
