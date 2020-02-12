/* sdl2text.cpp */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2text.h"
#include "sdl2font.h"
#include "sdl2window.h"

static const int margin = 3;

struct Line {
    char *text;
    Uint32 attributes;
    boolean mixed;
};

struct TextPrivate {
    struct Line *m_contents;
    size_t m_num_lines;
    size_t m_lines_alloc;
    unsigned m_first_line;
    unsigned m_page_size;
};

static void sdl2_text_create(struct SDL2Window *win);
static void sdl2_text_destroy(struct SDL2Window *win);
static void sdl2_text_clear(struct SDL2Window *win);
static void sdl2_text_setVisible(struct SDL2Window *win,
        boolean visible);
static void sdl2_text_put_string(struct SDL2Window *win, int attr,
        const char *str, boolean mixed);
static void sdl2_text_redraw(struct SDL2Window *win);

static void doPage(struct SDL2Window *win, Uint32 ch);
static void expandText(struct SDL2Window *win);

struct SDL2Window_Methods const sdl2_text_procs = {
    sdl2_text_create,
    sdl2_text_destroy,
    sdl2_text_clear,
    sdl2_text_setVisible,
    NULL,
    sdl2_text_put_string,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_text_redraw
};

static void
sdl2_text_create(struct SDL2Window *win)
{
    struct TextPrivate *data = (struct TextPrivate *) alloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
    win->data = data;

    /* We won't show the window until all lines are present */
    sdl2_text_setVisible(win, FALSE);

    /* Text window font */
    sdl2_window_set_font(win, iflags.wc_font_text, iflags.wc_fontsiz_text,
            sdl2_font_defaultMonoFont, 20);
}

static void
sdl2_text_destroy(struct SDL2Window *win)
{
    struct TextPrivate *data = (struct TextPrivate *) win->data;
    size_t i;

    for (i = 0; i < data->m_num_lines; ++i) {
        free(data->m_contents[i].text);
    }
    free(data->m_contents);
    free(data);
}

static void
sdl2_text_redraw(struct SDL2Window *win)
{
    SDL_Color foreground, background;
    struct TextPrivate *data = (struct TextPrivate *) win->data;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;
    SDL_Rect rect;
    int y;
    unsigned i;

    foreground = sdl2_text_fg(iflags.wc_foregrnd_text,
                              iflags.wc_backgrnd_text,
                              ATR_NONE);
    background = sdl2_text_bg(iflags.wc_foregrnd_text,
                              iflags.wc_backgrnd_text,
                              ATR_NONE, 160);
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    sdl2_window_fill(win, &rect, background);

    /* Border around window, one pixel wide */
    sdl2_window_draw_box(win, 0, 0, width - 1, height - 1, foreground);

    y = margin;
    for (i = 0; i < data->m_page_size; ++i) {
        unsigned j;
        int attr;

        if (y >= height) { break; }
        j = i + data->m_first_line;
        if (j >= data->m_num_lines) { break; }
        attr = data->m_contents[j].attributes;
        sdl2_window_renderStrBG(win, data->m_contents[j].text, margin, y,
                                sdl2_text_fg(iflags.wc_foregrnd_text,
                                             iflags.wc_backgrnd_text,
                                             attr),
                                sdl2_text_bg(iflags.wc_foregrnd_text,
                                             iflags.wc_backgrnd_text,
                                             attr, 0));
        y += win->m_line_height;
    }
    if (data->m_page_size < data->m_num_lines) {
        /* Display the page indicator */
        char page[QBUFSZ];

        y = height - win->m_line_height - margin;
        snprintf(page, SIZE(page), "Page %lu of %lu",
                (unsigned long) (data->m_first_line / data->m_page_size + 1),
                (unsigned long) ((data->m_num_lines + data->m_page_size - 1) / data->m_page_size));
        sdl2_window_renderStrBG(win, page, margin, y,
                                foreground, background);
    }
}

static void
sdl2_text_clear(struct SDL2Window *win)
{
    struct TextPrivate *data = (struct TextPrivate *) win->data;
    size_t i;

    for (i = 0; i < data->m_num_lines; ++i) {
        free(data->m_contents[i].text);
    }
    data->m_num_lines = 0;
}

static void
sdl2_text_put_string(struct SDL2Window *win, int attr, const char *str,
                     boolean mixed)
{
    struct TextPrivate *data = (struct TextPrivate *) win->data;
    struct Line *new_line;

    expandText(win);
    new_line = &data->m_contents[data->m_num_lines++];
    new_line->text = dupstr(str);
    new_line->attributes = attr;
    new_line->mixed = mixed;
}

static void
sdl2_text_setVisible(struct SDL2Window *win, boolean visible)
{
    if (visible) {
        struct TextPrivate *data = (struct TextPrivate *) win->data;
        int width, height;
        int disp_width, disp_height;
        size_t i;
        int x, y;

        width = 0;
        for (i = 0; i < data->m_num_lines; ++i) {
            SDL_Rect rect;

            rect = sdl2_font_textSizeStr(win->m_font, data->m_contents[i].text);
            if (width < rect.w) {
                width = rect.w;
            }
        }

        sdl2_display_size(&disp_width, &disp_height);

        height = data->m_num_lines * win->m_line_height;
        if (height + margin * 2 > disp_height) {
            data->m_page_size = (disp_height - margin * 2) / win->m_line_height - 1;
            if (data->m_page_size < 1) { data->m_page_size = 1; }
            height = (data->m_page_size + 1) * win->m_line_height;
        } else {
            data->m_page_size = data->m_num_lines;
        }
        width += margin * 2;
        height += margin * 2;

        x = (disp_width  - width) / 2;
        y = (disp_height - height) / 2;
        sdl2_window_resize(win, x, y, x + width - 1, y + height - 1);
    }

    win->m_visible = visible;
    sdl2_redraw();

    if (visible) {
        Uint32 ch;
        do {
            ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
            doPage(win, ch);
        } while (ch != '\033' && ch != '\r');
    }
}

static void
doPage(struct SDL2Window *win, Uint32 ch)
{
    struct TextPrivate *data = (struct TextPrivate *) win->data;

    switch (ch) {
    case MENU_FIRST_PAGE:
        if (data->m_first_line != 0) {
            data->m_first_line = 0;
            sdl2_redraw();
        }
        break;

    case MENU_LAST_PAGE:
        if (data->m_page_size < data->m_num_lines) {
            unsigned pagenum = (data->m_num_lines - 1) / data->m_page_size;
            unsigned first_line = pagenum * data->m_page_size;
            if (data->m_first_line != first_line) {
                data->m_first_line = first_line;
                sdl2_redraw();
            }
        }
        break;

    case MENU_PREVIOUS_PAGE:
        if (data->m_first_line >= data->m_page_size) {
            data->m_first_line -= data->m_page_size;
            sdl2_redraw();
        }
        break;

    case MENU_NEXT_PAGE:
        if (data->m_first_line + data->m_page_size < data->m_num_lines) {
            data->m_first_line += data->m_page_size;
            sdl2_redraw();
        }
        break;
    }
}

static void
expandText(struct SDL2Window *win)
{
    struct TextPrivate *data = (struct TextPrivate *) win->data;

    if (data->m_num_lines >= data->m_lines_alloc) {
        struct Line *new_contents;
        size_t i;

        data->m_lines_alloc += 64;
        new_contents = (struct Line *)
                alloc(data->m_lines_alloc * sizeof(struct Line));
        for (i = 0; i < data->m_num_lines; ++i) {
            new_contents[i] = data->m_contents[i];
        }
        free(data->m_contents);
        data->m_contents = new_contents;
    }
}