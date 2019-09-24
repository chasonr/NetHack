/* sdl2font_mac.cpp */

#include "hack.h"
#undef yn
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include "sdl2font.h"

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

/* ISO 8859-1 to UTF-16 conversion */
static UniChar *
appleString(const char *inpstr)
{
    size_t i;
    UniChar *outstr;

    /* Allocate the output string */
    outstr = (UniChar *) alloc((strlen(inpstr) + 1U) * sizeof(UniChar));

    /* Convert to UTF-16 */
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        outstr[i] = (unsigned char) inpstr[i];
    }
    outstr[i] = 0U;

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
    static const CGFloat whitepoint[] = { 1.0, 1.0, 1.0 };
    CFRange range;
    CFStringRef string = NULL; /* custodial */
    CFMutableAttributedStringRef attrstring = NULL; /* custodial */
    CGFloat colors[4];
    CGColorSpaceRef cs_ref = NULL; /* custodial */
    CGColorRef fg_ref = NULL; /* custodial */
    CGColorRef bg_ref = NULL; /* custodial */
    CTLineRef line = NULL; /* custodial */
    CGContextRef context_ref = NULL; /* custodial */
    int x, y, w, h;
    CGFloat width, ascent, descent, leading;
    SDL_Surface *surface;

    string = CFStringCreateWithCharacters(NULL, text, appleLength(text));

    /* Create a string object */
    attrstring = CFAttributedStringCreateMutable(NULL, 0);
    range.location = 0;
    range.length = 0;
    CFAttributedStringReplaceString(attrstring, range, string);

    /* Set the font */
    range.length = CFStringGetLength(string);
    CFAttributedStringSetAttributes(attrstring, range, mfont->attributes, TRUE);

    /* Set the colors */
    cs_ref = CGColorSpaceCreateCalibratedRGB(
            whitepoint, NULL, NULL, NULL);
    colors[0] = foreground.r / 255.0;
    colors[1] = foreground.g / 255.0;
    colors[2] = foreground.b / 255.0;
    colors[3] = foreground.a / 255.0;
    fg_ref = CGColorCreate(cs_ref, colors);
    CFAttributedStringSetAttribute(attrstring, range,
            kCTForegroundColorAttributeName,
            fg_ref);
    colors[0] = background.r / 255.0;
    colors[1] = background.g / 255.0;
    colors[2] = background.b / 255.0;
    colors[3] = background.a / 255.0;
    bg_ref = CGColorCreate(cs_ref, colors);
    CFAttributedStringSetAttribute(attrstring, range,
            kCTBackgroundColorAttributeName,
            bg_ref);

    /* CoreText line with complex rendering */
    line = CTLineCreateWithAttributedString(attrstring);

    /* Size of text */
    width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    w = (int) width;
    h = (int) (ascent + descent + leading);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

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
    CGContextSetTextDrawingMode(context_ref, kCGTextFill);

    CGContextSetTextPosition(context_ref, 0.0, descent + leading/2.0);
    CTLineDraw(line, context_ref);

    for (y = 0; y < surface->h; ++y) {
        Uint32 *row = (Uint32 *) (
                (unsigned char *) surface->pixels + surface->pitch * y);
        for (x = 0; x < surface->w; ++x) {
            Uint32 pixel;
            unsigned char r, g, b, a;
            /* CoreGraphics returns RGB prescaled by alpha, but SDL2 needs it
             * without the prescaling */
            pixel = row[x];
            r = (unsigned char) (pixel >>  0);
            g = (unsigned char) (pixel >>  8);
            b = (unsigned char) (pixel >> 16);
            a = (unsigned char) (pixel >> 24);
            if (a != 0) {
                r = r * 255 / a;
                g = g * 255 / a;
                b = b * 255 / a;
            }
            row[x] = ((Uint32) (r) <<  0)
                   | ((Uint32) (g) <<  8)
                   | ((Uint32) (b) << 16)
                   | ((Uint32) (a) << 24);
        }
    }

end:
    if (fg_ref) CFRelease(fg_ref);
    if (bg_ref) CFRelease(bg_ref);
    if (cs_ref) CFRelease(cs_ref);
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
