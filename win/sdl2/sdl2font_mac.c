/* sdl2font_mac.cpp */

#include "hack.h"
#undef yn
#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include "sdl2font.h"
#include "sdl2unicode.h"

/* RGB color space */
static CGColorSpaceRef rgb_colorspace = NULL;

/* Transparent background */
static SDL_Color transparent = { 0, 0, 0, 0 };

struct SDL2Font_Impl
{
    CTFontRef font;
    CFDictionaryRef attributes;
};

static UniChar *
singleChar(UniChar str16[3], Uint32 ch)
{
    if (ch > 0x10FFFF || (ch & 0xFFFFF800) == 0xD800) {
        ch = 0xFFFD;
    }

    if (ch < 0x10000) {
        str16[0] = (UniChar) ch;
        str16[1] = 0;
    } else {
        str16[0] = 0xD7C0 + (ch >> 10);
        str16[1] = 0xDC00 + (ch & 0x3FF);
        str16[2] = 0;
    }
    return str16;
}

/* UTF-8 to UTF-16 conversion */
static UniChar *
appleString(const char *inpstr)
{
    size_t outsize;
    size_t i, j;
    UniChar *outstr;

    /* Determine the output size */
    outsize = 1U;
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        unsigned char byte = (unsigned char) inpstr[i];
        if (byte < 0x80 || 0xBF < byte) {
            ++outsize;
        }
        if (byte >= 0xF0) {
            ++outsize;
        }
    }

    /* Allocate the output string */
    outstr = (UniChar *) alloc(outsize * sizeof(UniChar));

    /* Convert to UTF-16 */
    j = 0U;
    i = 0U;
    while (inpstr[i] != '\0') {
        unsigned char byte = (unsigned char) inpstr[i++];
        Uint32 ch32;
        Uint32 min;
        unsigned count;

        if (byte < 0x80) {
            ch32 = byte;
            min = 0x00;
            count = 0;
        } else if (byte < 0xC0) {
            ch32 = 0xFFFD;
            min = 0x00;
            count = 0;
        } else if (byte < 0xE0) {
            ch32 = byte & 0x1F;
            min = 0x80;
            count = 1;
        } else if (byte < 0xF0) {
            ch32 = byte & 0x0F;
            min = 0x800;
            count = 2;
        } else if (byte < 0xF5) {
            ch32 = byte & 0x07;
            min = 0x10000;
            count = 3;
        } else {
            ch32 = 0xFFFD;
            min = 0x00;
            count = 0;
        }

        for (; count != 0; --count) {
            byte = (unsigned char) inpstr[i];
            if (byte < 0x80 || 0xBF < byte) {
                break;
            }
            ch32 = (ch32 << 6) | (byte & 0x3F);
            ++i;
        }
        if (count != 0 || ch32 > 0x10FFFF
                || (0xD800 <= ch32 && ch32 <= 0xDFFF)) {
            ch32 = 0xFFFD;
        }
        if (ch32 < 0x10000) {
            outstr[j++] = ch32;
        } else {
            outstr[j++] = 0xD7C0 + (ch32 >> 10);
            outstr[j++] = 0xDC00 + (ch32 & 0x3FF);
        }
    }
    assert(j < outsize);
    outstr[j] = 0U;

    return outstr;
}

static size_t
appleLength(const UniChar *str)
{
    size_t len;

    for (len = 0; str[len] != 0; ++len) {}
    return len;
}

static SDL_Surface *renderImpl
        (SDL2Font *mfont, const UniChar *text,
        SDL_Color foreground, SDL_Color background);
static SDL_Rect textSizeImpl(SDL2Font *mfont, const UniChar *text);

SDL2Font *
sdl2_font_new(const char *name, int ptsize)
{
    SDL2Font *mfont;
    CFStringRef nameref;
    CFStringRef keys[1];
    CFTypeRef values[1];

    if (rgb_colorspace == NULL) {
        rgb_colorspace = CGColorSpaceCreateDeviceRGB();
    }

    mfont = (SDL2Font *) alloc(sizeof(*mfont));
    nameref = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    mfont->font = CTFontCreateWithName(nameref, ptsize, NULL);

    keys[0] = kCTFontAttributeName;
    values[0] = mfont->font;
    mfont->attributes = CFDictionaryCreate(
            NULL,
            (const void **) keys, (const void **) values,
            SIZE(keys),
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

    CFRelease(nameref);
    return mfont;
}

void
sdl2_font_free(SDL2Font *mfont)
{
    CFRelease(mfont->font);
    CFRelease(mfont->attributes);

    free(mfont);
}

/* Font metrics */
int
sdl2_font_ascent(SDL2Font *mfont)
{
    return (int) CTFontGetAscent(mfont->font);
}

int
sdl2_font_descent(SDL2Font *mfont)
{
    return (int) CTFontGetDescent(mfont->font);
}

int
sdl2_font_lineSkip(SDL2Font *mfont)
{
    return (int) (CTFontGetAscent(mfont->font) +
                  CTFontGetDescent(mfont->font));
}

int
sdl2_font_height(SDL2Font *mfont)
{
    return (int) (CTFontGetAscent(mfont->font) +
                  CTFontGetDescent(mfont->font) +
                  CTFontGetLeading(mfont->font));
}

/* Text rendering */
/* If no background is given, background is transparent */
SDL_Surface *
sdl2_font_renderChar(SDL2Font *mfont, Uint32 ch, SDL_Color foreground)
{
    return sdl2_font_renderCharBG(mfont, ch, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderStr(SDL2Font *mfont, const char *text, SDL_Color foreground)
{
    return sdl2_font_renderStrBG(mfont, text, foreground, transparent);
}

SDL_Surface *
sdl2_font_renderCharBG(SDL2Font *mfont, Uint32 ch, SDL_Color foreground,
                       SDL_Color background)
{
    SDL_Surface *surface;
    UniChar text16[3];

    surface = renderImpl(mfont, singleChar(text16, ch), foreground, background);
    return surface;
}

SDL_Surface *
sdl2_font_renderStrBG(SDL2Font *mfont, const char *text, SDL_Color foreground,
                      SDL_Color background)
{
    SDL_Surface *surface;
    UniChar *text16 = appleString(text);

    surface = renderImpl(mfont, text16, foreground, background);
    free(text16);

    return surface;
}

static SDL_Surface *
renderImpl(SDL2Font *mfont, const UniChar *text, SDL_Color foreground,
           SDL_Color background)
{
    CFStringRef string = NULL; /* custodial */
    CFAttributedStringRef attrstring = NULL; /* custodial */
    CTLineRef line = NULL; /* custodial */
    CGContextRef context_ref = NULL; /* custodial */
    int x, y, w, h;
    CGFloat width, ascent, descent, leading;
    SDL_Surface *surface;

    string = CFStringCreateWithCharacters(NULL, text, appleLength(text));

    attrstring = CFAttributedStringCreate(NULL, string, mfont->attributes);

    /* CoreText line with complex rendering */
    line = CTLineCreateWithAttributedString(attrstring);

    /* Size of text */
    width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    w = (int) width;
    h = (int) (ascent + descent + leading);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    /*
     * SDL expects the color components not to be premultiplied by the alpha.
     * CoreGraphics does not support this usage, so we have to fake it: the
     * text is drawn in black and the returned pixels have only alpha, and we
     * fill in the colors later.
     */

    /* SDL surface to receive the bitmap */
    surface = SDL_CreateRGBSurface(
                SDL_SWSURFACE,
                w, h, 32,
                0x000000FF,  /* red */
                0x0000FF00,  /* green */
                0x00FF0000,  /* blue */
                0xFF000000); /* alpha */
    if (surface == NULL) goto end;

    /* CoreGraphics bitmap context */
    context_ref = CGBitmapContextCreate(surface->pixels, w, h, 8,
                surface->pitch, rgb_colorspace,
                kCGImageAlphaPremultipliedLast);
    CGContextSetTextDrawingMode(context_ref, kCGTextFillStrokeClip);

    CGContextSetTextPosition(context_ref, 0.0, descent + leading/2.0);
    CTLineDraw(line, context_ref);

    for (y = 0; y < surface->h; ++y) {
        Uint32 *row = (Uint32 *) (
                (uint8_t *) surface->pixels + surface->pitch * y);
        for (x = 0; x < surface->w; ++x) {
            unsigned alpha = row[x] >> 24;
            uint8_t r, g, b, a;
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
                Uint32 srcA, dstA, outA;
                srcA = foreground.a * 256 * alpha / 255;
                dstA = background.a * 256;
                dstA = dstA * (0xFF00 - srcA) / 0xFF00;
                outA = srcA + dstA;
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
            row[x] = ((Uint32) (r) <<  0)
                   | ((Uint32) (g) <<  8)
                   | ((Uint32) (b) << 16)
                   | ((Uint32) (a) << 24);
        }
    }

end:
    if (string) CFRelease(string);
    if (attrstring) CFRelease(attrstring);
    if (line) CFRelease(line);
    if (context_ref) CFRelease(context_ref);
    return surface;
}


/* Text extent */
SDL_Rect
sdl2_font_textSizeChar(SDL2Font *mfont, Uint32 ch)
{
    UniChar text16[3];

    return textSizeImpl(mfont, singleChar(text16, ch));
}

SDL_Rect
sdl2_font_textSizeStr(SDL2Font *mfont, const char *text)
{
    UniChar *text16 = appleString(text);

    SDL_Rect size = textSizeImpl(mfont, text16);
    free(text16);
    return size;
}

/* Text extent */
static SDL_Rect
textSizeImpl(SDL2Font *mfont, const UniChar *text)
{
    CFStringRef string;
    CFAttributedStringRef attrstring;
    CTLineRef line;
    CGFloat width, ascent, descent, leading;
    SDL_Rect size;

    string = CFStringCreateWithCharacters(NULL, text, appleLength(text));

    attrstring = CFAttributedStringCreate(NULL, string, mfont->attributes);

    line = CTLineCreateWithAttributedString(attrstring);

    width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    
    size.x = 0;
    size.y = 0;
    size.w = width;
    size.h = ascent + descent + leading;

    return size;
}

/* Default font names */
const char sdl2_font_defaultMonoFont[] = "CourierNewPSMT";
const char sdl2_font_defaultSerifFont[] = "TimesNewRomanPSMT";
const char sdl2_font_defaultSansFont[] = "ArialMT";
