/* sdl2status.c */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2status.h"
#include "sdl2font.h"
#include "sdl2window.h"

struct StatusItem {
    boolean enable;
    const char *name;
    const char *format;
    char *value;
    int change;
    int percent;
    int color;
};

struct StatusPrivate {
    struct StatusItem status[MAXBLSTATS];
    unsigned long conditions;
    const unsigned long *colormasks;
    unsigned width;
    unsigned column2;
};

static void sdl2_status_create(struct SDL2Window *win);
static void sdl2_status_destroy(struct SDL2Window *win);
static void sdl2_status_redraw(struct SDL2Window *win);

static SDL_Color sdl2_status_fg(int color);
static SDL_Color sdl2_status_bg(int color);

struct SDL2Window_Methods const sdl2_status_procs = {
    sdl2_status_create,
    sdl2_status_destroy,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sdl2_status_redraw
};

static void
sdl2_status_create(struct SDL2Window *win)
{
    struct StatusPrivate *data = (struct StatusPrivate *) alloc(sizeof(*data));
    win->data = data;
    win->m_visible = TRUE;

    memset(data, 0, sizeof(*data));

    /* Status window font */
    sdl2_window_set_font(win, iflags.wc_font_status, iflags.wc_fontsiz_status,
            sdl2_font_defaultSerifFont, 20);
}

static void
sdl2_status_destroy(struct SDL2Window *win)
{
    struct StatusPrivate *data = (struct StatusPrivate *) win->data;
    size_t i;

    for (i = 0; i < SIZE(data->status); ++i) {
        free(data->status[i].value);
    }

    free(data);
}

int
sdl2_status_width_hint(struct SDL2Window *win)
{
    struct StatusPrivate *data = (struct StatusPrivate *) win->data;

    if (data->width == 0) {
        SDL_Rect r = sdl2_font_textSizeChar(win->m_font, 'X');
        data->width = r.w * 20;
    }
    return data->width;
}

int
sdl2_status_height_hint(struct SDL2Window *win)
{
    return win->m_line_height * 2;
}

void
sdl2_status_resize(struct SDL2Window *win, int x1, int y1, int x2, int y2)
{
    sdl2_window_resize(win, x1, y1, x2, y2);
}

static void
sdl2_status_redraw(struct SDL2Window *win)
{
    struct CondData {
        const char *name;
        long bit;
    };
    static const struct CondData conditions[] = {
        { "Stone",    BL_MASK_STONE    },
        { "Slime",    BL_MASK_SLIME    },
        { "Strngl",   BL_MASK_STRNGL   },
        { "FoodPois", BL_MASK_FOODPOIS },
        { "TermIll",  BL_MASK_TERMILL  },
        { "Blind",    BL_MASK_BLIND    },
        { "Deaf",     BL_MASK_DEAF     },
        { "Stun",     BL_MASK_STUN     },
        { "Conf",     BL_MASK_CONF     },
        { "Hallu",    BL_MASK_HALLU    },
        { "Lev",      BL_MASK_LEV      },
        { "Fly",      BL_MASK_FLY      },
        { "Ride",     BL_MASK_RIDE     },
    };

    static const char *label_list[] = {
        /* BL_TITLE     */ "",
        /* BL_STR       */ "Strength:",
        /* BL_DX        */ "Dexterity:",
        /* BL_CO        */ "Constitution:",
        /* BL_IN        */ "Intelligence:",
        /* BL_WI        */ "Wisdom:",
        /* BL_CH        */ "Charisma:",
        /* BL_ALIGN     */ "Alignment:",
        /* BL_SCORE     */ "",
        /* BL_CAP       */ "",
        /* BL_GOLD      */ "Gold:",
        /* BL_ENE       */ "Magic Power:",
        /* BL_ENEMAX    */ "",
        /* BL_XP        */ "Experience:",
        /* BL_AC        */ "Armor Class:",
        /* BL_HD        */ "Hit Dice:",
        /* BL_TIME      */ "Time:",
        /* BL_HUNGER    */ "",
        /* BL_HP        */ "Hit Points:",
        /* BL_HPMAX     */ "",
        /* BL_LEVELDESC */ "",
        /* BL_EXP       */ "",
        /* BL_CONDITION */ ""
    };

    struct ItemData {
        const char *str;
        int color;
        boolean mixed;
    };
    struct StatusPrivate *data = (struct StatusPrivate *) win->data;
    SDL_Color background;
    SDL_Rect rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = win->m_xmax - win->m_xmin - 1;
    rect.h = win->m_ymax - win->m_ymin - 1;
    background = sdl2_text_bg(iflags.wc_foregrnd_status,
                              iflags.wc_backgrnd_status,
                              ATR_NONE, 255);
    sdl2_window_fill(win, &rect, background);

    if (iflags.wc_align_status == ALIGN_LEFT ||
        iflags.wc_align_status == ALIGN_RIGHT) {
        int color;
        SDL_Color fg, bg;
        int y;
        int i;

        /* Vertical status window */
        /* If we don't know the placement of the second column, find that
           now */
        if (data->column2 == 0) {
            for (i = 0; i < SIZE(label_list); ++i) {
                rect = sdl2_font_textSizeStr(win->m_font, label_list[i]);
                if (data->column2 < (unsigned) rect.w) {
                    data->column2 = (unsigned) rect.w;
                }
            }
            rect = sdl2_font_textSizeStr(win->m_font, "  ");
            data->column2 += rect.w;
        }

        y = 0;
        if (data->status[BL_TITLE].enable) {
            color = data->status[BL_TITLE].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_TITLE].value,
                                           0, y, fg, bg);
            y += rect.h;
        }
        if (data->status[BL_LEVELDESC].enable) {
            color = data->status[BL_LEVELDESC].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_LEVELDESC].value,
                                           0, y, fg, bg);
            y += rect.h;
        }
        y += win->m_line_height;
        for (i = BL_STR; i <= BL_CH; ++i) {
            color = data->status[i].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[i],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[i].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_ALIGN].enable) {
            color = data->status[BL_ALIGN].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_ALIGN],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_ALIGN].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }

        /* TODO: dungeon level */

        if (data->status[BL_GOLD].enable) {
            char *str;
            color = data->status[BL_GOLD].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_GOLD],
                                           0, y, fg, bg);
            str = strchr(data->status[BL_GOLD].value, ':');
            str = str ? str + 1 : data->status[BL_GOLD].value;
            rect = sdl2_window_render_mixed(win, str,
                                            data->column2, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_HP].enable) {
            color = data->status[BL_HP].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_HP],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_HP].value,
                                           data->column2, y, fg, bg);
            if (data->status[BL_HPMAX].enable) {
                color = data->status[BL_HPMAX].color;
                fg = sdl2_status_fg(color);
                bg = sdl2_status_bg(color);
                rect = sdl2_window_renderStrBG(win, "(",
                                               rect.x + rect.w, y, fg, bg);
                rect = sdl2_window_renderStrBG(win,
                                               data->status[BL_HPMAX].value,
                                               rect.x + rect.w, y, fg, bg);
                rect = sdl2_window_renderStrBG(win, ")",
                                               rect.x + rect.w, y, fg, bg);
            }
            y += rect.h;
        }

        if (data->status[BL_ENE].enable) {
            color = data->status[BL_ENE].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_ENE],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_ENE].value,
                                           data->column2, y, fg, bg);
            if (data->status[BL_ENEMAX].enable) {
                color = data->status[BL_ENEMAX].color;
                fg = sdl2_status_fg(color);
                bg = sdl2_status_bg(color);
                rect = sdl2_window_renderStrBG(win, "(",
                                               rect.x + rect.w, y, fg, bg);
                rect = sdl2_window_renderStrBG(win,
                                               data->status[BL_ENEMAX].value,
                                               rect.x + rect.w, y, fg, bg);
                rect = sdl2_window_renderStrBG(win, ")",
                                               rect.x + rect.w, y, fg, bg);
            }
            y += rect.h;
        }
        if (data->status[BL_AC].enable) {
            color = data->status[BL_AC].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_AC],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_AC].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_XP].enable) {
            color = data->status[BL_XP].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_XP],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_XP].value,
                                           data->column2, y, fg, bg);
            if (data->status[BL_EXP].enable) {
                color = data->status[BL_EXP].color;
                fg = sdl2_status_fg(color);
                bg = sdl2_status_bg(color);
                rect = sdl2_window_renderStrBG(win, "/",
                                               rect.x + rect.w, y, fg, bg);
                rect = sdl2_window_renderStrBG(win,
                                               data->status[BL_EXP].value,
                                               rect.x + rect.w, y, fg, bg);
            }
            y += rect.h;
        }

        if (data->status[BL_HD].enable) {
            color = data->status[BL_HD].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_HD],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_HD].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_TIME].enable) {
            color = data->status[BL_TIME].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_TIME],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_TIME].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }
        if (data->status[BL_SCORE].enable) {
            color = data->status[BL_SCORE].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           label_list[BL_SCORE],
                                           0, y, fg, bg);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_SCORE].value,
                                           data->column2, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_HUNGER].enable
                && data->status[BL_HUNGER].value[0] != ' '
                && data->status[BL_HUNGER].value[0] != '\0') {
            color = data->status[BL_HUNGER].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_HUNGER].value,
                                           0, y, fg, bg);
            y += rect.h;
        }

        if (data->status[BL_CAP].enable
                && data->status[BL_CAP].value[0] != ' '
                && data->status[BL_CAP].value[0] != '\0') {
            color = data->status[BL_CAP].color;
            fg = sdl2_status_fg(color);
            bg = sdl2_status_bg(color);
            rect = sdl2_window_renderStrBG(win,
                                           data->status[BL_CAP].value,
                                           0, y, fg, bg);
            y += rect.h;
        }

        for (i = 0; i < SIZE(conditions); ++i) {
            if ((data->conditions & conditions[i].bit) != 0) {
                const char *str = conditions[i].name;
                unsigned j;
                color = NO_COLOR;
                for (j = 0; j < CLR_MAX; ++j) {
                    if (data->colormasks[j] & conditions[i].bit) {
                        color = j;
                        break;
                    }
                }
                for (; j < BL_ATTCLR_MAX; ++j) {
                    if (data->colormasks[j] & conditions[i].bit) {
                        color |= (j - CLR_MAX) << 8;
                        break;
                    }
                }
                fg = sdl2_status_fg(color);
                bg = sdl2_status_bg(color);
                rect = sdl2_window_renderStrBG(win, str, 0, y, fg, bg);
                y += rect.h;
            }
        }
    } else {
        /* Horizontal status window */
        struct ItemData items[100];
        unsigned num_items, i, j;
        int x, y, h;

        /* First line */
        num_items = 0;
        if (data->status[BL_TITLE].enable) {
            items[num_items].str = data->status[BL_TITLE].value;
            items[num_items].color = data->status[BL_TITLE].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_STR].enable) {
            items[num_items].str = "St:";
            items[num_items].color = data->status[BL_STR].color;
            ++num_items;
            items[num_items].str = data->status[BL_STR].value;
            items[num_items].color = data->status[BL_STR].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_DX].enable) {
            items[num_items].str = "Dx:";
            items[num_items].color = data->status[BL_DX].color;
            ++num_items;
            items[num_items].str = data->status[BL_DX].value;
            items[num_items].color = data->status[BL_DX].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_CO].enable) {
            items[num_items].str = "Co:";
            items[num_items].color = data->status[BL_CO].color;
            ++num_items;
            items[num_items].str = data->status[BL_CO].value;
            items[num_items].color = data->status[BL_CO].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_IN].enable) {
            items[num_items].str = "In:";
            items[num_items].color = data->status[BL_IN].color;
            ++num_items;
            items[num_items].str = data->status[BL_IN].value;
            items[num_items].color = data->status[BL_IN].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_WI].enable) {
            items[num_items].str = "Wi:";
            items[num_items].color = data->status[BL_WI].color;
            ++num_items;
            items[num_items].str = data->status[BL_WI].value;
            items[num_items].color = data->status[BL_WI].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_CH].enable) {
            items[num_items].str = "Ch:";
            items[num_items].color = data->status[BL_CH].color;
            ++num_items;
            items[num_items].str = data->status[BL_CH].value;
            items[num_items].color = data->status[BL_CH].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_ALIGN].enable) {
            items[num_items].str = data->status[BL_ALIGN].value;
            items[num_items].color = data->status[BL_ALIGN].color;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            ++num_items;
        }
        if (data->status[BL_SCORE].enable) {
            items[num_items].str = "S:";
            items[num_items].color = data->status[BL_SCORE].color;
            ++num_items;
            items[num_items].str = data->status[BL_SCORE].value;
            items[num_items].color = data->status[BL_SCORE].color;
            ++num_items;
        }
        if (num_items > 0 && strcmp(items[num_items - 1].str, " ") == 0) {
            --num_items;
        }

        x = 0;
        y = 0;
        h = 0;
        for (i = 0; i < num_items; ++i) {
            SDL_Color fg = sdl2_status_fg(items[i].color);
            SDL_Color bg = sdl2_status_bg(items[i].color);
            rect = sdl2_window_renderStrBG(win, items[i].str, x, y, fg, bg);
            x += rect.w;
            h = (h > rect.h) ? h : rect.h;
        }

        /* Fill to right side of screen */
        rect.x = x;
        rect.y = y;
        rect.w = win->m_xmax - x + 1;
        rect.h = h;
        sdl2_window_fill(win, &rect, background);

        /* Second line: */
        num_items = 0;
        if (data->status[BL_LEVELDESC].enable) {
            items[num_items].str = data->status[BL_LEVELDESC].value;
            items[num_items].color = data->status[BL_LEVELDESC].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_GOLD].enable) {
            items[num_items].str = data->status[BL_GOLD].value;
            items[num_items].color = data->status[BL_GOLD].color;
            items[num_items].mixed = TRUE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_HP].enable) {
            items[num_items].str = "HP:";
            items[num_items].color = data->status[BL_HP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_HP].value;
            items[num_items].color = data->status[BL_HP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_HPMAX].enable) {
            items[num_items].str = "(";
            items[num_items].color = data->status[BL_HPMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_HPMAX].value;
            items[num_items].color = data->status[BL_HPMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = ")";
            items[num_items].color = data->status[BL_HPMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_HP].enable || data->status[BL_HPMAX].enable) {
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_ENE].enable) {
            items[num_items].str = "Pw:";
            items[num_items].color = data->status[BL_ENE].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_ENE].value;
            items[num_items].color = data->status[BL_ENE].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_ENEMAX].enable) {
            items[num_items].str = "(";
            items[num_items].color = data->status[BL_ENEMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_ENEMAX].value;
            items[num_items].color = data->status[BL_ENEMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = ")";
            items[num_items].color = data->status[BL_ENEMAX].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_ENE].enable || data->status[BL_ENEMAX].enable) {
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_AC].enable) {
            items[num_items].str = "AC:";
            items[num_items].color = data->status[BL_AC].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_AC].value;
            items[num_items].color = data->status[BL_AC].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_XP].enable) {
            items[num_items].str = "Xp:";
            items[num_items].color = data->status[BL_XP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_XP].value;
            items[num_items].color = data->status[BL_XP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_EXP].enable) {
            items[num_items].str = "/";
            items[num_items].color = data->status[BL_EXP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_EXP].value;
            items[num_items].color = data->status[BL_EXP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_XP].enable || data->status[BL_EXP].enable) {
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_HD].enable) {
            items[num_items].str = "HD:";
            items[num_items].color = data->status[BL_HD].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_HD].value;
            items[num_items].color = data->status[BL_HD].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_TIME].enable) {
            items[num_items].str = "T:";
            items[num_items].color = data->status[BL_TIME].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = data->status[BL_TIME].value;
            items[num_items].color = data->status[BL_TIME].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_HUNGER].enable
                && data->status[BL_HUNGER].value[0] != ' '
                && data->status[BL_HUNGER].value[0] != '\0') {
            items[num_items].str = data->status[BL_HUNGER].value;
            items[num_items].color = data->status[BL_HUNGER].color;
            items[num_items].mixed = FALSE;
            ++num_items;
            items[num_items].str = " ";
            items[num_items].color = NO_COLOR;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (data->status[BL_CAP].enable
                && data->status[BL_CAP].value[0] != ' '
                && data->status[BL_CAP].value[0] != '\0') {
            items[num_items].str = data->status[BL_CAP].value;
            items[num_items].color = data->status[BL_CAP].color;
            items[num_items].mixed = FALSE;
            ++num_items;
        }
        if (num_items > 0 && strcmp(items[num_items - 1].str, " ") == 0) {
            --num_items;
        }

        for (i = 0; i < SIZE(conditions); ++i) {
            if ((data->conditions & conditions[i].bit) != 0) {
                items[num_items].str = " ";
                items[num_items].color = NO_COLOR;
                items[num_items].mixed = FALSE;
                ++num_items;
                items[num_items].str = conditions[i].name;
                items[num_items].color = NO_COLOR;
                items[num_items].mixed = FALSE;
                for (j = 0; j < CLR_MAX; ++j) {
                    if (data->colormasks[j] & conditions[i].bit) {
                        items[num_items].color = j;
                        break;
                    }
                }
                for (; j < BL_ATTCLR_MAX; ++j) {
                    if (data->colormasks[j] & conditions[i].bit) {
                        items[num_items].color |= (j - CLR_MAX) << 8;
                        break;
                    }
                }
                ++num_items;
            }
        }

        x = 0;
        y = h;
        h = 0;
        for (i = 0; i < num_items; ++i) {
            SDL_Color fg = sdl2_status_fg(items[i].color);
            SDL_Color bg = sdl2_status_bg(items[i].color);
            if (items[i].mixed) {
                rect = sdl2_window_render_mixed(win, items[i].str, x, y, fg, bg);
            } else {
                rect = sdl2_window_renderStrBG(win, items[i].str, x, y, fg, bg);
            }
            x += rect.w;
            h = (h > rect.h) ? h : rect.h;
        }

        /* Fill to right side of screen */
        rect.x = x;
        rect.y = y;
        rect.w = win->m_xmax - x + 1;
        rect.h = h;
        sdl2_window_fill(win, &rect, background);
    }
}

static SDL_Color
sdl2_status_fg(int color)
{
    const char *fgcolor = iflags.wc_foregrnd_status;
    const char *bgcolor = iflags.wc_backgrnd_status;
    int attr = color >> 8;
    color &= 0xFF;

    if (color != NO_COLOR) {
        fgcolor = c_obj_colors[color];
    }

    return sdl2_text_fg(fgcolor, bgcolor, attr);
}

static SDL_Color
sdl2_status_bg(int color)
{
    const char *fgcolor = iflags.wc_foregrnd_status;
    const char *bgcolor = iflags.wc_backgrnd_status;
    int attr = color >> 8;
    color &= 0xFF;

    if (color != NO_COLOR) {
        fgcolor = c_obj_colors[color];
    }

    return sdl2_text_bg(fgcolor, bgcolor, attr, 255);
}

/*ARGSUSED*/
void
sdl2_status_do_enablefield(struct SDL2Window *win, int index, const char *name,
                           const char *format, boolean enable)
{
    struct StatusPrivate *data = (struct StatusPrivate *) win->data;

    if (index < 0 || index >= MAXBLSTATS) return;

    data->status[index].name = name;
    data->status[index].format = format;
    data->status[index].enable = enable;
    if (index != BL_CONDITION) {
        free(data->status[index].value);
        data->status[index].value = enable ? dupstr("") : NULL;
    }
}

/*ARGSUSED*/
void
sdl2_status_do_update(struct SDL2Window *win, int index, genericptr_t ptr,
                      int change, int percent, int color,
                      const unsigned long *colormasks)
{
    struct StatusPrivate *data = (struct StatusPrivate *) win->data;

    if (index < 0 || index >= MAXBLSTATS) return;
    if (!data->status[index].enable) return;

    if (index == BL_CONDITION) {
        data->conditions = *(unsigned long *) ptr;
        data->colormasks = colormasks;
    } else {
        free(data->status[index].value);
        data->status[index].value = dupstr((char *) ptr);
    }
    data->status[index].change = change;
    data->status[index].percent = percent;
    data->status[index].color = color;
}