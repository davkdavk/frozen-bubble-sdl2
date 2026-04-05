#include "sdl_ogc_shim.h"
#include "sdl_ogc_impl.h"
#include "wii_input.h"
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/video.h>
#include <ogc/gx.h>
#include <fat.h>
#include <wiiuse/wpad.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>

static SDL_PixelFormat* create_pixel_format(uint32_t format)
{
    SDL_PixelFormat* pixel_format = (SDL_PixelFormat*)malloc(sizeof(SDL_PixelFormat));
    if (!pixel_format) return NULL;

    memset(pixel_format, 0, sizeof(SDL_PixelFormat));
    pixel_format->BitsPerPixel = 32;
    pixel_format->BytesPerPixel = 4;

    switch (format) {
        case SDL_PIXELFORMAT_RGBA8888:
            pixel_format->Rmask = 0xFF000000;
            pixel_format->Gmask = 0x00FF0000;
            pixel_format->Bmask = 0x0000FF00;
            pixel_format->Amask = 0x000000FF;
            pixel_format->Rshift = 24;
            pixel_format->Gshift = 16;
            pixel_format->Bshift = 8;
            pixel_format->Ashift = 0;
            break;
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_ARGB8888:
        default:
            pixel_format->Rmask = 0x00FF0000;
            pixel_format->Gmask = 0x0000FF00;
            pixel_format->Bmask = 0x000000FF;
            pixel_format->Amask = 0xFF000000;
            pixel_format->Rshift = 16;
            pixel_format->Gshift = 8;
            pixel_format->Bshift = 0;
            pixel_format->Ashift = 24;
            break;
    }

    return pixel_format;
}

static void ensure_pref_path(void)
{
    mkdir("sd:/apps", 0777);
    mkdir("sd:/apps/frozenbubble", 0777);
}

static inline u8 clamp_u8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (u8)value;
}

// ---------------------------------------------------------------------------
// GX texture tiling: convert linear RGBA8888 → GX_TF_RGBA8 tile layout.
// GX_TF_RGBA8 stores 4×4 pixel blocks as 32 bytes:
//   bytes 0–15  = AR pairs for all 16 pixels (row-major within block)
//   bytes 16–31 = GB pairs for the same 16 pixels
// src is RGBA8888 (R in bits 31:24, G 23:16, B 15:8, A 7:0).
// w and h must be multiples of 4; caller is responsible for padding.
// ---------------------------------------------------------------------------
static void tile_rgba8(const uint32_t* src, int w, int h, uint8_t* dst)
{
    int blocks_x = w / 4;
    int blocks_y = h / 4;
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            uint8_t* block = dst + (by * blocks_x + bx) * 64;
            uint8_t* ar = block;
            uint8_t* gb = block + 32;
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    uint32_t p = src[(by * 4 + py) * w + (bx * 4 + px)];
                    uint8_t r = (p >> 24) & 0xFF;
                    uint8_t g = (p >> 16) & 0xFF;
                    uint8_t b = (p >>  8) & 0xFF;
                    uint8_t a = (p      ) & 0xFF;
                    int i = py * 4 + px;
                    ar[i * 2 + 0] = a;
                    ar[i * 2 + 1] = r;
                    gb[i * 2 + 0] = g;
                    gb[i * 2 + 1] = b;
                }
            }
        }
    }
}

// Round w/h up to next multiple of 4 for GX tiling.
static inline int gx_pad4(int v) { return (v + 3) & ~3; }

// Minimal 8x8 bitmap font covering ASCII 32–126.
// Each character is 8 bytes; bit 7 of each byte is the leftmost pixel.
// Sourced from the classic public-domain VGA ROM font.
static const uint8_t font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '\''
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // '('
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ','
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // '.'
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // '1'
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // '2'
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // '3'
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // '5'
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // '6'
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // '7'
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // '8'
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // '9'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // ':'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ';'
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // '<'
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // '='
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // '?'
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 'A'
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 'H'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'I'
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 'Q'
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 'S'
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'T'
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 'U'
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 'X'
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 'Y'
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // '['
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\\'
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 'c'
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 'd'
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 'e'
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 'f'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 'i'
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 'j'
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'l'
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 'n'
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 'o'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 'r'
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 's'
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 'y'
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 'z'
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // '}'
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // '~'
};

// Render text into an RGBA surface using the embedded 8x8 bitmap font.
// Each glyph cell is 8px wide x 16px tall (rows doubled for readability).
// Transparent background; foreground color fg.
static SDL_Surface* create_text_surface(const char* text, SDL_Color fg, uint32_t wrap)
{
    if (!text) text = "";
    int text_len = (int)strlen(text);

    // Measure: split into lines, respecting '\n' and wrap width.
    const int CELL_W = 8;
    const int CELL_H = 16; // 8px glyph doubled vertically

    // First pass: measure lines
    struct Line { int start; int len; };
    static Line lines[256];
    int n_lines = 0;
    int max_cols = wrap > 0 ? (int)(wrap / CELL_W) : 1024;

    int line_start = 0;
    int line_len   = 0;
    for (int i = 0; i <= text_len; i++) {
        char c = (i < text_len) ? text[i] : '\n';
        if (c == '\n' || line_len >= max_cols) {
            if (n_lines < 256) { lines[n_lines].start = line_start; lines[n_lines].len = line_len; n_lines++; }
            line_start = i + (c == '\n' ? 1 : 0);
            line_len   = (c == '\n') ? 0 : 1;
        } else {
            line_len++;
        }
    }
    if (n_lines == 0) { lines[0].start = 0; lines[0].len = 0; n_lines = 1; }

    int max_line = 0;
    for (int i = 0; i < n_lines; i++) if (lines[i].len > max_line) max_line = lines[i].len;
    if (max_line < 1) max_line = 1;

    int surf_w = max_line * CELL_W;
    int surf_h = n_lines * CELL_H;

    SDL_Surface* surf = SDL_CreateRGBSurface(0, surf_w, surf_h, 32,
        0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);
    if (!surf) return nullptr;

    uint32_t* px = (uint32_t*)surf->pixels;
    int stride   = surf->pitch / 4;

    // Clear to transparent black
    for (int i = 0; i < surf_h * stride; i++) px[i] = 0x00000000u;

    uint32_t fg_px = ((uint32_t)fg.r << 24) | ((uint32_t)fg.g << 16) |
                     ((uint32_t)fg.b <<  8) | 0xFFu;

    for (int li = 0; li < n_lines; li++) {
        int row_y = li * CELL_H;
        for (int ci = 0; ci < lines[li].len; ci++) {
            unsigned char c = (unsigned char)text[lines[li].start + ci];
            if (c < 32 || c > 126) c = '?';
            const uint8_t* glyph = font8x8[c - 32];
            int cx = ci * CELL_W;
            for (int gy = 0; gy < 8; gy++) {
                uint8_t row_bits = glyph[gy];
                for (int gx = 0; gx < 8; gx++) {
                    if (row_bits & (0x80u >> gx)) {
                        // Draw pixel at (cx+gx, row_y + gy*2) and (gy*2+1) for 2x height
                        int px_x = cx + gx;
                        int py0  = row_y + gy * 2;
                        int py1  = py0 + 1;
                        if (px_x < surf_w && py0 < surf_h) px[py0 * stride + px_x] = fg_px;
                        if (px_x < surf_w && py1 < surf_h) px[py1 * stride + px_x] = fg_px;
                    }
                }
            }
        }
    }

    return surf;
}

#define BACKBUFFER_W 640
#define BACKBUFFER_H 480

static bool sdl_initialized = false;
static SDL_Window* sdl_window = NULL;
static SDL_Renderer* sdl_renderer = NULL;
static SDL_Surface* screen_surface = NULL;
static void* fifo_buffer = NULL;

// Minimal CPU surface blitter used by SDL_BlitSurface (surface-to-surface only).
static void blit_surface_cpu(const uint32_t* src, int src_w, int src_h, const SDL_Rect* srcrect,
                              uint32_t* dst, int dst_w, int dst_h, SDL_Rect* dstrect)
{
    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int sw = srcrect ? srcrect->w : src_w;
    int sh = srcrect ? srcrect->h : src_h;
    int dx = dstrect ? dstrect->x : 0;
    int dy = dstrect ? dstrect->y : 0;
    int dw = dstrect ? dstrect->w : sw;
    int dh = dstrect ? dstrect->h : sh;

    if (dx >= dst_w || dy >= dst_h) return;
    if (sx >= src_w || sy >= src_h) return;
    if (dx + dw <= 0 || dy + dh <= 0) return;
    if (sx + sw <= 0 || sy + sh <= 0) return;
    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dx + dw > dst_w) { sw -= (dx + dw - dst_w); dw = dst_w - dx; }
    if (dy + dh > dst_h) { sh -= (dy + dh - dst_h); dh = dst_h - dy; }
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    int copy_w = sw < dw ? sw : dw;
    int copy_h = sh < dh ? sh : dh;
    for (int y = 0; y < copy_h; y++) {
        const uint32_t* s = src + (sy + y) * src_w + sx;
        uint32_t* d = dst + (dy + y) * dst_w + dx;
        for (int x = 0; x < copy_w; x++) {
            uint32_t p = s[x];
            uint8_t a = p & 0xFF;
            if (a == 0) continue;
            if (a == 255) { d[x] = p; continue; }
            uint8_t sr = (p >> 24) & 0xFF, sg = (p >> 16) & 0xFF, sb = (p >> 8) & 0xFF;
            uint32_t q = d[x];
            uint8_t dr = (q >> 24) & 0xFF, dg = (q >> 16) & 0xFF, db = (q >> 8) & 0xFF;
            d[x] = ((uint32_t)((sr * a + dr * (255 - a)) / 255) << 24) |
                   ((uint32_t)((sg * a + dg * (255 - a)) / 255) << 16) |
                   ((uint32_t)((sb * a + db * (255 - a)) / 255) <<  8) |
                   (uint32_t)(a + (uint8_t)(((int)(q & 0xFF)) * (255 - a) / 255));
        }
    }
}

int SDL_Init(uint32_t flags) {
    if (sdl_initialized) return 0;
    
    VIDEO_Init();
    PAD_Init();
    WiiInput::Instance()->Init();
    ensure_pref_path();
    
    fifo_buffer = memalign(32, 256 * 1024);
    memset(fifo_buffer, 0, 256 * 1024);
    GX_Init((GXFifoObj*)fifo_buffer, 256 * 1024);
    
    sdl_initialized = true;
    return 0;
}

void SDL_Quit(void) {
    if (screen_surface) {
        SDL_FreeSurface(screen_surface);
    }
    if (sdl_renderer) {
        SDL_DestroyRenderer(sdl_renderer);
    }
    if (sdl_window) {
        SDL_DestroyWindow(sdl_window);
    }
    
    if (fifo_buffer) {
        free(fifo_buffer);
        fifo_buffer = NULL;
    }
    
    sdl_initialized = false;
}

uint32_t SDL_GetTicks(void) {
    return (uint32_t)ticks_to_millisecs(gettime());
}

void SDL_Delay(uint32_t ms) {
    usleep(ms * 1000);
}

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags) {
    sdl_window = new SDL_Window();
    sdl_window->title = title;
    sdl_window->width = w;
    sdl_window->height = h;
    sdl_window->fullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    return sdl_window;
}

void SDL_DestroyWindow(SDL_Window* window) {
    if (window) {
        delete window;
    }
    sdl_window = NULL;
}

void SDL_SetWindowTitle(SDL_Window* window, const char* title) {
    if (window) {
        window->title = title;
    }
}

void SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags) {
    if (window) {
        window->fullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    }
}

void SDL_SetWindowIcon(SDL_Window* window, SDL_Surface* icon) {
}

void SDL_SetHint(const char* name, const char* value) {
}

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, uint32_t flags) {
    sdl_renderer = new SDL_Renderer();
    sdl_renderer->width = BACKBUFFER_W;
    sdl_renderer->height = BACKBUFFER_H;
    sdl_renderer->logical_width = BACKBUFFER_W;
    sdl_renderer->logical_height = BACKBUFFER_H;
    sdl_renderer->draw_r = 0;
    sdl_renderer->draw_g = 0;
    sdl_renderer->draw_b = 0;
    sdl_renderer->draw_a = 255;
    sdl_renderer->blend_mode = SDL_BLENDMODE_BLEND;
    sdl_renderer->current_fb = 0;
    sdl_renderer->video_active = false;
    sdl_renderer->gx_frame_started = false;

    GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
    sdl_renderer->vmode = rmode;

    // Configure video output.
    VIDEO_Configure(rmode);
    u32 fb_size = rmode->fbWidth * rmode->xfbHeight * 2;
    sdl_renderer->frame_buffer[0] = memalign(32, fb_size);
    sdl_renderer->frame_buffer[1] = memalign(32, fb_size);
    // YUY2: Y=0,Cb=0,Cr=0 = green in BT.601; fill with 0x00800080 for true black.
    { u32* p = (u32*)sdl_renderer->frame_buffer[0]; for (u32 i = 0; i < fb_size/4; i++) p[i] = 0x00800080; }
    { u32* p = (u32*)sdl_renderer->frame_buffer[1]; for (u32 i = 0; i < fb_size/4; i++) p[i] = 0x00800080; }
    DCFlushRange(sdl_renderer->frame_buffer[0], fb_size);
    DCFlushRange(sdl_renderer->frame_buffer[1], fb_size);
    VIDEO_SetNextFramebuffer(sdl_renderer->frame_buffer[0]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    // GX display copy setup.
    // EFB is rendered at fbWidth x efbHeight (e.g. 640x480 for both NTSC and PAL).
    // YScale converts EFB lines → XFB lines (>1.0 for PAL interlaced).
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight));
    GXColor clear_color = {0, 0, 0, 0xFF};
    GX_SetCopyClear(clear_color, 0x00FFFFFF);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    // Ortho projection: pixel coords (0,0)=(top-left) → (640,480)=(bottom-right).
    Mtx44 proj;
    guOrtho(proj, 0, BACKBUFFER_H, 0, BACKBUFFER_W, 0, 1);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    // EFB viewport is always fbWidth x efbHeight (480 for NTSC/PAL progressive).
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);

    // Identity model-view matrix — required even for 2D; without this GX
    // transforms vertices by uninitialised matrix data → blank screen.
    Mtx mv;
    guMtxIdentity(mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);

    // Default GX state used every frame.
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    // Separate colour-only vertex format (for SDL_RenderFillRect).
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_F32, 0);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    return sdl_renderer;
}

void SDL_DestroyRenderer(SDL_Renderer* renderer) {
    if (renderer) {
        if (renderer->frame_buffer[0]) {
            free(renderer->frame_buffer[0]);
            renderer->frame_buffer[0] = NULL;
        }
        if (renderer->frame_buffer[1]) {
            free(renderer->frame_buffer[1]);
            renderer->frame_buffer[1] = NULL;
        }
        delete renderer;
    }
    sdl_renderer = NULL;
}

int SDL_RenderSetLogicalSize(SDL_Renderer* renderer, int w, int h) {
    if (renderer) {
        renderer->logical_width = w;
        renderer->logical_height = h;
    }
    return 0;
}

void SDL_RenderClear(SDL_Renderer* renderer) {
    if (!renderer || !renderer->vmode) return;

    GXColor cc = { renderer->draw_r, renderer->draw_g, renderer->draw_b, renderer->draw_a };
    GX_SetCopyClear(cc, 0x00FFFFFF);

    // Invalidate texture cache and restart vertex pipeline for this frame.
    GX_InvVtxCache();
    GX_InvalidateTexAll();
    renderer->gx_frame_started = true;
}

// Draw one textured quad from (sx,sy,sw,sh) in texture space to (dx,dy,dw,dh) in screen space.
// tex_w/tex_h are the actual tiled texture dimensions (padded to multiple of 4).
static void gx_draw_textured_quad(SDL_Texture* texture,
                                   float sx, float sy, float sw, float sh,
                                   float dx, float dy, float dw, float dh)
{
    GX_LoadTexObj(&texture->texObj, GX_TEXMAP0);

    int tw = gx_pad4(texture->w);
    int th = gx_pad4(texture->h);
    float u0 = sx / tw;
    float v0 = sy / th;
    float u1 = (sx + sw) / tw;
    float v1 = (sy + sh) / th;

    float x0 = dx,      y0 = dy;
    float x1 = dx + dw, y1 = dy + dh;

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(x0, y0); GX_TexCoord2f32(u0, v0);
    GX_Position2f32(x1, y0); GX_TexCoord2f32(u1, v0);
    GX_Position2f32(x1, y1); GX_TexCoord2f32(u1, v1);
    GX_Position2f32(x0, y1); GX_TexCoord2f32(u0, v1);
    GX_End();
}

void SDL_RenderPresent(SDL_Renderer* renderer) {
    if (!renderer || !renderer->vmode) return;

    GX_DrawDone();

    void* fb = renderer->frame_buffer[renderer->current_fb];
    GX_CopyDisp(fb, GX_TRUE);
    GX_Flush();

    VIDEO_SetNextFramebuffer(fb);
    renderer->video_active = true;
    VIDEO_Flush();
    VIDEO_WaitVSync();

    renderer->current_fb = 1 - renderer->current_fb;
    renderer->gx_frame_started = false;

    // Allow next frame's first SDL_PollEvent call to re-scan hardware.
    extern bool input_polled_this_frame;
    input_polled_this_frame = false;
}

int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture,
                   const SDL_Rect* srcrect, const SDL_Rect* dstrect) {
    if (!renderer || !texture || !texture->gx_buffer) return -1;

    float sx = srcrect ? (float)srcrect->x : 0.0f;
    float sy = srcrect ? (float)srcrect->y : 0.0f;
    float sw = srcrect ? (float)srcrect->w : (float)texture->w;
    float sh = srcrect ? (float)srcrect->h : (float)texture->h;

    float dx = dstrect ? (float)dstrect->x : 0.0f;
    float dy = dstrect ? (float)dstrect->y : 0.0f;
    float dw = dstrect ? (float)dstrect->w : sw;
    float dh = dstrect ? (float)dstrect->h : sh;

    // Apply blend mode — most game textures need alpha blending.
    if (texture->blend_mode == SDL_BLENDMODE_NONE) {
        GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
    } else {
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    }

    // TEV: textured, no vertex colour channel.
    GX_SetNumChans(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

    gx_draw_textured_quad(texture, sx, sy, sw, sh, dx, dy, dw, dh);

    return 0;
}

int SDL_RenderCopyEx(SDL_Renderer* renderer, SDL_Texture* texture,
                     const SDL_Rect* srcrect, const SDL_Rect* dstrect,
                     double angle, const SDL_Point* center, int flip) {
    if (!renderer || !texture || !texture->gx_buffer) return -1;

    float sx = srcrect ? (float)srcrect->x : 0.0f;
    float sy = srcrect ? (float)srcrect->y : 0.0f;
    float sw = srcrect ? (float)srcrect->w : (float)texture->w;
    float sh = srcrect ? (float)srcrect->h : (float)texture->h;

    float dx = dstrect ? (float)dstrect->x : 0.0f;
    float dy = dstrect ? (float)dstrect->y : 0.0f;
    float dw = dstrect ? (float)dstrect->w : sw;
    float dh = dstrect ? (float)dstrect->h : sh;

    // Apply blend mode
    if (texture->blend_mode == SDL_BLENDMODE_NONE) {
        GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
    } else {
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    }

    GX_SetNumChans(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_LoadTexObj(&texture->texObj, GX_TEXMAP0);

    int tw = gx_pad4(texture->w);
    int th = gx_pad4(texture->h);
    float u0 = sx / tw, v0 = sy / th;
    float u1 = (sx + sw) / tw, v1 = (sy + sh) / th;

    // Pivot in screen space (SDL center coords are relative to dstrect origin)
    float px = dx + (center ? (float)center->x : dw * 0.5f);
    float py = dy + (center ? (float)center->y : dh * 0.5f);

    // SDL convention: positive angle = clockwise in screen space (y-down).
    // Standard 2-D rotation in screen coords: CW = positive, y-down.
    // x' = (x-px)*cos - (y-py)*sin + px
    // y' = (x-px)*sin + (y-py)*cos + py
    float rad = (float)(angle * M_PI / 180.0);
    float c = cosf(rad), s = sinf(rad);

    // Corner offsets from pivot
    float cx0 = dx      - px,  cy0 = dy      - py;
    float cx1 = dx + dw - px,  cy1 = dy + dh - py;

    // Rotated corners: TL, TR, BR, BL
    float x00 = cx0*c - cy0*s + px,  y00 = cx0*s + cy0*c + py;
    float x10 = cx1*c - cy0*s + px,  y10 = cx1*s + cy0*c + py;
    float x11 = cx1*c - cy1*s + px,  y11 = cx1*s + cy1*c + py;
    float x01 = cx0*c - cy1*s + px,  y01 = cx0*s + cy1*c + py;

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(x00, y00); GX_TexCoord2f32(u0, v0);
    GX_Position2f32(x10, y10); GX_TexCoord2f32(u1, v0);
    GX_Position2f32(x11, y11); GX_TexCoord2f32(u1, v1);
    GX_Position2f32(x01, y01); GX_TexCoord2f32(u0, v1);
    GX_End();

    return 0;
}

void SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    if (!renderer) return;

    float x0 = rect ? (float)rect->x : 0.0f;
    float y0 = rect ? (float)rect->y : 0.0f;
    float x1 = rect ? (float)(rect->x + rect->w) : (float)BACKBUFFER_W;
    float y1 = rect ? (float)(rect->y + rect->h) : (float)BACKBUFFER_H;

    // Use a colour-only TEV pass.
    GX_SetNumChans(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);

    GXColor col = { renderer->draw_r, renderer->draw_g, renderer->draw_b, renderer->draw_a };
    GX_SetChanMatColor(GX_COLOR0A0, col);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_F32, 0);

    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    GX_Position2f32(x0, y0);
    GX_Position2f32(x1, y0);
    GX_Position2f32(x1, y1);
    GX_Position2f32(x0, y1);
    GX_End();

    // Restore standard textured vertex desc for subsequent SDL_RenderCopy calls.
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetNumChans(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
}

void SDL_SetRenderDrawColor(SDL_Renderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (renderer) {
        renderer->draw_r = r;
        renderer->draw_g = g;
        renderer->draw_b = b;
        renderer->draw_a = a;
    }
}

void SDL_SetRenderDrawBlendMode(SDL_Renderer* renderer, int blendMode) {
    if (renderer) {
        renderer->blend_mode = blendMode;
    }
}

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, uint32_t format, int access, int w, int h) {
    SDL_Texture* tex = new SDL_Texture();
    if (!tex) return NULL;
    tex->w = w;
    tex->h = h;
    tex->format = format;
    tex->access = access;
    tex->renderer = renderer;
    tex->blend_mode = SDL_BLENDMODE_BLEND;

    // Linear staging buffer for CPU writes.
    tex->buffer = memalign(32, w * h * 4);
    if (!tex->buffer) { delete tex; return NULL; }
    memset(tex->buffer, 0, w * h * 4);

    // GX tiled buffer — dimensions padded to multiple of 4.
    int pw = gx_pad4(w);
    int ph = gx_pad4(h);
    tex->gx_buffer = memalign(32, pw * ph * 4);
    if (!tex->gx_buffer) { free(tex->buffer); delete tex; return NULL; }
    memset(tex->gx_buffer, 0, pw * ph * 4);
    DCFlushRange(tex->gx_buffer, pw * ph * 4);

    GX_InitTexObj(&tex->texObj, tex->gx_buffer, pw, ph, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    return tex;
}

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface) {
    if (!surface || !surface->pixels) return NULL;
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
    if (!tex) return NULL;
    SDL_UpdateTexture(tex, NULL, surface->pixels, surface->pitch);
    return tex;
}

void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture) {
        if (texture->buffer) { free(texture->buffer); }
        if (texture->gx_buffer) { free(texture->gx_buffer); }
        delete texture;
    }
}

int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch) {
    if (!texture || !texture->buffer || !pixels) return -1;

    // Copy into linear staging buffer.
    const uint8_t* src = (const uint8_t*)pixels;
    uint8_t* dst = (uint8_t*)texture->buffer;
    int row_bytes = texture->w * 4;
    for (int y = 0; y < texture->h; y++) {
        memcpy(dst + y * row_bytes, src + y * pitch, row_bytes);
    }

    // Tile into gx_buffer.
    // If w/h are not multiples of 4 we need to blit into a padded scratch
    // before tiling. We use a local stack buffer for small textures and
    // heap for large ones; easiest approach is always pad into gx_buffer
    // by building a padded linear image first.
    int pw = gx_pad4(texture->w);
    int ph = gx_pad4(texture->h);
    size_t padded_size = pw * ph * 4;

    // Allocate temporary padded image (stack is fine for small textures,
    // but textures can be large so use heap).
    uint32_t* padded = (uint32_t*)malloc(padded_size);
    if (!padded) return -1;
    memset(padded, 0, padded_size);
    const uint32_t* lin = (const uint32_t*)texture->buffer;
    for (int y = 0; y < texture->h; y++) {
        memcpy(padded + y * pw, lin + y * texture->w, texture->w * 4);
    }

    tile_rgba8(padded, pw, ph, (uint8_t*)texture->gx_buffer);
    free(padded);

    DCFlushRange(texture->gx_buffer, padded_size);
    GX_InitTexObj(&texture->texObj, texture->gx_buffer, pw, ph,
                  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    return 0;
}


int SDL_LockTexture(SDL_Texture* texture, const SDL_Rect* rect, void** pixels, int* pitch) {
    return -1;
}

void SDL_UnlockTexture(SDL_Texture* texture) {
}

int SDL_QueryTexture(SDL_Texture* texture, uint32_t* format, int* access, int* w, int* h) {
    if (!texture) return -1;
    if (format) *format = texture->format;
    if (access) *access = texture->access;
    if (w) *w = texture->w;
    if (h) *h = texture->h;
    return 0;
}

void SDL_SetTextureBlendMode(SDL_Texture* texture, int blendMode) {
    if (texture) {
        texture->blend_mode = blendMode;
    }
}

int SDL_SetTextureAlphaMod(SDL_Texture* texture, uint8_t alpha) {
    return 0;
}

static bool fat_initialized = false;
static uint8_t keyboard_state[256] = {0};

// Update keyboard_state from the already-polled WPAD and GC pad.
// Must only be called AFTER WPAD_ScanPads() and PAD_ScanPads() have run for this frame.
static void update_keyboard_state_from_input(void) {
    memset(keyboard_state, 0, sizeof(keyboard_state));

    // GC pad (already scanned this frame) — digital D-pad
    u16 gc = PAD_ButtonsHeld(0);
    if (gc & PAD_BUTTON_LEFT)  keyboard_state[SDL_SCANCODE_LEFT]   = 1;
    if (gc & PAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT]  = 1;
    if (gc & PAD_BUTTON_UP)    keyboard_state[SDL_SCANCODE_UP]     = 1;
    if (gc & PAD_BUTTON_DOWN)  keyboard_state[SDL_SCANCODE_DOWN]   = 1;
    if (gc & PAD_BUTTON_A)     keyboard_state[SDL_SCANCODE_RETURN] = 1;

    // GC pad — main analog stick (covers users with arrow keys bound to stick)
    {
        s8 sx = PAD_StickX(0);
        s8 sy = PAD_StickY(0);
        const s8 DEAD = 40;
        if (sx < -DEAD) keyboard_state[SDL_SCANCODE_LEFT]  = 1;
        if (sx >  DEAD) keyboard_state[SDL_SCANCODE_RIGHT] = 1;
        // GC stick Y is +up/-down; SDL UP scancode = up on screen
        if (sy >  DEAD) keyboard_state[SDL_SCANCODE_UP]    = 1;
        if (sy < -DEAD) keyboard_state[SDL_SCANCODE_DOWN]  = 1;
    }

    // Wiimote — horizontal hold (NES-style): d-pad left/right aim, buttons 1/2 fire.
    // UP/DOWN d-pad are menu-nav edge events; held state not needed for gameplay.
    {
        WPADData* wd = WPAD_Data(0);
        if (wd) {
            u32 btns = wd->btns_h;
            if (btns & WPAD_BUTTON_LEFT)  keyboard_state[SDL_SCANCODE_LEFT]   = 1;
            if (btns & WPAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT]  = 1;
            // Buttons 1 and 2 fire (RETURN held = shooterAction).
            if (btns & (WPAD_BUTTON_1 | WPAD_BUTTON_2))
                                          keyboard_state[SDL_SCANCODE_RETURN] = 1;
        }
    }
}
static void init_fat(void) {
    if (!fat_initialized) {
        fat_initialized = fatInitDefault();
    }
}

static uint8_t *load_file(const char *file, size_t *out_size) {
    init_fat();
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return NULL; }
    *out_size = sz;
    return buf;
}

SDL_Surface* SDL_LoadBMP(const char* file) {
    size_t sz = 0;
    uint8_t *data = load_file(file, &sz);
    if (!data) return NULL;
    if (sz < 54 || data[0] != 'B' || data[1] != 'M') { free(data); return NULL; }
    int w = data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
    int h = data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24);
    int bits = data[28] | (data[29] << 8);
    int data_off = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
    int row_bytes = ((w * bits + 31) / 32) * 4;
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
    if (!surf) { free(data); return NULL; }
    uint32_t *dst = (uint32_t *)surf->pixels;
    for (int y = h - 1; y >= 0; y--) {
        uint8_t *src = data + data_off + (h - 1 - y) * row_bytes;
        for (int x = 0; x < w; x++) {
            if (bits == 24) {
                dst[y * w + x] = 0xFF000000 | (src[x*3+2] << 16) | (src[x*3+1] << 8) | src[x*3];
            } else if (bits == 32) {
                dst[y * w + x] = 0xFF000000 | (src[x*4+2] << 16) | (src[x*4+1] << 8) | src[x*4];
            }
        }
    }
    free(data);
    return surf;
}

SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, SDL_PixelFormat* fmt, uint32_t flags) {
    return NULL;
}

void SDL_FreeSurface(SDL_Surface* surface) {
    if (surface) {
        if (surface->pixels && surface->owns_pixels) {
            free(surface->pixels);
        }
        if (surface->format) {
            free(surface->format);
        }
        delete surface;
    }
}

int SDL_SaveBMP(SDL_Surface* surface, const char* file) {
    return -1;
}

SDL_Surface* SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
    SDL_Surface* surf = new SDL_Surface();
    if (!surf) return NULL;
    surf->w = width;
    surf->h = height;
    surf->pitch = width * 4;
    uint32_t pixel_format = SDL_PIXELFORMAT_RGBA8888;
    if (!(Rmask == 0 && Gmask == 0 && Bmask == 0 && Amask == 0)) {
        pixel_format = (Rmask == 0xFF000000 && Amask == 0x000000FF) ? SDL_PIXELFORMAT_RGBA8888 : SDL_PIXELFORMAT_ARGB8888;
    }
    surf->format = create_pixel_format(pixel_format);
    if (!surf->format) {
        delete surf;
        return NULL;
    }
    surf->clip_rect.x = 0;
    surf->clip_rect.y = 0;
    surf->clip_rect.w = width;
    surf->clip_rect.h = height;
    surf->alpha = 255;
    surf->owns_pixels = 1;
    surf->pixels = memalign(32, surf->pitch * height);
    if (!surf->pixels) {
        free(surf->format);
        delete surf;
        return NULL;
    }
    memset(surf->pixels, 0, surf->pitch * height);
    return surf;
}

SDL_Surface* SDL_CreateRGBSurfaceFrom(void* pixels, int width, int height, int depth, int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
    SDL_Surface* surf = new SDL_Surface();
    if (!surf) return NULL;
    surf->w = width;
    surf->h = height;
    surf->pitch = pitch;
    uint32_t pixel_format = SDL_PIXELFORMAT_RGBA8888;
    if (!(Rmask == 0 && Gmask == 0 && Bmask == 0 && Amask == 0)) {
        pixel_format = (Rmask == 0xFF000000 && Amask == 0x000000FF) ? SDL_PIXELFORMAT_RGBA8888 : SDL_PIXELFORMAT_ARGB8888;
    }
    surf->format = create_pixel_format(pixel_format);
    if (!surf->format) {
        delete surf;
        return NULL;
    }
    surf->clip_rect.x = 0;
    surf->clip_rect.y = 0;
    surf->clip_rect.w = width;
    surf->clip_rect.h = height;
    surf->alpha = 255;
    surf->owns_pixels = 0;
    surf->pixels = pixels;
    return surf;
}

uint32_t SDL_MapRGB(const SDL_PixelFormat* format, uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

uint32_t SDL_MapRGBA(const SDL_PixelFormat* format, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void SDL_GetRGB(uint32_t pixel, const SDL_PixelFormat* format, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (r) *r = (pixel >> 16) & 0xFF;
    if (g) *g = (pixel >> 8) & 0xFF;
    if (b) *b = pixel & 0xFF;
}

void SDL_GetRGBA(uint32_t pixel, const SDL_PixelFormat* format, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (r) *r = (pixel >> 24) & 0xFF;
    if (g) *g = (pixel >> 16) & 0xFF;
    if (b) *b = (pixel >> 8) & 0xFF;
    if (a) *a = pixel & 0xFF;
}

// Per-frame event queue — filled once per frame, drained by SDL_PollEvent calls.
#define EVT_QUEUE_SIZE 16
static SDL_Event evt_queue[EVT_QUEUE_SIZE];
static int evt_queue_head = 0;
static int evt_queue_tail = 0;
bool input_polled_this_frame = false;
static u16 gc_held_prev = 0;

static void evt_push(SDL_Event* e) {
    int next = (evt_queue_tail + 1) % EVT_QUEUE_SIZE;
    if (next != evt_queue_head) {
        evt_queue[evt_queue_tail] = *e;
        evt_queue_tail = next;
    }
}

static bool evt_pop(SDL_Event* e) {
    if (evt_queue_head == evt_queue_tail) return false;
    *e = evt_queue[evt_queue_head];
    evt_queue_head = (evt_queue_head + 1) % EVT_QUEUE_SIZE;
    return true;
}

static void push_key(SDL_Event* e, SDL_Keycode sym, SDL_Scancode sc) {
    e->type = SDL_KEYDOWN;
    e->key.keysym.sym = sym;
    e->key.keysym.scancode = sc;
    e->key.state = SDL_PRESSED;
    e->key.repeat = 0;
    evt_push(e);
}

// Previous-frame analog stick state for edge detection (stick entered zone).
static bool stick_left_prev  = false;
static bool stick_right_prev = false;
static bool stick_up_prev    = false;
static bool stick_down_prev  = false;

// Previous-frame WPAD button state for direct edge detection.
static u32 wpad_held_prev = 0;

// Called once per frame on the first SDL_PollEvent of each frame.
static void generate_input_events(void) {
    WiiInput* wi = WiiInput::Instance();
    wi->Poll();
    PAD_ScanPads();
    WPAD_ScanPads();
    update_keyboard_state_from_input();

    SDL_Event e;
    memset(&e, 0, sizeof(e));

    // --- Wiimote via direct WPAD (bypasses WiiInput wrapper) ---
    {
        WPADData* wd = WPAD_Data(0);
        u32 btns_h = wd ? wd->btns_h : 0;
        u32 btns_just = btns_h & ~wpad_held_prev;
        wpad_held_prev = btns_h;

        // Home button: return to main menu (not quit to HBC).
        if (btns_just & WPAD_BUTTON_HOME)  push_key(&e, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
        // Buttons 1 and 2: fire / menu confirm (edge event).
        if (btns_just & (WPAD_BUTTON_1 | WPAD_BUTTON_2))
            push_key(&e, SDLK_RETURN, SDL_SCANCODE_RETURN);
        // All four d-pad directions fire edge events for menu navigation.
        // Left/right also set held state (via update_keyboard_state_from_input) for gameplay aiming.
        if (btns_just & WPAD_BUTTON_UP)    push_key(&e, SDLK_UP,    SDL_SCANCODE_UP);
        if (btns_just & WPAD_BUTTON_DOWN)  push_key(&e, SDLK_DOWN,  SDL_SCANCODE_DOWN);
        if (btns_just & WPAD_BUTTON_LEFT)  push_key(&e, SDLK_LEFT,  SDL_SCANCODE_LEFT);
        if (btns_just & WPAD_BUTTON_RIGHT) push_key(&e, SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    }

    // --- GC pad digital D-pad + Start ---
    u16 gc_held = PAD_ButtonsHeld(0);
    u16 gc_just = gc_held & ~gc_held_prev;
    gc_held_prev = gc_held;
    if (gc_just & PAD_BUTTON_START)  push_key(&e, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    if (gc_just & PAD_BUTTON_A)      push_key(&e, SDLK_RETURN, SDL_SCANCODE_RETURN);
    if (gc_just & PAD_BUTTON_UP)     push_key(&e, SDLK_UP,     SDL_SCANCODE_UP);
    if (gc_just & PAD_BUTTON_DOWN)   push_key(&e, SDLK_DOWN,   SDL_SCANCODE_DOWN);
    if (gc_just & PAD_BUTTON_LEFT)   push_key(&e, SDLK_LEFT,   SDL_SCANCODE_LEFT);
    if (gc_just & PAD_BUTTON_RIGHT)  push_key(&e, SDLK_RIGHT,  SDL_SCANCODE_RIGHT);
    if (gc_just & PAD_BUTTON_B)      push_key(&e, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);

    // --- GC pad analog stick — emit edge event when stick enters a zone ---
    {
        s8 sx = PAD_StickX(0);
        s8 sy = PAD_StickY(0);
        const s8 DEAD = 40;
        bool sl = sx < -DEAD, sr = sx > DEAD;
        bool su = sy >  DEAD, sd = sy < -DEAD;

        if (sl && !stick_left_prev)  push_key(&e, SDLK_LEFT,  SDL_SCANCODE_LEFT);
        if (sr && !stick_right_prev) push_key(&e, SDLK_RIGHT, SDL_SCANCODE_RIGHT);
        if (su && !stick_up_prev)    push_key(&e, SDLK_UP,    SDL_SCANCODE_UP);
        if (sd && !stick_down_prev)  push_key(&e, SDLK_DOWN,  SDL_SCANCODE_DOWN);

        stick_left_prev  = sl;
        stick_right_prev = sr;
        stick_up_prev    = su;
        stick_down_prev  = sd;
    }
}

int SDL_PollEvent(SDL_Event* event) {
    if (!event) return 0;

    if (!input_polled_this_frame) {
        evt_queue_head = 0;
        evt_queue_tail = 0;
        generate_input_events();
        input_polled_this_frame = true;
    }

    return evt_pop(event) ? 1 : 0;
}

int SDL_WaitEvent(SDL_Event* event) {
    while (!SDL_PollEvent(event)) {
        usleep(1000);
    }
    return 1;
}

int SDL_PushEvent(SDL_Event* event) {
    return 0;
}

void SDL_SetEventFilter(SDL_EventFilter filter, void* userdata) {
}

void SDL_StartTextInput(void) {
}

void SDL_StopTextInput(void) {
}

SDL_bool SDL_SetHintWithPriority(const char* name, const char* value, int priority) {
    return SDL_FALSE;
}

SDL_RWops* SDL_RWFromFile(const char* file, const char* mode) {
    return NULL;
}

int TTF_Init(void) { return 0; }
void TTF_Quit(void) { }
void* TTF_OpenFont(const char* file, int ptsize) { return (void*)file; }
void TTF_CloseFont(void* font) { }
int TTF_GetFontStyle(void* font) { return 0; }
void TTF_SetFontStyle(void* font, int style) { }
int TTF_FontHeight(const void* font) { return 16; }
SDL_Surface* TTF_RenderText_Solid(void* font, const char* text, SDL_Color fg) { return create_text_surface(text, fg, 0); }
SDL_Surface* TTF_RenderUTF8_Solid(void* font, const char* text, SDL_Color fg) { return create_text_surface(text, fg, 0); }
SDL_Surface* TTF_RenderText_Blended(void* font, const char* text, SDL_Color fg) { return create_text_surface(text, fg, 0); }
SDL_Surface* TTF_RenderUTF8_Blended(void* font, const char* text, SDL_Color fg) { return create_text_surface(text, fg, 0); }
SDL_Surface* TTF_RenderText_Blended_Wrapped(void* font, const char* text, SDL_Color fg, uint32_t wrap) { return create_text_surface(text, fg, wrap); }
SDL_Surface* TTF_RenderUTF8_Blended_Wrapped(void* font, const char* text, SDL_Color fg, uint32_t wrap) { return create_text_surface(text, fg, wrap); }

int TTF_SetFontSize(void* font, int size) { return 0; }
void TTF_SetFontWrappedAlign(void* font, int align) { }

SDL_Surface* IMG_Load(const char* file) {
    char rgba_path[512];
    snprintf(rgba_path, sizeof(rgba_path), "%s.rgba", file);
    
    size_t sz = 0;
    uint8_t *data = load_file(rgba_path, &sz);
    if (data && sz >= 8) {
        int w = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        int h = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        if (w > 0 && h > 0 && (size_t)(w * h * 4) == sz - 8) {
            SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
            if (surf) {
                memcpy(surf->pixels, data + 8, w * h * 4);
                free(data);
                return surf;
            }
        }
    }
    if (data) free(data);
    
    SDL_Surface *surf = SDL_CreateRGBSurface(0, 16, 16, 32, 0, 0, 0, 0);
    if (surf) {
        uint32_t *pixels = (uint32_t *)surf->pixels;
        for (int i = 0; i < 256; i++) pixels[i] = 0xFF808080;
    }
    return surf;
}

SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file) {
    SDL_Surface* surf = IMG_Load(file);
    if (!surf) return NULL;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

char* SDL_GetPrefPath(const char* org, const char* app) {
    return (char*)"sd:/apps/frozenbubble/";
}

void SDL_LogWarn(int category, const char* fmt, ...) {
}

void SDL_LogError(int category, const char* fmt, ...) {
}

int SDL_SetSurfaceBlendMode(SDL_Surface* surface, int blendMode) {
    return 0;
}

int SDL_BlitSurface(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
    if (!src || !dst || !src->pixels || !dst->pixels) return -1;
    blit_surface_cpu((uint32_t*)src->pixels, src->w, src->h, srcrect,
                     (uint32_t*)dst->pixels, dst->w, dst->h, dstrect);
    return 0;
}

int SDL_BlitScaled(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
    return SDL_BlitSurface(src, srcrect, dst, dstrect);
}

const uint8_t* SDL_GetKeyboardState(int* numkeys) {
    // keyboard_state is already up-to-date from the last SDL_PollEvent call.
    // Do NOT re-scan here — that causes mid-frame double-polling.
    if (numkeys) *numkeys = 256;
    return keyboard_state;
}

const char* SDL_GetKeyName(SDL_Keycode key) {
    return "Unknown";
}

double SDL_sin(double x) {
    return sin(x);
}

SDL_Surface* SDL_CreateRGBSurfaceWithFormat(uint32_t flags, int width, int height, int depth, uint32_t format) {
    return SDL_CreateRGBSurface(flags, width, height, depth, 0, 0, 0, 0);
}

const char* SDL_GetError(void) {
    return "";
}

const char* Mix_GetError(void) {
    return "";
}

int SDL_LockSurface(SDL_Surface* surface) {
    return 0;
}

void SDL_UnlockSurface(SDL_Surface* surface) {
}

void SDL_RenderGetScale(SDL_Renderer* renderer, float* scaleX, float* scaleY) {
    if (scaleX) *scaleX = 1.0f;
    if (scaleY) *scaleY = 1.0f;
}

int SDL_RenderReadPixels(SDL_Renderer* renderer, const SDL_Rect* rect, uint32_t format, void* pixels, int pitch) {
    return 0;
}
