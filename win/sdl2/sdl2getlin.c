/* sdl2getlin.c */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "func_tab.h"
#include "sdl2.h"
#include "sdl2getlin.h"
#include "sdl2window.h"
#include "sdl2font.h"

static const int margin = 3;
static const int min_width = 400;

struct GetlinePrivate {
    char *m_contents;
    size_t m_contents_size;
    size_t m_contents_alloc;
    int m_box_width;
    char *m_prompt;
    void FDECL((*add_char), (struct SDL2Window *win, Uint32 ch));
};

static void FDECL(sdl2_getline_create, (struct SDL2Window *win));
static void FDECL(sdl2_getline_destroy, (struct SDL2Window *win));
static void FDECL(sdl2_getline_redraw, (struct SDL2Window *win));

static void FDECL(sdl2_getline_add_char, (struct SDL2Window *win, Uint32 ch));
static void FDECL(sdl2_extcmd_add_char, (struct SDL2Window *win, Uint32 ch));

struct SDL2Window_Methods const sdl2_getline_procs = {
    sdl2_getline_create,
    sdl2_getline_destroy,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_getline_redraw
};

static void
sdl2_getline_create(win)
struct SDL2Window *win;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) alloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
    win->data = data;

    data->m_contents_alloc = 0;
    data->m_contents = NULL;
    data->add_char = sdl2_getline_add_char;

    win->m_visible = FALSE;

    /* Message window font */
    sdl2_window_set_font(win, iflags.wc_font_message, iflags.wc_fontsiz_message,
            sdl2_font_defaultSerifFont, 20);
}

static void
sdl2_getline_destroy(win)
struct SDL2Window *win;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;

    free(data->m_prompt);
    free(data);
}

static void
sdl2_getline_redraw(win)
struct SDL2Window *win;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;
    const SDL_Color black = {   0,   0,   0, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;
    SDL_Rect rect;
    SDL_Rect source;
    SDL_Surface *text;

    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    sdl2_window_fill(win, &rect, black);

    /* Draw a border at the edges of the window */
    sdl2_window_draw_box(win, 0, 0, width - 1, height - 1, white);

    /* Draw a border around the text being entered */
    sdl2_window_draw_box(win, margin, 2*margin + win->m_line_height,
            width - 1 - margin, height  - 1 - margin,
            white);

    /* Draw the prompt */
    sdl2_window_renderStrBG(win, data->m_prompt, margin, margin, white, black);

    /* Draw the text as it currently stands */
    text = sdl2_font_renderStrBG(win->m_font, data->m_contents, white, black);
    if (text->w < data->m_box_width) {
        /* The text fits within the box width */
        source.w = text->w;
    } else {
        /* Clip the text at the left */
        source.x = text->w - (data->m_box_width - 1);
    }
    source.x = text->w - source.w;
    source.y = 0;
    source.h = text->h;
    rect.x = 3*margin;
    rect.y = 3*margin + win->m_line_height;
    rect.w = source.w;
    rect.h = source.h;
    sdl2_window_blit(win, &rect, text, &source);
    SDL_FreeSurface(text);

    /* Draw a cursor */
    rect.x += rect.w;
    rect.w = 1;
    rect.h = win->m_line_height;
    sdl2_window_fill(win, &rect, white);
}

boolean
sdl2_getline_run(
        struct SDL2Window *win,
        const char *prompt,
        char *buf,
        size_t bufsize)
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;
    int disp_width, disp_height;
    int x0, y0, w, h;

    /* Set up the return buffer */
    data->m_contents = buf;
    data->m_contents_size = 0;
    data->m_contents_alloc = bufsize;
    buf[0] = 0;

    /* Set the prompt */
    free(data->m_prompt);
    data->m_prompt = dupstr(prompt);

    /* Height is a margin, a line, two margins, a line and two margins */
    h = margin * 5 + win->m_line_height * 2;

    /* Width is a margin, the longer of the prompt or min_width, and a margin */
    w = (sdl2_font_textSizeStr(win->m_font, prompt)).w;
    if (w < min_width) { w = min_width; }
    w += margin * 2;

    /* Width is two margins, the box width and two margins */
    data->m_box_width = w - margin * 4;

    /* Center the window */
    sdl2_display_size(&disp_width, &disp_height);
    x0 = (disp_width  - w) / 2;
    y0 = (disp_height - h) / 2;
    sdl2_window_resize(win, x0, y0, x0 + w - 1, y0 + h - 1);

    /* Make it visible */
    win->m_visible = TRUE;
    sdl2_redraw();

    /* Accept keys */
    while (TRUE) {
        Uint32 ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
        if (ch == '\033') {
            buf[0] = '\0';
            return FALSE;
        } /* escape */
        if (ch == '\n' || ch == '\r') { break; }
        data->add_char(win, ch);
    }

    /* Returned false above if escape */

    return TRUE;
}

void
sdl2_getline_add_char(win, ch)
struct SDL2Window *win;
Uint32 ch;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;

    /* TODO:  process left and right arrows */
    if (ch > 0x10FFFF) { return; }

    if ((ch == '\b' || ch == '\177') && data->m_contents_size != 0) {
        data->m_contents[--data->m_contents_size] = '\0';
    } else {
        /* Don't include control characters in the string */
        if (ch < 0x20 || (0x7F <= ch && ch <= 0x9F)) { return; }

        /* TODO: To support Unicode, we'll need to normalize the resulting
           string, and that opens a can of worms; we'll also need non-trivial
           changes to the core. For now, support only ISO 8859-1. */
        if (ch <= 0xFF && data->m_contents_size + 2 < data->m_contents_alloc) {
            /* Add this character */
            data->m_contents[data->m_contents_size++] = (char) ch;
            data->m_contents[data->m_contents_size] = '\0';
        }
    }

    sdl2_redraw();
}

int
sdl2_extcmd_run(win)
struct SDL2Window *win;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;
    char cmd[BUFSZ];
    int index;

    data->add_char = sdl2_extcmd_add_char;

    index = -1;
    if (sdl2_getline_run(win, "Enter extended command:", cmd, SIZE(cmd))) {
        int i;
        for (i = 0; extcmdlist[i].ef_txt != NULL; ++i) {
            if (strcmp(cmd, extcmdlist[i].ef_txt) == 0) {
                index = i;
                break;
            }
        }
    }

    return index;
}

void
sdl2_extcmd_add_char(win, ch)
struct SDL2Window *win;
Uint32 ch;
{
    struct GetlinePrivate *data = (struct GetlinePrivate *) win->data;
    unsigned matches;
    const char *cmd;
    unsigned i;

    /* Add the character the same way SDL2GetLine does */
    sdl2_getline_add_char(win, ch);

    /* Compare to the list of extended commands; if only one match, set
       m_contents to the match */

    matches = 0;
    cmd = NULL;

    for (i = 0; extcmdlist[i].ef_txt != NULL; ++i) {
        const char *text8 = extcmdlist[i].ef_txt;
        if (strncmp(text8, data->m_contents, data->m_contents_size) == 0) {
            ++matches;
            if (matches > 1) { break; }
            cmd = text8;
        }
    }

    if (matches == 1) {
        /* Assumes no command is longer than BUFSZ */
        strcpy(data->m_contents, cmd);
        data->m_contents_size = strlen(cmd);
        sdl2_redraw();
    }
}
