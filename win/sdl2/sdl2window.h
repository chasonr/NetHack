/* sdl2window.h */

#ifndef WIN_SDL2_SDL2WINDOW_H
#define WIN_SDL2_SDL2WINDOW_H

#include "sdl2font.h"

struct SDL2Window;

/* Structure containing the window-specific methods */
struct SDL2Window_Methods {
    void FDECL((*create), (struct SDL2Window *win));
    void FDECL((*destroy), (struct SDL2Window *win));
    void FDECL((*clear), (struct SDL2Window *win));
    void FDECL((*setVisible), (struct SDL2Window *win, BOOLEAN_P visible));
    void FDECL((*setCursor), (struct SDL2Window *win, int x, int y));
    void FDECL((*putStr), (struct SDL2Window *win, int attr, const char *str,
            BOOLEAN_P mixed));
    void FDECL((*startMenu), (struct SDL2Window *win));
    void FDECL((*addMenu), (struct SDL2Window *win, int glyph,
            const anything* identifier, CHAR_P ch, CHAR_P gch, int attr,
            const char *str, BOOLEAN_P preselected));
    void FDECL((*endMenu), (struct SDL2Window *win, const char *prompt));
    int  FDECL((*selectMenu), (struct SDL2Window *win, int how,
            menu_item ** menu_list));
    void FDECL((*printGlyph), (struct SDL2Window *win, XCHAR_P x, XCHAR_P y,
            int glyph, int bkglyph));
    void FDECL((*redraw), (struct SDL2Window *win));
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

extern struct SDL2Window *FDECL(sdl2_window_create,
        (struct SDL2Window_Methods const *methods));
extern void FDECL(sdl2_window_destroy, (struct SDL2Window *window));
extern void FDECL(sdl2_window_blit,
        (struct SDL2Window *window, const SDL_Rect *window_rect,
         SDL_Surface *surface, const SDL_Rect *surface_rect));
extern void FDECL(sdl2_window_set_font,
        (struct SDL2Window *window,
         const char *name_option, int size_option,
         const char *default_name, int default_size));
extern void FDECL(sdl2_window_resize, (struct SDL2Window *window,
        int x1, int y1, int x2, int y2));
extern void FDECL(sdl2_window_fill, (struct SDL2Window *window,
        const SDL_Rect *window_rect, SDL_Color color));
extern SDL_Rect FDECL(sdl2_window_renderStrBG,
        (struct SDL2Window *win, const char *str, int x, int y,
         SDL_Color fg, SDL_Color bg));
extern SDL_Rect sdl2_window_renderCharBG(
         struct SDL2Window *win, Uint32 chr, int x, int y,
         SDL_Color fg, SDL_Color bg);
extern SDL_Rect FDECL(sdl2_window_render_mixed,
        (struct SDL2Window *win, const char *str, int x, int y,
         SDL_Color fg, SDL_Color bg));
extern void FDECL(sdl2_window_draw_box, (
        struct SDL2Window *win,
        int x1, int y1,
        int x2, int y2,
        SDL_Color color));

extern Uint32 FDECL(sdl2_get_key, (BOOLEAN_P cmd, int *x, int *y, int *mod));
extern void NDECL(sdl2_redraw);
extern void FDECL(sdl2_display_size, (int *x, int *y));
extern void FDECL(sdl2_set_message, (const char *message));
extern Uint32 FDECL(sdl2_chr_convert, (Uint32 ch));

extern const SDL_Color sdl2_colors[16];
extern SDL_Color FDECL(sdl2_text_fg, (int attr));
extern SDL_Color FDECL(sdl2_text_bg, (int attr));

#endif
