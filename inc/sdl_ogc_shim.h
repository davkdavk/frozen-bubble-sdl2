#ifndef SDL_OGC_SHIM_H
#define SDL_OGC_SHIM_H

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;

#ifdef __cplusplus
extern "C" {
#endif

typedef int SDL_bool;
#define SDL_TRUE 1
#define SDL_FALSE 0

typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_B = 5,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_G = 10,
    SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14,
    SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16,
    SDL_SCANCODE_N = 17,
    SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24,
    SDL_SCANCODE_V = 25,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_EQUALS = 46,
    SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_BACKSLASH = 49,
    SDL_SCANCODE_NONUSHASH = 50,
    SDL_SCANCODE_SEMICOLON = 51,
    SDL_SCANCODE_APOSTROPHE = 52,
    SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54,
    SDL_SCANCODE_PERIOD = 55,
    SDL_SCANCODE_SLASH = 56,
    SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_F2 = 59,
    SDL_SCANCODE_F3 = 60,
    SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62,
    SDL_SCANCODE_F6 = 63,
    SDL_SCANCODE_F7 = 64,
    SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66,
    SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68,
    SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_PRINTSCREEN = 70,
    SDL_SCANCODE_SCROLLLOCK = 71,
    SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73,
    SDL_SCANCODE_HOME = 74,
    SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76,
    SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_KP_DIVIDE = 84,
    SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86,
    SDL_SCANCODE_KP_PLUS = 87,
    SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90,
    SDL_SCANCODE_KP_3 = 91,
    SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93,
    SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95,
    SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98,
    SDL_SCANCODE_KP_PERIOD = 99,
    SDL_SCANCODE_LCTRL = 224,
} SDL_Scancode;

typedef enum {
    SDLK_UNKNOWN = 0,
    SDLK_RETURN = '\r',
    SDLK_ESCAPE = '\033',
    SDLK_BACKSPACE = '\b',
    SDLK_TAB = '\t',
    SDLK_SPACE = ' ',
    SDLK_EXCLAIM = '!',
    SDLK_QUOTEDBL = '"',
    SDLK_HASH = '#',
    SDLK_PERCENT = '%',
    SDLK_DOLLAR = '$',
    SDLK_AMPERSAND = '&',
    SDLK_QUOTE = '\'',
    SDLK_LEFTPAREN = '(',
    SDLK_RIGHTPAREN = ')',
    SDLK_ASTERISK = '*',
    SDLK_PLUS = '+',
    SDLK_COMMA = ',',
    SDLK_MINUS = '-',
    SDLK_PERIOD = '.',
    SDLK_SLASH = '/',
    SDLK_0 = '0',
    SDLK_1 = '1',
    SDLK_2 = '2',
    SDLK_3 = '3',
    SDLK_4 = '4',
    SDLK_5 = '5',
    SDLK_6 = '6',
    SDLK_7 = '7',
    SDLK_8 = '8',
    SDLK_9 = '9',
    SDLK_COLON = ':',
    SDLK_SEMICOLON = ';',
    SDLK_LESS = '<',
    SDLK_EQUALS = '=',
    SDLK_GREATER = '>',
    SDLK_QUESTION = '?',
    SDLK_AT = '@',
    SDLK_LEFTBRACKET = '[',
    SDLK_BACKSLASH = '\\',
    SDLK_RIGHTBRACKET = ']',
    SDLK_CARET = '^',
    SDLK_UNDERSCORE = '_',
    SDLK_BACKQUOTE = '`',
    SDLK_a = 'a',
    SDLK_b = 'b',
    SDLK_c = 'c',
    SDLK_d = 'd',
    SDLK_e = 'e',
    SDLK_f = 'f',
    SDLK_g = 'g',
    SDLK_h = 'h',
    SDLK_i = 'i',
    SDLK_j = 'j',
    SDLK_k = 'k',
    SDLK_l = 'l',
    SDLK_m = 'm',
    SDLK_n = 'n',
    SDLK_o = 'o',
    SDLK_p = 'p',
    SDLK_q = 'q',
    SDLK_r = 'r',
    SDLK_s = 's',
    SDLK_t = 't',
    SDLK_u = 'u',
    SDLK_v = 'v',
    SDLK_w = 'w',
    SDLK_x = 'x',
    SDLK_y = 'y',
    SDLK_z = 'z',
    SDLK_F1 = 0x40000063,
    SDLK_F2 = 0x40000064,
    SDLK_F3 = 0x40000065,
    SDLK_F4 = 0x40000066,
    SDLK_F5 = 0x40000067,
    SDLK_F6 = 0x40000068,
    SDLK_F7 = 0x40000069,
    SDLK_F8 = 0x4000006A,
    SDLK_F9 = 0x4000006B,
    SDLK_F10 = 0x4000006C,
    SDLK_F11 = 0x4000006D,
    SDLK_F12 = 0x4000006E,
    SDLK_PAUSE = 0x40000071,
    SDLK_UP = 0x40000052,
    SDLK_DOWN = 0x40000051,
    SDLK_LEFT = 0x40000050,
    SDLK_RIGHT = 0x4000004F,
} SDL_Keycode;

typedef struct SDL_Rect {
    int x, y;
    int w, h;
} SDL_Rect;

typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode sym;
    uint16_t mod;
    uint32_t unicode;
} SDL_Keysym;

#define SDL_INIT_VIDEO      0x00000001
#define SDL_INIT_AUDIO      0x00000002
#define SDL_INIT_TIMER      0x00000004
#define SDL_INIT_JOYSTICK   0x00000008

#define SDL_WINDOW_SHOWN    0x00000001
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x00001000

#define SDL_WINDOWPOS_CENTERED 0x1FFFFFF

typedef struct SDL_Window {
    int width;
    int height;
    bool fullscreen;
    const char* title;
} SDL_Window;

typedef struct SDL_Renderer SDL_Renderer;

typedef struct SDL_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} SDL_Color;

typedef struct SDL_PixelFormat {
    SDL_Color* palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint8_t Rloss, Gloss, Bloss, Aloss;
    uint8_t Rshift, Gshift, Bshift, Ashift;
    uint32_t Rmask, Gmask, Bmask, Amask;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    uint32_t flags;
    SDL_PixelFormat* format;
    int w, h;
    uint16_t pitch;
    void* pixels;
    SDL_Rect clip_rect;
    uint8_t alpha;
    uint8_t owns_pixels;
} SDL_Surface;

typedef struct SDL_Point {
    int x;
    int y;
} SDL_Point;

typedef struct SDL_KeyboardEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint8_t state;
    uint8_t repeat;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_TextInputEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    char text[32];
} SDL_TextInputEvent;

typedef struct SDL_MouseMotionEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    int32_t x;
    int32_t y;
    int32_t xrel;
    int32_t yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    uint8_t button;
    uint8_t state;
    uint8_t clicks;
    int32_t x;
    int32_t y;
} SDL_MouseButtonEvent;

typedef struct SDL_MouseWheelEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    int32_t x;
    int32_t y;
} SDL_MouseWheelEvent;

typedef struct SDL_JoyAxisEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t axis;
    int16_t value;
} SDL_JoyAxisEvent;

typedef struct SDL_JoyButtonEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t button;
    uint8_t state;
} SDL_JoyButtonEvent;

typedef struct SDL_JoyDeviceEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
} SDL_JoyDeviceEvent;

typedef struct SDL_QuitEvent {
    uint32_t type;
    uint32_t timestamp;
} SDL_QuitEvent;

typedef struct SDL_WindowEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint8_t event;
    int32_t data1;
    int32_t data2;
} SDL_WindowEvent;

typedef union SDL_Event {
    uint32_t type;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyButtonEvent jbutton;
    SDL_JoyDeviceEvent jdevice;
    SDL_QuitEvent quit;
    SDL_WindowEvent window;
} SDL_Event;

#define SDL_KEYDOWN 0x300
#define SDL_KEYUP 0x301
#define SDL_TEXTINPUT 0x303
#define SDL_MOUSEMOTION 0x400
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP 0x402
#define SDL_MOUSEWHEEL 0x403
#define SDL_JOYAXISMOTION 0x600
#define SDL_JOYBUTTONDOWN 0x603
#define SDL_JOYBUTTONUP 0x604
#define SDL_JOYDEVICEADDED 0x605
#define SDL_JOYDEVICEREMOVED 0x606
#define SDL_QUIT 0x100
#define SDL_WINDOWEVENT 0x200
#define SDL_WINDOWEVENT_CLOSE 0x201

#define SDL_RELEASED 0
#define SDL_PRESSED 1

#define SDL_RENDERER_ACCELERATED 0x00000001
#define SDL_RENDERER_SOFTWARE 0x00000002

#define SDL_TEXTUREACCESS_STATIC 0x00000000
#define SDL_TEXTUREACCESS_STREAMING 0x00000001
#define SDL_TEXTUREACCESS_TARGET 0x00000002

#define SDL_PIXELFORMAT_RGB888 0x16161804
#define SDL_PIXELFORMAT_RGBA8888 0x16462004
#define SDL_PIXELFORMAT_ARGB8888 0x16361804

#define SDL_BYTEORDER 1234
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321

#define SDL_MUSTLOCK(s) 0
int SDL_LockSurface(SDL_Surface* surface);
void SDL_UnlockSurface(SDL_Surface* surface);

#define SDL_BLENDMODE_NONE 0x00000000
#define SDL_BLENDMODE_BLEND 0x00000001
#define SDL_BLENDMODE_ADD 0x00000002
#define SDL_BLENDMODE_MOD 0x00000004

#define SDL_FLIP_NONE 0
#define SDL_FLIP_HORIZONTAL 1
#define SDL_FLIP_VERTICAL 2

#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"

typedef struct SDL_Texture SDL_Texture;

typedef struct SDL_RWops {
    int (*seek)(struct SDL_RWops* context, int offset, int whence);
    int (*read)(struct SDL_RWops* context, void* ptr, int size, int maxnum);
    int (*write)(struct SDL_RWops* context, const void* ptr, int size, int num);
    void (*close)(struct SDL_RWops* context);
    uint32_t type;
    void *hidden;
} SDL_RWops;

typedef void (*SDL_EventFilter)(const SDL_Event* event);

int SDL_Init(uint32_t flags);
void SDL_Quit(void);
uint32_t SDL_GetTicks(void);
void SDL_Delay(uint32_t ms);

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags);
void SDL_DestroyWindow(SDL_Window* window);
void SDL_SetWindowTitle(SDL_Window* window, const char* title);
void SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags);
void SDL_SetWindowIcon(SDL_Window* window, SDL_Surface* icon);
void SDL_SetHint(const char* name, const char* value);

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags);
void SDL_DestroyRenderer(SDL_Renderer* renderer);
int SDL_RenderSetLogicalSize(SDL_Renderer* renderer, int w, int h);
void SDL_RenderClear(SDL_Renderer* renderer);
void SDL_RenderPresent(SDL_Renderer* renderer);
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect);
int SDL_RenderCopyEx(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect, double angle, const SDL_Point* center, int flip);
void SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect);
void SDL_RenderGetScale(SDL_Renderer* renderer, float* scaleX, float* scaleY);
int SDL_RenderReadPixels(SDL_Renderer* renderer, const SDL_Rect* rect, uint32_t format, void* pixels, int pitch);
void SDL_SetRenderDrawColor(SDL_Renderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void SDL_SetRenderDrawBlendMode(SDL_Renderer* renderer, int blendMode);

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface);
void SDL_DestroyTexture(SDL_Texture* texture);
int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch);
int SDL_LockTexture(SDL_Texture* texture, const SDL_Rect* rect, void** pixels, int* pitch);
void SDL_UnlockTexture(SDL_Texture* texture);
int SDL_QueryTexture(SDL_Texture* texture, uint32_t* format, int* access, int* w, int* h);
void SDL_SetTextureBlendMode(SDL_Texture* texture, int blendMode);
int SDL_SetTextureAlphaMod(SDL_Texture* texture, uint8_t alpha);

const uint8_t* SDL_GetKeyboardState(int* numkeys);
const char* SDL_GetKeyName(SDL_Keycode key);
double SDL_sin(double x);

SDL_Surface* SDL_CreateRGBSurfaceWithFormat(uint32_t flags, int width, int height, int depth, uint32_t format);

SDL_Surface* SDL_LoadBMP(const char* file);
SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, SDL_PixelFormat* fmt, uint32_t flags);
void SDL_FreeSurface(SDL_Surface* surface);
int SDL_SaveBMP(SDL_Surface* surface, const char* file);
SDL_Surface* SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
SDL_Surface* SDL_CreateRGBSurfaceFrom(void* pixels, int width, int height, int depth, int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
uint32_t SDL_MapRGB(const SDL_PixelFormat* format, uint8_t r, uint8_t g, uint8_t b);
uint32_t SDL_MapRGBA(const SDL_PixelFormat* format, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void SDL_GetRGB(uint32_t pixel, const SDL_PixelFormat* format, uint8_t* r, uint8_t* g, uint8_t* b);
void SDL_GetRGBA(uint32_t pixel, const SDL_PixelFormat* format, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

int SDL_PollEvent(SDL_Event* event);
int SDL_WaitEvent(SDL_Event* event);
int SDL_PushEvent(SDL_Event* event);
void SDL_SetEventFilter(SDL_EventFilter filter, void* userdata);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);

SDL_bool SDL_SetHintWithPriority(const char* name, const char* value, int priority);

SDL_RWops* SDL_RWFromFile(const char* file, const char* mode);

int TTF_Init(void);
void TTF_Quit(void);
void* TTF_OpenFont(const char* file, int ptsize);
void TTF_CloseFont(void* font);
int TTF_GetFontStyle(void* font);
void TTF_SetFontStyle(void* font, int style);
int TTF_FontHeight(const void* font);
int TTF_SetFontSize(void* font, int size);
void TTF_SetFontWrappedAlign(void* font, int align);
SDL_Surface* TTF_RenderText_Solid(void* font, const char* text, SDL_Color fg);
SDL_Surface* TTF_RenderUTF8_Solid(void* font, const char* text, SDL_Color fg);
SDL_Surface* TTF_RenderText_Blended(void* font, const char* text, SDL_Color fg);
SDL_Surface* TTF_RenderUTF8_Blended(void* font, const char* text, SDL_Color fg);
SDL_Surface* TTF_RenderText_Blended_Wrapped(void* font, const char* text, SDL_Color fg, uint32_t wrap);
SDL_Surface* TTF_RenderUTF8_Blended_Wrapped(void* font, const char* text, SDL_Color fg, uint32_t wrap);

#define TTF_STYLE_NORMAL        0
#define TTF_STYLE_BOLD          1
#define TTF_STYLE_ITALIC        2
#define TTF_STYLE_UNDERLINE     4

typedef void TTF_Font;
typedef void Mix_Chunk;
typedef void Mix_Music;

enum TTF_WrappedAlign {
    TTF_WRAPPED_ALIGN_LEFT = 0,
    TTF_WRAPPED_ALIGN_CENTER = 1,
    TTF_WRAPPED_ALIGN_RIGHT = 2
};

char* SDL_GetPrefPath(const char* org, const char* app);
void SDL_LogWarn(int category, const char* fmt, ...);
void SDL_LogError(int category, const char* fmt, ...);
void SDL_Log(const char* fmt, ...);
int SDL_SetSurfaceBlendMode(SDL_Surface* surface, int blendMode);
int SDL_BlitSurface(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect);
int SDL_BlitScaled(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect);

#define IMG_Init(x) 0
#define IMG_Quit()
SDL_Surface* IMG_Load(const char* file);
SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file);

#define Mix_OpenAudio(a, b, c, d) 0
#define Mix_CloseAudio()
#define Mix_AllocateChannels(x) 0
#define Mix_Volume(x, y) 100

#define MIX_DEFAULT_FREQUENCY 22050
#define MIX_DEFAULT_FORMAT 0
#define MIX_DEFAULT_CHANNELS 2

const char* Mix_GetError(void);

#define Mix_LoadWAV(x) NULL
#define Mix_LoadMUS(x) NULL
#define Mix_FreeChunk(x)
#define Mix_FreeMusic(x)

#define Mix_PlayChannel(a, b, c) -1
#define Mix_PlayMusic(x, y) -1
#define Mix_FadeInMusic(x, y, z) -1
#define Mix_FadeOutMusic(x) 0
#define Mix_HaltMusic() 0
#define Mix_HaltChannel(x) 0
#define Mix_PlayingMusic() 0
#define Mix_FadingMusic() 0

#define Mix_PauseMusic()
#define Mix_ResumeMusic()
#define Mix_Pause(x)
#define Mix_Resume(x)

#define Mix_Quit()

const char* SDL_GetError(void);

#ifdef __cplusplus
}
#endif

#endif
