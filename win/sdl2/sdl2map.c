/* sdl2map.cpp */

#include "hack.h"
#include "tileset.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2window.h"
#include "sdl2map.h"
#include "sdl2font.h"

extern short glyph2tile[];
extern int total_tiles_used;

struct Glyph {
    /* The background glyph for this position */
    int bkglyph;

    /* The foreground glyph for this position */
    int glyph;

    /* Zap effect */
    int zap_glyph;

    /* Explosion effect */
    int expl_glyph;
    clock_t expl_timer;
};

enum ZoomMode {
    SDL2_ZOOMMODE_NORMAL,
    SDL2_ZOOMMODE_FULLSCREEN,
    SDL2_ZOOMMODE_HORIZONTAL,
    SDL2_ZOOMMODE_VERTICAL
};

struct MapPrivate {
    struct Glyph m_map[ROWNO][COLNO];
    boolean m_tile_mode;
    int m_text_w, m_text_h;
    int m_tile_w, m_tile_h;
    int m_scroll_x, m_scroll_y;
    int m_cursor_x, m_cursor_y;

    SDL_Surface **m_tiles;
    int m_tilemap_w, m_tilemap_h;

    SDL_Surface *m_map_image;

    enum ZoomMode m_zoom_mode;

    int cursor_color;
};

static void sdl2_map_create(struct SDL2Window *win);
static void sdl2_map_destroy(struct SDL2Window *win);
static void sdl2_map_clear(struct SDL2Window *win);
static void sdl2_map_setCursor(struct SDL2Window *win, int x, int y);
static void sdl2_map_printGlyph(struct SDL2Window *win,
        xchar x, xchar y, int glyph, int bkglyph);
static void sdl2_map_redraw(struct SDL2Window *win);

static void sdl2_map_setup(struct SDL2Window *win);
static void sdl2_map_draw(struct SDL2Window *win, int x, int y,
        boolean cursor);
static boolean sdl2_map_wall_draw(struct SDL2Window *win,
        Uint32 ch, const SDL_Rect *rect, SDL_Color color);
static SDL_Surface *convert_tile(int);

struct SDL2Window_Methods const sdl2_map_procs = {
    sdl2_map_create,
    sdl2_map_destroy,
    sdl2_map_clear,
    NULL,
    sdl2_map_setCursor,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_map_printGlyph,
    sdl2_map_redraw
};

static void
sdl2_map_create(struct SDL2Window *win)
{
    SDL_Rect rect;
    struct MapPrivate *data = (struct MapPrivate *)
            alloc(sizeof(struct MapPrivate));
    memset(data, 0, sizeof(*data));
    win->data = data;

    data->m_tile_mode = iflags.wc_tiled_map;
    data->m_tilemap_w = iflags.wc_tile_width;
    data->m_tilemap_h = iflags.wc_tile_height;
    data->m_zoom_mode = SDL2_ZOOMMODE_NORMAL;

    /* Map window font */
    sdl2_window_set_font(win,
            iflags.wc_font_map, iflags.wc_fontsiz_map,
            sdl2_font_defaultMonoFont, 20);

    rect = sdl2_font_textSizeStr(win->m_font, "M");
    data->m_text_w = rect.w;
    data->m_text_h = rect.h;

    /* Tile map file */
    if (read_tiles(iflags.wc_tile_file, TRUE)) {
        int i;
        data->m_tiles = (SDL_Surface **) alloc(sizeof(SDL_Surface *)
                                         * total_tiles_used);
        for (i = 0; i < total_tiles_used; ++i) {
            data->m_tiles[i] = NULL;
        }
    } else {
        data->m_tile_mode = FALSE;
    }
    data->m_tilemap_w = iflags.wc_tile_width;
    data->m_tilemap_h = iflags.wc_tile_height;

    /* Cells are initially unstretched */
    data->m_tile_w = data->m_tile_mode ? data->m_tilemap_w : data->m_text_w;
    data->m_tile_h = data->m_tile_mode ? data->m_tilemap_h : data->m_text_h;

    data->m_map_image = SDL_CreateRGBSurface(
            SDL_SWSURFACE,
            data->m_tile_w * COLNO,
            data->m_tile_h * ROWNO,
            32,
            0x000000FF,  /* red */
            0x0000FF00,  /* green */
            0x00FF0000,  /* blue */
            0xFF000000); /* alpha */

    sdl2_map_clear(win);
}

static void
sdl2_map_destroy(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;
    int i;
    if (data->m_tiles) {
        for (i = 0; i < total_tiles_used; ++i) {
            if (data->m_tiles[i]) {
                SDL_FreeSurface(data->m_tiles[i]);
            }
        }
        free(data->m_tiles);
    }
    if (data->m_map_image) { SDL_FreeSurface(data->m_map_image); }
    free(data);
    win->data = NULL;
}

void
sdl2_map_resize(struct SDL2Window *win, int x1, int y1, int x2, int y2)
{
    sdl2_window_resize(win, x1, y1, x2, y2);
    sdl2_map_setup(win);
}

static void
sdl2_map_redraw(struct SDL2Window *win)
{
    static const SDL_Color background = {  96,  32,   0, 255 };
    struct MapPrivate *data = (struct MapPrivate *) win->data;
    SDL_Rect srect, drect;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;
    unsigned x, y;

    /* Effects may spontaneously change the map. Update them here. */
    for (y = 0; y < ROWNO; ++y) {
        for (x = 0; x < COLNO; ++x) {
            if (data->m_map[y][x].zap_glyph != NO_GLYPH
            ||  data->m_map[y][x].expl_timer != 0) {
                sdl2_map_draw(win, x, y, TRUE);
            }
        }
    }

    /* Draw the map image */
    if (data->m_scroll_x < 0) {
        srect.x = -data->m_scroll_x;
        drect.x = 0;
        srect.w = data->m_map_image->w + data->m_scroll_x;
    } else {
        srect.x = 0;
        drect.x = data->m_scroll_x;
        srect.w = data->m_map_image->w;
    }
    if (srect.w > width) {
        srect.w = width;
    }
    drect.w = srect.w;
    if (data->m_scroll_y < 0) {
        srect.y = -data->m_scroll_y;
        drect.y = 0;
        srect.h = data->m_map_image->h + data->m_scroll_y;
    } else {
        srect.y = 0;
        drect.y = data->m_scroll_y;
        srect.h = data->m_map_image->h;
    }
    if (srect.h > height) {
        srect.h = height;
    }
    drect.h = srect.h;
    sdl2_window_blit(win, &drect, data->m_map_image, &srect);

    /* Erase above the map image */
    drect.x = 0;
    drect.y = 0;
    drect.w = width;
    drect.h = data->m_scroll_y;
    if (drect.h > 0) {
        sdl2_window_fill(win, &drect, background);
    }

    /* Erase below the map image */
    drect.y = data->m_scroll_y + data->m_map_image->h;
    drect.h = height - drect.y;
    if (drect.h > 0) {
        sdl2_window_fill(win, &drect, background);
    }

    /* Erase left of the map image */
    drect.x = 0;
    drect.y = data->m_scroll_y;
    drect.w = data->m_scroll_x;
    drect.h = data->m_map_image->h;
    if (drect.w > 0) {
        sdl2_window_fill(win, &drect, background);
    }

    /* Erase right of the map image */
    drect.x = data->m_scroll_x + data->m_map_image->w;
    drect.w = width - drect.x;
    if (drect.w > 0) {
        sdl2_window_fill(win, &drect, background);
    }
}

void
sdl2_map_clear(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;
    int background_glyph = cmap_to_glyph(S_stone);
    unsigned x, y;
    SDL_Rect rect1, rect2;

    for (y = 0; y < ROWNO; ++y) {
        for (x = 0; x < COLNO; ++x) {
            data->m_map[y][x].bkglyph = background_glyph;
            data->m_map[y][x].glyph = background_glyph;
            data->m_map[y][x].zap_glyph = NO_GLYPH;
            data->m_map[y][x].expl_glyph = NO_GLYPH;
            data->m_map[y][x].expl_timer = 0;
        }
    }

    /* This is faster than calling mapDraw on each cell */
    sdl2_map_draw(win, 0, 0, FALSE);
    rect1.x = 0;
    rect1.y = 0;
    rect1.w = data->m_tile_w;
    rect1.h = data->m_tile_h;
    rect2.w = rect1.w;
    rect2.h = rect1.h;
    for (y = 0; y < ROWNO; ++y) {
        rect2.y = y * rect2.h;
        for (x = 0; x < COLNO; ++x) {
            rect2.x = x * rect2.w;
            SDL_BlitSurface(data->m_map_image, &rect1,
                            data->m_map_image, &rect2);
        }
    }

    if (win->m_visible) { sdl2_redraw(); }
}

static void
sdl2_map_printGlyph(struct SDL2Window *win, xchar x, xchar y, int glyph,
                    int bkglyph)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    if (x < 0 || COLNO <= x || y < 0 || ROWNO <= y) { return; }

    /* Handle zaps and explosions specially */
    if (GLYPH_EXPLODE_OFF <= glyph
    &&  glyph <= GLYPH_EXPLODE_OFF + MAXEXPCHARS * EXPL_MAX) {
        data->m_map[y][x].expl_glyph = glyph;
        data->m_map[y][x].expl_timer = clock() + 1 * CLOCKS_PER_SEC;
    } else if (GLYPH_ZAP_OFF <= glyph
    &&  glyph <= GLYPH_ZAP_OFF + 4 * NUM_ZAP) {
        data->m_map[y][x].zap_glyph = glyph;
    } else {
        data->m_map[y][x].zap_glyph = NO_GLYPH;
        data->m_map[y][x].bkglyph = bkglyph;
        data->m_map[y][x].glyph = glyph;
        sdl2_map_draw(win, x, y, TRUE);
    }
}

void
sdl2_map_setCursor(struct SDL2Window *win, int x, int y)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    sdl2_map_draw(win, data->m_cursor_x, data->m_cursor_y, FALSE);

    if (0 <= x && x < COLNO) {
        data->m_cursor_x = x;
    }
    if (0 <= y && y < ROWNO) {
        data->m_cursor_y = y;
    }

    sdl2_map_draw(win, data->m_cursor_x, data->m_cursor_y, TRUE);
}

void
sdl2_map_cliparound(struct SDL2Window *win, int x, int y)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;

    /* -1 means center at current cursor position */
    if (x == -1) {
        x = data->m_cursor_x;
    }
    if (y == -1) {
        y = data->m_cursor_y;
    }

    if (data->m_map_image->w <= width) {
        /* The map fits horizontally within the window */
        data->m_scroll_x = (width - data->m_map_image->w) / 2;
        /* data->m_scroll_x is at least zero */
    } else {
        int min_x;
        int cell_w = data->m_tile_w;

        /* Initially place x at the center of the window */
        int cell_x = x * cell_w;              /* x in map */
        int seen_x = (width - cell_w) / 2;    /* x in window */
        data->m_scroll_x = seen_x - cell_x;

        /* Adjust so that maximum width is visible */
        if (data->m_scroll_x > 0) { data->m_scroll_x = 0; }
        min_x = width - data->m_map_image->w;
        if (data->m_scroll_x < min_x) { data->m_scroll_x = min_x; }
        /* data->m_scroll_x is at most zero */
    }

    if (data->m_map_image->h <= height) {
        /* The map fits vertically within the window */
        data->m_scroll_y = (height - data->m_map_image->h) / 2;
        /* data->m_scroll_y is at least zero */
    } else {
        int min_y;
        int cell_h = data->m_tile_h;

        /* Initially place y at the center of the window */
        int cell_y = y * cell_h;              /* y in map */
        int seen_y = (height - cell_h) / 2;   /* y in window */
        data->m_scroll_y = seen_y - cell_y;

        /* Adjust so that maximum height is visible */
        if (data->m_scroll_y > 0) { data->m_scroll_y = 0; }
        min_y = height - data->m_map_image->h;
        if (data->m_scroll_y < min_y) { data->m_scroll_y = min_y; }
        /* data->m_scroll_y is at most zero */
    }
}

void
sdl2_map_toggle_tile_mode(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    /* Disable tile mode if no tile map */
    if (data->m_tiles == NULL) {
        sdl2_set_message("No tile set available");
        return;
    }

    data->m_tile_mode = !data->m_tile_mode;
    sdl2_set_message(
            data->m_tile_mode ? "Display Mode: Tiles"
                              : "Display Mode: Characters");

    /* Map image size may have changed */
    sdl2_map_setup(win);
}

void
sdl2_map_next_zoom_mode(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    switch (data->m_zoom_mode) {
    default:
    case SDL2_ZOOMMODE_VERTICAL:
        data->m_zoom_mode = SDL2_ZOOMMODE_NORMAL;
        sdl2_set_message("Zoom Mode: Tileset 1:1");
        break;

    case SDL2_ZOOMMODE_NORMAL:
        data->m_zoom_mode = SDL2_ZOOMMODE_FULLSCREEN;
        sdl2_set_message("Zoom Mode: Scale to screen");
        break;

    case SDL2_ZOOMMODE_FULLSCREEN:
        data->m_zoom_mode = SDL2_ZOOMMODE_HORIZONTAL;
        sdl2_set_message("Zoom Mode: Fit horizontally");
        break;

    case SDL2_ZOOMMODE_HORIZONTAL:
        data->m_zoom_mode = SDL2_ZOOMMODE_VERTICAL;
        sdl2_set_message("Zoom Mode: Fit vertically");
        break;
    }

    sdl2_map_setup(win);
}

static void
sdl2_map_setup(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    int cell_w = data->m_tile_mode ? data->m_tilemap_w : data->m_text_w;
    int cell_h = data->m_tile_mode ? data->m_tilemap_h : data->m_text_h;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;
    unsigned x, y;

    switch (data->m_zoom_mode) {
    default:
    case SDL2_ZOOMMODE_NORMAL:
        data->m_tile_w = cell_w;
        data->m_tile_h = cell_h;
        break;

    case SDL2_ZOOMMODE_FULLSCREEN:
        data->m_tile_w = width  / COLNO;
        data->m_tile_h = height / ROWNO;
        break;

    case SDL2_ZOOMMODE_HORIZONTAL:
        data->m_tile_w = width  / COLNO;
        data->m_tile_h = data->m_tile_w * cell_h / cell_w;
        break;

    case SDL2_ZOOMMODE_VERTICAL:
        data->m_tile_h = height / ROWNO;
        data->m_tile_w = data->m_tile_h * cell_w / cell_h;
        break;
    }

    width  = data->m_tile_w * COLNO;
    height = data->m_tile_h * ROWNO;
    if (width != data->m_map_image->w || height != data->m_map_image->h) {
        SDL_FreeSurface(data->m_map_image);
        data->m_map_image = SDL_CreateRGBSurface(
                SDL_SWSURFACE,
                data->m_tile_w * COLNO,
                data->m_tile_h * ROWNO,
                32,
                0x000000FF,  /* red */
                0x0000FF00,  /* green */
                0x00FF0000,  /* blue */
                0xFF000000); /* alpha */
    }

    /* Redraw the entire map */
    for (y = 0; y < ROWNO; ++y) {
        for (x = 0; x < COLNO; ++x) {
            sdl2_map_draw(win, x, y, TRUE);
        }
    }

    sdl2_map_cliparound(win, data->m_cursor_x, data->m_cursor_y);
}

static void
sdl2_map_draw(struct SDL2Window *win, int x, int y, boolean cursor)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    SDL_Rect tile, cell;
    int glyphs[5]; /* background, map, zap, explosion, shield */
    unsigned i;
    Uint32 bg;

    cell.x = x * data->m_tile_w;
    cell.y = y * data->m_tile_h;
    cell.w = data->m_tile_w;
    cell.h = data->m_tile_h;

    glyphs[0] = data->m_map[y][x].bkglyph;
    glyphs[1] = data->m_map[y][x].glyph;
    glyphs[2] = NO_GLYPH;
    glyphs[3] = data->m_map[y][x].zap_glyph;
    glyphs[4] = NO_GLYPH;
    if (clock() < data->m_map[y][x].expl_timer) {
        glyphs[2] = data->m_map[y][x].expl_glyph;
    } else {
        data->m_map[y][x].expl_timer = 0;
    }

    if (data->m_tile_mode) {
        int tilenum;

        /* Fill background just in case */
        bg = SDL_MapRGBA(data->m_map_image->format, 0, 0, 0, 255);
        SDL_FillRect(data->m_map_image, &cell, bg);

        /* Overlay possible tiles on this background */
        for (i = 0; i < SIZE(glyphs); ++i) {
            if (glyphs[i] != NO_GLYPH) {
                tilenum = glyph2tile[glyphs[i]];
                if (0 <= tilenum && tilenum < total_tiles_used) {
                    /* If the tile is used for the first time, convert it to */
                    /* an SDL_Surface */
                    if (data->m_tiles[tilenum] == NULL) {
                        data->m_tiles[tilenum] = convert_tile(tilenum);
                    }
                    if (data->m_tiles[tilenum] != NULL) {
                        tile.x = 0;
                        tile.y = 0;
                        tile.w = data->m_tilemap_w;
                        tile.h = data->m_tilemap_h;
                        SDL_BlitScaled(data->m_tiles[tilenum], &tile,
                                       data->m_map_image, &cell);
                    }
                }
            }
        }
    } else {
        int ch;
        Uint32 ochar;
        int ocolor;
        unsigned ospecial;
        SDL_Color text_fg;
        SDL_Color text_bg = { 0, 0, 0, 160 };

        /* Fill background just in case */
        /* TODO: implement reverse video here if needed */
        bg = SDL_MapRGBA(data->m_map_image->format, 0, 0, 0, 255);
        SDL_FillRect(data->m_map_image, &cell, bg);

        /* Overlay possible characters on this background */
        for (i = 0; i < SIZE(glyphs); ++i) {
            if (glyphs[i] != NO_GLYPH) {
                mapglyph(glyphs[i], &ch, &ocolor, &ospecial, x, y, 0);
                ochar = sdl2_chr_convert(ch);
                text_fg = sdl2_colors[ocolor];
                if (!sdl2_map_wall_draw(win, ochar, &cell, text_fg)) {
                    SDL_Surface *text = sdl2_font_renderCharBG(win->m_font,
                            ochar, text_fg, text_bg);
                    tile.x = 0;
                    tile.y = 0;
                    tile.w = text->w;
                    tile.h = text->h;
                    SDL_BlitScaled(text, &tile, data->m_map_image, &cell);
                    SDL_FreeSurface(text);
                }
            }
        }
    }

    if (cursor && x == data->m_cursor_x && y == data->m_cursor_y) {
        SDL_Rect line;
        SDL_Color rgb = sdl2_colors[data->cursor_color];
        Uint32 color = SDL_MapRGBA(data->m_map_image->format,
                rgb.r, rgb.g, rgb.b, rgb.a);

        /* top */
        line.x = cell.x;
        line.y = cell.y;
        line.w = cell.w;
        line.h = 1;
        SDL_FillRect(data->m_map_image, &line, color);
        /* bottom */
        line.y = cell.y + cell.h - 1;
        SDL_FillRect(data->m_map_image, &line, color);
        /* left */
        line.x = cell.x;
        line.y = cell.y;
        line.w = 1;
        line.h = cell.h;
        SDL_FillRect(data->m_map_image, &line, color);
        /* right */
        line.x = cell.x + cell.w - 1;
        SDL_FillRect(data->m_map_image, &line, color);
    }
}

boolean
sdl2_map_wall_draw(struct SDL2Window *win, Uint32 ch, const SDL_Rect *rect,
                   SDL_Color color)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    enum
    {
        w_left      = 0x01,
        w_right     = 0x02,
        w_up        = 0x04,
        w_down      = 0x08,
        w_sq_top    = 0x10,
        w_sq_bottom = 0x20,
        w_sq_left   = 0x40,
        w_sq_right  = 0x80
    };
    unsigned walls;
    SDL_Rect linerect;
    unsigned linewidth;
    Uint32 c = SDL_MapRGBA(data->m_map_image->format,
            color.r, color.g, color.b, color.a);
    Uint32 bg = SDL_MapRGBA(data->m_map_image->format, 0, 0, 0, 255);

    linewidth = ((rect->w < rect->h) ? rect->w : rect->h)/8;
    if (linewidth == 0) linewidth = 1;

    /* Single walls */
    walls = 0;
    switch (ch)
    {
    case 0x2500: /* BOX DRAWINGS LIGHT HORIZONTAL */
        walls = w_left | w_right;
        break;

    case 0x2502: /* BOX DRAWINGS LIGHT VERTICAL */
        walls = w_up | w_down;
        break;

    case 0x250C: /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
        walls = w_down | w_right;
        break;

    case 0x2510: /* BOX DRAWINGS LIGHT DOWN AND LEFT */
        walls = w_down | w_left;
        break;

    case 0x2514: /* BOX DRAWINGS LIGHT UP AND RIGHT */
        walls = w_up | w_right;
        break;

    case 0x2518: /* BOX DRAWINGS LIGHT UP AND LEFT */
        walls = w_up | w_left;
        break;

    case 0x251C: /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
        walls = w_up | w_down | w_right;
        break;

    case 0x2524: /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
        walls = w_up | w_down | w_left;
        break;

    case 0x252C: /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
        walls = w_down | w_left | w_right;
        break;

    case 0x2534: /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
        walls = w_up | w_left | w_right;
        break;

    case 0x253C: /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
        walls = w_up | w_down | w_left | w_right;
        break;
    }

    if (walls != 0)
    {
        SDL_FillRect(data->m_map_image, rect, bg);
        linerect.x = rect->x + (rect->w - linewidth)/2;
        linerect.w = linewidth;
        switch (walls & (w_up | w_down))
        {
        case w_up:
            linerect.y = rect->y;
            linerect.h = rect->h/2;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;

        case w_down:
            linerect.y = rect->y + rect->h/2;
            linerect.h = rect->h - rect->h/2;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;

        case w_up | w_down:
            linerect.y = rect->y;
            linerect.h = rect->h;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;
        }

        linerect.y = rect->y + (rect->h - linewidth)/2;
        linerect.h = linewidth;
        switch (walls & (w_left | w_right))
        {
        case w_left:
            linerect.x = rect->x;
            linerect.w = rect->w/2;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;

        case w_right:
            linerect.x = rect->x + rect->w/2;
            linerect.w = rect->w - rect->w/2;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;

        case w_left | w_right:
            linerect.x = rect->x;
            linerect.w = rect->w;
            SDL_FillRect(data->m_map_image, &linerect, c);
            break;
        }

        return TRUE;
    }

    /* Double walls */
    walls = 0;
    switch (ch)
    {
    case 0x2550: /* BOX DRAWINGS DOUBLE HORIZONTAL */
        walls = w_left | w_right | w_sq_top | w_sq_bottom;
        break;

    case 0x2551: /* BOX DRAWINGS DOUBLE VERTICAL */
        walls = w_up | w_down | w_sq_left | w_sq_right;
        break;

    case 0x2554: /* BOX DRAWINGS DOUBLE DOWN AND RIGHT */
        walls = w_down | w_right | w_sq_top | w_sq_left;
        break;

    case 0x2557: /* BOX DRAWINGS DOUBLE DOWN AND LEFT */
        walls = w_down | w_left | w_sq_top | w_sq_right;
        break;

    case 0x255A: /* BOX DRAWINGS DOUBLE UP AND RIGHT */
        walls = w_up | w_right | w_sq_bottom | w_sq_left;
        break;

    case 0x255D: /* BOX DRAWINGS DOUBLE UP AND LEFT */
        walls = w_up | w_left | w_sq_bottom | w_sq_right;
        break;

    case 0x2560: /* BOX DRAWINGS DOUBLE VERTICAL AND RIGHT */
        walls = w_up | w_down | w_right | w_sq_left;
        break;

    case 0x2563: /* BOX DRAWINGS DOUBLE VERTICAL AND LEFT */
        walls = w_up | w_down | w_left | w_sq_right;
        break;

    case 0x2566: /* BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL */
        walls = w_down | w_left | w_right | w_sq_top;
        break;

    case 0x2569: /* BOX DRAWINGS DOUBLE UP AND HORIZONTAL */
        walls = w_up | w_left | w_right | w_sq_bottom;
        break;

    case 0x256C: /* BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL */
        walls = w_up | w_down | w_left | w_right;
        break;
    }
    if (walls != 0)
    {
        if (walls & w_up)
        {
            linerect.x = rect->x + rect->w/2 - linewidth*2;
            linerect.w = linewidth;
            linerect.y = rect->y;
            linerect.h = rect->h/2 - linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
            linerect.x += linewidth*3;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_down)
        {
            linerect.x = rect->x + rect->w/2 - linewidth*2;
            linerect.w = linewidth;
            linerect.y = rect->y + rect->h/2 + linewidth;
            linerect.h = rect->h - rect->h/2 - linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
            linerect.x += linewidth*3;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_left)
        {
            linerect.x = rect->x;
            linerect.w = rect->w/2 - linewidth;
            linerect.y = rect->y + rect->h/2 - linewidth*2;
            linerect.h = linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
            linerect.y += linewidth*3;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_right)
        {
            linerect.x = rect->x + rect->w/2 + linewidth;
            linerect.w = rect->w - rect->w/2 - linewidth;
            linerect.y = rect->y + rect->h/2 - linewidth*2;
            linerect.h = linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
            linerect.y += linewidth*3;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_sq_top)
        {
            linerect.x = rect->x + rect->w/2 - linewidth*2;
            linerect.w = linewidth*4;
            linerect.y = rect->y + rect->h/2 - linewidth*2;
            linerect.h = linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_sq_bottom)
        {
            linerect.x = rect->x + rect->w/2 - linewidth*2;
            linerect.w = linewidth*4;
            linerect.y = rect->y + rect->h/2 + linewidth;
            linerect.h = linewidth;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_sq_left)
        {
            linerect.x = rect->x + rect->w/2 - linewidth*2;
            linerect.w = linewidth;
            linerect.y = rect->y + rect->h/2 - linewidth*2;
            linerect.h = linewidth*4;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        if (walls & w_sq_right)
        {
            linerect.x = rect->x + rect->w/2 + linewidth;
            linerect.w = linewidth;
            linerect.y = rect->y + rect->h/2 - linewidth*2;
            linerect.h = linewidth*4;
            SDL_FillRect(data->m_map_image, &linerect, c);
        }
        return TRUE;
    }

    /* Solid blocks */
    if (0x2591 <= ch && ch <= 0x2593)
    {
        unsigned shade = ch - 0x2590;
        SDL_Color color2;
        Uint32 c2;
        color2.r = color.r*shade/4;
        color2.g = color.g*shade/4;
        color2.b = color.b*shade/4;
        color2.a = color.a;
        linerect = *rect;
        c2 = SDL_MapRGBA(data->m_map_image->format,
                color2.r, color2.g, color2.b, color2.a);
        SDL_FillRect(data->m_map_image, &linerect, c2);
        return TRUE;
    }

    return FALSE;
}

int
sdl2_map_height_hint(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;
    int tile_height;

    if (data->m_tile_mode) {
        if (data->m_zoom_mode == SDL2_ZOOMMODE_HORIZONTAL) {
            tile_height = data->m_tile_h;
        } else {
            tile_height = data->m_tilemap_h;
        }
    } else {
        tile_height = data->m_text_h;
    }
    return tile_height * ROWNO;
}

int
sdl2_map_width(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    return data->m_tile_w * COLNO;
}

int
sdl2_map_xpos(struct SDL2Window *win)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    return data->m_scroll_x;
}


boolean
sdl2_map_mouse(struct SDL2Window *win, int x_in, int y_in,
               int *x_out, int *y_out)
{
    struct MapPrivate *data = (struct MapPrivate *) win->data;

    x_in -= data->m_scroll_x;
    y_in -= data->m_scroll_y;
    if (x_in < 0 || y_in < 0) { return FALSE; }
    *x_out = x_in / data->m_tile_w;
    *y_out = y_in / data->m_tile_h;
    return (*x_out < COLNO && *y_out < ROWNO);
}

/*ARGSUSED*/
void
sdl2_map_update_cursor(struct SDL2Window *window,
            int index, genericptr_t ptr, int chg, int percent, int color,
            const unsigned long *colormasks)
{
    struct MapPrivate *data = (struct MapPrivate *) window->data;

    if (index == BL_HP) {
        data->cursor_color = color & 0x0F;
    }
}

static SDL_Surface *
convert_tile(int tilenum)
{
    const struct TileImage *tile = get_tile(tilenum);
    SDL_Surface *sdltile;
    Uint32 *pixels;
    unsigned x, y;
    size_t i;

    sdltile = SDL_CreateRGBSurface(
            SDL_SWSURFACE,
            tile->width,
            tile->height,
            32,
            0x000000FF,  /* red */
            0x0000FF00,  /* green */
            0x00FF0000,  /* blue */
            0xFF000000); /* alpha */
    if (sdltile == NULL) {
        char buf[BUFSZ];
        sprintf(buf, "Failed to create tile %d", tilenum);
        paniclog("GUI", buf);
        return NULL;
    }

    pixels = (Uint32 *) sdltile->pixels;
    i = 0;
    for (y = 0; y < tile->height; ++y) {
        for (x = 0; x < tile->width; ++x) {
            unsigned r, g, b, a;
            r = tile->pixels[i].r;
            g = tile->pixels[i].g;
            b = tile->pixels[i].b;
            a = tile->pixels[i].a;
            pixels[i++] = SDL_MapRGBA(sdltile->format, r, g, b, a);
        }
    }

    return sdltile;
}
