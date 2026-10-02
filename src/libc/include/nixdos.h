/* NixDOS 2 - native program interface to the kernel.
 * The kernel stores a pointer to its function table at 0x500; the order of
 * the table comes from src/kernel/api.h, so it never goes out of sync. */
#ifndef _NIXDOS_H
#define _NIXDOS_H

#include "../../kernel/api.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
#define API(name, fn, nargs, ret) NX_##name,
    API_LIST
#undef API
    NX_API_COUNT
};

#define NX_FN(name) ((*(void ***)0x500)[NX_##name])
#define NX_CALL(name, type) ((type)NX_FN(name))

/* convenience wrappers */
static inline int  nx_vgamode(int m)                 { return NX_CALL(vgamode, int (*)(int))(m); }
static inline void nx_vgapalette(const unsigned char *rgb, int first, int n)
                                                     { NX_CALL(vgapalette, void (*)(const unsigned char *, int, int))(rgb, first, n); }
static inline void nx_vgablit(const unsigned char *f){ NX_CALL(vgablit, void (*)(const unsigned char *))(f); }
static inline void nx_vgawait(void)                  { NX_CALL(vgawait, void (*)(void))(); }
static inline void nx_kbdraw(int on)                 { NX_CALL(kbdraw, void (*)(int))(on); }
static inline int  nx_scancode(void)                 { return NX_CALL(scancode, int (*)(void))(); }
static inline int  nx_ticks(void)                    { return NX_CALL(ticks, int (*)(void))(); }
static inline void nx_sleep(int ms)                  { NX_CALL(sleep, void (*)(int))(ms); }
static inline int  nx_getkey(void)                   { return NX_CALL(getkey, int (*)(void))(); }
static inline int  nx_kbhit(void)                    { return NX_CALL(kbhit, int (*)(void))(); }
static inline int  nx_filesize(const char *n)        { return NX_CALL(filesize, int (*)(const char *))(n); }

#ifdef __cplusplus
}
#endif
#endif
