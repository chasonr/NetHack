/* sdl2unicode.c */

#include "hack.h"
#undef yn
#include "sdl2.h"
#include "sdl2unicode.h"

void
sdl2_uni_convert_char(char ch[5], Uint32 ch32)
{
    if (ch32 < 0x80) {
        ch[0] = (char) ch32;
        ch[1] = 0;
    } else if (ch32 < 0x800) {
        ch[0] = (char) (0xC0 | (ch32 >> 6));
        ch[1] = (char) (0x80 | (ch32 & 0x3F));
    } else if (ch32 < 0x10000) {
        ch[0] = (char) (0xE0 | (ch32 >> 12));
        ch[1] = (char) (0x80 | ((ch32 >> 6) & 0x3F));
        ch[2] = (char) (0x80 | (ch32 & 0x3F));
        ch[3] = 0;
    } else {
        ch[0] = (char) (0xE0 | (ch32 >> 18));
        ch[1] = (char) (0x80 | ((ch32 >> 12) & 0x3F));
        ch[2] = (char) (0x80 | ((ch32 >> 6) & 0x3F));
        ch[3] = (char) (0x80 | (ch32 & 0x3F));
        ch[4] = 0;
    }
}

Uint32 *
sdl2_uni_8to32(const char *inpstr)
{
    size_t outsize;
    size_t i, j;
    Uint32 *outstr;

    /* Determine the output size */
    outsize = 1U;
    for (i = 0U; inpstr[i] != '\0'; ++i) {
        unsigned char byte = (unsigned char) inpstr[i];
        if (byte < 0x80 || 0xBF < byte) {
            ++outsize;
        }
    }

    /* Allocate the output string */
    outstr = (Uint32 *) alloc(outsize * sizeof(Uint32));

    /* Convert to UTF-32 */
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
        outstr[j++] = ch32;
    }
    outstr[j] = 0U;

    return outstr;
}

size_t
sdl2_uni_length32(const Uint32 *str)
{
    size_t len;

    for (len = 0U; str[len] != 0; ++len) {}
    return len;
}
