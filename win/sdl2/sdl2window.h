/* sdl2window.h */

#ifndef WIN_SDL2_SDL2WINDOW_H
#define WIN_SDL2_SDL2WINDOW_H

#include "sdl2font.h"

struct SDL2Window;

/* Structure containing the window-specific methods */
struct SDL2Window_Methods {
    void (*create)(struct SDL2Window *win);
    void (*destroy)(struct SDL2Window *win);
    void (*clear)(struct SDL2Window *win);
    void (*setVisible)(struct SDL2Window *win, boolean visible);
    void (*setCursor)(struct SDL2Window *win, int x, int y);
    void (*putStr)(struct SDL2Window *win, int attr, const char *str,
            boolean mixed);
    void (*startMenu)(struct SDL2Window *win);
    void (*addMenu)(struct SDL2Window *win, int glyph,
            const anything* identifier, char ch, char gch, int attr,
            const char *str, boolean preselected);
    void (*endMenu)(struct SDL2Window *win, const char *prompt);
    int  (*selectMenu)(struct SDL2Window *win, int how,
            menu_item ** menu_list);
    void (*printGlyph)(struct SDL2Window *win, xchar x, xchar y,
            int glyph, int bkglyph);
    void (*redraw)(struct SDL2Window *win);
};

struct SDL2Window {
    struct SDL2Window_Methods const *methods;
    void *data;

    winid id;

    int m_xmin;
    int m_xmax;
    int m_ymin;
    int m_ymax;
    int m_visible;

    SDL2Font *m_font;
    int m_line_height;
};

extern struct SDL2Window *sdl2_window_create(
        struct SDL2Window_Methods const *methods);
extern void sdl2_window_destroy(struct SDL2Window *window);
extern void sdl2_window_blit(struct SDL2Window *window,
        const SDL_Rect *window_rect, SDL_Surface *surface,
        const SDL_Rect *surface_rect);
extern void sdl2_window_set_font(struct SDL2Window *window,
        const char *name_option, int size_option,
        const char *default_name, int default_size);
extern void sdl2_window_resize(struct SDL2Window *window,
        int x1, int y1, int x2, int y2);
extern void sdl2_window_fill(struct SDL2Window *window,
        const SDL_Rect *window_rect, SDL_Color color);
extern SDL_Rect sdl2_window_renderStrBG(struct SDL2Window *win,
        const char *str, int x, int y, SDL_Color fg, SDL_Color bg);
extern SDL_Rect sdl2_window_renderCharBG( struct SDL2Window *win, Uint32 chr,
        int x, int y, SDL_Color fg, SDL_Color bg);
extern SDL_Rect sdl2_window_render_mixed(struct SDL2Window *win,
        const char *str, int x, int y, SDL_Color fg, SDL_Color bg);
extern void sdl2_window_draw_box(struct SDL2Window *win, int x1, int y1,
        int x2, int y2, SDL_Color color);

extern Uint32 sdl2_get_key(boolean cmd, int *x, int *y, int *mod);
extern void sdl2_redraw(void);
extern void sdl2_display_size(int *x, int *y);
extern void sdl2_set_message(const char *message);
extern Uint32 sdl2_chr_convert(Uint32 ch);

extern const SDL_Color sdl2_colors[16];
extern SDL_Color sdl2_text_fg(int attr);
extern SDL_Color sdl2_text_bg(int attr);

#endif
