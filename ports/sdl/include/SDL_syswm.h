/* NixDOS: no window manager */
#ifndef NIXDOS_SDL_SYSWM_H
#define NIXDOS_SDL_SYSWM_H
#include "SDL.h"
struct SDL_SysWMinfo { int version; };
static inline int SDL_GetWindowWMInfo(SDL_Window *w, struct SDL_SysWMinfo *i) { (void)w; (void)i; return -1; }
#endif
