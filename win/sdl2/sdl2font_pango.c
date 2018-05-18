/* sdl2_font_pango.c */

#include "hack.h"
#undef yn
#include <pango/pangoft2.h>
#include "sdl2font.h"

/* Classes and functions used internally here: */
struct SDL2Font_Impl {
    PangoFontDescription *desc;
    PangoFontMap *map;
    PangoContext *ctx;
    PangoLayout *layout;
};

/* Simplified functions to create and free an FT_Bitmap */
static FT_Bitmap *FT_Bitmap_new(int width, int height);
static void FT_Bitmap_free(FT_Bitmap *bitmap);

static void ch_to_utf8(char utf8[5], Uint32 ch);

static FT_Bitmap *
FT_Bitmap_new(int width, int height)
{
    /* Per the pango-view example */
    FT_Bitmap *bitmap = (FT_Bitmap *) alloc(sizeof(FT_Bitmap));
    bitmap->width = width;
    bitmap->pitch = (bitmap->width + 3) & ~3;
    bitmap->rows = height;
    bitmap->buffer = (unsigned char *) alloc(bitmap->pitch * bitmap->rows);
    memset(bitmap->buffer, 0x00, bitmap->pitch * bitmap->rows);
    bitmap->num_grays = 255;
    bitmap->pixel_mode = ft_pixel_mode_grays;
    bitmap->palette_mode = 0;
    bitmap->palette = NULL;
    return bitmap;
}

static void
FT_Bitmap_free(FT_Bitmap *bitmap)
{
    if (bitmap != NULL) {
        free(bitmap->buffer);
        free(bitmap);
    }
};

static PangoFontMetrics *getMetrics(const SDL2Font *font);

/* Create a new SDL2Font */
SDL2Font *
sdl2_font_new(const char *name, int ptsize)
{
    SDL2Font *font = (SDL2Font *) alloc(sizeof(*font));

    font->desc = pango_font_description_new();
    font->map = pango_ft2_font_map_new();
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

/* Font metrics */
int
sdl2_font_ascent(SDL2Font *font)
{
    PangoFontMetrics *metrics = getMetrics(font);
    int ascent = pango_font_metrics_get_ascent(metrics);
    pango_font_metrics_unref(metrics);
    return ascent / PANGO_SCALE;
}

int
sdl2_font_descent(SDL2Font *font)
{
    PangoFontMetrics *metrics = getMetrics(font);
    int descent = pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    return descent / PANGO_SCALE;
}

int
sdl2_font_lineSkip(SDL2Font *font)
{
    PangoFontMetrics *metrics = getMetrics(font);
    int skip = pango_font_metrics_get_ascent(metrics)
             + pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    return skip / PANGO_SCALE;
}

int
sdl2_font_height(SDL2Font *font)
{
    PangoFontMetrics *metrics = getMetrics(font);
    int height = pango_font_metrics_get_ascent(metrics)
               + pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    return height / PANGO_SCALE;
}

/* Text rendering */
/* If no background is given, background is transparent */
SDL_Surface *
sdl2_font_renderChar(SDL2Font *font, utf32_t ch, SDL_Color foreground)
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
sdl2_font_renderCharBG(SDL2Font *font, utf32_t ch, SDL_Color foreground,
                       SDL_Color background)
{
    char utf8[5];
    SDL_Surface *surface;

    ch_to_utf8(utf8, ch);
    surface = sdl2_font_renderStrBG(font, utf8, foreground, background);
    return surface;
}

SDL_Surface *
sdl2_font_renderStrBG(SDL2Font *font, const char *text, SDL_Color foreground,
                      SDL_Color background)
{
    pango_layout_set_text(font->layout, text, strlen(text));

    int width, height;
    pango_layout_get_pixel_size(font->layout, &width, &height);
    FT_Bitmap *bitmap = FT_Bitmap_new(width, height);

    pango_ft2_render_layout(bitmap, font->layout, 0, 0);

    /* Convert Pango bitmap format to SDL */
    SDL_Surface *surface = SDL_CreateRGBSurface(
            SDL_SWSURFACE,
            bitmap->width, bitmap->rows, 32,
            0x000000FF,  /* red */
            0x0000FF00,  /* green */
            0x00FF0000,  /* blue */
            0xFF000000); /* alpha */
    if (surface != NULL) {
        unsigned char *row1;
        Uint32 *row2;

        for (int y = 0; y < bitmap->rows; ++y) {
            row1 = bitmap->buffer + bitmap->pitch * y;
            row2 = (Uint32 *) ((unsigned char *) surface->pixels + surface->pitch * y);
            for (int x = 0; x < bitmap->width; ++x) {
                unsigned char r, g, b, a;
                unsigned char alpha = row1[x];
                /* The most common cases are alpha == 0 and alpha == 255 */
                if (alpha == 0) {
                    r = background.r;
                    g = background.g;
                    b = background.b;
                    a = background.a;
                } else if (alpha == 255 && foreground.a == 255) {
                    r = foreground.r;
                    g = foreground.g;
                    b = foreground.b;
                    a = foreground.a;
                } else {
                    /* srcA, dstA and outA are fixed point quantities
                       such that 0xFF00 represents an alpha of 1 */
                    Uint32 srcA = foreground.a * 256 * alpha / 255;
                    Uint32 dstA = background.a * 256;
                    dstA = dstA * (0xFF00 - srcA) / 0xFF00;
                    Uint32 outA = srcA + dstA;
                    if (outA == 0) {
                        r = 0;
                        g = 0;
                        b = 0;
                    } else {
                        r = (foreground.r*srcA + background.r*dstA) / outA;
                        g = (foreground.g*srcA + background.g*dstA) / outA;
                        b = (foreground.b*srcA + background.b*dstA) / outA;
                    }
                    a = outA / 256;
                }
                row2[x] = ((Uint32) (r) <<  0)
                        | ((Uint32) (g) <<  8)
                        | ((Uint32) (b) << 16)
                        | ((Uint32) (a) << 24);
            }
        }
    }

    FT_Bitmap_free(bitmap);
    return surface;
}

/* Text extent */
SDL_Rect
sdl2_font_textSizeChar(SDL2Font *font, utf32_t ch)
{
    char utf8[5];
    SDL_Rect rect;

    ch_to_utf8(utf8, ch);
    rect = sdl2_font_textSizeStr(font, utf8);
    return rect;
}

SDL_Rect
sdl2_font_textSizeStr(SDL2Font *font, const char *text)
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

/* Convert code point to UTF-8 */
static void
ch_to_utf8(char utf8[5], Uint32 ch)
{
    /* Filter invalid code points */
    if (ch > 0x10FFFF || (0xD800 <= ch && ch <= 0xDFFF)) {
        ch = 0xFFFD;
    }

    /* Convert */
    if (ch < 0x80) {
        utf8[0] = (char) ch;
        utf8[1] = '\0';
    } else if (ch < 0x800) {
        utf8[0] = (char) (0xC0 | (ch >> 6));
        utf8[1] = (char) (0x80 | (ch & 0x3F));
        utf8[2] = '\0';
    } else if (ch < 0x10000) {
        utf8[0] = (char) (0xC0 | (ch >> 12));
        utf8[1] = (char) (0x80 | ((ch >> 6) & 0x3F));
        utf8[2] = (char) (0x80 | (ch & 0x3F));
        utf8[3] = '\0';
    } else {
        utf8[0] = (char) (0xC0 |  (ch >> 18));
        utf8[1] = (char) (0x80 | ((ch >> 12) & 0x3F));
        utf8[2] = (char) (0x80 | ((ch >>  6) & 0x3F));
        utf8[3] = (char) (0x80 | (ch & 0x3F));
        utf8[4] = '\0';
    }
}
