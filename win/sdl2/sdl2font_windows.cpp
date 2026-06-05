// sdl2font_windows2.cpp
// Supports rendering of fonts to an SDL2 surface under Microsoft Windows.
// Uses DirectWrite to support color fonts.
// Falls back to GDI if DirectWrite is not available.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
extern "C" {
#include "hack.h"
}
#undef yn
#include "sdl2font.h"

static HMODULE d2d1_handle;
static ID2D1Factory *d2d1_factory = nullptr;
static HMODULE dwrite_handle;
static IDWriteFactory *write_factory = nullptr;
static bool loaded_dwrite = false;
static void load_dwrite(void);
static bool have_dwrite(void);

namespace {

template <typename T>
class RefWrapper {
public:
    RefWrapper(void) {
        ptr = nullptr;
    }
    RefWrapper(T *p) {
        ptr = p;
    }
    ~RefWrapper(void) {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }

    T *ptr;
};

template <typename Handle>
class HandleWrapper {
public:
    HandleWrapper(void) {
        h = nullptr;
    }
    HandleWrapper(Handle bmp) {
        h = bmp;
    }
    ~HandleWrapper(void) {
        ::DeleteObject(h);
    }

    Handle h;
};

template <>
class HandleWrapper<HDC> {
public:
    HandleWrapper(void) {
        h = nullptr;
    }
    HandleWrapper(HDC dc) {
        h = dc;
    }
    ~HandleWrapper(void) {
        ::DeleteDC(h);
    }

    HDC h;
};

struct RuntimeError {
    const char *file;
    int line;
    HRESULT result;

    RuntimeError(const char *file_, int line_, HRESULT result_) {
        file = file_;
        line = line_;
        result = result_;
    }
};

}

struct SDL2Font_Impl
{
    // DirectWrite mode
    RefWrapper<IDWriteTextFormat> pITextFormat;
    // GDI mode
    HandleWrapper<HFONT> hfont;
    HandleWrapper<HDC> memory_dc;
};

static wchar_t *
singleChar(wchar_t str16[3], Uint32 ch)
{
    if (ch > 0x10FFFF || (ch & 0xFFFFF800) == 0xD800) {
        ch = 0xFFFD;
    }

    if (ch < 0x10000) {
        str16[0] = static_cast<wchar_t>(ch);
        str16[1] = 0;
    } else {
        str16[0] = 0xD7C0 + (ch >> 10);
        str16[1] = 0xDC00 + (ch & 0x3FF);
        str16[2] = 0;
    }
    return str16;
}


// ISO 8859-1 to UTF-16 conversion
static std::wstring
win32String(const char *inpstr)
{
    size_t i;
    wchar_t *wstr;

    // Allocate the output string
    wstr = new wchar_t[std::strlen(inpstr) + 1U];

    // Convert to UTF-16
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        wstr[i] = static_cast<unsigned char>(inpstr[i]);
    }
    wstr[i] = 0U;

    std::wstring outstr(wstr);
    delete[] wstr;
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
    load_dwrite();

    SDL2Font *font = new SDL2Font;

    if (have_dwrite()) {
        std::wstring wname = win32String(name);

        HRESULT hr = write_factory->CreateTextFormat(
                wname.c_str(),
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                ptsize * 96.0/72.0,
                L"en-US", // TODO make this locale dependent?
                &font->pITextFormat.ptr);
        if (!SUCCEEDED(hr)) {
            ::panic("Cannot initialize font->piTextFormat: hr=%08lX",
                    static_cast<long>(hr));
        }
    } else {
        int height;

        font->memory_dc.h = ::CreateCompatibleDC(NULL);
        if (font->memory_dc.h == NULL) {
            ::panic("Cannot initialize font->memory_dc");
        }
        height = -::MulDiv(ptsize,
                           ::GetDeviceCaps(font->memory_dc.h, LOGPIXELSY),
                           72);

        font->hfont.h = ::CreateFontA(
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
        if (font->hfont.h == nullptr) {
            ::panic("Cannot initialize font->hfont");
        }

        ::SelectObject(font->memory_dc.h, font->hfont.h);
    }

    return font;
}

static void
load_dwrite(void)
{
    HRESULT (WINAPI *imp_D2D1CreateFactory)(
            _In_ D2D1_FACTORY_TYPE factoryType,
            _In_ REFIID riid,
            _In_opt_ CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
            _Out_ void **ppIFactory);
    HRESULT (WINAPI *imp_DWriteCreateFactory)(
            _In_ DWRITE_FACTORY_TYPE factoryType,
            _In_ REFIID iid,
            _COM_Outptr_ IUnknown **factory);

    if (loaded_dwrite) {
        return;
    }

    loaded_dwrite = true; // whether successful or not
    d2d1_factory = nullptr;
    write_factory = nullptr;

    do { // while (false)
        const char *gdi = std::getenv("NH_SDL2_FORCE_GDI");
        if (gdi && gdi[0]) {
            // Force DirectWrite load to fail to test GDI fallback
            break;
        }

        d2d1_handle = ::LoadLibraryA("d2d1.dll");
        if (d2d1_handle == nullptr) {
            break;
        }
        imp_D2D1CreateFactory = reinterpret_cast<
            HRESULT (WINAPI *)(
                    _In_ D2D1_FACTORY_TYPE factoryType,
                    _In_ REFIID riid,
                    _In_opt_ CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
                    _Out_ void **ppIFactory)
        >(::GetProcAddress(d2d1_handle, "D2D1CreateFactory"));
        if (imp_D2D1CreateFactory == nullptr) {
            break;
        }

        dwrite_handle = ::LoadLibraryA("dwrite.dll");
        if(dwrite_handle == nullptr) {
            break;
        }
        imp_DWriteCreateFactory = reinterpret_cast<
            HRESULT (WINAPI *)(
                _In_ DWRITE_FACTORY_TYPE factoryType,
                _In_ REFIID iid,
                _COM_Outptr_ IUnknown **factory)
        >(::GetProcAddress(dwrite_handle, "DWriteCreateFactory"));
        if (imp_DWriteCreateFactory == nullptr) {
            break;
        }

        D2D1_FACTORY_OPTIONS options = {D2D1_DEBUG_LEVEL_NONE};

        imp_D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                __uuidof(ID2D1Factory),
                &options,
                reinterpret_cast<void **>(&d2d1_factory));

        imp_DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown **>(&write_factory));
    } while (false);

    if (!have_dwrite()) {
        ::FreeLibrary(d2d1_handle);
        ::FreeLibrary(dwrite_handle);
        d2d1_handle = nullptr;
        dwrite_handle = nullptr;
    }
}

static bool
have_dwrite(void)
{
    return d2d1_factory != nullptr && write_factory != nullptr;
}

void
sdl2_font_free(SDL2Font *font)
{
    if (font == nullptr) return;

    delete font;
}

static const SDL_Color transparent = { 0, 0, 0, 0 };

// Text rendering
// If no background is given, background is transparent
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
    std::wstring text16 = win32String(text);

    surface = renderImpl(font, text16.c_str(), foreground, background);
    return surface;
}

static SDL_Surface *
renderImpl(
        SDL2Font *font,
        const wchar_t *text,
        SDL_Color foreground,
        SDL_Color background)
{
    SDL_Surface *surface = nullptr; // custodial, returned

    try {
        if (have_dwrite()) {
            // Render text, DirectWrite version
            // MinGW headers lack D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            // so use the value of 0x4 from the Microsoft headers
            static D2D1_DRAW_TEXT_OPTIONS text_opts =
                    static_cast<D2D1_DRAW_TEXT_OPTIONS>(0x4);

            HRESULT hr;
            DWRITE_TEXT_METRICS metrics;
            RefWrapper<IDWriteTextLayout> pITextLayout;
            int size_x, size_y;
            BITMAPINFO bi;
            void *bits;

            hr = write_factory->CreateTextLayout(
                    text, std::wcslen(text),
                    font->pITextFormat.ptr,
                    9999.0, 9999.0,
                    &pITextLayout.ptr);
            if (!SUCCEEDED(hr)) {
                throw RuntimeError(__FILE__, __LINE__, hr);
            }

            // We'll render to this device context
            HandleWrapper<HDC> memory_dc(::CreateCompatibleDC(nullptr));
            if (memory_dc.h == nullptr) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            // Get the size of the bitmap
            hr = pITextLayout.ptr->GetMetrics(&metrics);
            if (!SUCCEEDED(hr)) {
                throw RuntimeError(__FILE__, __LINE__, hr);
            }
            size_x = std::ceil(metrics.widthIncludingTrailingWhitespace);
            size_y = std::ceil(metrics.height);
            // Bitmap creation fails if the size is zero
            if (size_x == 0) { size_x = 1; }
            if (size_y == 0) { size_y = 1; }

            // We'll use this bitmap
            std::memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(bi);
            bi.bmiHeader.biWidth = size_x;
            bi.bmiHeader.biHeight = size_y;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            HandleWrapper<HBITMAP> hbitmap(::CreateDIBSection(
                    memory_dc.h, &bi, 0, &bits, nullptr, 0));
            if (hbitmap.h == nullptr) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            if (!::SelectObject(memory_dc.h,
                                reinterpret_cast<HGDIOBJ>(hbitmap.h))) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            while (true) {
                D2D1_RENDER_TARGET_PROPERTIES properties;
                RECT wrect;
                D2D1_RECT_F drect;
                D2D1_POINT_2F origin;
                D2D1_TAG tag1, tag2;
                D3DCOLORVALUE color;

                RefWrapper<ID2D1DCRenderTarget> dcRenderTarget;
                RefWrapper<ID2D1SolidColorBrush> fgBrush;
                RefWrapper<ID2D1SolidColorBrush> bgBrush;

                properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
                properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
                properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
                properties.dpiX = 0.0;
                properties.dpiY = 0.0;
                properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
                properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
                hr = d2d1_factory->CreateDCRenderTarget(
                        &properties, &dcRenderTarget.ptr);
                if (!SUCCEEDED(hr)) {
                    throw RuntimeError(__FILE__, __LINE__, hr);
                }

                wrect.left = 0;
                wrect.top = 0;
                wrect.right = size_x * 2;
                wrect.bottom = size_y * 2;
                hr = dcRenderTarget.ptr->BindDC(memory_dc.h, &wrect);
                if (!SUCCEEDED(hr)) {
                    throw RuntimeError(__FILE__, __LINE__, hr);
                }

                // Use these colors
                color.r = foreground.r / 255.0F;
                color.g = foreground.g / 255.0F;
                color.b = foreground.b / 255.0F;
                color.a = foreground.a / 255.0F;
                hr = dcRenderTarget.ptr->CreateSolidColorBrush(
                        color, &fgBrush.ptr);
                if (!SUCCEEDED(hr)) {
                    throw RuntimeError(__FILE__, __LINE__, hr);
                }
                color.r = background.r / 255.0F;
                color.g = background.g / 255.0F;
                color.b = background.b / 255.0F;
                color.a = background.a / 255.0F;
                hr = dcRenderTarget.ptr->CreateSolidColorBrush(
                        color, &bgBrush.ptr);
                if (!SUCCEEDED(hr)) {
                    throw RuntimeError(__FILE__, __LINE__, hr);
                }

                // Render the text
                drect.left = 0.0;
                drect.top = 0.0;
                drect.right = wrect.right;
                drect.bottom = wrect.bottom;
                origin.x = 0.0;
                origin.y = 0.0;
                dcRenderTarget.ptr->BeginDraw();
                dcRenderTarget.ptr->FillRectangle(drect, bgBrush.ptr);
                dcRenderTarget.ptr->DrawTextLayout(
                        origin, pITextLayout.ptr, fgBrush.ptr, text_opts);
                hr = dcRenderTarget.ptr->EndDraw(&tag1, &tag2);
                if (SUCCEEDED(hr)) {
                    break;
                } else if (text_opts != D2D1_DRAW_TEXT_OPTIONS_NONE) {
                    // Try again without enabling color fonts
                    text_opts = D2D1_DRAW_TEXT_OPTIONS_NONE;
                } else {
                    throw RuntimeError(__FILE__, __LINE__, hr);
                }
            }

            // Get the bitmap
            std::memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(bi);
            bi.bmiHeader.biWidth = size_x;
            bi.bmiHeader.biHeight = size_y;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            surface = ::SDL_CreateRGBSurface(
                    SDL_SWSURFACE,
                    size_x, size_y, 32,
                    0x000000FF,  // red
                    0x0000FF00,  // green
                    0x00FF0000,  // blue
                    0xFF000000); // alpha
            if (surface == nullptr) {
                throw RuntimeError(__FILE__, __LINE__, 0);
            }
            if (!::GetDIBits(memory_dc.h, hbitmap.h, 0, size_y,
                             surface->pixels, &bi, DIB_RGB_COLORS)) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            for (int y = 0; y < size_y; ++y) {
                Uint32 *row1 = reinterpret_cast<Uint32 *>(bits) + size_x*y;
                Uint32 *row2 = reinterpret_cast<Uint32 *>(surface->pixels)
                        + size_x*(size_y-1-y);
                for (int x = 0; x < size_x; ++x) {
                    unsigned char b = static_cast<unsigned char>(row1[x] >>  0);
                    unsigned char g = static_cast<unsigned char>(row1[x] >>  8);
                    unsigned char r = static_cast<unsigned char>(row1[x] >> 16);
                    unsigned char a = static_cast<unsigned char>(row1[x] >> 24);
                    // DirectWrite returns RGB prescaled by alpha, but SDL2
                    // needs it without the prescaling
                    if (a != 0) {
                        r = r * 255 / a;
                        g = g * 255 / a;
                        b = b * 255 / a;
                    }
                    row2[x] = (static_cast<Uint32>(r <<  0))
                            | (static_cast<Uint32>(g <<  8))
                            | (static_cast<Uint32>(b << 16))
                            | (static_cast<Uint32>(a << 24));
                }
            }
        } else {
            // Render text, GDI version
            SIZE size;
            BITMAPINFO bi;
            void *bits;

            /* We'll render to this device context */
            HandleWrapper<HDC> memory_dc(::CreateCompatibleDC(NULL));
            if (memory_dc.h == NULL) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            if (!::SelectObject(memory_dc.h,
                                reinterpret_cast<HGDIOBJ>(font->hfont.h))) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            /* We'll do alpha blending ourselves by using these colors */
            if (::SetTextColor(memory_dc.h, RGB(255, 255, 255)) == CLR_INVALID) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            if (::SetBkColor(memory_dc.h, RGB(0, 0, 0)) == CLR_INVALID) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            if (!::SetBkMode(memory_dc.h, OPAQUE)) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            /* Get the size of the bitmap */
            if (!::GetTextExtentPoint32W(memory_dc.h, text, std::wcslen(text),
                                         &size)) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            /* Bitmap creation fails if the size is zero */
            if (size.cx == 0) { size.cx = 1; }
            if (size.cy == 0) { size.cy = 1; }

            /* We'll use this bitmap */
            std::memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(bi);
            bi.bmiHeader.biWidth = size.cx;
            bi.bmiHeader.biHeight = size.cy;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            HandleWrapper<HBITMAP> hbitmap(
                    ::CreateDIBSection(memory_dc.h, &bi, 0, &bits, NULL, 0));
            if (hbitmap.h == NULL) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            if (!::SelectObject(memory_dc.h,
                                reinterpret_cast<HGDIOBJ>(hbitmap.h))) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            /* Render the text */
            if (!::TextOutW(memory_dc.h, 0, 0, text, std::wcslen(text))) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }

            /* Get the bitmap */
            std::memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(bi);
            bi.bmiHeader.biWidth = size.cx;
            bi.bmiHeader.biHeight = size.cy;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            surface = ::SDL_CreateRGBSurface(
                    SDL_SWSURFACE,
                    size.cx, size.cy, 32,
                    0x000000FF,  /* red */
                    0x0000FF00,  /* green */
                    0x00FF0000,  /* blue */
                    0xFF000000); /* alpha */
            if (surface == NULL) {
                throw RuntimeError(__FILE__, __LINE__, 0);
            }
            if (!::GetDIBits(memory_dc.h, hbitmap.h, 0, size.cy,
                             surface->pixels, &bi, DIB_RGB_COLORS)) {
                throw RuntimeError(__FILE__, __LINE__, ::GetLastError());
            }
            for (int y = 0; y < size.cy; ++y)
            {
                Uint32 *row1 = reinterpret_cast<Uint32 *>(bits) + size.cx*y;
                Uint32 *row2 = reinterpret_cast<Uint32 *>(surface->pixels)
                        + size.cx*(size.cy-1-y);
                for (int x = 0; x < size.cx; ++x)
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
                    row2[x] = (static_cast<Uint32>((r) <<  0))
                            | (static_cast<Uint32>((g) <<  8))
                            | (static_cast<Uint32>((b) << 16))
                            | (static_cast<Uint32>((a) << 24));
                }
            }
        }
    }
    catch (RuntimeError e) {
        ::panic("%s(%d): hr = %08lX\n",
                e.file, e.line, static_cast<unsigned long>(e.result));
    }

    return surface;
}

// Text extent
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
    std::wstring text16 = win32String(text);

    rect = textSizeImpl(font, text16.c_str());
    return rect;
}

static SDL_Rect
textSizeImpl(SDL2Font *font, const wchar_t *text)
{
    SDL_Rect rect;

    try {
        if (have_dwrite()) {
            // Get rendered text size, DirectWrite version
            RefWrapper<IDWriteTextLayout> pITextLayout;
            DWRITE_TEXT_METRICS metrics;
            HRESULT hr;

            // Get the size of the text
            hr = write_factory->CreateTextLayout(
                    text, std::wcslen(text),
                    font->pITextFormat.ptr,
                    9999.0, 9999.0,
                    &pITextLayout.ptr);
            if (!SUCCEEDED(hr)) {
                throw RuntimeError(__FILE__, __LINE__, hr);
            }

            // Get the size of the bitmap
            hr = pITextLayout.ptr->GetMetrics(&metrics);
            if (!SUCCEEDED(hr)) {
                throw RuntimeError(__FILE__, __LINE__, hr);
            }
            rect.x = 0;
            rect.y = 0;
            rect.w = std::ceil(metrics.widthIncludingTrailingWhitespace);
            rect.h = std::ceil(metrics.height);
        } else {
            // Get rendered text size, GDI version
            SIZE size;

            /* Get the size of the text */
            ::GetTextExtentPoint32W(font->memory_dc.h, text, std::wcslen(text),
                                    &size);

            rect.x = 0;
            rect.y = 0;
            rect.w = size.cx;
            rect.h = size.cy;
        }
    }
    catch (RuntimeError e) {
        ::panic("%s(%d): hr = %08lX\n",
                e.file, e.line, static_cast<unsigned long>(e.result));
    }

    return rect;
}

// Default font names
const char sdl2_font_defaultMonoFont[] = "Courier New";
const char sdl2_font_defaultSerifFont[] = "Times New Roman";
const char sdl2_font_defaultSansFont[] = "Arial";
