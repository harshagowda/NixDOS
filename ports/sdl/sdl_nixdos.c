/* NixDOS 2 - SDL2-compatible layer: video, input, timing.
 *
 * The display is VGA mode 13h (320x200, 256 colours). Programs render into
 * 8-bit surfaces; SDL_UpdateTexture() copies the frame to video memory and
 * NixSDL_SetDisplayPalette() programs the VGA DAC.
 */
#include "SDL.h"
#include "SDL_mixer.h"
#include <stdio.h>
#include <nixdos.h>

/* ---- init / errors --------------------------------------------------------------- */
static char sdl_error[128] = "";

static void set_error(const char *msg)
{
    strncpy(sdl_error, msg, sizeof(sdl_error) - 1);
}

const char *SDL_GetError(void) { return sdl_error; }
SDL_bool SDL_SetHint(const char *name, const char *value) { (void)name; (void)value; return SDL_TRUE; }

static int video_on;

int SDL_Init(Uint32 flags)
{
    (void)flags;
    nx_kbdraw(1);
    return 0;
}

void SDL_Quit(void)
{
    Mix_CloseAudio();
    nx_kbdraw(0);
    if (video_on) {
        nx_vgamode(3);
        video_on = 0;
    }
}

/* ---- video ------------------------------------------------------------------------- */
struct SDL_Window { int w, h; };
struct SDL_Renderer { int dummy; };
struct SDL_Texture { int w, h; };

static struct SDL_Window the_window;
static struct SDL_Renderer the_renderer;
static struct SDL_Texture the_texture;

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    (void)title; (void)x; (void)y; (void)flags;
    if (w != 320 || h != 200) {
        set_error("NixDOS only supports a 320x200 display");
        return NULL;
    }
    the_window.w = w;
    the_window.h = h;
    nx_vgamode(0x13);
    video_on = 1;
    return &the_window;
}

void SDL_DestroyWindow(SDL_Window *w) { (void)w; }
Uint32 SDL_GetWindowPixelFormat(SDL_Window *w) { (void)w; return SDL_PIXELFORMAT_INDEX8; }
Uint32 SDL_GetWindowID(SDL_Window *w) { (void)w; return 1; }
void SDL_GetWindowSize(SDL_Window *w, int *wi, int *he) { if (wi) *wi = w->w; if (he) *he = w->h; }
void SDL_SetWindowSize(SDL_Window *w, int wi, int he) { (void)w; (void)wi; (void)he; }
void SDL_SetWindowMinimumSize(SDL_Window *w, int wi, int he) { (void)w; (void)wi; (void)he; }
int SDL_SetWindowFullscreen(SDL_Window *w, Uint32 flags) { (void)w; (void)flags; return 0; }

SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int index, Uint32 flags)
{
    (void)w; (void)index; (void)flags;
    return &the_renderer;
}

int SDL_RenderSetLogicalSize(SDL_Renderer *r, int w, int h) { (void)r; (void)w; (void)h; return 0; }
int SDL_RenderClear(SDL_Renderer *r) { (void)r; return 0; }
int SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *s, const SDL_Rect *d) { (void)r; (void)t; (void)s; (void)d; return 0; }
void SDL_RenderPresent(SDL_Renderer *r) { (void)r; NixAudio_Pump(); }

SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 format, int access, int w, int h)
{
    (void)r; (void)format; (void)access;
    the_texture.w = w;
    the_texture.h = h;
    return &the_texture;
}

/* Present a frame: the texture is the VGA screen. */
int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch)
{
    (void)rect;
    if (!video_on) return 0;
    if (pitch == 320 && t->w == 320 && t->h == 200) {
        nx_vgablit((const unsigned char *)pixels);
    } else {
        unsigned char *vga = (unsigned char *)0xA0000;
        const unsigned char *src = pixels;
        int h = t->h < 200 ? t->h : 200, w = t->w < 320 ? t->w : 320;
        for (int y = 0; y < h; y++) memcpy(vga + y * 320, src + y * pitch, (size_t)w);
    }
    return 0;
}

/* Called by the (patched) game whenever its palette changes. */
void NixSDL_SetDisplayPalette(const SDL_Color *colors)
{
    unsigned char rgb[768];
    for (int i = 0; i < 256; i++) {
        rgb[i * 3] = colors[i].r;
        rgb[i * 3 + 1] = colors[i].g;
        rgb[i * 3 + 2] = colors[i].b;
    }
    nx_vgapalette(rgb, 0, 256);
}

SDL_bool SDL_PixelFormatEnumToMasks(Uint32 format, int *bpp, Uint32 *r, Uint32 *g, Uint32 *b, Uint32 *a)
{
    *bpp = format == SDL_PIXELFORMAT_INDEX8 ? 8 : 32;
    *r = *g = *b = *a = 0;
    if (*bpp == 32) { *r = 0xFF0000; *g = 0xFF00; *b = 0xFF; }
    return SDL_TRUE;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
    (void)flags; (void)r; (void)g; (void)b; (void)a;
    if (depth != 8 && depth != 32) { set_error("unsupported surface depth"); return NULL; }
    SDL_Surface *s = calloc(1, sizeof(SDL_Surface));
    if (!s) { set_error("out of memory"); return NULL; }
    s->w = w;
    s->h = h;
    s->format = &s->fmt_storage;
    s->fmt_storage.format = depth == 8 ? SDL_PIXELFORMAT_INDEX8 : 0;
    s->fmt_storage.BitsPerPixel = (Uint8)depth;
    s->fmt_storage.BytesPerPixel = (Uint8)(depth / 8);
    s->pitch = w * (depth / 8);
    s->pixels = calloc((size_t)s->pitch * (size_t)h, 1);
    if (!s->pixels) { free(s); set_error("out of memory"); return NULL; }
    if (depth == 8) {
        s->pal_storage.ncolors = 256;
        s->pal_storage.colors = s->pal_colors;
        s->fmt_storage.palette = &s->pal_storage;
    }
    return s;
}

void SDL_FreeSurface(SDL_Surface *s)
{
    if (!s) return;
    free(s->pixels);
    free(s);
}

int SDL_LockSurface(SDL_Surface *s) { (void)s; return 0; }
void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }

static int clip(SDL_Rect *r, int w, int h)
{
    if (r->x < 0) { r->w += r->x; r->x = 0; }
    if (r->y < 0) { r->h += r->y; r->y = 0; }
    if (r->x + r->w > w) r->w = w - r->x;
    if (r->y + r->h > h) r->h = h - r->y;
    return r->w > 0 && r->h > 0;
}

int SDL_FillRect(SDL_Surface *s, const SDL_Rect *rect, Uint32 color)
{
    SDL_Rect r = rect ? *rect : (SDL_Rect){ 0, 0, s->w, s->h };
    if (!clip(&r, s->w, s->h)) return 0;
    int bpp = s->format->BytesPerPixel;
    for (int y = r.y; y < r.y + r.h; y++) {
        Uint8 *row = (Uint8 *)s->pixels + y * s->pitch + r.x * bpp;
        if (bpp == 1) memset(row, (int)color, (size_t)r.w);
        else for (int x = 0; x < r.w; x++) ((Uint32 *)row)[x] = color;
    }
    return 0;
}

int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    SDL_Rect sr = srcrect ? *srcrect : (SDL_Rect){ 0, 0, src->w, src->h };
    int dx = dstrect ? dstrect->x : 0, dy = dstrect ? dstrect->y : 0;
    /* clip against the source, then the destination */
    if (sr.x < 0) { dx -= sr.x; sr.w += sr.x; sr.x = 0; }
    if (sr.y < 0) { dy -= sr.y; sr.h += sr.y; sr.y = 0; }
    if (sr.x + sr.w > src->w) sr.w = src->w - sr.x;
    if (sr.y + sr.h > src->h) sr.h = src->h - sr.y;
    if (dx < 0) { sr.x -= dx; sr.w += dx; dx = 0; }
    if (dy < 0) { sr.y -= dy; sr.h += dy; dy = 0; }
    if (dx + sr.w > dst->w) sr.w = dst->w - dx;
    if (dy + sr.h > dst->h) sr.h = dst->h - dy;
    if (sr.w <= 0 || sr.h <= 0) return 0;

    int sb = src->format->BytesPerPixel, db = dst->format->BytesPerPixel;
    for (int y = 0; y < sr.h; y++) {
        const Uint8 *s = (const Uint8 *)src->pixels + (sr.y + y) * src->pitch + sr.x * sb;
        Uint8 *d = (Uint8 *)dst->pixels + (dy + y) * dst->pitch + dx * db;
        if (sb == db) {
            memcpy(d, s, (size_t)(sr.w * sb));
        } else if (sb == 1 && db == 4) {
            SDL_Color *pal = src->pal_colors;
            for (int x = 0; x < sr.w; x++) {
                SDL_Color c = pal[s[x]];
                ((Uint32 *)d)[x] = (Uint32)c.r << 16 | (Uint32)c.g << 8 | c.b;
            }
        }
    }
    if (dstrect) { dstrect->w = sr.w; dstrect->h = sr.h; }
    return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b)
{
    if (f->BytesPerPixel == 4) return (Uint32)r << 16 | (Uint32)g << 8 | b;
    /* nearest palette entry */
    Uint32 best = 0, bestd = 0xFFFFFFFF;
    for (int i = 0; f->palette && i < f->palette->ncolors; i++) {
        SDL_Color c = f->palette->colors[i];
        int dr = c.r - r, dg = c.g - g, db = c.b - b;
        Uint32 d = (Uint32)(dr * dr + dg * dg + db * db);
        if (d < bestd) { bestd = d; best = (Uint32)i; }
    }
    return best;
}

SDL_Palette *SDL_AllocPalette(int n)
{
    SDL_Palette *p = calloc(1, sizeof(SDL_Palette));
    if (!p) return NULL;
    p->ncolors = n;
    p->colors = calloc((size_t)n, sizeof(SDL_Color));
    if (!p->colors) { free(p); return NULL; }
    return p;
}

void SDL_FreePalette(SDL_Palette *p)
{
    if (!p) return;
    free(p->colors);
    free(p);
}

int SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *c, int first, int n)
{
    if (first < 0 || first + n > p->ncolors) return -1;
    memcpy(p->colors + first, c, (size_t)n * sizeof(SDL_Color));
    return 0;
}

int SDL_SetSurfacePalette(SDL_Surface *s, SDL_Palette *p)
{
    if (!s->format->palette || !p) return -1;
    int n = p->ncolors < 256 ? p->ncolors : 256;
    memcpy(s->pal_colors, p->colors, (size_t)n * sizeof(SDL_Color));
    return 0;
}

int SDL_SaveBMP(SDL_Surface *s, const char *file) { (void)s; (void)file; set_error("not supported"); return -1; }

/* ---- timing / threads -------------------------------------------------------------- */
Uint32 SDL_GetTicks(void) { return (Uint32)nx_ticks(); }

void SDL_Delay(Uint32 ms)
{
    Uint32 end = SDL_GetTicks() + ms;
    NixAudio_Pump();
    while ((Sint32)(end - SDL_GetTicks()) > 0) {
        Uint32 left = end - SDL_GetTicks();
        nx_sleep(left > 5 ? 5 : (int)left);
        NixAudio_Pump();
    }
}

struct SDL_mutex { int dummy; };
static struct SDL_mutex the_mutex;
SDL_mutex *SDL_CreateMutex(void) { return &the_mutex; }
void SDL_DestroyMutex(SDL_mutex *m) { (void)m; }
/* audio is mixed synchronously (NixAudio_Pump), so no locking is needed */
int SDL_LockMutex(SDL_mutex *m) { (void)m; return 0; }
int SDL_UnlockMutex(SDL_mutex *m) { (void)m; return 0; }

/* ---- keyboard --------------------------------------------------------------------- */
struct keymap { Uint8 sc; SDL_Keycode sym; };

/* PS/2 scancode set 1 -> SDL scancode */
static const Uint8 sc_normal[0x59] = {
    [0x01] = SDL_SCANCODE_ESCAPE,
    [0x02] = 30, [0x03] = 31, [0x04] = 32, [0x05] = 33, [0x06] = 34,       /* 1..5 */
    [0x07] = 35, [0x08] = 36, [0x09] = 37, [0x0A] = 38, [0x0B] = 39,       /* 6..0 */
    [0x0C] = 45, [0x0D] = 46, [0x0E] = SDL_SCANCODE_BACKSPACE, [0x0F] = SDL_SCANCODE_TAB,
    [0x10] = 20, [0x11] = 26, [0x12] = 8, [0x13] = 21, [0x14] = 23,        /* q w e r t */
    [0x15] = 28, [0x16] = 24, [0x17] = 12, [0x18] = 18, [0x19] = 19,       /* y u i o p */
    [0x1A] = 47, [0x1B] = 48, [0x1C] = SDL_SCANCODE_RETURN, [0x1D] = SDL_SCANCODE_LCTRL,
    [0x1E] = 4, [0x1F] = 22, [0x20] = 7, [0x21] = 9, [0x22] = 10,          /* a s d f g */
    [0x23] = 11, [0x24] = 13, [0x25] = 14, [0x26] = 15,                    /* h j k l */
    [0x27] = 51, [0x28] = 52, [0x29] = 53, [0x2A] = SDL_SCANCODE_LSHIFT, [0x2B] = 49,
    [0x2C] = 29, [0x2D] = 27, [0x2E] = 6, [0x2F] = 25, [0x30] = 5,         /* z x c v b */
    [0x31] = 17, [0x32] = 16, [0x33] = 54, [0x34] = 55, [0x35] = 56,       /* n m , . / */
    [0x36] = SDL_SCANCODE_RSHIFT, [0x37] = SDL_SCANCODE_KP_MULTIPLY, [0x38] = SDL_SCANCODE_LALT,
    [0x39] = SDL_SCANCODE_SPACE, [0x3A] = SDL_SCANCODE_CAPSLOCK,
    [0x3B] = 58, [0x3C] = 59, [0x3D] = 60, [0x3E] = 61, [0x3F] = 62,       /* F1..F5 */
    [0x40] = 63, [0x41] = 64, [0x42] = 65, [0x43] = 66, [0x44] = 67,       /* F6..F10 */
    [0x45] = SDL_SCANCODE_NUMLOCKCLEAR, [0x46] = SDL_SCANCODE_SCROLLLOCK,
    [0x47] = SDL_SCANCODE_KP_7, [0x48] = SDL_SCANCODE_KP_8, [0x49] = SDL_SCANCODE_KP_9,
    [0x4A] = SDL_SCANCODE_KP_MINUS, [0x4B] = SDL_SCANCODE_KP_4, [0x4C] = SDL_SCANCODE_KP_5,
    [0x4D] = SDL_SCANCODE_KP_6, [0x4E] = SDL_SCANCODE_KP_PLUS, [0x4F] = SDL_SCANCODE_KP_1,
    [0x50] = SDL_SCANCODE_KP_2, [0x51] = SDL_SCANCODE_KP_3, [0x52] = SDL_SCANCODE_KP_0,
    [0x53] = SDL_SCANCODE_KP_PERIOD, [0x57] = 68, [0x58] = 69,              /* F11 F12 */
};

static Uint8 sc_extended(Uint8 sc)
{
    switch (sc) {
    case 0x1C: return SDL_SCANCODE_KP_ENTER;
    case 0x1D: return SDL_SCANCODE_RCTRL;
    case 0x35: return SDL_SCANCODE_KP_DIVIDE;
    case 0x38: return SDL_SCANCODE_RALT;
    case 0x47: return SDL_SCANCODE_HOME;
    case 0x48: return SDL_SCANCODE_UP;
    case 0x49: return SDL_SCANCODE_PAGEUP;
    case 0x4B: return SDL_SCANCODE_LEFT;
    case 0x4D: return SDL_SCANCODE_RIGHT;
    case 0x4F: return SDL_SCANCODE_END;
    case 0x50: return SDL_SCANCODE_DOWN;
    case 0x51: return SDL_SCANCODE_PAGEDOWN;
    case 0x52: return SDL_SCANCODE_INSERT;
    case 0x53: return SDL_SCANCODE_DELETE;
    case 0x5B: return SDL_SCANCODE_LGUI;
    case 0x5C: return SDL_SCANCODE_RGUI;
    }
    return 0;
}

static SDL_Keycode keycode_of(int sc)
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    if (sc >= 4 && sc <= 39) return chars[sc - 4];
    switch (sc) {
    case SDL_SCANCODE_RETURN: return SDLK_RETURN;
    case SDL_SCANCODE_ESCAPE: return SDLK_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return SDLK_BACKSPACE;
    case SDL_SCANCODE_TAB: return SDLK_TAB;
    case SDL_SCANCODE_SPACE: return SDLK_SPACE;
    case 45: return '-';
    case 46: return '=';
    case 47: return '[';
    case 48: return ']';
    case 49: return '\\';
    case 51: return ';';
    case 52: return '\'';
    case 53: return '`';
    case 54: return ',';
    case 55: return '.';
    case 56: return '/';
    case SDL_SCANCODE_DELETE: return SDLK_DELETE;
    }
    return SDL_SCANCODE_TO_KEYCODE(sc);
}

static Uint16 modstate;
static int pending_e0, pending_e1;

#define QSIZE 64
static SDL_Event queue[QSIZE];
static int qhead, qtail;

static void enqueue(const SDL_Event *e)
{
    int next = (qhead + 1) % QSIZE;
    if (next == qtail) return;
    queue[qhead] = *e;
    qhead = next;
}

static Uint16 mod_bit(int sc)
{
    switch (sc) {
    case SDL_SCANCODE_LSHIFT: return KMOD_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return KMOD_RSHIFT;
    case SDL_SCANCODE_LCTRL: return KMOD_LCTRL;
    case SDL_SCANCODE_RCTRL: return KMOD_RCTRL;
    case SDL_SCANCODE_LALT: return KMOD_LALT;
    case SDL_SCANCODE_RALT: return KMOD_RALT;
    }
    return 0;
}

static void translate_scancodes(void)
{
    int raw;
    while ((raw = nx_scancode()) >= 0) {
        if (raw == 0xE0) { pending_e0 = 1; continue; }
        if (raw == 0xE1) { pending_e1 = 2; continue; }     /* Pause: E1 1D 45 E1 9D C5 */
        int release = raw & 0x80, code = raw & 0x7F, sc;
        if (pending_e1) {
            if (--pending_e1 > 0) continue;
            if (code != 0x45) continue;
            sc = SDL_SCANCODE_PAUSE;
        } else if (pending_e0) {
            pending_e0 = 0;
            if (code == 0x2A || code == 0x36) continue;     /* fake shifts */
            sc = sc_extended((Uint8)code);
        } else {
            sc = code < (int)sizeof(sc_normal) ? sc_normal[code] : 0;
        }
        if (!sc) continue;

        Uint16 bit = mod_bit(sc);
        if (bit) {
            if (release) modstate &= (Uint16)~bit;
            else modstate |= bit;
        }
        if (!release && sc == SDL_SCANCODE_CAPSLOCK) modstate ^= KMOD_CAPS;
        if (!release && sc == SDL_SCANCODE_NUMLOCKCLEAR) modstate ^= KMOD_NUM;

        SDL_Event e;
        memset(&e, 0, sizeof(e));
        e.type = release ? SDL_KEYUP : SDL_KEYDOWN;
        e.key.state = release ? 0 : 1;
        e.key.windowID = 1;
        e.key.keysym.scancode = (SDL_Scancode)sc;
        e.key.keysym.sym = keycode_of(sc);
        e.key.keysym.mod = modstate;
        enqueue(&e);
    }
}

SDL_Keymod SDL_GetModState(void) { return (SDL_Keymod)modstate; }

int SDL_PushEvent(SDL_Event *e) { enqueue(e); return 1; }

int SDL_PollEvent(SDL_Event *e)
{
    NixAudio_Pump();
    translate_scancodes();
    if (qhead == qtail) return 0;
    if (e) {
        *e = queue[qtail];
        qtail = (qtail + 1) % QSIZE;
    }
    return 1;
}

int SDL_WaitEvent(SDL_Event *e)
{
    while (!SDL_PollEvent(e)) nx_sleep(2);
    return 1;
}

/* ---- mouse and joystick: none ------------------------------------------------------ */
Uint32 SDL_GetRelativeMouseState(int *x, int *y) { if (x) *x = 0; if (y) *y = 0; return 0; }
int SDL_SetRelativeMouseMode(SDL_bool on) { (void)on; return 0; }
void SDL_WarpMouseInWindow(SDL_Window *w, int x, int y) { (void)w; (void)x; (void)y; }
int SDL_NumJoysticks(void) { return 0; }
SDL_Joystick *SDL_JoystickOpen(int i) { (void)i; return NULL; }
void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
int SDL_JoystickNumButtons(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickNumHats(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickEventState(int s) { (void)s; return 0; }
void SDL_JoystickUpdate(void) {}
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int a) { (void)j; (void)a; return 0; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int b) { (void)j; (void)b; return 0; }
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int h) { (void)j; (void)h; return 0; }
