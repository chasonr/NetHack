/* sdl2font_windows.c */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include "hack.h"
#undef yn
#include "sdl2font.h"

struct SDL2Font_Impl
{
    HFONT hfont;
    HDC memory_dc;
};

static wchar_t *
singleChar(wchar_t str16[3], Uint32 ch)
{
    if (ch > 0x10FFFF || (ch & 0xFFFFF800) == 0xD800) {
        ch = 0xFFFD;
    }

    if (ch < 0x10000) {
        str16[0] = (wchar_t) ch;
        str16[1] = 0;
    } else {
        str16[0] = 0xD7C0 + (ch >> 10);
        str16[1] = 0xDC00 + (ch & 0x3FF);
        str16[2] = 0;
    }
    return str16;
}


/* ISO 8859-1 to UTF-16 conversion */
static wchar_t *
win32String(const char *inpstr)
{
    size_t i;
    wchar_t *outstr;

    /* Allocate the output string */
    outstr = (wchar_t *) alloc((strlen(inpstr) + 1U) * sizeof(wchar_t));

    /* Convert to UTF-16 */
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        outstr[i] = (unsigned char) inpstr[i];
    }
    outstr[i] = 0U;

    return outstr;
}

static SDL_Surface *renderImpl(
        SDL2Font *m_impl,
        const wchar_t *text,
        SDL_Color foreground,
        SDL_Color background);
static SDL_Rect textSizeImpl(SDL2Font *m_impl, const wchar_t *text);

SDL2Font *
sdl2_font_new(const char *name, int ptsize)
{
    SDL2Font *font = (SDL2Font *) alloc(sizeof(SDL2Font));
    int height;

    font->hfont = NULL;
    font->memory_dc = NULL;

    font->memory_dc = CreateCompatibleDC(NULL);
    if (font->memory_dc == NULL) {
        panic("Cannot initialize font->memory_dc");
    }
    /*RLC The constant 72 is specified in Microsoft's documentation for
       CreateFont; but 97 comes closer to the behavior of the original
       SDL_ttf library. */
    height = -MulDiv(ptsize, GetDeviceCaps(font->memory_dc, LOGPIXELSY), 97);

    font->hfont = CreateFontA(
            height,
            0,
            0,
            0,
            FW_DONTCARE,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            name);
    if (font->hfont == NULL) {
        panic("Cannot initialize font->hfont");
    }

    SelectObject(font->memory_dc, font->hfont);
    return font;
}

void
sdl2_font_free(SDL2Font *font)
{
    if (font == NULL) return;

    if (font->hfont != NULL) {
        DeleteObject(font->hfont);
    }
    if (font->memory_dc != NULL) {
        DeleteDC(font->memory_dc);
    }

    free(font);
}

/* Font metrics */
int
sdl2_font_ascent(SDL2Font *font)
{
    TEXTMETRIC metrics;

    GetTextMetrics(font->memory_dc, &metrics);
    return metrics.tmAscent;
}

int
sdl2_font_descent(SDL2Font *font)
{
    TEXTMETRIC metrics;

    GetTextMetrics(font->memory_dc, &metrics);
    return metrics.tmDescent;
}

int
sdl2_font_lineSkip(SDL2Font *font)
{
    TEXTMETRIC metrics;

    GetTextMetrics(font->memory_dc, &metrics);
    return metrics.tmHeight + metrics.tmExternalLeading;
}

int
sdl2_font_height(SDL2Font *font)
{
    TEXTMETRIC metrics;

    GetTextMetrics(font->memory_dc, &metrics);
    return metrics.tmHeight;
}

static const SDL_Color transparent = { 0, 0, 0, 0 };

/* Text rendering */
/* If no background is given, background is transparent */
SDL_Surface *
sdl2_font_renderChar(SDL2Font *font, Uint32 ch, SDL_Color foreground)
{
    return sdl2_font_renderCharBG(font, ch, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderStr(SDL2Font *font, const char *text, SDL_Color foreground)
{
    return sdl2_font_renderStrBG(font, text, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderCharBG(SDL2Font *font, Uint32 ch, SDL_Color foreground,
        SDL_Color background)
{
    wchar_t text16[3];

    return renderImpl(font, singleChar(text16, ch), foreground, background);
}

SDL_Surface *
sdl2_font_renderStrBG(
        SDL2Font *font,
        const char *text,
        SDL_Color foreground,
        SDL_Color background)
{
    SDL_Surface *surface;
    wchar_t *text16 = win32String(text);

    surface = renderImpl(font, text16, foreground, background);
    free(text16);
    return surface;
}

static SDL_Surface *
renderImpl(
        SDL2Font *font,
        const wchar_t *text,
        SDL_Color foreground,
        SDL_Color background)
{
    HFONT hfont = font->hfont;
    HDC memory_dc = NULL; /* custodial */
    HBITMAP hbitmap = NULL; /* custodial */
    SIZE size;
    BITMAPINFO bi;
    void *bits;
    SDL_Surface *surface = NULL; /* custodial, returned */
    int x, y;
    Uint32 *row1, *row2;

    /* We'll render to this device context */
    memory_dc = CreateCompatibleDC(NULL);
    if (memory_dc == NULL) goto error;
    if (!SelectObject(memory_dc, (HGDIOBJ)hfont)) goto error;

    /* We'll do alpha blending ourselves by using these colors */
    if (SetTextColor(memory_dc, RGB(255, 255, 255)) == CLR_INVALID)
        goto error;
    if (SetBkColor(memory_dc, RGB(0, 0, 0)) == CLR_INVALID)
        goto error;
    if (!SetBkMode(memory_dc, OPAQUE)) goto error;

    /* Get the size of the bitmap */
    if (!GetTextExtentPoint32W(memory_dc, text, wcslen(text), &size)) goto error;
    /* Bitmap creation fails if the size is zero */
    if (size.cx == 0) { size.cx = 1; }
    if (size.cy == 0) { size.cy = 1; }

    /* We'll use this bitmap */
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi);
    bi.bmiHeader.biWidth = size.cx;
    bi.bmiHeader.biHeight = size.cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    hbitmap = CreateDIBSection(memory_dc, &bi, 0, &bits, NULL, 0);
    if (hbitmap == NULL) goto error;
    if (!SelectObject(memory_dc, (HGDIOBJ)hbitmap)) goto error;

    /* Render the text */
    if (!TextOutW(memory_dc, 0, 0, text, wcslen(text))) goto error;

    /* Get the bitmap */
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi);
    bi.bmiHeader.biWidth = size.cx;
    bi.bmiHeader.biHeight = size.cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    surface = SDL_CreateRGBSurface(
            SDL_SWSURFACE,
            size.cx, size.cy, 32,
            0x000000FF,  /* red */
            0x0000FF00,  /* green */
            0x00FF0000,  /* blue */
            0xFF000000); /* alpha */
    if (surface == NULL) goto error;
    if (!GetDIBits(memory_dc, hbitmap, 0, size.cy, surface->pixels, &bi,
            DIB_RGB_COLORS))
        goto error;
    for (y = 0; y < size.cy; ++y)
    {
        row1 = (Uint32 *)bits + size.cx*y;
        row2 = (Uint32 *)surface->pixels + size.cx*(size.cy-1-y);
        for (x = 0; x < size.cx; ++x)
        {
            unsigned char r, g, b, a;
            unsigned alpha = row1[x] & 0xFF;
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
                uint_fast32_t srcA = foreground.a * 256 * alpha / 255;
                uint_fast32_t dstA = background.a * 256;
                dstA = dstA * (0xFF00 - srcA) / 0xFF00;
                uint_fast32_t outA = srcA + dstA;
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

    DeleteDC(memory_dc);
    DeleteObject(hbitmap);
    return surface;

error:
    DeleteDC(memory_dc);
    DeleteObject(hbitmap);
    if (surface) SDL_FreeSurface(surface);
    return NULL;
}

/* Text extent */
SDL_Rect
sdl2_font_textSizeChar(SDL2Font *font, Uint32 ch)
{
    wchar_t text16[3];

    return textSizeImpl(font, singleChar(text16, ch));
}

extern SDL_Rect
sdl2_font_textSizeStr(SDL2Font *font, const char *text)
{
    SDL_Rect rect;
    wchar_t *text16 = win32String(text);

    rect = textSizeImpl(font, text16);
    free(text16);
    return rect;
}

static SDL_Rect
textSizeImpl(SDL2Font *font, const wchar_t *text)
{
    SIZE size;

    /* Get the size of the text */
    GetTextExtentPoint32W(font->memory_dc, text, wcslen(text), &size);

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = size.cx;
    rect.h = size.cy;

    return rect;
}

/* Default font names */
const char sdl2_font_defaultMonoFont[] = "Courier New";
const char sdl2_font_defaultSerifFont[] = "Times New Roman";
const char sdl2_font_defaultSansFont[] = "Arial";
