/* NixDOS 2 - a tiny SDL2-compatible layer (only what Wolf4SDL uses).
 * Video: 320x200 8-bit surfaces shown in VGA mode 13h.
 * Input: PS/2 scancodes translated to SDL keycodes.
 * Audio: see SDL_mixer.h (Sound Blaster 16). */
#ifndef NIXDOS_SDL_H
#define NIXDOS_SDL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_INIT_TIMER    0x01
#define SDL_INIT_AUDIO    0x10
#define SDL_INIT_VIDEO    0x20
#define SDL_INIT_JOYSTICK 0x200
#define SDL_ENABLE  1
#define SDL_DISABLE 0
#define SDL_QUERY  -1

/* ---- video ---------------------------------------------------------------- */
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;
typedef struct SDL_Palette { int ncolors; SDL_Color *colors; } SDL_Palette;
typedef struct SDL_PixelFormat {
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel, BytesPerPixel;
} SDL_PixelFormat;
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h, pitch;
    void *pixels;
    SDL_PixelFormat fmt_storage;
    SDL_Color pal_colors[256];
    SDL_Palette pal_storage;
} SDL_Surface;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

#define SDL_PIXELFORMAT_INDEX8 0x13000801u
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000
#define SDL_WINDOW_FULLSCREEN 0x1
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x1001
#define SDL_WINDOW_RESIZABLE 0x20
#define SDL_WINDOW_ALLOW_HIGHDPI 0x2000
#define SDL_RENDERER_SOFTWARE 0x1
#define SDL_RENDERER_PRESENTVSYNC 0x4
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_MUSTLOCK(s) 0

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);
SDL_bool SDL_SetHint(const char *name, const char *value);

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags);
void SDL_DestroyWindow(SDL_Window *w);
Uint32 SDL_GetWindowPixelFormat(SDL_Window *w);
Uint32 SDL_GetWindowID(SDL_Window *w);
void SDL_GetWindowSize(SDL_Window *w, int *wi, int *he);
void SDL_SetWindowSize(SDL_Window *w, int wi, int he);
void SDL_SetWindowMinimumSize(SDL_Window *w, int wi, int he);
int SDL_SetWindowFullscreen(SDL_Window *w, Uint32 flags);
SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int index, Uint32 flags);
int SDL_RenderSetLogicalSize(SDL_Renderer *r, int w, int h);
int SDL_RenderClear(SDL_Renderer *r);
int SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *src, const SDL_Rect *dst);
void SDL_RenderPresent(SDL_Renderer *r);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 format, int access, int w, int h);
int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch);
SDL_bool SDL_PixelFormatEnumToMasks(Uint32 format, int *bpp, Uint32 *r, Uint32 *g, Uint32 *b, Uint32 *a);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 r, Uint32 g, Uint32 b, Uint32 a);
void SDL_FreeSurface(SDL_Surface *s);
int SDL_LockSurface(SDL_Surface *s);
void SDL_UnlockSurface(SDL_Surface *s);
int SDL_FillRect(SDL_Surface *s, const SDL_Rect *rect, Uint32 color);
int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b);
SDL_Palette *SDL_AllocPalette(int ncolors);
void SDL_FreePalette(SDL_Palette *p);
int SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *colors, int first, int n);
int SDL_SetSurfacePalette(SDL_Surface *s, SDL_Palette *p);
int SDL_SaveBMP(SDL_Surface *s, const char *file);

/* ---- timing / threads ------------------------------------------------------- */
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);
typedef struct SDL_mutex SDL_mutex;
SDL_mutex *SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_mutex *m);
int SDL_LockMutex(SDL_mutex *m);
int SDL_UnlockMutex(SDL_mutex *m);

/* ---- keyboard --------------------------------------------------------------- */
typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43, SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_CAPSLOCK = 57, SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_PRINTSCREEN = 70, SDL_SCANCODE_SCROLLLOCK = 71, SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_NUMLOCKCLEAR = 83, SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86, SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89, SDL_SCANCODE_KP_2 = 90, SDL_SCANCODE_KP_3 = 91, SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93, SDL_SCANCODE_KP_6 = 94, SDL_SCANCODE_KP_7 = 95, SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97, SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_PERIOD = 99,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226, SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230, SDL_SCANCODE_RGUI = 231,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;
#define SDL_SCANCODE_TO_KEYCODE(x) ((x) | (1 << 30))
enum {
    SDLK_UNKNOWN = 0, SDLK_RETURN = '\r', SDLK_ESCAPE = 27, SDLK_BACKSPACE = 8, SDLK_TAB = '\t',
    SDLK_SPACE = ' ', SDLK_EXCLAIM = '!', SDLK_QUOTEDBL = '"', SDLK_HASH = '#', SDLK_PERCENT = '%',
    SDLK_DOLLAR = '$', SDLK_AMPERSAND = '&', SDLK_QUOTE = '\'', SDLK_LEFTPAREN = '(',
    SDLK_RIGHTPAREN = ')', SDLK_ASTERISK = '*', SDLK_PLUS = '+', SDLK_COMMA = ',', SDLK_MINUS = '-',
    SDLK_PERIOD = '.', SDLK_SLASH = '/',
    SDLK_0 = '0', SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_COLON = ':', SDLK_SEMICOLON = ';', SDLK_LESS = '<', SDLK_EQUALS = '=', SDLK_GREATER = '>',
    SDLK_QUESTION = '?', SDLK_AT = '@', SDLK_LEFTBRACKET = '[', SDLK_BACKSLASH = '\\',
    SDLK_RIGHTBRACKET = ']', SDLK_CARET = '^', SDLK_UNDERSCORE = '_', SDLK_BACKQUOTE = '`',
    SDLK_a = 'a', SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j, SDLK_k,
    SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, SDLK_u, SDLK_v, SDLK_w,
    SDLK_x, SDLK_y, SDLK_z,
    SDLK_DELETE = 127,
    SDLK_CAPSLOCK = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CAPSLOCK),
    SDLK_F1 = SDL_SCANCODE_TO_KEYCODE(58), SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7,
    SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12,
    SDLK_PRINTSCREEN = SDL_SCANCODE_TO_KEYCODE(70), SDLK_SCROLLLOCK, SDLK_PAUSE, SDLK_INSERT,
    SDLK_HOME, SDLK_PAGEUP, SDLK_DELETE_ = SDL_SCANCODE_TO_KEYCODE(76), SDLK_END, SDLK_PAGEDOWN,
    SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP, SDLK_NUMLOCKCLEAR, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY,
    SDLK_KP_MINUS, SDLK_KP_PLUS, SDLK_KP_ENTER, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4,
    SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9, SDLK_KP_0, SDLK_KP_PERIOD,
    SDLK_LCTRL = SDL_SCANCODE_TO_KEYCODE(224), SDLK_LSHIFT, SDLK_LALT, SDLK_LGUI,
    SDLK_RCTRL, SDLK_RSHIFT, SDLK_RALT, SDLK_RGUI,
    SDLK_F13 = SDL_SCANCODE_TO_KEYCODE(104), SDLK_F14, SDLK_F15, SDLK_F16, SDLK_F17, SDLK_F18,
    SDLK_F19, SDLK_F20, SDLK_F21, SDLK_F22, SDLK_F23, SDLK_F24
};
/* SDL 1.2 names that Wolf4SDL still uses */
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#define SDLK_PRINT SDLK_PRINTSCREEN

typedef enum {
    KMOD_NONE = 0, KMOD_LSHIFT = 0x1, KMOD_RSHIFT = 0x2, KMOD_LCTRL = 0x40, KMOD_RCTRL = 0x80,
    KMOD_LALT = 0x100, KMOD_RALT = 0x200, KMOD_LGUI = 0x400, KMOD_RGUI = 0x800,
    KMOD_NUM = 0x1000, KMOD_CAPS = 0x2000,
    KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL, KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT,
    KMOD_ALT = KMOD_LALT | KMOD_RALT, KMOD_GUI = KMOD_LGUI | KMOD_RGUI
} SDL_Keymod;

typedef struct SDL_Keysym { SDL_Scancode scancode; SDL_Keycode sym; Uint16 mod; Uint32 unused; } SDL_Keysym;
SDL_Keymod SDL_GetModState(void);

/* ---- events ------------------------------------------------------------------ */
typedef enum {
    SDL_QUIT = 0x100, SDL_WINDOWEVENT = 0x200, SDL_KEYDOWN = 0x300, SDL_KEYUP,
    SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL
} SDL_EventType;
enum {
    SDL_WINDOWEVENT_NONE, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED,
    SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED,
    SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE, SDL_WINDOWEVENT_FOCUS_GAINED,
    SDL_WINDOWEVENT_FOCUS_LOST
};
typedef struct SDL_KeyboardEvent { Uint32 type, timestamp, windowID; Uint8 state, repeat; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct SDL_WindowEvent { Uint32 type, timestamp, windowID; Uint8 event; Sint32 data1, data2; } SDL_WindowEvent;
typedef struct SDL_MouseButtonEvent { Uint32 type, timestamp, windowID, which; Uint8 button, state, clicks; Sint32 x, y; } SDL_MouseButtonEvent;
typedef struct SDL_MouseWheelEvent { Uint32 type, timestamp, windowID, which; Sint32 x, y; } SDL_MouseWheelEvent;
typedef union SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
    SDL_WindowEvent window;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    Uint8 padding[56];
} SDL_Event;
int SDL_PollEvent(SDL_Event *e);
int SDL_WaitEvent(SDL_Event *e);
int SDL_PushEvent(SDL_Event *e);

/* ---- mouse / joystick (not supported: always idle) ---------------------------- */
#define SDL_BUTTON(x) (1 << ((x) - 1))
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
Uint32 SDL_GetRelativeMouseState(int *x, int *y);
int SDL_SetRelativeMouseMode(SDL_bool on);
void SDL_WarpMouseInWindow(SDL_Window *w, int x, int y);
typedef struct SDL_Joystick SDL_Joystick;
#define SDL_HAT_CENTERED 0
#define SDL_HAT_UP 1
#define SDL_HAT_RIGHT 2
#define SDL_HAT_DOWN 4
#define SDL_HAT_LEFT 8
int SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int i);
void SDL_JoystickClose(SDL_Joystick *j);
int SDL_JoystickNumButtons(SDL_Joystick *j);
int SDL_JoystickNumHats(SDL_Joystick *j);
int SDL_JoystickEventState(int state);
void SDL_JoystickUpdate(void);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int axis);
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int b);
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int h);

/* ---- audio ------------------------------------------------------------------- */
#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_S16LSB 0x8010
#define AUDIO_S16SYS AUDIO_S16LSB
#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x1
typedef Uint16 SDL_AudioFormat;
typedef struct SDL_AudioCVT {
    int needed;
    SDL_AudioFormat src_format, dst_format;
    double rate_incr;
    Uint8 *buf;
    int len, len_cvt, len_mult;
    double len_ratio;
    /* private */
    int src_channels, dst_channels, src_rate, dst_rate;
} SDL_AudioCVT;
int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat sf, Uint8 sc, int sr,
                      SDL_AudioFormat df, Uint8 dc, int dr);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);

#define SDL_VERSION(x) ((void)(x))

#ifdef __cplusplus
}
#endif
#endif
