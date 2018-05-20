#ifndef CURSSTAT_H
#define CURSSTAT_H


/* Global declarations */

void curses_update_stats(boolean redraw);

void curses_status_init(void);

void curses_status_finish(void);

void curses_status_enablefield(int fieldidx, const char *nm, const char *fmt,
                               BOOLEAN_P enable);

void curses_status_update(int idx, genericptr_t ptr, int chg, int percent,
                          int color, unsigned long *colormasks);

#endif  /* CURSSTAT_H */
