/* sdl2menu.c */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2map.h"
#include "sdl2menu.h"
#include "sdl2font.h"
#include "sdl2window.h"

struct MenuEntry
{
    int glyph;
    anything identifier;
    Uint32 ch;
    Uint32 gch;
    Uint32 def_ch;
    int attr;
    char *str;
    boolean mixed;
    boolean selected;
    int color;
    unsigned long count;
};

struct MenuColumn
{
    int x;
    int width;
};

struct MenuPrivate {
    boolean m_text;
    struct MenuEntry *m_menu;
    size_t m_menu_size;
    size_t m_menu_alloc;
    size_t m_first_line;
    char *m_prompt;
    unsigned m_page_size;
    unsigned long m_count;

    struct MenuColumn *m_columns;
    size_t m_num_columns;
};

static void sdl2_menu_create(struct SDL2Window *win);
static void sdl2_menu_destroy(struct SDL2Window *win);
static void sdl2_menu_clear(struct SDL2Window *win);
static void sdl2_menu_setVisible(struct SDL2Window *win, boolean visible);
static void sdl2_menu_put_string(struct SDL2Window *win, int attr,
        const char *str, boolean mixed);
static void sdl2_menu_redraw(struct SDL2Window *win);

static char **tabSplit(const char *str);
static void freeColumns(char **columns);
static size_t numColumns(char **columns);
static size_t numColumnsInStr(const char *str);
static void expandMenu(struct SDL2Window *win);
static void setWindowSize(struct SDL2Window *win);
static boolean selectEntry(struct SDL2Window *win, Uint32 ch, int how);
static void doPage(struct SDL2Window *win, Uint32 ch);
static int buildColumnTable(struct SDL2Window *win);

struct SDL2Window_Methods const sdl2_menu_procs = {
    sdl2_menu_create,
    sdl2_menu_destroy,
    sdl2_menu_clear,
    sdl2_menu_setVisible,
    NULL,
    sdl2_menu_put_string,
    sdl2_menu_startMenu,
    sdl2_menu_addMenu,
    sdl2_menu_endMenu,
    sdl2_menu_selectMenu,
    NULL,
    sdl2_menu_redraw
};

static void
sdl2_menu_create(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) alloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
    win->data = data;

    /* We won't show the window until all entries are present */
    sdl2_menu_setVisible(win, FALSE);

    /* Menu window font */
    sdl2_window_set_font(win, iflags.wc_font_menu, iflags.wc_fontsiz_menu,
            sdl2_font_defaultSerifFont, 20);

    sdl2_menu_clear(win);
}

static void
sdl2_menu_destroy(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    size_t i;

    for (i = 0; i < data->m_menu_size; ++i) {
        free(data->m_menu[i].str);
    }
    free(data->m_menu);
    free(data->m_prompt);
    free(data->m_columns);
    free(data);
}

static void
sdl2_menu_clear(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    size_t i;

    for (i = 0; i < data->m_menu_size; ++i) {
        free(data->m_menu[i].str);
    }
    data->m_menu_size = 0;
    data->m_first_line = 0;
    free(data->m_prompt);
    data->m_prompt = dupstr("");
    data->m_count = 0;
}

static void
sdl2_menu_redraw(struct SDL2Window *win)
{
    SDL_Color foreground, background;
    size_t i;

    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    int y;
    int width  = win->m_xmax - win->m_xmin + 1;
    int height = win->m_ymax - win->m_ymin + 1;
    SDL_Rect rect;

    foreground = sdl2_text_fg(iflags.wc_foregrnd_menu,
                              iflags.wc_backgrnd_menu,
                              ATR_NONE);
    background = sdl2_text_bg(iflags.wc_foregrnd_menu,
                              iflags.wc_backgrnd_menu,
                              ATR_NONE, 160);
    rect.x = 0;
    rect.y = 0;
    rect.w = win->m_xmax - win->m_xmin + 1;
    rect.h = win->m_ymax - win->m_ymin + 1;
    sdl2_window_fill(win, &rect, background);

    /* Border around window, one pixel wide */
    sdl2_window_draw_box(win, 0, 0, width - 1, height - 1, foreground);

    /* Draw from here downwards */
    y = 1;

    if (!data->m_text) {
        /* Display the prompt */
        sdl2_window_renderStrBG(win, data->m_prompt, 1, y,
                                foreground, background);
        y += win->m_line_height;

        /* Display a border below the prompt, one pixel wide */
        rect.x = 1;
        rect.y = y;
        rect.w = width - 2;
        rect.h = 1;
        sdl2_window_fill(win, &rect, foreground);
        y += 1;
    }

    /* Display the text for this page */
    for (i = 0; i < data->m_page_size; ++i) {
        const char *color;
        char **columns;
        size_t num_columns;
        int attr;
        size_t j = i + data->m_first_line;
        if (j >= data->m_menu_size) { break; }
        if (data->m_menu[j].color < 0) {
            color = iflags.wc_foregrnd_menu;
        } else {
            color = c_obj_colors[data->m_menu[j].color];
        }
        columns = tabSplit(data->m_menu[j].str);
        num_columns = numColumns(columns);
        attr = data->m_menu[j].attr;
        if (data->m_menu[j].identifier.a_void == NULL && num_columns == 1) {
            /* Nonselectable line with no column dividers */
            /* Assume this is a section header */
            sdl2_window_renderStrBG(win, columns[0], 1, y + i*win->m_line_height,
                    sdl2_text_fg(color,
                                 iflags.wc_backgrnd_menu,
                                 attr),
                    sdl2_text_bg(color,
                                 iflags.wc_backgrnd_menu,
                                 attr, 0));
        } else {
            size_t k;
            if (data->m_menu[j].identifier.a_void != NULL) {
                Uint32 ch = data->m_menu[j].ch;
                char tag[BUFSZ];
                if (ch == 0) { ch = data->m_menu[j].def_ch; }
                snprintf(tag, SIZE(tag), "%c %c - ",
                        !data->m_menu[j].selected ? ' ' : data->m_menu[j].count == 0 ? '*' : '#',
                        ch);
                sdl2_window_renderStrBG(win, tag, 1, y + i*win->m_line_height,
                        sdl2_text_fg(color,
                                     iflags.wc_backgrnd_menu,
                                     attr),
                        sdl2_text_bg(color,
                                     iflags.wc_backgrnd_menu,
                                     attr, 0));
            } else {
                if (strncmp(columns[0], "      ", 6) == 0) {
                    columns[0] += 6;
                }
            }
            for (k = 0; k < num_columns; ++k) {
                sdl2_window_renderStrBG(win, columns[k],
                        data->m_columns[k].x, y + i*win->m_line_height,
                        sdl2_text_fg(color,
                                     iflags.wc_backgrnd_menu,
                                     attr),
                        sdl2_text_bg(color,
                                     iflags.wc_backgrnd_menu,
                                     attr, 0));
            }
        }
        freeColumns(columns);
    }

    if (data->m_page_size < data->m_menu_size) {
        /* Display the page indicator */
        char page[BUFSZ];
        snprintf(page, SIZE(page), "Page %lu of %lu",
                (unsigned long) (data->m_first_line / data->m_page_size + 1),
                (unsigned long) ((data->m_menu_size + data->m_page_size - 1) / data->m_page_size));
        sdl2_window_renderStrBG(win, page, 1, y + data->m_page_size*win->m_line_height,
                                foreground, background);
    }
}

void
sdl2_menu_startMenu(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;

    data->m_text = FALSE;
}

void
sdl2_menu_addMenu(struct SDL2Window *win, int glyph,
                  const anything* identifier, char ch, char gch, int attr,
                  const char *str, boolean preselected)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    struct MenuEntry *entry;
    int mcolor, mattr;

    expandMenu(win);

    /* Add the entry to data->m_menu */
    entry = &data->m_menu[data->m_menu_size++];
    entry->glyph = glyph;
    entry->identifier = *identifier;
    entry->ch = ch;
    entry->gch = gch;
    entry->def_ch = 0;
    entry->attr = attr;
    entry->str = dupstr(str);
    entry->selected = preselected;
    entry->mixed = FALSE;
    entry->color = -1;
    entry->count = 0;

    if (attr == 0
    &&  get_menu_coloring(str, &mcolor, &mattr)) {
        entry->attr = mattr;
        entry->color = mcolor;
    }
}

void
sdl2_menu_endMenu(struct SDL2Window *win, const char *prompt)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;

    free(data->m_prompt);
    data->m_prompt = dupstr(prompt ? prompt : "");
}

int
sdl2_menu_selectMenu(struct SDL2Window *win, int how, menu_item ** menu_list)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    Uint32 ch;
    int count;
    size_t i;

    /* Now show the window */
    setWindowSize(win);
    sdl2_menu_setVisible(win, TRUE);
    sdl2_redraw();

    /* Read keys and select accordingly */
    data->m_count = 0;
    do {
        ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
        if (selectEntry(win, ch, how) && how == PICK_ONE) { break; }
        doPage(win, ch);
    } while (ch != '\033' && ch != '\r');

    *menu_list = NULL;
    count = 0;
    if (ch == '\033') {
        count = -1;
    } else if (how == PICK_ONE) {
        *menu_list = NULL;
        count = 0;
        for (i = 0; i < data->m_menu_size; ++i) {
            if (data->m_menu[i].selected) {
                *menu_list = (menu_item *) alloc(sizeof(menu_item) * 1);
                (*menu_list)[0].item = data->m_menu[i].identifier;
                (*menu_list)[0].count = data->m_menu[i].count ? data->m_menu[i].count : -1;
                count = 1;
                break;
            }
        }
    } else if (how == PICK_ANY) {
        size_t j;

        for (i = 0; i < data->m_menu_size; ++i) {
            if (data->m_menu[i].selected) {
                ++count;
            }
        }
        if (count != 0) {
            *menu_list = (menu_item *) alloc(sizeof(menu_item) * count);
            j = 0;
            for (i = 0; i < data->m_menu_size; ++i) {
                if (data->m_menu[i].selected) {
                    (*menu_list)[j].item = data->m_menu[i].identifier;
                    (*menu_list)[j].count = data->m_menu[i].count ? data->m_menu[i].count : -1;
                    ++j;
                }
            }
        }
    }
    sdl2_menu_setVisible(win, FALSE);
    sdl2_menu_clear(win);
    return count;
}

/* For text windows */
static void
sdl2_menu_put_string(struct SDL2Window *win, int attr, const char *str,
                     boolean mixed)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    struct MenuEntry *entry;

    expandMenu(win);
    data->m_text = TRUE;

    /* Add the entry to data->m_menu */
    entry = &data->m_menu[data->m_menu_size++];
    entry->glyph = 0;
    entry->identifier.a_void = NULL;
    entry->ch = 0;
    entry->gch = 0;
    entry->def_ch = 0;
    entry->attr = attr;
    entry->str = dupstr(str);
    entry->selected = FALSE;
    entry->color = -1;
    entry->count = 0;
    entry->mixed = mixed;
}

static void
sdl2_menu_setVisible(struct SDL2Window *win, boolean visible)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;

    win->m_visible = visible;
    if (data->m_text && visible) {
        Uint32 ch;
        setWindowSize(win);
        do {
            sdl2_redraw();
            ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
            doPage(win, ch);
        } while (ch != '\033' && ch != '\r');
    }
}

static void
setWindowSize(struct SDL2Window *win)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    int x0, y0, w, h;
    int disp_width, disp_height;
    size_t i, j;

    if (win->m_font == NULL) { return; } /* Not completely set up */

    /* Height is the sum of all line heights, plus two for borders */
    h = data->m_menu_size * win->m_line_height + 2;
    /* ...plus one more border and one more line if this is a menu */
    if (!data->m_text) {
        h += win->m_line_height + 1;
    }

    /* Width is the greatest width of any line, plus two for borders */
    w = buildColumnTable(win);

    w += 2;

    /* Add this to the width for a right margin */
    w += (sdl2_font_textSizeStr(win->m_font, "  ")).w;

    sdl2_display_size(&disp_width, &disp_height);
    if (h <= disp_height) {
        /* The height fits within the main window */
        data->m_page_size = data->m_menu_size;
        if (data->m_page_size > 52) { data->m_page_size = 52; }
    } else if (data->m_text) {
        /* We need to break the menu into pages */
        data->m_page_size = (disp_height - 2) / win->m_line_height - 1;
        if (data->m_page_size < 1) { data->m_page_size = 1; }
        h = (data->m_page_size + 1) * win->m_line_height + 3;
    } else {
        /* We need to break the menu into pages */
        data->m_page_size = (disp_height - 3) / win->m_line_height - 2;
        if (data->m_page_size < 1) { data->m_page_size = 1; }
        if (data->m_page_size > 52) { data->m_page_size = 52; }
        h = (data->m_page_size + 2) * win->m_line_height + 3;
    }

    x0 = (disp_width  - w) / 2;
    y0 = (disp_height - h) / 2;
    sdl2_window_resize(win, x0, y0, x0 + w - 1, y0 + h - 1);

    /* Assign default letters to entries without a letter */
    for (i = 0; i < data->m_menu_size; i += data->m_page_size) {
        unsigned l = 0;
        for (j = 0; j < data->m_page_size; ++j) {
            if (i+j >= data->m_menu_size) { break; }
            if (data->m_menu[i+j].identifier.a_void != NULL) {
                /* l never overflows because the page size is limited to 52 */
                data->m_menu[i+j].def_ch = letters[l++];
            }
        }
    }
}

static int
buildColumnTable(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    size_t i;
    int width;

    if (data->m_text) {
        width = 0;
    } else {
        width = (sdl2_font_textSizeStr(win->m_font, data->m_prompt)).w;
    }

    /* Clear column table */
    data->m_num_columns = 0;
    for (i = 0; i < data->m_menu_size; ++i) {
        size_t ncols = numColumnsInStr(data->m_menu[i].str);
        if (data->m_num_columns < ncols) {
            data->m_num_columns = ncols;
        }
    }
    free(data->m_columns);
    data->m_columns = (struct MenuColumn *)
            alloc(data->m_num_columns * sizeof(struct MenuColumn));
    for (i = 0; i < data->m_num_columns; ++i) {
        data->m_columns[i].x = 0;
        data->m_columns[i].width = 0;
    }

    /* Add and extend columns */
    for (i = 0; i < data->m_menu_size; ++i) {
        char **columns = tabSplit(data->m_menu[i].str);
        size_t num_columns = numColumns(columns);
        if (data->m_menu[i].identifier.a_void == NULL && num_columns == 1) {
            /* Nonselectable line with no column dividers */
            /* Assume this is a section header */
            int w2 = (sdl2_font_textSizeStr(win->m_font, data->m_menu[i].str)).w;
            if (width < w2) {
                width = w2;
            }
        } else {
            /* Widen each column to fit the line */
            size_t j;
            for (j = 0; columns[j] != NULL; ++j) {
                int w2 = (sdl2_font_textSizeStr(win->m_font, columns[j])).w;
                if (data->m_columns[j].width < w2) {
                    data->m_columns[j].width = w2;
                }
            }
        }
        freeColumns(columns);
    }

    if (data->m_num_columns != 0) {
        /* Place the left edge of each column */
        int pad = (sdl2_font_textSizeStr(win->m_font, "  ")).w;
        int w2;
        if (!data->m_text) {
            data->m_columns[0].x = sdl2_font_textSizeStr(win->m_font, "# A -").w;
        }
        for (i = 1; i < data->m_num_columns; ++i) {
            data->m_columns[i].x = data->m_columns[i-1].x + data->m_columns[i-1].width + pad;
        }

        /* Extend overall width for widest columned line */
        w2 = data->m_columns[data->m_num_columns - 1].x
           + data->m_columns[data->m_num_columns - 1].width
           + pad;
        if (width < w2) {
            width = w2;
        }
    }

    /* Return overall width */
    return width;
}

static boolean
selectEntry(struct SDL2Window *win, Uint32 ch, int how)
{
    size_t i;
    boolean selected;
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;
    if (how == PICK_NONE) { return FALSE; }

    /* Process count */
    if ('0' <= ch && ch <= '9') {
        unsigned long count2;
        unsigned digit;

        count2 = data->m_count * 10;
        if (count2 / 10 != data->m_count) { return FALSE; } /* overflow */
        digit = ch - '0';
        count2 += digit;
        if (count2 >= digit) {
            data->m_count = count2;
        }
        return FALSE;
    }

    if (how == PICK_ANY) {
        switch (ch) {
        case MENU_SELECT_ALL:
            data->m_count = 0;
            for (i = 0; i < data->m_menu_size; ++i) {
                if (data->m_menu[i].identifier.a_void == NULL) { continue; }
                data->m_menu[i].selected = TRUE;
                data->m_menu[i].count = 0;
            }
            sdl2_redraw();
            return TRUE;

        case MENU_UNSELECT_ALL:
            data->m_count = 0;
            for (i = 0; i < data->m_menu_size; ++i) {
                if (data->m_menu[i].identifier.a_void == NULL) { continue; }
                data->m_menu[i].selected = FALSE;
                data->m_menu[i].count = 0;
            }
            sdl2_redraw();
            return TRUE;

        case MENU_INVERT_ALL:
            data->m_count = 0;
            for (i = 0; i < data->m_menu_size; ++i) {
                if (data->m_menu[i].identifier.a_void == NULL) { continue; }
                data->m_menu[i].selected = !data->m_menu[i].selected;
                data->m_menu[i].count = 0;
            }
            sdl2_redraw();
            return TRUE;

        case MENU_SELECT_PAGE:
            data->m_count = 0;
            for (i = 0; i < data->m_page_size; ++i) {
                size_t j = i + data->m_first_line;
                if (j >= data->m_menu_size) { break; }
                if (data->m_menu[j].identifier.a_void == NULL) { continue; }
                data->m_menu[j].selected = TRUE;
                data->m_menu[j].count = 0;
            }
            sdl2_redraw();
            return TRUE;

        case MENU_UNSELECT_PAGE:
            data->m_count = 0;
            for (i = 0; i < data->m_page_size; ++i) {
                size_t j = i + data->m_first_line;
                if (j >= data->m_menu_size) { break; }
                if (data->m_menu[j].identifier.a_void == NULL) { continue; }
                data->m_menu[j].selected = FALSE;
                data->m_menu[j].count = 0;
            }
            sdl2_redraw();
            return TRUE;

        case MENU_INVERT_PAGE:
            data->m_count = 0;
            for (i = 0; i < data->m_page_size; ++i) {
                size_t j = i + data->m_first_line;
                if (j >= data->m_menu_size) { break; }
                if (data->m_menu[j].identifier.a_void == NULL) { continue; }
                data->m_menu[j].selected = !data->m_menu[i].selected;
                data->m_menu[j].count = 0;
            }
            sdl2_redraw();
            return TRUE;
        }
    }

    selected = FALSE;

    for (i = 0; i < data->m_menu_size; ++i) {
        if (data->m_menu[i].identifier.a_void == NULL) { continue; }
        if (ch == data->m_menu[i].ch || ch == data->m_menu[i].gch
        ||  (data->m_menu[i].ch == 0 && ch == data->m_menu[i].def_ch && data->m_first_line <= i && i < data->m_first_line + data->m_page_size)) {
            if (data->m_count != 0) {
                data->m_menu[i].selected = TRUE;
                data->m_menu[i].count = data->m_count;
            } else {
                data->m_menu[i].selected = !data->m_menu[i].selected;
                data->m_menu[i].count = 0;
            }
            selected = TRUE;
        }
    }

    if (selected) {
        data->m_count = 0;
        sdl2_redraw();
    }

    return selected;
}

static void
doPage(struct SDL2Window *win, Uint32 ch)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;

    switch (ch) {
    case MENU_FIRST_PAGE:
        if (data->m_first_line != 0) {
            data->m_first_line = 0;
            sdl2_redraw();
        }
        break;

    case MENU_LAST_PAGE:
        if (data->m_page_size < data->m_menu_size) {
            unsigned pagenum = (data->m_menu_size - 1) / data->m_page_size;
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
        if (data->m_first_line + data->m_page_size < data->m_menu_size) {
            data->m_first_line += data->m_page_size;
            sdl2_redraw();
        }
        break;
    }
}

static void
expandMenu(struct SDL2Window *win)
{
    struct MenuPrivate *data = (struct MenuPrivate *) win->data;

    if (data->m_menu_size >= data->m_menu_alloc) {
        struct MenuEntry *new_menu;
        size_t i;

        data->m_menu_alloc += 64;
        new_menu = (struct MenuEntry *)
                alloc(data->m_menu_alloc * sizeof(struct MenuEntry));
        for (i = 0; i < data->m_menu_size; ++i) {
            new_menu[i] = data->m_menu[i];
        }
        free(data->m_menu);
        data->m_menu = new_menu;
    }
}

/* Returned data structure is freed by freeColumns */
static char **
tabSplit(const char *str)
{
    const char *p, *q;
    char **columns;
    size_t num_columns, col, len;

    num_columns = numColumnsInStr(str);
    columns = (char **) alloc((num_columns + 1) * sizeof(columns[0]));

    col = 0;
    p = str;
    while (1) {
        q = index(p, '\t');
        if (q == NULL) {
            columns[col++] = dupstr(p);
            break;
        }
        len = q - p;
        columns[col] = (char *) alloc(len + 1U);
        memcpy(columns[col], p, len);
        columns[col][len] = '\0';
        ++col;
        p = q + 1;
    }

    columns[col] = NULL;
    return columns;
}

static void
freeColumns(char **columns)
{
    if (columns != NULL) {
        size_t i;

        for (i = 0; columns[i] != NULL; ++i) {
            free(columns[i]);
        }
        free(columns);
    }
}

static size_t
numColumns(char **columns)
{
    size_t cols;

    for (cols = 0; columns[cols] != NULL; ++cols) {}
    return cols;
}

/* Count the columns in the string */
static size_t
numColumnsInStr(const char *str)
{
    size_t num_columns = 1;
    size_t i;
    for (i = 0; str[i] != 0; ++i) {
        if (str[i] == '\t') {
            ++num_columns;
        }
    }
    return num_columns;
}
