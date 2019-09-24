/* sdl2_font_pango.c */

#include "hack.h"
#undef yn
#include <pango/pangocairo.h>
#include "sdl2font.h"

/* Classes and functions used internally here: */
struct SDL2Font_Impl {
    PangoFontDescription *desc;
    PangoFontMap *map;
    PangoContext *ctx;
    PangoLayout *layout;
};

/* Encoding conversions */
static SDL_Rect sdl2_font_textSizeStr_UTF8(SDL2Font *font, const char *text);
static SDL_Surface *sdl2_font_renderStrBG_UTF8(SDL2Font *font,
        const char *text, SDL_Color foreground, SDL_Color background);
static char * iso8859_1_to_utf8(const char *inpstr);

static PangoFontMetrics *getMetrics(const SDL2Font *font);

/* Create a new SDL2Font */
SDL2Font *
sdl2_font_new(const char *name, int ptsize)
{
    SDL2Font *font = (SDL2Font *) alloc(sizeof(*font));

    font->desc = pango_font_description_new();
    font->map = pango_cairo_font_map_new();
    font->ctx = pango_font_map_create_context(font->map);
    font->layout = pango_layout_new(font->ctx);

    pango_font_description_set_family(font->desc, name);
    pango_font_description_set_size(font->desc, ptsize * PANGO_SCALE);
    pango_layout_set_font_description(font->layout, font->desc);

    return font;
}

/* Free an SDL2Font */
void
sdl2_font_free(SDL2Font *font)
{
    g_object_unref(font->layout);
    g_object_unref(font->ctx);
    g_object_unref(font->map);
    pango_font_description_free(font->desc);
    free(font);
}

/* Text rendering */
/* If no background is given, background is transparent */
SDL_Surface *
sdl2_font_renderChar(SDL2Font *font, Uint32 ch, SDL_Color foreground)
{
    static const SDL_Color transparent = { 0, 0, 0, 0 };
    return sdl2_font_renderCharBG(font, ch, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderStr(SDL2Font *font, const char *text, SDL_Color foreground)
{
    static const SDL_Color transparent = { 0, 0, 0, 0 };
    return sdl2_font_renderStrBG(font, text, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderCharBG(SDL2Font *font, Uint32 ch, SDL_Color foreground,
                       SDL_Color background)
{
    char utf8[5];
    SDL_Surface *surface;

    char_to_utf8(utf8, ch);
    surface = sdl2_font_renderStrBG_UTF8(font, utf8, foreground, background);
    return surface;
}

SDL_Surface *
sdl2_font_renderStrBG(SDL2Font *font, const char *text, SDL_Color foreground,
                      SDL_Color background)
{
    char *utf8 = iso8859_1_to_utf8(text);
    SDL_Surface *surface = sdl2_font_renderStrBG_UTF8(font, utf8, foreground,
                                                      background);
    free(utf8);
    return surface;
}

static SDL_Surface *
sdl2_font_renderStrBG_UTF8(SDL2Font *font, const char *text,
                           SDL_Color foreground, SDL_Color background)
{
    int width, height;
    cairo_surface_t *csurface;
    cairo_t *cr;
    SDL_Surface *surface;
    unsigned char *image_data;

    pango_layout_set_text(font->layout, text, strlen(text));

    pango_layout_get_pixel_size(font->layout, &width, &height);
    csurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(csurface);

    /* Render the background */
    cairo_set_source_rgba(cr,
                          background.r / 255.0,
                          background.g / 255.0,
                          background.b / 255.0,
                          background.a / 255.0);
    cairo_paint(cr);

    /* Render the text */
    cairo_set_source_rgba(cr,
                          foreground.r / 255.0,
                          foreground.g / 255.0,
                          foreground.b / 255.0,
                          foreground.a / 255.0);
    pango_cairo_update_layout(cr, font->layout);
    pango_cairo_show_layout(cr, font->layout);

    /* Convert to an SDL2 surface */
    image_data = cairo_image_surface_get_data(csurface);

    /* Convert Pango bitmap format to SDL */
    surface = SDL_CreateRGBSurface(
            SDL_SWSURFACE,
            width, height, 32,
            0x000000FF,  /* red */
            0x0000FF00,  /* green */
            0x00FF0000,  /* blue */
            0xFF000000); /* alpha */
    if (surface != NULL) {
        unsigned char *row1;
        unsigned char *row2;

        for (int y = 0; y < height; ++y) {
            row1 = image_data + width * 4 * y;
            row2 = (unsigned char *) surface->pixels + surface->pitch * y;
            for (int x = 0; x < width; ++x) {
                unsigned char r, g, b, a;
                /* Cairo returns RGB prescaled by alpha, but SDL2 needs it
                 * without the prescaling */
                r = row1[x*4 + 0];
                g = row1[x*4 + 1];
                b = row1[x*4 + 2];
                a = row1[x*4 + 3];
                if (a != 0) {
                    r = r * 255 / a;
                    g = g * 255 / a;
                    b = b * 255 / a;
                }
                row2[x*4 + 0] = b;
                row2[x*4 + 1] = g;
                row2[x*4 + 2] = r;
                row2[x*4 + 3] = a;
            }
        }
    }

    cairo_surface_destroy(csurface);
    cairo_destroy(cr);
    return surface;
}

/* Text extent */
SDL_Rect
sdl2_font_textSizeChar(SDL2Font *font, Uint32 ch)
{
    char utf8[5];
    SDL_Rect rect;

    char_to_utf8(utf8, ch);
    rect = sdl2_font_textSizeStr_UTF8(font, utf8);
    return rect;
}

SDL_Rect
sdl2_font_textSizeStr(SDL2Font *font, const char *text)
{
    char *utf8 = iso8859_1_to_utf8(text);
    SDL_Rect rect = sdl2_font_textSizeStr_UTF8(font, utf8);
    free(utf8);
    return rect;
}

static SDL_Rect
sdl2_font_textSizeStr_UTF8(SDL2Font *font, const char *text)
{
    pango_layout_set_text(font->layout, text, strlen(text));

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    pango_layout_get_pixel_size(font->layout, &rect.w, &rect.h);

    return rect;
}

/* Default font names */
const char sdl2_font_defaultMonoFont[] = "DejaVu Sans Mono";
const char sdl2_font_defaultSerifFont[] = "DejaVu Serif";
const char sdl2_font_defaultSansFont[] = "DejaVu Sans";

/* Retrieve metrics for a font */
static PangoFontMetrics *
getMetrics(const SDL2Font *font)
{
    PangoFont *pfont = pango_font_map_load_font(font->map, font->ctx, font->desc);

    PangoFontMetrics *metrics = pango_font_get_metrics(pfont, NULL);

    g_object_unref(pfont);
    return metrics;
}

/* Convert ISO 8859-1 string to UTF-8 */
static char *
iso8859_1_to_utf8(const char *inpstr)
{
    /* Max 2 output bytes per input byte */
    char *outstr = (char *) alloc(strlen(inpstr) * 2U + 1U);
    size_t i, j;

    j = 0U;
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        char ch = inpstr[i];

        if ((ch & 0x80) != 0) {
            outstr[j++] = (char) (0xC0 | (ch >> 6));
            outstr[j++] = (char) (0x80 | (ch & 0x3F));
        } else {
            outstr[j++] = (char) ch;
        }
    }

    outstr[j] = '\0';
    return outstr;
}
