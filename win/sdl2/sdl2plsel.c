/* sdlplsel.c */

#include "hack.h"
#undef max
#undef min
#undef yn
#include "sdl2.h"
#include "sdl2plsel.h"
#include "sdl2menu.h"

static boolean select_role(void);
static boolean select_race(void);
static boolean select_gender(void);
static boolean selectAlignment(void);

static boolean valid_selection(int role, int race, int gend, int alignment);

boolean
sdl2_player_select(void)
{
    return select_role()
        && select_race()
        && select_gender()
        && selectAlignment();
}

static boolean
select_role(void)
{
    int role;
    unsigned count;
    anything id;
    struct SDL2Window *menu;
    int i;
    menu_item *selection;
    int rc;

    if (flags.initrole != -1) {
        return TRUE;
    }

    role = -1;
    count = 0;

    menu = sdl2_window_create(&sdl2_menu_procs);

    sdl2_menu_startMenu(menu);

    id.a_void = NULL;
    for (i = 0; roles[i].name.m != NULL; ++i) {
        if (valid_selection(i, flags.initrace, flags.initgend, flags.initalign)) {
            const char *name_m = roles[i].name.m;
            const char *name_f = roles[i].name.f;
            char text[BUFSZ];

            if (flags.initgend == 0 || name_f == NULL) {
                snprintf(text, SIZE(text), "%s", name_m);
            } else if (flags.initgend == 1) {
                snprintf(text, SIZE(text), "%s", name_f);
            } else {
                snprintf(text, SIZE(text), "%s/%s", name_m, name_f);
            }

            role = i;
            ++count;
            id.a_int = i + 1;
            sdl2_menu_addMenu(menu, NO_GLYPH, &id, 0, 0, 0, text, FALSE);
        }
    }

    /* Bail if no valid roles */
    if (count == 0) {
        goto return_false;
    }

    /* If only one role is compatible with other settings, don't bother with
       the menu */
    if (count == 1) {
        flags.initrole = role;
        goto return_true;
    }

    id.a_int = -1;
    sdl2_menu_addMenu(menu, NO_GLYPH, &id, '*', 0, 0, "Random", FALSE);

    sdl2_menu_endMenu(menu, "Choose your role:");

    rc = sdl2_menu_selectMenu(menu, PICK_ONE, &selection);
    if (rc <= 0) {
        goto return_false;
    }

    role = selection[0].item.a_int - 1;
    if (role < 0) {
        role = pick_role(flags.initrace, flags.initgend, flags.initalign, PICK_RANDOM);
    }

    flags.initrole = role;
    goto return_true;

return_true:
    sdl2_window_destroy(menu);
    return TRUE;

return_false:
    sdl2_window_destroy(menu);
    return FALSE;
}

static boolean
select_race(void)
{
    int race;
    unsigned count;
    anything id;
    struct SDL2Window *menu;
    int i;
    menu_item *selection;
    int rc;

    if (flags.initrace != -1) {
        return TRUE;
    }

    race = -1;
    count = 0;

    menu = sdl2_window_create(&sdl2_menu_procs);

    sdl2_menu_startMenu(menu);

    id.a_void = 0;
    for (i = 0; races[i].noun != NULL; ++i) {
        if (valid_selection(flags.initrole, i, flags.initgend, flags.initalign)) {
            const char *text = races[i].noun;

            race = i;
            ++count;
            id.a_int = i + 1;
            sdl2_menu_addMenu(menu, NO_GLYPH, &id, 0, 0, 0, text, FALSE);
        }
    }

    /* Bail if no valid races */
    if (count == 0) {
        goto return_false;
    }

    /* If only one race is compatible with other settings, don't bother with
       the menu */
    if (count == 1) {
        flags.initrace = race;
        goto return_true;
    }

    id.a_int = -1;
    sdl2_menu_addMenu(menu, NO_GLYPH, &id, '*', 0, 0, "Random", FALSE);

    sdl2_menu_endMenu(menu, "Choose your race:");

    rc = sdl2_menu_selectMenu(menu, PICK_ONE, &selection);
    if (rc <= 0) {
        goto return_false;
    }

    race = selection[0].item.a_int - 1;
    if (race < 0) {
        race = pick_race(flags.initrole, flags.initgend, flags.initalign, PICK_RANDOM);
    }

    flags.initrace = race;
    goto return_true;

return_true:
    sdl2_window_destroy(menu);
    return TRUE;

return_false:
    sdl2_window_destroy(menu);
    return FALSE;
}

static boolean
select_gender(void)
{
    int gend;
    unsigned count;
    anything id;
    struct SDL2Window *menu;
    int i;
    menu_item *selection;
    int rc;

    if (flags.initgend != -1) {
        return TRUE;
    }

    gend = -1;
    count = 0;

    menu = sdl2_window_create(&sdl2_menu_procs);

    sdl2_menu_startMenu(menu);

    id.a_void = NULL;
    for (i = 0; i < 2; ++i) {
        if (valid_selection(flags.initrole, flags.initrace, i, flags.initalign)) {
            const char *text = genders[i].adj;

            gend = i;
            ++count;
            id.a_int = i + 1;
            sdl2_menu_addMenu(menu, NO_GLYPH, &id, 0, 0, 0, text, FALSE);
        }
    }

    /* Bail if no valid genders */
    if (count == 0) {
        goto return_false;
    }

    /* If only one gender is compatible with other settings, don't bother with
       the menu */
    if (count == 1) {
        flags.initgend = gend;
        goto return_true;
    }

    id.a_int = -1;
    sdl2_menu_addMenu(menu, NO_GLYPH, &id, '*', 0, 0, "Random", FALSE);

    sdl2_menu_endMenu(menu, "Choose your gender:");

    rc = sdl2_menu_selectMenu(menu, PICK_ONE, &selection);
    if (rc <= 0) {
        goto return_false;
    }

    gend = selection[0].item.a_int - 1;
    if (gend < 0) {
        gend = pick_gend(flags.initrole, flags.initrace, flags.initalign, PICK_RANDOM);
    }

    flags.initgend = gend;
    goto return_true;

return_true:
    sdl2_window_destroy(menu);
    return TRUE;

return_false:
    sdl2_window_destroy(menu);
    return FALSE;
}

static boolean
selectAlignment(void)
{
    int alignment;
    unsigned count;
    anything id;
    struct SDL2Window *menu;
    int i;
    menu_item *selection;
    int rc;

    if (flags.initalign != -1) {
        return TRUE;
    }

    alignment = -1;
    count = 0;

    menu = sdl2_window_create(&sdl2_menu_procs);

    sdl2_menu_startMenu(menu);

    id.a_void = NULL;
    for (i = 0; i < 3; ++i) {
        if (valid_selection(flags.initrole, flags.initrace, flags.initgend, i)) {
            const char *text = aligns[i].adj;

            alignment = i;
            ++count;
            id.a_int = i + 1;
            sdl2_menu_addMenu(menu, NO_GLYPH, &id, 0, 0, 0, text, FALSE);
        }
    }

    /* Bail if no valid alignments */
    if (count == 0) {
        goto return_false;
    }

    /* If only one alignment is compatible with other settings, don't bother
       with the menu */
    if (count == 1) {
        flags.initalign = alignment;
        goto return_true;
    }

    id.a_int = -1;
    sdl2_menu_addMenu(menu, NO_GLYPH, &id, '*', 0, 0, "Random", FALSE);

    sdl2_menu_endMenu(menu, "Choose your alignment:");

    rc = sdl2_menu_selectMenu(menu, PICK_ONE, &selection);
    if (rc <= 0) {
        goto return_false;
    }

    alignment = selection[0].item.a_int - 1;
    if (alignment < 0) {
        alignment = pick_align(flags.initrole, flags.initrace, flags.initgend, PICK_RANDOM);
    }

    flags.initalign = alignment;
    goto return_true;

return_true:
    sdl2_window_destroy(menu);
    return TRUE;

return_false:
    sdl2_window_destroy(menu);
    return FALSE;
}

static boolean
valid_selection(int role, int race, int gender, int alignment)
{
    /* Any input set to -1 is unspecified; the combination is valid if there
       exists a setting which is valid with the others */
    if (role == -1) {
        for (role = 0; roles[role].name.m != NULL; ++role) {
            if (valid_selection(role, race, gender, alignment)) {
                return TRUE;
            }
        }
        return FALSE;
    }

    if (race == -1) {
        for (race = 0; races[race].noun != NULL; ++race) {
            if (valid_selection(role, race, gender, alignment)) {
                return TRUE;
            }
        }
        return FALSE;
    }

    if (gender == -1) {
        for (gender = 0; gender < ROLE_GENDERS; ++gender) {
            if (valid_selection(role, race, gender, alignment)) {
                return TRUE;
            }
        }
        return FALSE;
    }

    if (alignment == -1) {
        for (alignment = 0; alignment < ROLE_ALIGNS; ++alignment) {
            if (valid_selection(role, race, gender, alignment)) {
                return TRUE;
            }
        }
        return FALSE;
    }

    return validrole(role)
        && validrace(role, race)
        && validgend(role, race, gender)
        && validalign(role, race, alignment);
}
