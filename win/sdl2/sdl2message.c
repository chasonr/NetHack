/* sdl2message.cpp */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2message.h"
#include "sdl2font.h"
#include "sdl2map.h"

struct Line
{
    boolean mixed;
    char *text;
    unsigned attributes;
    unsigned num_messages;
};

struct LineList
{
    struct Line *lines;
    size_t size;
    size_t head;
    size_t tail;
};

struct MessagePrivate {
    struct LineList m_contents; /* Uncombined messages */
    struct LineList m_lines; /* Messages combined into lines */

    unsigned m_line_offset;
    unsigned width;
    boolean m_more;
    boolean m_combine;
};

static void sdl2_message_create(struct SDL2Window *win);
static void sdl2_message_destroy(struct SDL2Window *win);
static void sdl2_message_put_string(struct SDL2Window *win, int attr,
        const char *str, boolean mixed);
static void sdl2_message_redraw(struct SDL2Window *win);

static void initLineList(struct LineList *list, size_t size);
static void freeLineList(struct LineList *list);
static size_t numberOfLines(const struct LineList *list);
static struct Line *addLine(struct LineList *list, int attr, const char *str,
                            boolean mixed);

struct SDL2Window_Methods const sdl2_message_procs = {
    sdl2_message_create,
    sdl2_message_destroy,
    NULL,
    NULL,
    NULL,
    sdl2_message_put_string,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_message_redraw
};

static void
sdl2_message_create(struct SDL2Window *win)
{
    struct MessagePrivate *data =
            (struct MessagePrivate *) alloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
    win->data = data;

    /* Message window font */
    sdl2_window_set_font(win,
            iflags.wc_font_message, iflags.wc_fontsiz_message,
            sdl2_font_defaultSerifFont, 20);

    initLineList(&data->m_contents, iflags.msg_history + 1);
    initLineList(&data->m_lines, iflags.msg_history + 1);
}

static void
sdl2_message_destroy(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;

    freeLineList(&data->m_contents);
    freeLineList(&data->m_lines);

    free(data);
    win->data = NULL;
}

static void
sdl2_message_redraw(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;

    SDL_Color background;
    int num_lines;  /* Number of visible lines */
    int i;
    SDL_Rect rect;
    size_t p;

    background = sdl2_text_bg(iflags.wc_foregrnd_message,
                              iflags.wc_backgrnd_message,
                              ATR_NONE, 255);
    num_lines = (height + win->m_line_height - 1) / win->m_line_height;
    p = data->m_lines.tail;
    if (data->m_lines.head > data->m_lines.tail) {
        p += data->m_lines.size;
    }
    if (p - data->m_lines.head < data->m_line_offset) {
        p = data->m_lines.head;
    } else {
        p -= data->m_line_offset;
        if (p >= data->m_lines.size) {
            p -= data->m_lines.size;
        }
    }
    for (i = 0; i < num_lines && p != data->m_lines.head; ++i) {
        int attr;
        int x, y;

        if (p == 0) {
            p = data->m_lines.size;
        }
        --p;
        attr = data->m_lines.lines[p].attributes;
        x = 0;
        y = height - win->m_line_height*(i+1);
        if (data->m_lines.lines[p].mixed) {
            rect = sdl2_window_render_mixed(win, data->m_lines.lines[p].text,
                    x, y,
                    sdl2_text_fg(iflags.wc_foregrnd_message,
                                 iflags.wc_backgrnd_message,
                                 attr),
                    sdl2_text_bg(iflags.wc_foregrnd_message,
                                 iflags.wc_backgrnd_message,
                                 attr, 255));
        } else {
            rect = sdl2_window_renderStrBG(win, data->m_lines.lines[p].text,
                    x, y,
                    sdl2_text_fg(iflags.wc_foregrnd_message,
                                 iflags.wc_backgrnd_message,
                                 attr),
                    sdl2_text_bg(iflags.wc_foregrnd_message,
                                 iflags.wc_backgrnd_message,
                                 attr, 255));
        }
        x += rect.w;
        if (i == 0 && data->m_more) {
            rect = sdl2_window_renderStrBG(win, "--More--", x, y,
                sdl2_text_fg(iflags.wc_foregrnd_message,
                             iflags.wc_backgrnd_message,
                             ATR_INVERSE),
                sdl2_text_bg(iflags.wc_foregrnd_message,
                             iflags.wc_backgrnd_message,
                             ATR_INVERSE, 255));
            x += rect.w;
        }
        rect.x = x;
        rect.w = width - rect.x;
        sdl2_window_fill(win, &rect, background);
    }
    rect.x = 0;
    rect.y = 0;
    rect.w = width + 1;
    rect.h = height - win->m_line_height*i;
    if (rect.h > 0) {
        sdl2_window_fill(win, &rect, background);
    }
}

int
sdl2_message_width_hint(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;

    if (data->width == 0) {
        SDL_Rect r = sdl2_font_textSizeChar(win->m_font, 'X');
        data->width = r.w * 30;
    }
    return data->width;
}

int
sdl2_message_height_hint(struct SDL2Window *win)
{
    return win->m_line_height * 2;
}

static void
sdl2_message_put_string(struct SDL2Window *win, int attr, const char *str,
                        boolean mixed)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;
    int width = win->m_xmax - win->m_xmin + 1;
    boolean add_new;
    boolean do_more;

    /* Add new string to m_contents */
    addLine(&data->m_contents, attr, str, mixed);

    /* Add new string to m_lines.
       This is a new line if this is the first message for a turn, if the
       attribute or that of the prior string is non-default, if the string is
       one requiring emphasis, if a glyph is present, or if the width plus
       --More-- would exceed the width of the window. */
    add_new = FALSE;
    do_more = FALSE;
    if (!data->m_combine) {
        add_new = TRUE;
    } else if (attr != ATR_NONE) {
        add_new = TRUE;
    } else if (strcmp(str, "You die...") == 0) {
        add_new = TRUE;
        do_more = TRUE;
    } else if (data->m_lines.head == data->m_lines.tail) {
        add_new = TRUE;
    } else if (mixed) {
        add_new = TRUE;
    } else {
        size_t tail = data->m_lines.tail - 1;
        if (data->m_lines.tail == 0) {
            tail = data->m_lines.size - 1;
        }
        if (data->m_lines.lines[tail].attributes != ATR_NONE) {
            add_new = TRUE;
        } else {
            char str2[BUFSZ];
            int new_width;

            snprintf(str2, SIZE(str2), "%s  %s--More--",
                    data->m_lines.lines[tail].text, str);
            new_width = sdl2_font_textSizeStr(win->m_font, str2).w;
            if (new_width > width) {
                add_new = TRUE;
                do_more = TRUE;
            }
        }
    }
    /* Show --More-- if we need it */
    if (do_more) {
        sdl2_message_more(win);
    }
    /* Add the line */
    if (add_new) {
        struct Line *new_cline = addLine(&data->m_lines, attr, str, mixed);
        new_cline->num_messages = 1;
    } else {
        char *buf;
        size_t tail = data->m_lines.tail - 1;
        if (data->m_lines.tail == 0) {
            tail = data->m_lines.size - 1;
        }
        buf = (char *)alloc(strlen(data->m_lines.lines[tail].text) + strlen(str) + 3);
        sprintf(buf, "%s  %s", data->m_lines.lines[tail].text, str);
        free(data->m_lines.lines[tail].text);
        data->m_lines.lines[tail].text = buf;
        data->m_lines.lines[tail].num_messages += 1;
    }

    /* Discard lines exceeding the configured count */
    while (numberOfLines(&data->m_lines) > 1 && numberOfLines(&data->m_lines) > iflags.msg_history) {
        int num_messages = data->m_lines.lines[data->m_lines.head].num_messages;
        int i;
        for (i = 0; i < num_messages; ++i) {
            free(data->m_contents.lines[data->m_contents.head].text);
            data->m_contents.lines[data->m_contents.head].text = NULL;
            ++data->m_contents.head;
            if (data->m_contents.head >= data->m_contents.size) {
                data->m_contents.head = 0;
            }
        }
        free(data->m_lines.lines[data->m_lines.head].text);
        data->m_lines.lines[data->m_lines.head].text = NULL;
        ++data->m_lines.head;
        if (data->m_lines.head >= data->m_lines.size) {
            data->m_lines.head = 0;
        }
    }

    data->m_line_offset = 0;
    sdl2_redraw();

    /* Combine next string */
    data->m_combine = TRUE;
}

void
sdl2_message_previous(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;
    unsigned height = (unsigned)(win->m_ymax - win->m_ymin + 1);
    unsigned visible = height / win->m_line_height;

    if (visible < numberOfLines(&data->m_lines)) {
        unsigned max_scroll = numberOfLines(&data->m_lines) - visible;
        if (data->m_line_offset < max_scroll) {
            ++data->m_line_offset;
            sdl2_redraw();
        }
    }
}

void
sdl2_message_more(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;

    data->m_more = TRUE;
    sdl2_get_key(FALSE, NULL, NULL, NULL);
    data->m_more = FALSE;
}

void
sdl2_message_new_turn(struct SDL2Window *win)
{
    struct MessagePrivate *data = (struct MessagePrivate *) win->data;

    data->m_combine = FALSE;
}

void
sdl2_message_resize(struct SDL2Window *win, int x1, int y1, int x2, int y2)
{
    sdl2_window_resize(win, x1, y1, x2, y2);
}

static void
initLineList(struct LineList *list, size_t size)
{
    size_t i;

    list->size = size + 1;
    list->lines = (struct Line *) alloc(list->size * sizeof(struct Line));
    list->head = 0;
    list->tail = 0;
    for (i = 0; i < list->size; ++i) {
        list->lines[i].text = NULL;
    }
}

void
freeLineList(struct LineList *list)
{
    size_t i;

    for (i = 0; i < list->size; ++i) {
        free(list->lines[i].text);
    }
    free(list->lines);
}

size_t
numberOfLines(const struct LineList *list)
{
    size_t count = list->tail - list->head;
    if (list->tail < list->head) {
        count += list->size;
    }
    return count;
}

static struct Line *
addLine(struct LineList *list, int attr, const char *str, boolean mixed)
{
    struct Line *new_line = &list->lines[list->tail++];
    if (list->tail >= list->size) {
        list->tail = 0;
    }
    if (list->tail == list->head) {
        ++list->head;
        if (list->head >= list->size) {
            list->head = 0;
        }
    }
    free(new_line->text);
    new_line->text = (char *) dupstr(str);
    new_line->attributes = attr;
    new_line->mixed = mixed;
    return new_line;
}
