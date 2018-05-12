/* sdl2.h -- to include the SDL headers, accounting for different
   install locations */

#ifndef WIN_SDL2_SDL2_H
#define WIN_SDL2_SDL2_H

#ifdef __APPLE__
#include <SDL.h>
#elif defined(_WIN32)
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#endif
