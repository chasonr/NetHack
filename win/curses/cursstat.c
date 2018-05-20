#include "curses.h"
#undef getch
#include "hack.h"
#include "wincurs.h"
#include "cursstat.h"

/* Status window functions for curses interface */

/* Private declarations */

typedef struct nhs {
    char *txt;
    boolean display;
    int highlight_turns;
    int highlight_color;
    char *label;
} nhstat;

static void set_labels(int label_width);

static void color_stat(int color, int onoff);

static nhstat status[MAXBLSTATS];
static unsigned long condition_bits;
static const unsigned long *cond_hilites;

#define COMPACT_LABELS  1
#define NORMAL_LABELS   2
#define WIDE_LABELS     3

/* Update the status win - this is called when NetHack would normally
write to the status window, so we know somwthing has changed.  We
override the write and update what needs to be updated ourselves. */

void
curses_update_stats(boolean redraw)
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

    char buf[BUFSZ];
    int count, enc, orient, sx_start, hp, hpmax, labels, swidth,
        sheight, sx_end, sy_end;
    WINDOW *win = curses_get_nhwin(STATUS_WIN);
    static int prev_labels = -1;
    static boolean first = TRUE;
    static boolean horiz;
    int sx = 0;
    int sy = 0;
    boolean border = curses_window_has_border(STATUS_WIN);

    curses_get_window_size(STATUS_WIN, &sheight, &swidth);

    if (border) {
        sx++;
        sy++;
        swidth--;
        sheight--;
    }

    sx_end = swidth - 1;
    sy_end = sheight - 1;
    sx_start = sx;

    if (redraw) {
        orient = curses_get_window_orientation(STATUS_WIN);

        if ((orient == ALIGN_RIGHT) || (orient == ALIGN_LEFT)) {
            horiz = FALSE;
        } else {
            horiz = TRUE;
        }
    }

    if (horiz) {
        if (curses_term_cols >= 80) {
            labels = NORMAL_LABELS;
        } else {
            labels = COMPACT_LABELS;
        }
    } else {
        labels = WIDE_LABELS;
    }

    if (labels != prev_labels) {
        set_labels(labels);
        prev_labels = labels;
    }

    curses_clear_nhwin(STATUS_WIN);

    /* Line 1 */

    /* Player name and title */
    if (status[BL_TITLE].display && labels != COMPACT_LABELS) {
        if (status[BL_TITLE].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_TITLE].label);
            sx += strlen(status[BL_TITLE].label);
        }

        if (status[BL_TITLE].txt != NULL) {
            color_stat(status[BL_TITLE].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_TITLE].txt);
            color_stat(status[BL_TITLE].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_TITLE].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Add dungeon name and level if status window is vertical */
    if (!horiz) {
        sprintf(buf, "%s", dungeons[u.uz.dnum].dname);
        mvwaddstr(win, sy, sx, buf);
        sy += 2;
    }

    /* Strength */
    if (status[BL_STR].display) {
        if (status[BL_STR].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_STR].label);
            sx += strlen(status[BL_STR].label);
        }

        if (status[BL_STR].txt != NULL) {
            color_stat(status[BL_STR].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_STR].txt);
            color_stat(status[BL_STR].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_STR].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Intelligence */
    if (status[BL_IN].display) {
        if (status[BL_IN].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_IN].label);
            sx += strlen(status[BL_IN].label);
        }

        if (status[BL_IN].label != NULL) {
            color_stat(status[BL_IN].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_IN].txt);
            color_stat(status[BL_IN].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_IN].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Wisdom */
    if (status[BL_WI].display) {
        if (status[BL_WI].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_WI].label);
            sx += strlen(status[BL_WI].label);
        }

        if (status[BL_WI].label != NULL) {
            color_stat(status[BL_WI].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_WI].txt);
            color_stat(status[BL_WI].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_WI].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Dexterity */
    if (status[BL_DX].display) {
        if (status[BL_DX].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_DX].label);
            sx += strlen(status[BL_DX].label);
        }

        if (status[BL_DX].txt != NULL) {
            color_stat(status[BL_DX].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_DX].txt);
            color_stat(status[BL_DX].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_DX].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Constitution */
    if (status[BL_CO].display) {
        if (status[BL_CO].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_CO].label);
            sx += strlen(status[BL_CO].label);
        }

        if (status[BL_CO].txt != NULL) {
            color_stat(status[BL_CO].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_CO].txt);
            color_stat(status[BL_CO].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_CO].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Charisma */
    if (status[BL_CH].display) {
        if (status[BL_CH].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_CH].label);
            sx += strlen(status[BL_CH].label);
        }

        if (status[BL_CH].label != NULL) {
            color_stat(status[BL_CH].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_CH].txt);
            color_stat(status[BL_CH].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_CH].txt) + 1;
            } else {
                sx = sx_start;
                sy ++;
            }
        }
    }

    /* Alignment */
    if (status[BL_ALIGN].display) {
        if (status[BL_ALIGN].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_ALIGN].label);
            sx += strlen(status[BL_ALIGN].label);
        }

        if (status[BL_ALIGN].txt != NULL) {
            color_stat(status[BL_ALIGN].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_ALIGN].txt);
            color_stat(status[BL_ALIGN].highlight_color, OFF);
        }
    }

    /* Line 2 */

    sx = sx_start;
    sy++;

    /* Dungeon Level */
    if (status[BL_LEVELDESC].display && status[BL_LEVELDESC].txt != NULL) {
        char *where = strchr(status[BL_LEVELDESC].txt, ':');
        if (where || !horiz) {
            if (status[BL_LEVELDESC].label != NULL) {
                mvwaddstr(win, sy, sx, status[BL_LEVELDESC].label);
                sx += strlen(status[BL_LEVELDESC].label);
            }
        }

        where = where ? where + 1 : status[BL_LEVELDESC].txt;

        color_stat(status[BL_LEVELDESC].highlight_color, ON);
        mvwaddstr(win, sy, sx, where);
        color_stat(status[BL_LEVELDESC].highlight_color, OFF);

        if (horiz) {
            sx += strlen(where) + 1;
        } else {
            sx = sx_start;
            sy++;
        }
    }

    /* Gold */
    if (status[BL_GOLD].display) {
        if (status[BL_GOLD].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_GOLD].label);
            sx += strlen(status[BL_GOLD].label);
        }

        if (status[BL_GOLD].txt != NULL) {
            char *where = strchr(status[BL_GOLD].txt, ':');
            where = where ? where + 1 : status[BL_GOLD].txt;
            color_stat(status[BL_GOLD].highlight_color, ON);
            mvwaddstr(win, sy, sx, where);
            color_stat(status[BL_GOLD].highlight_color, OFF);

            if (horiz) {
                sx += strlen(where) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }


    /* Hit Points */

    if (u.mtimedone) {  /* Currently polymorphed - show monster HP */
        hp = u.mh;
        hpmax = u.mhmax;
    } else { /* Not polymorphed */
        hp = u.uhp;
        hpmax = u.uhpmax;
    }

    if (status[BL_HP].display) {
        if (status[BL_HP].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_HP].label);
            sx += strlen(status[BL_HP].label);
        }

        if (status[BL_HP].txt != NULL) {
            color_stat(status[BL_HP].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_HP].txt);
            color_stat(status[BL_HP].highlight_color, OFF);

            sx += strlen(status[BL_HP].txt);
        }
    }

    /* Max Hit Points */

    if (status[BL_HPMAX].display) {
        if (status[BL_HPMAX].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_HPMAX].label);
            sx += strlen(status[BL_HPMAX].label);
        }

        if (status[BL_HPMAX].txt != NULL) {
            color_stat(status[BL_HPMAX].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_HPMAX].txt);
            color_stat(status[BL_HPMAX].highlight_color, OFF);

            if (horiz) {
                color_stat(status[BL_HPMAX].highlight_color, ON);
                sx += strlen(status[BL_HPMAX].txt) + 1;
                color_stat(status[BL_HPMAX].highlight_color, OFF);
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Power */
    if (status[BL_ENE].display) {
        if (status[BL_ENE].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_ENE].label);
            sx += strlen(status[BL_ENE].label);
        }

        if (status[BL_ENE].txt != NULL) {
            color_stat(status[BL_ENE].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_ENE].txt);
            color_stat(status[BL_ENE].highlight_color, OFF);

            sx += strlen(status[BL_ENE].txt);
        }
    }

    /* Max Power */
    if (status[BL_ENEMAX].display) {
        if (status[BL_ENEMAX].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_ENEMAX].label);
            sx += strlen(status[BL_ENEMAX].label);
        }

        if (status[BL_ENEMAX].txt != NULL) {
            color_stat(status[BL_ENEMAX].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_ENEMAX].txt);
            color_stat(status[BL_ENEMAX].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_ENEMAX].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }


    /* Armor Class */
    if (status[BL_AC].display) {
        if (status[BL_AC].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_AC].label);
            sx += strlen(status[BL_AC].label);
        }

        if (status[BL_AC].txt != NULL) {
            color_stat(status[BL_AC].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_AC].txt);
            color_stat(status[BL_AC].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_AC].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Level */
    if (status[BL_EXP].display) {
        if (status[BL_EXP].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_EXP].label);
            sx += strlen(status[BL_EXP].label);
        }

        if (status[BL_EXP].txt != NULL) {
            color_stat(status[BL_EXP].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_EXP].txt);
            color_stat(status[BL_EXP].highlight_color, OFF);

            sx += strlen(status[BL_EXP].txt);
        }
    }

    /* Experience */
    if (status[BL_XP].display) {
        const char *label = status[BL_EXP].display ? "/" : status[BL_XP].label;
        if (label != NULL) {
            mvwaddstr(win, sy, sx, label);
            sx += strlen(label);
        }

        if (status[BL_XP].txt != NULL) {
            color_stat(status[BL_XP].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_XP].txt);
            color_stat(status[BL_XP].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_XP].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }
    if (status[BL_HD].display) {
        if (status[BL_HD].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_HD].label);
            sx += strlen(status[BL_HD].label);
        }

        if (status[BL_HD].txt != NULL) {
            color_stat(status[BL_HD].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_HD].txt);
            color_stat(status[BL_HD].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_HD].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Time */
    if (status[BL_TIME].display) {
        if (status[BL_TIME].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_TIME].label);
            sx += strlen(status[BL_TIME].label);
        }

        if (status[BL_TIME].txt != NULL) {
            color_stat(status[BL_TIME].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_TIME].txt);
            color_stat(status[BL_TIME].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_TIME].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Score */
#ifdef SCORE_ON_BOTL
    if (status[BL_SCORE].display) {
        if (status[BL_SCORE].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_SCORE].label);
            sx += strlen(status[BL_SCORE].label);
        }

        if (status[BL_SCORE].txt != NULL) {
            color_stat(status[BL_SCORE].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_SCORE].txt);
            color_stat(status[BL_SCORE].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_SCORE].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }
#endif  /* SCORE_ON_BOTL */

    /* Hunger */
    if (status[BL_HUNGER].display) {
        if (status[BL_HUNGER].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_HUNGER].label);
            sx += strlen(status[BL_HUNGER].label);
        }

        if (status[BL_HUNGER].txt != NULL && strlen(status[BL_HUNGER].txt) > 0) {
            color_stat(status[BL_HUNGER].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_HUNGER].txt);
            color_stat(status[BL_HUNGER].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_HUNGER].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    /* Condition bits */
    if (cond_hilites != NULL) {
        unsigned i;
        int color = NO_COLOR;

        for (i = 0; i < SIZE(conditions); ++i) {
            if ((condition_bits & conditions[i].bit) != 0) {
                const char *str = conditions[i].name;
                unsigned j;
                color = NO_COLOR;
                for (j = 0; j < CLR_MAX; ++j) {
                    if (cond_hilites[j] & conditions[i].bit) {
                        color = j;
                        break;
                    }
                }
                for (; j < BL_ATTCLR_MAX; ++j) {
                    if (cond_hilites[j] & conditions[i].bit) {
                        color |= (j - CLR_MAX) << 8;
                        break;
                    }
                }
                color_stat(color, ON);
                mvwaddstr(win, sy, sx, str);
                color_stat(color, OFF);
                if (horiz) {
                    sx += strlen(str) + 1;
                } else {
                    sx = sx_start;
                    sy++;
                }
            }
        }
    }

    /* Encumberance */
    if (status[BL_CAP].display) {
        if (status[BL_CAP].label != NULL) {
            mvwaddstr(win, sy, sx, status[BL_CAP].label);
            sx += strlen(status[BL_CAP].label);
        }

        if (status[BL_CAP].txt != NULL) {
            color_stat(status[BL_CAP].highlight_color, ON);
            mvwaddstr(win, sy, sx, status[BL_CAP].txt);
            color_stat(status[BL_CAP].highlight_color, OFF);

            if (horiz) {
                sx += strlen(status[BL_CAP].txt) + 1;
            } else {
                sx = sx_start;
                sy++;
            }
        }
    }

    wrefresh(win);
}


/* Set labels based on orientation of status window.  If horizontal,
we want to compress this info; otherwise we know we have a width of at
least 26 characters. */

static void
set_labels(int label_width)
{
    char buf[BUFSZ];

    switch (label_width) {
    case COMPACT_LABELS: {
        /* Strength */
        status[BL_STR].label = "S:";

        /* Intelligence */
        status[BL_IN].label = "I:";

        /* Wisdom */
        status[BL_WI].label = "W:";

        /* Dexterity */
        status[BL_DX].label = "D:";

        /* Constitution */
        status[BL_CO].label = "C:";

        /* Charisma */
        status[BL_CH].label = "Ch:";

        /* Alignment */
        status[BL_ALIGN].label = NULL;

        /* Dungeon level */
        status[BL_LEVELDESC].label = "Dl:";

        /* Gold */
        /* FIXME: display gold glyph */
        status[BL_GOLD].label = "$:";

        /* Hit points */
        status[BL_HP].label = "HP:";
        status[BL_HPMAX].label = "/";

        /* Power */
        status[BL_ENE].label = "Pw:";
        status[BL_ENEMAX].label = "/";

        /* Armor Class */
        status[BL_AC].label = "AC:";

        /* Experience */
        status[BL_EXP].label = "XP:";

        /* Level */
        status[BL_HD].label = "HD:";
        status[BL_XP].label = "Lv:";

        /* Time */
        status[BL_TIME].label = "T:";

#ifdef SCORE_ON_BOTL
        /* Score */
        status[BL_SCORE].label = "S:";
#endif
        break;
    }
    case NORMAL_LABELS: {
        /* Strength */
        status[BL_STR].label = "Str:";

        /* Intelligence */
        status[BL_IN].label = "Int:";

        /* Wisdom */
        status[BL_WI].label = "Wis:";

        /* Dexterity */
        status[BL_DX].label = "Dex:";

        /* Constitution */
        status[BL_CO].label = "Con:";

        /* Charisma */
        status[BL_CH].label = "Cha:";

        /* Alignment */
        status[BL_ALIGN].label = NULL;

        /* Dungeon level */
        status[BL_LEVELDESC].label = "Dlvl:";

        /* Gold */
        /* FIXME: display gold glyph */
        status[BL_GOLD].label = "$:";

        /* Hit points */
        status[BL_HP].label = "HP:";
        status[BL_HPMAX].label = "/";

        /* Power */
        status[BL_ENE].label = "Pw:";
        status[BL_ENEMAX].label = "/";

        /* Armor Class */
        status[BL_AC].label = "AC:";

        /* Experience */
        status[BL_EXP].label = "XP:";

        /* Level */
        status[BL_HD].label = "HD:";
        status[BL_XP].label = "Lvl:";

        /* Time */
        status[BL_TIME].label = "T:";

#ifdef SCORE_ON_BOTL
        /* Score */
        status[BL_SCORE].label = "S:";
#endif
        break;
    }
    case WIDE_LABELS: {
        /* Strength */
        status[BL_STR].label = "Strength:      ";

        /* Intelligence */
        status[BL_IN].label = "Intelligence:  ";

        /* Wisdom */
        status[BL_WI].label = "Wisdom:        ";

        /* Dexterity */
        status[BL_DX].label = "Dexterity:     ";

        /* Constitution */
        status[BL_CO].label = "Constitution:  ";

        /* Charisma */
        status[BL_CH].label = "Charisma:      ";

        /* Alignment */
        status[BL_ALIGN].label = "Alignment:     ";

        /* Dungeon level */
        status[BL_LEVELDESC].label = "Dungeon Level: ";

        /* Gold */
        status[BL_GOLD].label = "Gold:          ";

        /* Hit points */
        status[BL_HP].label = "Hit Points:    ";
        status[BL_HPMAX].label = "/";

        /* Power */
        status[BL_ENE].label = "Magic Power:   ";
        status[BL_ENEMAX].label = "/";

        /* Armor Class */
        status[BL_AC].label = "Armor Class:   ";

        /* Experience */
        status[BL_EXP].label = "Experience:    ";

        /* Level */
        status[BL_HD].label = "Hit Dice:      ";
        status[BL_XP].label = "Level:         ";

        /* Time */
        status[BL_TIME].label = "Time:          ";

#ifdef SCORE_ON_BOTL
        /* Score */
        status[BL_SCORE].label = "Score:         ";
#endif
        break;
    }
    default: {
        panic( "set_labels(): Invalid label_width %d\n",
               label_width );
        break;
    }
    }
}


/* Set the color to the base color for the given stat, or highlight a
 changed stat. */

static void
color_stat(int color, int onoff)
{
    WINDOW *win = curses_get_nhwin(STATUS_WIN);

    if (color == NO_COLOR) {
        curses_toggle_color_attr(win, color, A_NORMAL, OFF);
    } else {
        curses_toggle_color_attr(win, color, A_NORMAL, onoff);
    }
}


void
curses_status_init(void)
{
    int index;

    for (index = 0; index < MAXBLSTATS; ++index) {
        status[index].txt = NULL;
        status[index].display = FALSE;
        status[index].highlight_color = 0;
        status[index].label = NULL;
    }
    genl_status_init();
}

void
curses_status_finish(void)
{
    genl_status_finish();
}

void
curses_status_enablefield(int index, const char *name, const char *format,
                          BOOLEAN_P enable)
{
    genl_status_enablefield(index, name, format, enable);
    if (0 <= index && index < MAXBLSTATS) {
        status[index].display = enable;
    }
}

void
curses_status_update(int index, genericptr_t ptr, int change, int percent,
                     int color, unsigned long *colormasks)
{
    switch (index) {
    case BL_FLUSH:
        curses_update_stats(TRUE);
        break;

    case BL_CONDITION:
        condition_bits = *((unsigned long *) ptr);
        cond_hilites = colormasks;
        break;

    default:
        if (0 <= index && index < MAXBLSTATS) {
            free(status[index].txt);
            if (ptr != NULL) {
                status[index].txt = dupstr((char *) ptr);
            } else {
                status[index].txt = NULL;
            }
            status[index].highlight_color = color;
        }
        break;
    }
    curses_update_stats(FALSE);
}
