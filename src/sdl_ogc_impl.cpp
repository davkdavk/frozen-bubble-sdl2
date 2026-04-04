#include "sdl_ogc_shim.h"
#include "sdl_ogc_impl.h"
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

static inline void rgb_to_ycbcr(u8 r, u8 g, u8 b, u8* y, u8* cb, u8* cr)
{
    int yi = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
    int cbi = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
    int cri = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
    *y = clamp_u8(yi);
    *cb = clamp_u8(cbi);
    *cr = clamp_u8(cri);
}

static void clear_xfb_black(void* fb, int width, int height)
{
    u16* xfb = (u16*)fb;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int out = y * width + x;
            xfb[out] = (16 << 8) | 128;
            xfb[out + 1] = (16 << 8) | 128;
        }
    }
    DCFlushRange(fb, width * height * 2);
}

static SDL_Surface* create_text_surface(const char* text, SDL_Color fg, uint32_t wrap)
{
    int text_len = text ? (int)strlen(text) : 0;
    int max_chars = wrap > 0 ? (int)(wrap / 8) : text_len;
    int line_width = 0;
    int max_width = 0;
    int lines = 1;

    if (max_chars <= 0) max_chars = text_len > 0 ? text_len : 1;

    for (int i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            lines++;
            continue;
        }

        line_width++;
        if (wrap > 0 && line_width >= max_chars) {
            if (line_width > max_width) max_width = line_width;
            line_width = 0;
            lines++;
        }
    }

    if (line_width > max_width) max_width = line_width;
    if (max_width <= 0) max_width = 1;
    if (lines <= 0) lines = 1;

    SDL_Surface* surf = SDL_CreateRGBSurface(0, max_width * 8, lines * 16, 32, 0, 0, 0, 0);
    if (!surf || !surf->pixels) return surf;

    uint32_t color = (0xFFu << 24) | (fg.r << 16) | (fg.g << 8) | fg.b;
    uint32_t* pixels = (uint32_t*)surf->pixels;
    int total = surf->w * surf->h;
    for (int i = 0; i < total; i++) {
        pixels[i] = color;
    }

    return surf;
}

#define BACKBUFFER_W 640
#define BACKBUFFER_H 480
#define BACKBUFFER_SIZE (BACKBUFFER_W * BACKBUFFER_H * 4)

static bool sdl_initialized = false;
static SDL_Window* sdl_window = NULL;
static SDL_Renderer* sdl_renderer = NULL;
static SDL_Surface* screen_surface = NULL;
static void* fifo_buffer = NULL;
static int frame_count = 0;

static void blit_rgba8(const uint32_t* src, int src_w, int src_h, const SDL_Rect* srcrect,
                       uint32_t* dst, int dst_w, int dst_h, const SDL_Rect* dstrect)
{
    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int sw = srcrect ? srcrect->w : src_w;
    int sh = srcrect ? srcrect->h : src_h;

    int dx = dstrect ? dstrect->x : 0;
    int dy = dstrect ? dstrect->y : 0;
    int dw = dstrect ? dstrect->w : sw;
    int dh = dstrect ? dstrect->h : sh;

    if (dx >= (int)dst_w || dy >= (int)dst_h) return;
    if (sx >= src_w || sy >= src_h) return;

    if (dx + dw <= 0 || dy + dh <= 0) return;
    if (sx + sw <= 0 || sy + sh <= 0) return;

    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dx + dw > (int)dst_w) { sw -= (dx + dw - dst_w); dw = dst_w - dx; }
    if (dy + dh > (int)dst_h) { sh -= (dy + dh - dst_h); dh = dst_h - dy; }

    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    auto blend_pixel = [](uint32_t src_px, uint32_t dst_px) {
        uint8_t src_a = src_px & 0xFF;
        if (src_a == 0) return dst_px;
        if (src_a == 255) return src_px;

        uint8_t src_r = (src_px >> 24) & 0xFF;
        uint8_t src_g = (src_px >> 16) & 0xFF;
        uint8_t src_b = (src_px >> 8) & 0xFF;
        uint8_t dst_r = (dst_px >> 24) & 0xFF;
        uint8_t dst_g = (dst_px >> 16) & 0xFF;
        uint8_t dst_b = (dst_px >> 8) & 0xFF;
        uint8_t out_a = src_a + (uint8_t)((((int)(dst_px & 0xFF)) * (255 - src_a)) / 255);
        uint8_t out_r = (uint8_t)((src_r * src_a + dst_r * (255 - src_a)) / 255);
        uint8_t out_g = (uint8_t)((src_g * src_a + dst_g * (255 - src_a)) / 255);
        uint8_t out_b = (uint8_t)((src_b * src_a + dst_b * (255 - src_a)) / 255);
        return ((uint32_t)out_r << 24) | ((uint32_t)out_g << 16) | ((uint32_t)out_b << 8) | out_a;
    };

    if (sw == dw && sh == dh) {
        for (int y = 0; y < dh; y++) {
            const uint32_t* s = src + (sy + y) * src_w + sx;
            uint32_t* d = dst + (dy + y) * dst_w + dx;
            for (int x = 0; x < dw; x++) {
                d[x] = blend_pixel(s[x], d[x]);
            }
        }
    } else {
        float xscale = (float)sw / dw;
        float yscale = (float)sh / dh;
        for (int y = 0; y < dh; y++) {
            int src_y = sy + (int)(y * yscale);
            if (src_y >= src_h) src_y = src_h - 1;
            uint32_t* d = dst + (dy + y) * dst_w + dx;
            for (int x = 0; x < dw; x++) {
                int src_x = sx + (int)(x * xscale);
                if (src_x >= src_w) src_x = src_w - 1;
                d[x] = blend_pixel(src[src_y * src_w + src_x], d[x]);
            }
        }
    }
}

static void blit_surface_rgba8(const uint32_t* src, int src_w, int src_h, const SDL_Rect* srcrect,
                               uint32_t* dst, int dst_w, int dst_h, SDL_Rect* dstrect)
{
    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int sw = srcrect ? srcrect->w : src_w;
    int sh = srcrect ? srcrect->h : src_h;

    if (dstrect) {
        int dx = dstrect->x;
        int dy = dstrect->y;
        int dw = dstrect->w;
        int dh = dstrect->h;

        if (dx >= (int)dst_w || dy >= (int)dst_h) return;
        if (sx >= src_w || sy >= src_h) return;
        if (dx + dw <= 0 || dy + dh <= 0) return;
        if (sx + sw <= 0 || sy + sh <= 0) return;

        if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
        if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
        if (dx + dw > (int)dst_w) { sw -= (dx + dw - dst_w); dw = dst_w - dx; }
        if (dy + dh > (int)dst_h) { sh -= (dy + dh - dst_h); dh = dst_h - dy; }

        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

        auto blend_pixel = [](uint32_t src_px, uint32_t dst_px) {
            uint8_t src_a = src_px & 0xFF;
            if (src_a == 0) return dst_px;
            if (src_a == 255) return src_px;

            uint8_t src_r = (src_px >> 24) & 0xFF;
            uint8_t src_g = (src_px >> 16) & 0xFF;
            uint8_t src_b = (src_px >> 8) & 0xFF;
            uint8_t dst_r = (dst_px >> 24) & 0xFF;
            uint8_t dst_g = (dst_px >> 16) & 0xFF;
            uint8_t dst_b = (dst_px >> 8) & 0xFF;
            uint8_t out_a = src_a + (uint8_t)((((int)(dst_px & 0xFF)) * (255 - src_a)) / 255);
            uint8_t out_r = (uint8_t)((src_r * src_a + dst_r * (255 - src_a)) / 255);
            uint8_t out_g = (uint8_t)((src_g * src_a + dst_g * (255 - src_a)) / 255);
            uint8_t out_b = (uint8_t)((src_b * src_a + dst_b * (255 - src_a)) / 255);
            return ((uint32_t)out_r << 24) | ((uint32_t)out_g << 16) | ((uint32_t)out_b << 8) | out_a;
        };

        if (sw == dw && sh == dh) {
            for (int y = 0; y < dh; y++) {
                const uint32_t* s = src + (sy + y) * src_w + sx;
                uint32_t* d = dst + (dy + y) * dst_w + dx;
                for (int x = 0; x < dw; x++) {
                    d[x] = blend_pixel(s[x], d[x]);
                }
            }
        } else {
            float xscale = (float)sw / dw;
            float yscale = (float)sh / dh;
            for (int y = 0; y < dh; y++) {
                int src_y = sy + (int)(y * yscale);
                if (src_y >= src_h) src_y = src_h - 1;
                uint32_t* d = dst + (dy + y) * dst_w + dx;
                for (int x = 0; x < dw; x++) {
                    int src_x = sx + (int)(x * xscale);
                    if (src_x >= src_w) src_x = src_w - 1;
                    d[x] = blend_pixel(src[src_y * src_w + src_x], d[x]);
                }
            }
        }
    } else {
        int dx = 0, dy = 0;
        int dw = sw, dh = sh;
        if (dx >= (int)dst_w || dy >= (int)dst_h) return;
        if (sx >= src_w || sy >= src_h) return;
        if (dx + dw <= 0 || dy + dh <= 0) return;
        if (sx + sw <= 0 || sy + sh <= 0) return;
        if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
        if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
        if (dx + dw > (int)dst_w) { sw -= (dx + dw - dst_w); dw = dst_w - dx; }
        if (dy + dh > (int)dst_h) { sh -= (dy + dh - dst_h); dh = dst_h - dy; }
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
        for (int y = 0; y < dh; y++) {
            const uint32_t* s = src + (sy + y) * src_w + sx;
            uint32_t* d = dst + (dy + y) * dst_w + dx;
            for (int x = 0; x < dw; x++) {
                uint32_t src_px = s[x];
                uint8_t src_a = src_px & 0xFF;
                if (src_a == 0) continue;
                if (src_a == 255) {
                    d[x] = src_px;
                    continue;
                }
                uint8_t src_r = (src_px >> 24) & 0xFF;
                uint8_t src_g = (src_px >> 16) & 0xFF;
                uint8_t src_b = (src_px >> 8) & 0xFF;
                uint32_t dst_px = d[x];
                uint8_t dst_r = (dst_px >> 24) & 0xFF;
                uint8_t dst_g = (dst_px >> 16) & 0xFF;
                uint8_t dst_b = (dst_px >> 8) & 0xFF;
                uint8_t out_a = src_a + (uint8_t)((((int)(dst_px & 0xFF)) * (255 - src_a)) / 255);
                uint8_t out_r = (uint8_t)((src_r * src_a + dst_r * (255 - src_a)) / 255);
                uint8_t out_g = (uint8_t)((src_g * src_a + dst_g * (255 - src_a)) / 255);
                uint8_t out_b = (uint8_t)((src_b * src_a + dst_b * (255 - src_a)) / 255);
                d[x] = ((uint32_t)out_r << 24) | ((uint32_t)out_g << 16) | ((uint32_t)out_b << 8) | out_a;
            }
        }
    }
}

static void draw_quad_scaled(GXRModeObj* vmode, void* fb, uint32_t* tex_data, int tex_w, int tex_h)
{
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GXTexObj texObj;
    GX_InitTexObj(&texObj, tex_data, tex_w, tex_h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);

    Mtx44 proj;
    guOrtho(proj, 0, vmode->xfbHeight, 0, vmode->fbWidth, 0, 1);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetViewport(0, 0, vmode->fbWidth, vmode->xfbHeight, 0, 1);
    GX_SetScissor(0, 0, vmode->fbWidth, vmode->xfbHeight);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(0, 0);
    GX_TexCoord2f32(0.0f, 0.0f);
    GX_Position2s16(vmode->fbWidth, 0);
    GX_TexCoord2f32(1.0f, 0.0f);
    GX_Position2s16(vmode->fbWidth, vmode->xfbHeight);
    GX_TexCoord2f32(1.0f, 1.0f);
    GX_Position2s16(0, vmode->xfbHeight);
    GX_TexCoord2f32(0.0f, 1.0f);
    GX_End();

    GX_DrawDone();
    GX_Flush();
}

int SDL_Init(uint32_t flags) {
    if (sdl_initialized) return 0;
    
    VIDEO_Init();
    PAD_Init();
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
    return (uint32_t)(gettime() / 1000ULL);
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
    sdl_renderer->back_buffer_dirty = false;
    sdl_renderer->current_fb = 0;
    sdl_renderer->video_active = false;

    GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
    sdl_renderer->vmode = rmode;

    u32 fb_size = rmode->fbWidth * rmode->xfbHeight * 2;
    sdl_renderer->frame_buffer[0] = memalign(32, fb_size);
    sdl_renderer->frame_buffer[1] = memalign(32, fb_size);
    clear_xfb_black(sdl_renderer->frame_buffer[0], rmode->fbWidth, rmode->xfbHeight);
    clear_xfb_black(sdl_renderer->frame_buffer[1], rmode->fbWidth, rmode->xfbHeight);

    sdl_renderer->back_buffer = memalign(32, BACKBUFFER_SIZE);
    memset(sdl_renderer->back_buffer, 0, BACKBUFFER_SIZE);
    GX_InitTexObj(&sdl_renderer->back_buffer_tex, sdl_renderer->back_buffer,
                  BACKBUFFER_W, BACKBUFFER_H, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

    GX_SetDispCopySrc(0, 0, BACKBUFFER_W, BACKBUFFER_H);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetDispCopyYScale((f32)(rmode->xfbHeight / (f32)BACKBUFFER_H));
    GXColor clear_color = {0, 0, 0, 0xFF};
    GX_SetCopyClear(clear_color, 0xFF);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    Mtx44 proj;
    guOrtho(proj, 0, rmode->xfbHeight, 0, rmode->fbWidth, 0, 1);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->xfbHeight, 0, 1);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->xfbHeight);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    return sdl_renderer;
}

void SDL_DestroyRenderer(SDL_Renderer* renderer) {
    if (renderer) {
        if (renderer->back_buffer) {
            free(renderer->back_buffer);
            renderer->back_buffer = NULL;
        }
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
    if (renderer && renderer->back_buffer) {
        uint32_t color = (renderer->draw_r << 24) | (renderer->draw_g << 16) |
                         (renderer->draw_b << 8) | renderer->draw_a;
        uint32_t* buf = (uint32_t*)renderer->back_buffer;
        for (int i = 0; i < BACKBUFFER_W * BACKBUFFER_H; i++) {
            buf[i] = color;
        }
        renderer->back_buffer_dirty = true;
    }
}

static void copy_backbuffer_to_fb(SDL_Renderer* renderer) {
    GXRModeObj* rmode = renderer->vmode;
    void* fb = renderer->frame_buffer[renderer->current_fb];
    uint32_t* bb = (uint32_t*)renderer->back_buffer;
    u16* xfb = (u16*)fb;

    for (int y = 0; y < BACKBUFFER_H; y++) {
        for (int x = 0; x < BACKBUFFER_W; x += 2) {
            uint32_t px0 = bb[y * BACKBUFFER_W + x];
            uint32_t px1 = bb[y * BACKBUFFER_W + x + 1];

            u8 r0 = (px0 >> 24) & 0xFF;
            u8 g0 = (px0 >> 16) & 0xFF;
            u8 b0 = (px0 >> 8) & 0xFF;
            u8 r1 = (px1 >> 24) & 0xFF;
            u8 g1 = (px1 >> 16) & 0xFF;
            u8 b1 = (px1 >> 8) & 0xFF;

            u8 y0, cb0, cr0;
            u8 y1, cb1, cr1;
            rgb_to_ycbcr(r0, g0, b0, &y0, &cb0, &cr0);
            rgb_to_ycbcr(r1, g1, b1, &y1, &cb1, &cr1);

            u8 cb = (u8)(((int)cb0 + (int)cb1) / 2);
            u8 cr = (u8)(((int)cr0 + (int)cr1) / 2);
            int out = y * rmode->fbWidth + x;
            xfb[out] = (y0 << 8) | cb;
            xfb[out + 1] = (y1 << 8) | cr;
        }
    }

    DCFlushRange(fb, rmode->fbWidth * rmode->xfbHeight * 2);
    VIDEO_SetNextFramebuffer(fb);
    renderer->video_active = true;
    VIDEO_Flush();
    VIDEO_WaitVSync();

    renderer->current_fb = 1 - renderer->current_fb;
}

void SDL_RenderPresent(SDL_Renderer* renderer) {
    if (renderer && renderer->back_buffer && renderer->vmode) {
        copy_backbuffer_to_fb(renderer);
    }
}

int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect) {
    if (!renderer || !renderer->back_buffer) return -1;
    if (!texture || !texture->buffer) return 0;

    SDL_Rect src, dst;
    if (srcrect) {
        src = *srcrect;
    } else {
        src.x = 0; src.y = 0; src.w = texture->w; src.h = texture->h;
    }

    if (dstrect) {
        dst = *dstrect;
    } else {
        dst.x = 0; dst.y = 0; dst.w = renderer->logical_width; dst.h = renderer->logical_height;
    }

    uint32_t* tex_pixels = (uint32_t*)texture->buffer;
    uint32_t* bb_pixels = (uint32_t*)renderer->back_buffer;

    blit_rgba8(tex_pixels, texture->w, texture->h, &src,
               bb_pixels, BACKBUFFER_W, BACKBUFFER_H, &dst);

    renderer->back_buffer_dirty = true;
    return 0;
}

int SDL_RenderCopyEx(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect, double angle, const SDL_Point* center, int flip) {
    return SDL_RenderCopy(renderer, texture, srcrect, dstrect);
}

void SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    if (!renderer || !renderer->back_buffer) return;

    uint32_t color = (renderer->draw_r << 24) | (renderer->draw_g << 16) |
                     (renderer->draw_b << 8) | renderer->draw_a;

    int x = rect ? rect->x : 0;
    int y = rect ? rect->y : 0;
    int w = rect ? rect->w : BACKBUFFER_W;
    int h = rect ? rect->h : BACKBUFFER_H;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > BACKBUFFER_W) w = BACKBUFFER_W - x;
    if (y + h > BACKBUFFER_H) h = BACKBUFFER_H - y;
    if (w <= 0 || h <= 0) return;

    uint32_t* buf = (uint32_t*)renderer->back_buffer;
    for (int row = 0; row < h; row++) {
        uint32_t* line = buf + (y + row) * BACKBUFFER_W + x;
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
    renderer->back_buffer_dirty = true;
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
    tex->buffer = memalign(32, w * h * 4);
    if (!tex->buffer) {
        delete tex;
        return NULL;
    }
    memset(tex->buffer, 0, w * h * 4);
    GX_InitTexObj(&tex->texObj, tex->buffer, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    return tex;
}

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface) {
    if (!surface || !surface->pixels) return NULL;
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
    if (!tex) return NULL;
    SDL_UpdateTexture(tex, NULL, surface->pixels, surface->pitch);
    return tex;
}

void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture) {
        if (texture->buffer) {
            free(texture->buffer);
        }
        delete texture;
    }
}

int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch) {
    if (!texture || !texture->buffer || !pixels) return -1;
    const uint8_t* src = (const uint8_t*)pixels;
    uint8_t* dst = (uint8_t*)texture->buffer;
    int row_bytes = texture->w * 4;
    for (int y = 0; y < texture->h; y++) {
        memcpy(dst + y * row_bytes, src + y * pitch, row_bytes);
    }
    GX_InitTexObj(&texture->texObj, texture->buffer, texture->w, texture->h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
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
static void refresh_keyboard_state(void) {
    WPAD_ScanPads();
    PAD_ScanPads();
    memset(keyboard_state, 0, sizeof(keyboard_state));

    u16 new_gc_buttons = PAD_ButtonsHeld(0);
    if (new_gc_buttons & PAD_BUTTON_LEFT) keyboard_state[SDL_SCANCODE_LEFT] = 1;
    if (new_gc_buttons & PAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT] = 1;
    if (new_gc_buttons & PAD_BUTTON_UP) keyboard_state[SDL_SCANCODE_UP] = 1;
    if (new_gc_buttons & PAD_BUTTON_DOWN) keyboard_state[SDL_SCANCODE_DOWN] = 1;
    if (new_gc_buttons & PAD_BUTTON_A) keyboard_state[SDL_SCANCODE_RETURN] = 1;

    for (int i = 0; i < 4; i++) {
        u32 type;
        if (WPAD_Probe(i, &type) == WPAD_ERR_NONE) {
            WPADData* data = WPAD_Data(i);
            u32 new_buttons = data->btns_h;
            if (new_buttons & WPAD_BUTTON_LEFT) keyboard_state[SDL_SCANCODE_LEFT] = 1;
            if (new_buttons & WPAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT] = 1;
            if (new_buttons & WPAD_BUTTON_UP) keyboard_state[SDL_SCANCODE_UP] = 1;
            if (new_buttons & WPAD_BUTTON_DOWN) keyboard_state[SDL_SCANCODE_DOWN] = 1;
            if ((new_buttons & WPAD_BUTTON_1) || (new_buttons & WPAD_BUTTON_A)) keyboard_state[SDL_SCANCODE_RETURN] = 1;
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
#ifdef WII
    if (!f && strncmp(file, "sd:/apps/frozenbubble/share/", 27) == 0) {
        char fallback[768];
        snprintf(fallback, sizeof(fallback), "/home/davey/frozenbubble/frozen-bubble-sdl2/share/%s", file + 27);
        f = fopen(fallback, "rb");
    }
#endif
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

int SDL_PollEvent(SDL_Event* event) {
    if (!event) return 0;
    
    refresh_keyboard_state();
    
    static u32 held_buttons = 0;
    static u16 gc_held_buttons = 0;
    u32 new_buttons = 0;
    u16 new_gc_buttons = PAD_ButtonsHeld(0);

    if (new_gc_buttons & PAD_BUTTON_LEFT) keyboard_state[SDL_SCANCODE_LEFT] = 1;
    if (new_gc_buttons & PAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT] = 1;
    if (new_gc_buttons & PAD_BUTTON_UP) keyboard_state[SDL_SCANCODE_UP] = 1;
    if (new_gc_buttons & PAD_BUTTON_DOWN) keyboard_state[SDL_SCANCODE_DOWN] = 1;
    if (new_gc_buttons & PAD_BUTTON_A) {
        keyboard_state[SDL_SCANCODE_RETURN] = 1;
    }

    for (int i = 0; i < 4; i++) {
        u32 type;
        if (WPAD_Probe(i, &type) == WPAD_ERR_NONE) {
            WPADData* data = WPAD_Data(i);
            new_buttons = data->btns_h;

            if (new_buttons & WPAD_BUTTON_LEFT) keyboard_state[SDL_SCANCODE_LEFT] = 1;
            if (new_buttons & WPAD_BUTTON_RIGHT) keyboard_state[SDL_SCANCODE_RIGHT] = 1;
            if (new_buttons & WPAD_BUTTON_UP) keyboard_state[SDL_SCANCODE_UP] = 1;
            if (new_buttons & WPAD_BUTTON_DOWN) keyboard_state[SDL_SCANCODE_DOWN] = 1;
            if ((new_buttons & WPAD_BUTTON_1) || (new_buttons & WPAD_BUTTON_A)) keyboard_state[SDL_SCANCODE_RETURN] = 1;

            if ((new_buttons & (WPAD_BUTTON_1 | WPAD_BUTTON_A)) && !(held_buttons & (WPAD_BUTTON_1 | WPAD_BUTTON_A))) {
                event->type = SDL_KEYDOWN;
                event->key.keysym.sym = SDLK_RETURN;
                event->key.keysym.scancode = SDL_SCANCODE_RETURN;
                event->key.state = SDL_PRESSED;
                held_buttons = new_buttons;
                return 1;
            }
            if ((new_buttons & WPAD_BUTTON_UP) && !(held_buttons & WPAD_BUTTON_UP)) {
                event->type = SDL_KEYDOWN;
                event->key.keysym.sym = SDLK_UP;
                event->key.state = SDL_PRESSED;
                held_buttons = new_buttons;
                return 1;
            }
            if ((new_buttons & WPAD_BUTTON_DOWN) && !(held_buttons & WPAD_BUTTON_DOWN)) {
                event->type = SDL_KEYDOWN;
                event->key.keysym.sym = SDLK_DOWN;
                event->key.state = SDL_PRESSED;
                held_buttons = new_buttons;
                return 1;
            }
            if ((new_buttons & WPAD_BUTTON_LEFT) && !(held_buttons & WPAD_BUTTON_LEFT)) {
                event->type = SDL_KEYDOWN;
                event->key.keysym.sym = SDLK_LEFT;
                event->key.state = SDL_PRESSED;
                held_buttons = new_buttons;
                return 1;
            }
            if ((new_buttons & WPAD_BUTTON_RIGHT) && !(held_buttons & WPAD_BUTTON_RIGHT)) {
                event->type = SDL_KEYDOWN;
                event->key.keysym.sym = SDLK_RIGHT;
                event->key.state = SDL_PRESSED;
                held_buttons = new_buttons;
                return 1;
            }
            if ((new_buttons & WPAD_BUTTON_HOME) && !(held_buttons & WPAD_BUTTON_HOME)) {
                event->type = SDL_QUIT;
                return 1;
            }
        }
    }

    if ((new_gc_buttons & PAD_BUTTON_A) && !(gc_held_buttons & PAD_BUTTON_A)) {
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_RETURN;
        event->key.keysym.scancode = SDL_SCANCODE_RETURN;
        event->key.state = SDL_PRESSED;
        gc_held_buttons = new_gc_buttons;
        return 1;
    }
    if ((new_gc_buttons & PAD_BUTTON_UP) && !(gc_held_buttons & PAD_BUTTON_UP)) {
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_UP;
        event->key.keysym.scancode = SDL_SCANCODE_UP;
        event->key.state = SDL_PRESSED;
        gc_held_buttons = new_gc_buttons;
        return 1;
    }
    if ((new_gc_buttons & PAD_BUTTON_DOWN) && !(gc_held_buttons & PAD_BUTTON_DOWN)) {
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_DOWN;
        event->key.keysym.scancode = SDL_SCANCODE_DOWN;
        event->key.state = SDL_PRESSED;
        gc_held_buttons = new_gc_buttons;
        return 1;
    }
    if ((new_gc_buttons & PAD_BUTTON_LEFT) && !(gc_held_buttons & PAD_BUTTON_LEFT)) {
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_LEFT;
        event->key.keysym.scancode = SDL_SCANCODE_LEFT;
        event->key.state = SDL_PRESSED;
        gc_held_buttons = new_gc_buttons;
        return 1;
    }
    if ((new_gc_buttons & PAD_BUTTON_RIGHT) && !(gc_held_buttons & PAD_BUTTON_RIGHT)) {
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_RIGHT;
        event->key.keysym.scancode = SDL_SCANCODE_RIGHT;
        event->key.state = SDL_PRESSED;
        gc_held_buttons = new_gc_buttons;
        return 1;
    }
    
    held_buttons = new_buttons;
    gc_held_buttons = new_gc_buttons;
    return 0;
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
    blit_surface_rgba8((uint32_t*)src->pixels, src->w, src->h, srcrect,
                       (uint32_t*)dst->pixels, dst->w, dst->h, dstrect);
    return 0;
}

int SDL_BlitScaled(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
    return SDL_BlitSurface(src, srcrect, dst, dstrect);
}

const uint8_t* SDL_GetKeyboardState(int* numkeys) {
    refresh_keyboard_state();
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
