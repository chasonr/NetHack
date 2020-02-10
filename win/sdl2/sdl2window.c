/* sdl2window.c */

#include "hack.h"
#include "dlb.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2font.h"
#include "sdl2window.h"
#include "sdl2getlin.h"
#include "sdl2map.h"
#include "sdl2menu.h"
#include "sdl2message.h"
#include "sdl2plsel.h"
#include "sdl2posbar.h"
#include "sdl2status.h"
#include "sdl2text.h"
#include "sdl2unicode.h"

const SDL_Color sdl2_colors[] =
{
    {  96,  96,  96, 255 }, /* "black" is really dark gray (e.g., black dragon) */
    { 176,   0,   0, 255 }, /* red */
    {   0, 191,   0, 255 }, /* green */
    { 127, 127,   0, 255 }, /* brown */
    {   0,   0, 176, 255 }, /* blue */
    { 176,   0, 176, 255 }, /* magenta */
    {   0, 176, 176, 255 }, /* cyan */
    { 176, 176, 176, 255 }, /* gray */
    { 255, 255, 255, 255 }, /* no color (Rogue level) */
    { 255, 127,   0, 255 }, /* orange */
    { 127, 255, 127, 255 }, /* bright green */
    { 255, 255,   0, 255 }, /* yellow */
    { 127, 127, 255, 255 }, /* bright blue */
    { 255, 127, 255, 255 }, /* bright magenta */
    { 127, 255, 255, 255 }, /* bright cyan */
    { 255, 255, 255, 255 }  /* white */
};

static SDL_Window *main_window;
static SDL_TimerID m_timer_id;

/* Video mode for full screen */
static int m_video_mode;
static unsigned m_max_bits;

/* On-screen message */
static char *m_message;
static Uint32 m_message_time;

/* Queue of characters previously typed */
static Uint32 key_queue[BUFSZ];
static size_t key_queue_head;
static size_t key_queue_tail;

/* Last window in this array appears on top */
static struct SDL2Window *window_stack[128];
static unsigned window_top;
static winid next_id;

/* Windows constituting the main display */
static struct SDL2Window *message_window;
static struct SDL2Window *status_window;
static struct SDL2Window *map_window;
#ifdef POSITIONBAR
static struct SDL2Window *posbar_window;
#endif

static Uint32 timer_callback(Uint32 interval, genericptr_t param);
static struct SDL2Window *find_window(winid id);
static void arrange_windows(void);
static void new_display(void);
static void non_key_event(const SDL_Event *event);
static void do_message_fade(void);
static void fade_message(void);
static void next_video_mode(void);

static void sdl2_init_nhwindows(int *, char **);
static void sdl2_exit_nhwindows(const char *);
static void sdl2_player_selection(void);
static void sdl2_askname(void);
static void sdl2_get_nh_event(void);
static void sdl2_suspend_nhwindows(const char *str);
static void sdl2_resume_nhwindows(void);
static winid sdl2_create_nhwindow(int);
static void sdl2_clear_nhwindow(winid);
static void sdl2_display_nhwindow(winid, BOOLEAN_P);
static void sdl2_destroy_nhwindow(winid);
static void sdl2_curs(winid, int, int);
static void sdl2_putstr(winid, int, const char *);
static void sdl2_putmixed(winid, int, const char *);
static void sdl2_display_file(const char *, BOOLEAN_P);
static void sdl2_start_menu(winid);
static void sdl2_add_menu(winid, int, const anything *, CHAR_P, CHAR_P, int,
                          const char *, BOOLEAN_P);
static void sdl2_end_menu(winid, const char *);
static int  sdl2_select_menu(winid, int, menu_item **);
static void sdl2_update_inventory(void);
static void sdl2_mark_synch(void);
static void sdl2_wait_synch(void);
#ifdef CLIPPING
static void sdl2_cliparound(int, int);
#endif
#ifdef POSITIONBAR
static void sdl2_update_positionbar(char *);
#endif
static void sdl2_print_glyph(winid, XCHAR_P, XCHAR_P, int, int);
static void sdl2_raw_print(const char *str);
static void sdl2_raw_print_bold(const char *str);
static int  sdl2_nhgetch(void);
static int  sdl2_nh_poskey(int *x, int *y, int *mod);
static void sdl2_nhbell(void);
static int  sdl2_doprev_message(void);
static char sdl2_yn_function(const char *ques, const char *choices,
                             CHAR_P def);
static void sdl2_getlin(const char *ques, char *input);
static int  sdl2_get_ext_cmd(void);
static void sdl2_number_pad(int state);
static void sdl2_delay_output(void);
static void sdl2_start_screen(void);
static void sdl2_end_screen(void);
static void sdl2_preference_update(const char *pref);
static void sdl2_status_init(void);
static void sdl2_status_finish(void);
static void sdl2_status_enablefield(int fieldidx, const char *nm,
        const char *fmt, BOOLEAN_P enable);
static void sdl2_status_update(int idx, genericptr_t ptr, int chg,
        int percent, int color, unsigned long *colormasks);

static void sdl2_window_clear(struct SDL2Window *win);
static void sdl2_window_setVisible(struct SDL2Window *win, boolean visible);
static void sdl2_window_setCursor(struct SDL2Window *win, int x, int y);
static void sdl2_window_putStr(struct SDL2Window *win, int attr,
        const char *str, boolean mixed);
static void sdl2_window_startMenu(struct SDL2Window *win);
static void sdl2_window_addMenu(struct SDL2Window *win, int glyph,
        const anything *identifier, char ch, char gch, int attr,
        const char *str, boolean preselected);
static void sdl2_window_endMenu(struct SDL2Window *win, const char *prompt);
static int  sdl2_window_selectMenu(struct SDL2Window *win, int how, menu_item ** menu_list);
static void sdl2_window_printGlyph(struct SDL2Window *win, xchar x, xchar y,
        int glyph, int bkglyph);
static void sdl2_window_redraw(struct SDL2Window *win);
static char *sdl2_getmsghistory(BOOLEAN_P);
static void sdl2_putmsghistory(const char *str, BOOLEAN_P is_restoring);

static SDL_Color color_from_string(const char *color, const char *dcolor,
        int alpha);

static void
sdl2_init_nhwindows(int *argc, char **argv)
{
    if (main_window != NULL) return;

    /* We support tabbed menu columns and background glyphs */
    iflags.menu_tab_sep = TRUE;
    iflags.use_background_glyph = TRUE;

    /* We allow the user to change the alignment of the message and status
       windows */
    set_wc_option_mod_status(WC_ALIGN_MESSAGE|WC_ALIGN_STATUS, SET_IN_GAME);

    /* Initialize SDL */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);

    /* The single SDL window created; we'll divide this ourselves into
       smaller windows */
    main_window = SDL_CreateWindow("NetHack",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1000, 600, SDL_WINDOW_RESIZABLE);
    if (iflags.wc2_fullscreen) {
        SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN);
        new_display();
    }

    /* Data structures relating to windows */
    window_top = 0;
    next_id = 0;
    message_window = NULL;
    status_window = NULL;
    map_window = NULL;
#ifdef POSITIONBAR
    posbar_window = NULL;
#endif

    /* Request a timer tick every 0.1 second */
    m_timer_id = SDL_AddTimer(100, timer_callback, NULL);

    /* Clear the on-screen message */
    m_message = NULL;
    m_message_time = 0;

    /* Empty the key queue */
    key_queue_head = 0;
    key_queue_tail = 0;
}

/*ARGSUSED*/
static Uint32
timer_callback(Uint32 interval, genericptr_t param)
{
    /* The timer callback is executed in a separate thread and we cannot do
       much directly.  Instead, we must post an event to the event queue. */
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = event.user.type = SDL_USEREVENT;
    event.user.code = 0;
    event.user.data1 = 0;
    event.user.data2 = 0;

    SDL_PushEvent(&event);
    return interval;
}

static void
sdl2_exit_nhwindows(const char *str)
{
    /* TODO: make this a popup if we have a main window */
    printf("%s\n", str);

    while (window_top != 0) {
        sdl2_window_destroy(window_stack[window_top - 1]);
    }

    SDL_RemoveTimer(m_timer_id);
    free(m_message);
    SDL_DestroyWindow(main_window);
    SDL_Quit();
}

static void
sdl2_player_selection(void)
{
    boolean selected = sdl2_player_select();
    if (!selected) {
        clearlocks();
        sdl2_exit_nhwindows("Quit from character selection.");
        nh_terminate(EXIT_SUCCESS);
    }
}

static void
sdl2_askname(void)
{
    struct SDL2Window *window = sdl2_window_create(&sdl2_getline_procs);
    sdl2_getline_run(window, "What is your name?", plname, SIZE(plname));
    sdl2_window_destroy(window);
}

static void
sdl2_get_nh_event(void)
{
    /* Place holder */
}

/*ARGSUSED*/
static void
sdl2_suspend_nhwindows(const char *str)
{
    /* Place holder */
}

static void
sdl2_resume_nhwindows(void)
{
    /* Place holder */
}


static winid
sdl2_create_nhwindow(int type)
{
    struct SDL2Window *window = NULL;
    switch (type) {
    case NHW_MESSAGE:
        window = message_window = sdl2_window_create(&sdl2_message_procs);
        arrange_windows();
        break;

    case NHW_STATUS:
        window = status_window = sdl2_window_create(&sdl2_status_procs);
        arrange_windows();
        break;

    case NHW_MAP:
        window = map_window = sdl2_window_create(&sdl2_map_procs);
#ifdef POSITIONBAR
        posbar_window = sdl2_window_create(&sdl2_posbar_procs);
        sdl2_posbar_set_map_window(posbar_window, map_window);
#endif
        arrange_windows();
        break;

    case NHW_MENU:
        window = sdl2_window_create(&sdl2_menu_procs);
        break;

    case NHW_TEXT:
        window = sdl2_window_create(&sdl2_text_procs);
        break;
    }

    return window->id;
}

/* Create the window structure */
struct SDL2Window *
sdl2_window_create(struct SDL2Window_Methods const *methods)
{
    struct SDL2Window *window;

    window = (struct SDL2Window *) alloc(sizeof(*window));
    memset(window, 0, sizeof(*window));
    window->methods = methods;
    methods->create(window);

    /* Find an unused id */
    while (find_window(next_id) != NULL) {
        if (next_id >= 32767) {
            next_id = 0;
        } else {
            ++next_id;
        }
    }
    window->id = next_id;

    /* Add to the stack */
    if (window_top >= SIZE(window_stack)) {
        panic("Too many windows created");
    }
    window_stack[window_top++] = window;

    return window;
}

static void
sdl2_destroy_nhwindow(winid id)
{
    sdl2_window_destroy(find_window(id));
}

void
sdl2_window_destroy(struct SDL2Window *window)
{
    unsigned index;

    if (window == NULL) return;

    /* Search from the top down, because we're most likely closing the window
       on the top */
    for (index = window_top; index != 0; --index) {
        if (window_stack[index-1] == window) {
            break;
        }
    }
    if (index == 0) {
        return;
    }

    /* index-1 is the position of the window to close */
    window = window_stack[index-1];
    window->methods->destroy(window);
    if (window->m_font) {
        sdl2_font_free(window->m_font);
    }
    free(window);

    /* Clear the saved pointers if applicable */
    if (window == message_window) {
        message_window = NULL;
    }
    if (window == status_window) {
        status_window = NULL;
    }
    if (window == map_window) {
        map_window = NULL;
    }
#ifdef POSITIONBAR
    if (window == posbar_window) {
        posbar_window = NULL;
    }
#endif

    /* If there are other windows above the closed one, move them down */
    for (; index < window_top; ++index) {
        window_stack[index-1] = window_stack[index];
    }
    /* One less window on the stack */
    --window_top;
}

static void
sdl2_clear_nhwindow(winid id)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_clear(window);
    }
}

static void
sdl2_display_nhwindow(winid id, BOOLEAN_P blocking)
{
    struct SDL2Window *window = find_window(id);
    sdl2_window_setVisible(window, TRUE);
    sdl2_redraw();
    if (blocking && (window == message_window || window == map_window)) {
        if (window == map_window) {
            flush_screen(TRUE);
        }
        sdl2_message_more(message_window);
    }
}

static void
sdl2_curs(winid id, int x, int y)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_setCursor(window, x, y);
    }
}


static void
sdl2_putstr(winid id, int attr, const char *str)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_putStr(window, attr, str, FALSE);
    }
}

static void
sdl2_putmixed(winid id, int attr, const char *str)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_putStr(window, attr, str, TRUE);
    }
}

static void
sdl2_display_file(const char *filename, BOOLEAN_P complain)
{
    dlb *file;

    file = dlb_fopen(filename, "r");
    if (file != NULL) {
        struct SDL2Window *window = sdl2_window_create(&sdl2_text_procs);

        while (TRUE) {
            char line[BUFSZ];
            size_t len;

            if (dlb_fgets(line, sizeof(line), file) == NULL) {
                break;
            }
            len = strlen(line);
            if (len != 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
            }
            sdl2_window_putStr(window, 0, line, FALSE);
        }
        dlb_fclose(file);

        sdl2_window_setVisible(window, TRUE);
        sdl2_window_destroy(window);
    } else if (complain) {
        char msg[BUFSZ];

        sprintf(msg, "Could not open file %.100s", filename);
        sdl2_window_putStr(message_window, 0, msg, FALSE);
        sdl2_redraw();
    }
}

static void
sdl2_start_menu(winid id)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_startMenu(window);
    }
}

static void
sdl2_add_menu(winid id, int glyph, const anything *identifier, CHAR_P ch,
              CHAR_P gch, int attr, const char *str, BOOLEAN_P preselected)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_addMenu(window, glyph, identifier, ch, gch, attr, str,
                            preselected);
    }
}

static void
sdl2_end_menu(winid id, const char *prompt)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_endMenu(window, prompt);
    }
}

static int
sdl2_select_menu(winid id, int how, menu_item **menu_list)
{
    struct SDL2Window *window = find_window(id);
    int rc;
    if (window != NULL) {
        rc = sdl2_window_selectMenu(window, how, menu_list);
    } else {
        rc = 0;
    }
    return rc;
}

static void
sdl2_update_inventory(void)
{
    /* Place holder */
}

static void
sdl2_mark_synch(void)
{
    sdl2_redraw();
}

static void
sdl2_wait_synch(void)
{
    if (main_window != NULL) {
        sdl2_redraw();
    } else {
        char s[80];
        printf("Press Enter to continue.\n");
        fgets(s, sizeof(s), stdin);
    }
}

#ifdef CLIPPING
static void
sdl2_cliparound(int x, int y)
{
    sdl2_map_cliparound(map_window, x, y);
}
#endif

#ifdef POSITIONBAR
static void
sdl2_update_positionbar(char *features)
{
    sdl2_posbar_update(posbar_window, features);
}
#endif

static void
sdl2_print_glyph(winid id, XCHAR_P x, XCHAR_P y, int glyph, int bkglyph)
{
    struct SDL2Window *window = find_window(id);
    if (window != NULL) {
        sdl2_window_printGlyph(window, x, y, glyph, bkglyph);
    }
}

static void
sdl2_raw_print(const char *str)
{
    if (message_window != NULL) {
        sdl2_window_putStr(message_window, 0, str, FALSE);
    } else {
        printf("%s\n", str);
    }
}

static void
sdl2_raw_print_bold(const char *str)
{
    if (message_window != NULL) {
        sdl2_window_putStr(message_window, ATR_BOLD, str, FALSE);
    } else {
        printf("%s\n", str);
    }
}

static int
sdl2_nhgetch(void)
{
    Uint32 ch;

    /* For now, accept only ASCII */
    do {
        ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
    } while (ch <= 0 || 127 < ch);
    return ch;
}

static int
sdl2_nh_poskey(int *x, int *y, int *mod)
{
    Uint32 ch;

    sdl2_message_new_turn(message_window);

    /* For now, accept only ASCII */
    /* TODO:  Accept alt-X and arrow keys */
    do {
        ch = sdl2_get_key(TRUE, x, y, mod);
    } while (127 < ch);
    return ch;
}

static void
sdl2_nhbell(void)
{
    /* TODO:  perhaps SDL_mixer or some visual indication */
    fputc('\a', stdout);
}

static int
sdl2_doprev_message(void)
{
    sdl2_message_previous(message_window);
    return 0;
}

static char
sdl2_yn_function(const char *ques, const char *choices, CHAR_P def)
{
    /* Table to determine the quit character */
    static struct {
        const char *choices;
        unsigned quit;
    } quit_table[] =
    {
        { "yn", 1 },
        { "ynq", 2 },
        { "ynaq", 3 },
        { "yn#aq", 4 },
        { ":ynq", 3 },
        { ":ynqm", 3 },
        { NULL, 0 }
    };

    Uint32 ch;
    unsigned i;

    /* Develop the prompt */
    char prompt[BUFSZ];
    if (choices != NULL) {
        if (def != 0) {
            snprintf(prompt, SIZE(prompt), "%s [%s] (%c)", ques, choices, def);
        } else {
            snprintf(prompt, SIZE(prompt), "%s [%s]", ques, choices);
        }
    } else {
        if (def != 0) {
            snprintf(prompt, SIZE(prompt), "%s (%c)", ques, def);
        } else {
            snprintf(prompt, SIZE(prompt), "%s", ques);
        }
    }
    sdl2_window_putStr(message_window, ATR_BOLD, prompt, FALSE);
    sdl2_redraw();

    if (choices == NULL) {
        /* Accept any key */
        do {
            ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
        } while (ch <= 0 || 0x10FFFF < ch);
    } else {
        /* Determine the quit character */
        char quitch = def;
        for (i = 0; quit_table[i].choices != NULL; ++i) {
            if (strcmp(quit_table[i].choices, choices) == 0) {
                quitch = choices[quit_table[i].quit];
                break;
            }
        }

        /* Wait for key */
        do {
            ch = sdl2_get_key(FALSE, NULL, NULL, NULL);
            if (ch == '\033' && quitch != 0) {
                ch = quitch;
                break;
            } else if (ch == ' ' || ch == '\r' || ch == '\n') {
                ch = def;
                break;
            }
        } while (ch <= 0x7F && strchr(choices, ch) == NULL);
    }

    return ch;
}

static void
sdl2_getlin(const char *ques, char *input)
{
    struct SDL2Window *window = sdl2_window_create(&sdl2_getline_procs);

    sdl2_getline_run(window, ques, input, BUFSZ);
    sdl2_window_destroy(window);
}

static int
sdl2_get_ext_cmd(void)
{
    struct SDL2Window *window = sdl2_window_create(&sdl2_getline_procs);
    int cmd = sdl2_extcmd_run(window);
    sdl2_window_destroy(window);
    return cmd;
}

/*ARGSUSED*/
static void
sdl2_number_pad(int state)
{
    /* Place holder */
}

static void
sdl2_delay_output(void)
{
    sdl2_redraw();
    SDL_Delay(1);
}

static void
sdl2_start_screen(void)
{
    /* Place holder */
}

static void
sdl2_end_screen(void)
{
    /* Place holder */
}

static void
sdl2_preference_update(const char *pref)
{
    if (strcmp(pref, "align_status") == 0 ||
        strcmp(pref, "align_message") == 0) {
        arrange_windows();
    }
}

static void
sdl2_status_init(void)
{
    if (status_window == NULL) {
        status_window = sdl2_window_create(&sdl2_status_procs);
        arrange_windows();
    }
}

static void
sdl2_status_finish(void)
{
    sdl2_window_destroy(status_window);
}

static void
sdl2_status_enablefield(int fieldidx, const char *nm,
            const char *fmt, BOOLEAN_P enable)
{
    sdl2_status_do_enablefield(status_window, fieldidx, nm, fmt, enable);
}

static void
sdl2_status_update(int idx, genericptr_t ptr, int chg,
            int percent, int color, unsigned long *colormasks)
{
    if (idx == BL_FLUSH) {
        sdl2_redraw();
    } else {
        sdl2_status_do_update(status_window, idx, ptr, chg, percent, color,
            colormasks);
        sdl2_map_update_cursor(map_window, idx, ptr, chg, percent, color,
            colormasks);
    }
}

static struct SDL2Window *
find_window(winid id)
{
    unsigned i;

    for (i = 0; i < window_top; ++i) {
        if (window_stack[i]->id == id) {
            return window_stack[i];
        }
    }
    return NULL;
}

/* Arrange the main windows: message, map, position bar and status */
static void
arrange_windows(void)
{
    SDL_Rect message_rect;
    SDL_Rect map_rect;
#ifdef POSITIONBAR
    SDL_Rect posbar_rect;
#endif
    SDL_Rect status_rect;
    int width, height;
    int excess;

    if (message_window == NULL) { return; }
    if (map_window == NULL)     { return; }
#ifdef POSITIONBAR
    if (posbar_window == NULL)  { return; }
#endif
    if (status_window == NULL)  { return; }

    /* Overall display size */
    SDL_GetWindowSize(main_window, &width, &height);
    map_rect.x = 0;
    map_rect.y = 0;
    map_rect.w = width;
    map_rect.h = height;

    /* Place the status window */
    switch (iflags.wc_align_status) {
    case ALIGN_TOP:
        status_rect.w = map_rect.w;
        status_rect.h = sdl2_status_height_hint(status_window);
        status_rect.x = map_rect.x;
        status_rect.y = map_rect.y;
        map_rect.h -= status_rect.h;
        map_rect.y += status_rect.h;
        break;

    case ALIGN_BOTTOM:
    default:
        status_rect.w = map_rect.w;
        status_rect.h = sdl2_status_height_hint(status_window);
        status_rect.x = map_rect.x;
        status_rect.y = map_rect.y + map_rect.h - status_rect.h;
        map_rect.h -= status_rect.h;
        break;

    case ALIGN_LEFT:
        status_rect.w = sdl2_status_width_hint(status_window);
        status_rect.h = map_rect.h;
        status_rect.x = map_rect.x;
        status_rect.y = map_rect.y;
        map_rect.w -= status_rect.w;
        map_rect.x += status_rect.w;
        break;

    case ALIGN_RIGHT:
        status_rect.w = sdl2_status_width_hint(status_window);
        status_rect.h = map_rect.h;
        status_rect.x = map_rect.x + map_rect.w - status_rect.w;
        status_rect.y = map_rect.y;
        map_rect.w -= status_rect.w;
        break;
    }

    /* Place the message window */
    switch (iflags.wc_align_message) {
    case ALIGN_TOP:
    default:
        message_rect.w = map_rect.w;
        message_rect.h = sdl2_message_height_hint(message_window);
        message_rect.x = map_rect.x;
        message_rect.y = map_rect.y;
        map_rect.h -= message_rect.h;
        map_rect.y += message_rect.h;
        break;

    case ALIGN_BOTTOM:
        message_rect.w = map_rect.w;
        message_rect.h = sdl2_message_height_hint(message_window);
        message_rect.x = map_rect.x;
        message_rect.y = map_rect.y + map_rect.h - message_rect.h;
        map_rect.h -= message_rect.h;
        break;

    case ALIGN_LEFT:
        message_rect.w = sdl2_message_width_hint(message_window);
        message_rect.h = map_rect.h;
        message_rect.x = map_rect.x;
        message_rect.y = map_rect.y;
        map_rect.w -= message_rect.w;
        map_rect.x += message_rect.w;
        break;

    case ALIGN_RIGHT:
        message_rect.w = sdl2_message_width_hint(message_window);
        message_rect.h = map_rect.h;
        message_rect.x = map_rect.x + map_rect.w - message_rect.w;
        message_rect.y = map_rect.y;
        map_rect.w -= message_rect.w;
        break;
    }

#ifdef POSITIONBAR
    /* Place the position bar */
    /* The Y position is not final if the message window is on the bottom */
    posbar_rect.w = map_rect.w;
    posbar_rect.h = sdl2_posbar_height_hint(posbar_window);
    posbar_rect.x = map_rect.x;
    posbar_rect.y = map_rect.y + map_rect.h - posbar_rect.h;
    map_rect.h -= posbar_rect.h;
#endif

    /* Map has remaining size, unless there is excess to assign to the
       message window */
    excess = map_rect.h - sdl2_map_height_hint(map_window);
    if (excess > 0) {
        switch (iflags.wc_align_message) {
        case ALIGN_TOP:
        default:
            message_rect.h += excess;
            map_rect.y += excess;
            map_rect.h -= excess;
            break;

        case ALIGN_BOTTOM:
            map_rect.h -= excess;
            message_rect.y -= excess;
            message_rect.y += excess;
            break;
        }
    }

    sdl2_message_resize(message_window,
                        message_rect.x, message_rect.y,
                        message_rect.x + message_rect.w - 1,
                        message_rect.y + message_rect.h - 1);
    sdl2_map_resize(map_window,
                    map_rect.x, map_rect.y,
                    map_rect.x + map_rect.w - 1,
                    map_rect.y + map_rect.h - 1);
#ifdef POSITIONBAR
    sdl2_posbar_resize(posbar_window,
                       posbar_rect.x, posbar_rect.y,
                       posbar_rect.x + posbar_rect.w - 1,
                       posbar_rect.y + posbar_rect.h - 1);
#endif
    sdl2_status_resize(status_window,
                       status_rect.x, status_rect.y,
                       status_rect.x + status_rect.w - 1,
                       status_rect.y + status_rect.h - 1);

    sdl2_map_cliparound(map_window, -1, -1);
}

void
sdl2_window_set_font(struct SDL2Window *window, const char *name_option,
                     int size_option, const char *default_name,
                     int default_size)
{
    const char *name;
    int size;

    name = (name_option != NULL && name_option[0] != '\0')
            ? name_option : default_name;
    size = (size_option != 0) ? size_option : default_size;

    if (window->m_font) {
        sdl2_font_free(window->m_font);
    }

    window->m_font = sdl2_font_new(name, size);
    window->m_line_height = (sdl2_font_textSizeStr(window->m_font, "X")).h;
}

void
sdl2_window_resize(struct SDL2Window *window, int x1, int y1, int x2, int y2)
{
    if (x1 < x2) {
        window->m_xmin = x1;
        window->m_xmax = x2;
    } else {
        window->m_xmin = x2;
        window->m_xmax = x1;
    }
    if (y1 < y2) {
        window->m_ymin = y1;
        window->m_ymax = y2;
    } else {
        window->m_ymin = y2;
        window->m_ymax = y1;
    }
}

void
sdl2_window_blit(struct SDL2Window *window, const SDL_Rect *window_rect,
                 SDL_Surface *surface, const SDL_Rect *surface_rect)
{
    SDL_Surface *main_surface = SDL_GetWindowSurface(main_window);
    SDL_Rect window_rect2;

    if (!window->m_visible) { return; }

    /* Shift the window rectangle to main surface coordinates */
    window_rect2 = *window_rect;
    window_rect2.x += window->m_xmin;
    window_rect2.y += window->m_ymin;

    /* Set the clipping rectangle */
    SDL_SetClipRect(main_surface, &window_rect2);

    /* Draw */
    SDL_BlitSurface(surface, surface_rect, main_surface, &window_rect2);
}

void
sdl2_window_fill(struct SDL2Window *window, const SDL_Rect *window_rect,
                 SDL_Color color)
{
    SDL_Surface *main_surface;
    SDL_Rect window_rect2;
    Uint32 cindex;

    if (!window->m_visible) { return; }
    if (window_rect->w < 0 || window_rect->h < 0) { return; }

    main_surface = SDL_GetWindowSurface(main_window);

    /* Shift the window rectangle to main surface coordinates */
    window_rect2 = *window_rect;
    window_rect2.x += window->m_xmin;
    window_rect2.y += window->m_ymin;

    /* Set the clipping rectangle */
    SDL_SetClipRect(main_surface, &window_rect2);

    /* Draw */
    cindex = SDL_MapRGBA(main_surface->format,
            color.r, color.g, color.b, color.a);
    if (color.a == 255 || SDL_BYTESPERPIXEL(main_surface->format->format) != 4) {
        SDL_FillRect(main_surface, &window_rect2, cindex);
    } else {
        Uint32 *row;
        unsigned char b1a, b2a, b3a, b4a;
        int y;

        if (SDL_MUSTLOCK(main_surface)) {
            SDL_LockSurface(main_surface);
        }
        row = (Uint32 *)((char *)main_surface->pixels + window_rect2.y*main_surface->pitch);
        b1a = (unsigned char)(cindex >> 24);
        b2a = (unsigned char)(cindex >> 16);
        b3a = (unsigned char)(cindex >>  8);
        b4a = (unsigned char)(cindex >>  0);
        for (y = 0; y < window_rect2.h; ++y) {
            Uint32 *pixel;
            int x;

            if (window_rect2.y + y < 0) {
                row = (Uint32 *)((char *)row + main_surface->pitch);
                continue;
            }
            if (window_rect2.y + y >= main_surface->h) { break; }
            pixel = row + window_rect2.x;
            for (x = 0; x < window_rect2.w; ++x) {
                unsigned char b1b, b2b, b3b, b4b;

                if (window_rect2.x + x < 0) {
                    ++pixel;
                    continue;
                }
                if (window_rect2.x + x >= main_surface->w) { break; }

                /* We can alpha blend even if the pixel format has no alpha */
                b1b = (unsigned char)(*pixel >> 24);
                b2b = (unsigned char)(*pixel >> 16);
                b3b = (unsigned char)(*pixel >>  8);
                b4b = (unsigned char)(*pixel >>  0);
                switch (SDL_PIXELORDER(main_surface->format->format)) {
                case SDL_PACKEDORDER_ARGB:
                case SDL_PACKEDORDER_ABGR:
                case SDL_PACKEDORDER_XRGB:
                case SDL_PACKEDORDER_XBGR:
                    b2b = ((b2a * color.a) + (b2b * (255 - color.a))) / 255;
                    b3b = ((b3a * color.a) + (b3b * (255 - color.a))) / 255;
                    b4b = ((b4a * color.a) + (b4b * (255 - color.a))) / 255;
                    b1b = 255;
                    break;

                case SDL_PACKEDORDER_RGBA:
                case SDL_PACKEDORDER_BGRA:
                case SDL_PACKEDORDER_RGBX:
                case SDL_PACKEDORDER_BGRX:
                    b1b = ((b1a * color.a) + (b1b * (255 - color.a))) / 255;
                    b2b = ((b2a * color.a) + (b2b * (255 - color.a))) / 255;
                    b3b = ((b3a * color.a) + (b3b * (255 - color.a))) / 255;
                    b4b = 255;
                    break;

                default:
                    /* We shouldn't get here; alpha blending is unsupported
                       on pixel formats less than 32 bits */
                    b1b = b1a;
                    b2b = b2a;
                    b3b = b3a;
                    b4b = b4a;
                    break;
                }
                *pixel = ((Uint32) b1b << 24)
                       | ((Uint32) b2b << 16)
                       | ((Uint32) b3b <<  8)
                       | ((Uint32) b4b <<  0);

                ++pixel;
            }
            row = (Uint32 *)((char *)row + main_surface->pitch);
        }
        if (SDL_MUSTLOCK(main_surface)) {
            SDL_UnlockSurface(main_surface);
        }
    }
}

/* Render text at given coordinates without stretching */
/* Return rectangle in which the text was rendered */
SDL_Rect
sdl2_window_renderStrBG(struct SDL2Window *win, const char *str,
                        int x, int y, SDL_Color fg, SDL_Color bg)
{
    SDL_Surface *text = sdl2_font_renderStrBG(win->m_font, str, fg, bg);
    SDL_Rect win_rect;
    SDL_Rect txt_rect;

    win_rect.x = x;
    win_rect.y = y;
    win_rect.w = text->w;
    win_rect.h = text->h;
    txt_rect.x = 0;
    txt_rect.y = 0;
    txt_rect.w = text->w;
    txt_rect.h = text->h;
    sdl2_window_blit(win, &win_rect, text, &txt_rect);
    SDL_FreeSurface(text);
    return win_rect;
}

/* Render single character at given coordinates without stretching */
/* Return rectangle in which the text was rendered */
SDL_Rect
sdl2_window_renderCharBG(struct SDL2Window *win, Uint32 chr, int x, int y,
                         SDL_Color fg, SDL_Color bg)
{
    SDL_Surface *text = sdl2_font_renderCharBG(win->m_font, chr, fg, bg);
    SDL_Rect win_rect;
    SDL_Rect txt_rect;

    win_rect.x = x;
    win_rect.y = y;
    win_rect.w = text->w;
    win_rect.h = text->h;
    txt_rect.x = 0;
    txt_rect.y = 0;
    txt_rect.w = text->w;
    txt_rect.h = text->h;
    sdl2_window_blit(win, &win_rect, text, &txt_rect);
    SDL_FreeSurface(text);
    return win_rect;
}

/* Render text that may contain glyph escapes */
/* Return rectangle in which the text was rendered */
SDL_Rect
sdl2_window_render_mixed(struct SDL2Window *win, const char *str, int x, int y,
                         SDL_Color fg, SDL_Color bg)
{
    char esc[20];
    char *str2;
    char *p1;
    SDL_Rect all_rect;

    /* Glyph escapes begin with this pattern */
    Sprintf(esc, "\\G%04X", context.rndencode);

    /* We need a copy of the string, so we can modify it */
    str2 = dupstr(str);
    p1 = str2;

    /* p1 points to the part of the string we haven't rendered yet */
    all_rect.x = x;
    all_rect.y = y;
    all_rect.w = 0;
    all_rect.h = 0;
    while (*p1 != '\0') {
        char *p2;
        SDL_Rect txt_rect;
        char hex[5];
        int glyph;
        int ch;
        int oc;
        unsigned os;
        Uint32 ch32;

        /* Find the next glyph escape */
        p2 = strstr(p1, esc);
        if (p2 == NULL) {
            /* No glyph escape found; render the rest of the string */
            p2 = p1 + strlen(p1);
        }
        if (p1 != p2) {
            /* Render literal text from p1 to p2 */
            char chr = *p2;
            *p2 = '\0';
            txt_rect = sdl2_window_renderStrBG(win, p1,
                    x + all_rect.w, y, fg, bg);
            *p2 = chr;

            /* Extend the rendered rectangle */
            all_rect.w += txt_rect.w;
            if (all_rect.h < txt_rect.h) {
                all_rect.h = txt_rect.h;
            }
        }

        /* Stop here if end of string */
        if (*p2 == '\0') {
            break;
        }

        /* Extract the glyph code */
        p2 += strlen(esc);
        strncpy(hex, p2, 4);
        hex[4] = 0;
        glyph = strtol(hex, NULL, 16);

        /* Render the glyph */
        mapglyph(glyph, &ch, &oc, &os, 0, 0, 0);
        ch32 = sdl2_chr_convert(ch);
        txt_rect = sdl2_window_renderCharBG(win, ch32,
                x + all_rect.w, y,
                sdl2_colors[oc], bg);

        /* Extend the rendered rectangle */
        all_rect.w += txt_rect.w;
        if (all_rect.h < txt_rect.h) {
            all_rect.h = txt_rect.h;
        }

        /* Proceed to the part of the string beyond the glyph escape */
        p1 = p2 + strlen(hex);
    }
    free(str2);
    return all_rect;
}

/* Draw a hollow rectangle in the given color */
void
sdl2_window_draw_box(struct SDL2Window *win, int x1, int y1, int x2, int y2,
                     SDL_Color color)
{
    SDL_Rect rect;

    /* Top */
    rect.x = x1;
    rect.y = y1;
    rect.w = x2 - x1 + 1;
    rect.h = 1;
    sdl2_window_fill(win, &rect, color);

    /* Bottom */
    rect.y = y2;
    sdl2_window_fill(win, &rect, color);

    /* Left */
    rect.x = x1;
    rect.y = y1;
    rect.w = 1;
    rect.h = y2 - y1 + 1;
    sdl2_window_fill(win, &rect, color);

    /* Right */
    rect.x = x2;
    sdl2_window_fill(win, &rect, color);
}

Uint32
sdl2_chr_convert(Uint32 ch)
{
    static const unsigned short cp437table[] = {
        0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
        0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
        0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
        0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
        0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
        0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
        0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
        0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
        0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
        0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
        0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
        0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
        0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
        0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
        0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
        0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
        0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
        0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
        0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
        0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
        0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
        0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
        0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
        0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
        0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
    };
    static const unsigned short dectable[] = {
        0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1,
        0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
        0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C,
        0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7
    };

    if (SYMHANDLING(H_IBM)) {
        return cp437table[(unsigned char)ch];
    } else if (SYMHANDLING(H_DEC)) {
        ch &= 0xFF;
        if (0xE0 <= ch && ch <= 0xFE) {
            return dectable[ch - 0xE0];
        } else {
            return ch;
        }
    } else if (SYMHANDLING(H_UNICODE)) {
        return ch;
    } else {
        return (unsigned char)ch;
    }
}

/****************************************************************************/
/*                           Window method calls                            */
/****************************************************************************/

static void
sdl2_window_clear(struct SDL2Window *win)
{
    if (win->methods->clear != NULL) {
        win->methods->clear(win);
    }
}

static void
sdl2_window_setVisible(struct SDL2Window *win, boolean visible)
{
    win->m_visible = visible;
    if (win->methods->setVisible != NULL) {
        win->methods->setVisible(win, visible);
    }
}

static void
sdl2_window_setCursor(struct SDL2Window *win, int x, int y)
{
    if (win->methods->setCursor != NULL) {
        win->methods->setCursor(win, x, y);
    }
}

static void
sdl2_window_putStr(struct SDL2Window *win, int attr, const char *str,
                   boolean mixed)
{
    if (win->methods->putStr != NULL) {
        win->methods->putStr(win, attr, str, mixed);
    }
}

static void
sdl2_window_startMenu(struct SDL2Window *win)
{
    if (win->methods->startMenu != NULL) {
        win->methods->startMenu(win);
    }
}

static void
sdl2_window_addMenu(struct SDL2Window *win, int glyph,
                    const anything *identifier, char ch, char gch, int attr,
                    const char *str, boolean preselected)
{
    if (win->methods->addMenu != NULL) {
        win->methods->addMenu(win, glyph, identifier, ch, gch, attr, str,
                              preselected);
    }
}

static void
sdl2_window_endMenu(struct SDL2Window *win, const char *prompt)
{
    if (win->methods->endMenu != NULL) {
        win->methods->endMenu(win, prompt);
    }
}

static int
sdl2_window_selectMenu(struct SDL2Window *win, int how, menu_item ** menu_list)
{
    int rc;

    if (win->methods->selectMenu != NULL) {
        rc = win->methods->selectMenu(win, how, menu_list);
    } else {
        rc = 0;
    }
    return rc;
}

static void
sdl2_window_printGlyph(struct SDL2Window *win, xchar x, xchar y,
                       int glyph, int bkglyph)
{
    if (win->methods->printGlyph != NULL) {
        win->methods->printGlyph(win, x, y, glyph, bkglyph);
    }
}

static void
sdl2_window_redraw(struct SDL2Window *win)
{
    if (win->methods->redraw != NULL) {
        win->methods->redraw(win);
    }
}

static char *
sdl2_getmsghistory(BOOLEAN_P init)
{
    if (message_window == NULL) {
        return NULL;
    } else {
        return sdl2_message_gethistory(message_window, init);
    }
}

static void
sdl2_putmsghistory(const char *str, BOOLEAN_P is_restoring)
{
    if (is_restoring && message_window != NULL) {
        sdl2_message_puthistory(message_window, str);
    }
}

/****************************************************************************/
/*                            SDL-specific stuff                            */
/****************************************************************************/

void
sdl2_display_size(int *x, int *y)
{
    SDL_GetWindowSize(main_window, x, y);
}

static void
new_display(void)
{
    int display = SDL_GetWindowDisplayIndex(main_window);
    int num_modes = SDL_GetNumDisplayModes(display);
    SDL_DisplayMode mode;
    int j;
    int w, h;

    /* First pass; find the maximum number of bits per pixel */
    m_max_bits = 0;
    for (j = 0; j < num_modes; ++j) {
        unsigned bits;

        SDL_GetDisplayMode(display, j, &mode);
        bits = SDL_BITSPERPIXEL(mode.format);
        printf("%s:  mode %d: width=%d height=%d depth=%d\n",
                __func__, j, mode.w, mode.h, bits);
        if (bits < m_max_bits) m_max_bits = bits;
    }

    /* Second pass; find the first (that is, highest resolution) mode with the
       maximum number of bits per pixel */
    m_video_mode = 0;
    for (j = 0; j < num_modes; ++j) {
        unsigned bits;

        SDL_GetDisplayMode(display, j, &mode);
        bits = SDL_BITSPERPIXEL(mode.format);
        if (bits >= m_max_bits) {
            printf("%s:  selected mode: width=%d height=%d depth=%d\n",
                    __func__, mode.w, mode.h, bits);
            m_video_mode = j;
            break;
        }
    }

    SDL_GetDisplayMode(display, m_video_mode, &mode);
    SDL_SetWindowFullscreen(main_window, 0);
    SDL_SetWindowDisplayMode(main_window, &mode);
    SDL_GetWindowSize(main_window, &w, &h);
    SDL_SetWindowPosition(main_window, 0, mode.h - h);
    SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN);
}

void
sdl2_redraw(void)
{
    size_t i;

    SDL_Surface *main_surface = SDL_GetWindowSurface(main_window);
    Uint32 background;

    if (main_surface == NULL) return;

    background = SDL_MapRGBA(main_surface->format, 0, 0, 0, 255);
    SDL_SetClipRect(main_surface, NULL);
    SDL_FillRect(main_surface, NULL, background);

    /* Draw each window; the blit method will perform any clipping */
    for (i = 0; i < window_top; ++i) {
        struct SDL2Window *window = window_stack[i];
        if (window->m_visible) {
            sdl2_window_redraw(window);
        }
    }

    /* If a message is present, alpha blend it on top of the screen */
    if (m_message != NULL && message_window != NULL) {
        SDL_Color yellow = { 255, 255,  0, 0 };
        SDL_Color black  = {   0,   0,  0, 0 };
        SDL_Surface *text;
        SDL_Rect rect;
        Uint32 ticks;
        unsigned alpha;

        SDL_SetClipRect(main_surface, NULL);
        ticks = SDL_GetTicks();
        if (m_message_time == 0) {
            alpha = 255;
        } else if (ticks - m_message_time >= 2000) {
            alpha = 0;
        } else {
            alpha = (2000 - (ticks - m_message_time)) * 255 / 2000;
        }
        yellow.a = alpha;
        black.a = alpha;
        text = sdl2_font_renderStrBG(message_window->m_font,
                m_message, yellow, black);
        rect.x = main_surface->w - text->w;
        rect.y = 0;
        rect.w = text->w;
        rect.h = text->h;
        SDL_BlitSurface(text, NULL, main_surface, &rect);
        SDL_FreeSurface(text);
    }

    SDL_UpdateWindowSurface(main_window);
}

Uint32
sdl2_get_key(boolean cmd, int *x, int *y, int *mod)
{
    Uint32 ch;
    boolean shift;
    SDL_Event event;

    if (mod) {
        /* If we inadvertently return a null character, don't make it into
           a mouse click */
        *mod = 0;
    }

    /* Return characters queued from a previous event */
    if (key_queue_head != key_queue_tail) {
        ch = key_queue[key_queue_head++];
        goto end;
    }

    sdl2_redraw();

    while (TRUE) {
        SDL_WaitEvent(&event);
        non_key_event(&event);

        switch (event.type) {
        case SDL_KEYDOWN:
            ch = event.key.keysym.sym;
            shift = event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
            /* Control codes and special keys don't produce a text input event */
            switch (ch) {

            /* Function keys */
            case SDLK_F1:
                sdl2_set_message("F3-Window/FullScreen F4-VidMode F5/Tab - Tiles/ASCII F6-PositionBar F8-ZmMode");
                sdl2_redraw();
                break;

            case SDLK_F3:
                iflags.wc2_fullscreen = !iflags.wc2_fullscreen;
                if (iflags.wc2_fullscreen) {
                    SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN);
                    sdl2_set_message("Full screen mode");
                    m_video_mode = 0;
                } else {
                    SDL_SetWindowFullscreen(main_window, 0);
                    sdl2_set_message("Windowed mode");
                }
                arrange_windows();
                sdl2_redraw();
                break;

            case SDLK_F4:
                {
                    int width, height;
                    char msg[BUFSZ];

                    next_video_mode();
                    arrange_windows();
                    SDL_GetWindowSize(main_window, &width, &height);

                    snprintf(msg, SIZE(msg), "Video Mode: %dx%d", width, height);
                    sdl2_set_message(msg);
                    sdl2_redraw();
                }
                break;

            case '\t': /* Tab key, but not control-I */
            case SDLK_F5:
                sdl2_map_toggle_tile_mode(map_window);
                arrange_windows();
                sdl2_redraw();
                break;

#ifdef POSITIONBAR
            case SDLK_F6:
                sdl2_window_setVisible(posbar_window,
                                       !posbar_window->m_visible);
                sdl2_set_message(posbar_window->m_visible
                        ? "Position Bar: On"
                        : "Position Bar: Off");
                arrange_windows();
                sdl2_redraw();
                break;
#endif

            case SDLK_F8:
                sdl2_map_next_zoom_mode(map_window);
                arrange_windows();
                sdl2_redraw();
                break;

            /* For the numeric keypad, the SDL_TEXTINPUT events seem to work
               just fine when num_pad is set, at least on Mac.
               TODO:  Test on Windows, Linux */

            /* Cursor keys */

            case SDLK_DOWN:
                if (cmd) {
                    ch = iflags.num_pad ? '2' : shift ? 'J' : 'j';
                }
                goto end;

            case SDLK_LEFT:
                if (cmd) {
                    ch = iflags.num_pad ? '4' : shift ? 'H' : 'h';
                }
                goto end;

            case SDLK_RIGHT:
                if (cmd) {
                    ch = iflags.num_pad ? '6' : shift ? 'L' : 'l';
                }
                goto end;

            case SDLK_UP:
                if (cmd) {
                    ch = iflags.num_pad ? '8' : shift ? 'K' : 'k';
                }
                goto end;

            case SDLK_HOME:
                ch = MENU_FIRST_PAGE;
                goto end;

            case SDLK_PAGEUP:
                ch = MENU_PREVIOUS_PAGE;
                goto end;

            case SDLK_END:
                ch = MENU_LAST_PAGE;
                goto end;

            case SDLK_PAGEDOWN:
                ch = MENU_NEXT_PAGE;
                goto end;

            default:
                if (ch < 0x20 || (0x7F <= ch && ch <= 0x9F) || 0x10FFFF < ch) {
                    /* Control characters such as Escape, Backspace, Delete,
                       and keys such as arrows, function keys, etc. that do not
                       strike a character */
                    goto end;
                } else if ('A' <= ch && ch <= '~'
                && (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0) {
                    /* CTRL+letter */
                    ch &= 0x1F;
                    goto end;
                }
                break;
            }
            break;

        case SDL_KEYUP:
            fade_message();
            break;

        case SDL_TEXTINPUT:
            {
                if (event.text.text[0] != 0) {
                    Uint32 *text = sdl2_uni_8to32(event.text.text);
                    ch = text[0];
                    key_queue_head = 0;
                    key_queue_tail = sdl2_uni_length32(text + 1);
                    if (key_queue_tail > SIZE(key_queue)) {
                        key_queue_tail = SIZE(key_queue);
                    }
                    memcpy(key_queue, text + 1, key_queue_tail * sizeof(text[0]));
                    free(text);
                    goto end;
                }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (cmd && x != NULL && y != NULL
            && (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {
                Sint32 xm = event.button.x;
                Sint32 ym = event.button.y;
                if (map_window->m_xmin <= xm && xm <= map_window->m_xmax
                &&  map_window->m_ymin <= ym && ym <= map_window->m_ymax
                &&  sdl2_map_mouse(map_window,
                            xm - map_window->m_xmin,
                            ym - map_window->m_ymin,
                            x, y)) {
                    *mod = event.button.button == SDL_BUTTON_LEFT ? CLICK_1 : CLICK_2;
                    ch = 0;
                    goto end;
                }
            }
            break;
        /* TODO:  process SDL_QUIT, others */
        }
    }

end:
    return ch;
}

static void
non_key_event(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_RESIZED:
            arrange_windows();
            sdl2_redraw();
            break;
        }
        break;

    case SDL_USEREVENT:     /* Timer tick */
        {
            /* Prevent timer events from piling up on the queue */
            SDL_Event event2;

            while (SDL_PeepEvents(&event2, 1, SDL_GETEVENT,
                    SDL_USEREVENT, SDL_USEREVENT) > 0) {}

            /* Fade the screen message out */
            do_message_fade();

            /* Redraw the message fade, map effects, etc. */
            sdl2_redraw();
        }
        break;
    }
}

static void
do_message_fade(void)
{
    if (m_message_time != 0 && m_message != NULL) {
        /* Message is present and it is being faded */
        if (SDL_GetTicks() - m_message_time > 2000) {
            /* Message has timed out */
            free(m_message);
            m_message = NULL;
            m_message_time = 0;
        }
    }
}

void
sdl2_set_message(const char *message)
{
    free(m_message);
    m_message = dupstr(message);
    m_message_time = 0; /* Not fading yet */
}

static void
fade_message(void)
{
    if (m_message_time == 0 && m_message != NULL) {
        m_message_time = SDL_GetTicks();
        if (m_message_time == 0) {
            m_message_time = 1;
        }
    }
}

static void
next_video_mode(void)
{
    int display = SDL_GetWindowDisplayIndex(main_window);
    int num_modes = SDL_GetNumDisplayModes(display);
    SDL_DisplayMode mode;

    /* Find next mode with the maximum number of bits per pixel */
    for (++m_video_mode; m_video_mode < num_modes; ++m_video_mode) {
        unsigned bits;

        SDL_GetDisplayMode(display, m_video_mode, &mode);
        bits = SDL_BITSPERPIXEL(mode.format);
        if (bits >= m_max_bits) { break; }
    }

    /* If we've passed the last mode for this display, try the next one */
    if (m_video_mode >= num_modes) {
        new_display();
    } else {
        SDL_GetDisplayMode(display, m_video_mode, &mode);
        SDL_SetWindowFullscreen(main_window, 0);
        SDL_SetWindowDisplayMode(main_window, &mode);
        SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN);
    }
}

/* Convert attribute flags to colors */
/* Underline is implemented as bold on a gray background */
/* Blink is not implemented */
SDL_Color
sdl2_text_fg(const char *fgcolor, const char *bgcolor, int attr)
{
    SDL_Color color;
    int r, g, b;

    switch (attr) {
    default:
    case ATR_NONE:
        color = color_from_string(fgcolor, "#B0B0B0", 255);
        break;

    case ATR_BOLD:
    case ATR_ULINE:
        color = color_from_string(fgcolor, "#B0B0B0", 255);
        r = color.r + 48;
        g = color.g + 48;
        b = color.b + 48;
        color.r = r < 255 ? r : 255;
        color.g = g < 255 ? g : 255;
        color.b = b < 255 ? b : 255;
        break;

    case ATR_DIM:
        color = color_from_string(fgcolor, "#B0B0B0", 255);
        color.r = color.r * 2 / 3;
        color.g = color.g * 2 / 3;
        color.b = color.b * 2 / 3;
        break;

    case ATR_INVERSE:
        color = color_from_string(bgcolor, "#000000", 255);
        break;
    }

    /* Foreground color is always opaque */
    color.a = 255;
    return color;
}

SDL_Color
sdl2_text_bg(const char *fgcolor, const char *bgcolor, int attr, int alpha)
{
    SDL_Color color;
    int r, g, b;

    switch (attr) {
    default:
    case ATR_NONE:
    case ATR_BOLD:
    case ATR_DIM:
        color = color_from_string(bgcolor, "#000000", alpha);
        break;

    case ATR_ULINE:
        color = color_from_string(bgcolor, "#000000", 255);
        r = color.r + 48;
        g = color.g + 48;
        b = color.b + 48;
        color.r = r < 255 ? r : 255;
        color.g = g < 255 ? g : 255;
        color.b = b < 255 ? b : 255;
        break;

    case ATR_INVERSE:
        color = color_from_string(fgcolor, "#B0B0B0", 255);
        break;
    }
    return color;
}

static SDL_Color
color_from_string(const char *color, const char *dcolor, int alpha)
{
    SDL_Color rgb;

    rgb.r = 176;
    rgb.g = 176;
    rgb.b = 176;
    rgb.a = alpha < 255 ? alpha : 255;
    if (color == NULL) {
        goto def_color;
    }
    if (color[0] == '#' && strlen(color) >= 7) {
        int r, g, b;
        int count = sscanf(color + 1, "%2x%2x%2x%2x", &r, &g, &b, &alpha);
        if (count < 3) {
            goto def_color;
        }
        rgb.r = r < 255 ? r : 255;
        rgb.g = g < 255 ? g : 255;
        rgb.b = b < 255 ? b : 255;
        if (count >= 4) {
            rgb.a = alpha < 255 ? alpha : 255;
        }
    } else {
        unsigned i;
        for (i = 0; i < CLR_MAX; ++i) {
            if (strcmp(color, c_obj_colors[i]) == 0) {
                rgb = sdl2_colors[i];
                break;
            }
        }
        if (i >= CLR_MAX) {
            goto def_color;
        }
    }

    return rgb;

def_color:
    if (dcolor == NULL) {
        return rgb;
    }
    return color_from_string(dcolor, NULL, alpha);
}

struct window_procs sdl2_procs = {
    "sdl2",
    WC_COLOR |
    WC_HILITE_PET |
    WC_TILED_MAP |
    WC_ALIGN_MESSAGE |
    WC_ALIGN_STATUS |
    WC_PRELOAD_TILES |
    WC_TILE_WIDTH |
    WC_TILE_HEIGHT |
    WC_TILE_FILE |
    WC_MAP_MODE |
    WC_TILED_MAP |
    WC_MOUSE_SUPPORT |
    WC_FONT_MAP |
    WC_FONT_MENU |
    WC_FONT_MESSAGE |
    WC_FONT_STATUS |
    WC_FONT_TEXT |
    WC_FONTSIZ_MAP |
    WC_FONTSIZ_MENU |
    WC_FONTSIZ_MESSAGE |
    WC_FONTSIZ_STATUS |
    WC_FONTSIZ_TEXT |
    WC_PLAYER_SELECTION,
    WC2_FULLSCREEN|
    WC2_HILITE_STATUS,
    { TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
      TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE },
    sdl2_init_nhwindows,
    sdl2_player_selection,
    sdl2_askname,
    sdl2_get_nh_event,
    sdl2_exit_nhwindows,
    sdl2_suspend_nhwindows,
    sdl2_resume_nhwindows,
    sdl2_create_nhwindow,
    sdl2_clear_nhwindow,
    sdl2_display_nhwindow,
    sdl2_destroy_nhwindow,
    sdl2_curs,
    sdl2_putstr,
    sdl2_putmixed,
    sdl2_display_file,
    sdl2_start_menu,
    sdl2_add_menu,
    sdl2_end_menu,
    sdl2_select_menu,
    genl_message_menu,
    sdl2_update_inventory,
    sdl2_mark_synch,
    sdl2_wait_synch,
#ifdef CLIPPING
    sdl2_cliparound,
#endif
#ifdef POSITIONBAR
    sdl2_update_positionbar,
#endif
    sdl2_print_glyph,
    sdl2_raw_print,
    sdl2_raw_print_bold,
    sdl2_nhgetch,
    sdl2_nh_poskey,
    sdl2_nhbell,
    sdl2_doprev_message,
    sdl2_yn_function,
    sdl2_getlin,
    sdl2_get_ext_cmd,
    sdl2_number_pad,
    sdl2_delay_output,
    sdl2_start_screen,
    sdl2_end_screen,
    genl_outrip,
    sdl2_preference_update,
    sdl2_getmsghistory,
    sdl2_putmsghistory,
    sdl2_status_init,
    sdl2_status_finish,
    sdl2_status_enablefield,
    sdl2_status_update,
    genl_can_suspend_no,
};
