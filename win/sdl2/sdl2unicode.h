/* sdl2unicode.h */

#ifndef SDL2UNICODE_H
#define SDL2UNICODE_H

void sdl2_uni_convert_char(char ch[5], Uint32 glyph);
Uint32 *sdl2_uni_8to32(const char *inpstr);
size_t sdl2_uni_length32(const Uint32 *str);

#endif
