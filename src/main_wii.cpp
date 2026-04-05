#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <gccore.h>

#include "frozenbubble.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

// Convert a single RGB triplet to Y (BT.601 full-range).
static inline u8 rgb_to_y(u8 r, u8 g, u8 b) {
    int y = 16 + (66*r + 129*g + 25*b + 128) / 256;
    if (y < 16)  y = 16;
    if (y > 235) y = 235;
    return (u8)y;
}
static inline u8 rgb_to_cb(u8 r, u8 g, u8 b) {
    int cb = 128 + (-38*r - 74*g + 112*b + 128) / 256;
    if (cb < 16)  cb = 16;
    if (cb > 240) cb = 240;
    return (u8)cb;
}
static inline u8 rgb_to_cr(u8 r, u8 g, u8 b) {
    int cr = 128 + (112*r - 94*g - 18*b + 128) / 256;
    if (cr < 16)  cr = 16;
    if (cr > 240) cr = 240;
    return (u8)cr;
}

// Blit an RGBA image (header-stripped, w*h*4 bytes) into the YUY2 XFB.
// dst_x/dst_y: top-left destination in the XFB (must be even x).
// XFB stride: fbWidth pixels, 2 bytes/pixel, packed as pairs [Y0 Cb Y1 Cr].
static void blit_rgba_to_xfb(const u8 *rgba, int img_w, int img_h,
                               int dst_x, int dst_y, int xfb_w, int xfb_h)
{
    // clamp to XFB bounds
    int blit_w = img_w; if (dst_x + blit_w > xfb_w) blit_w = xfb_w - dst_x;
    int blit_h = img_h; if (dst_y + blit_h > xfb_h) blit_h = xfb_h - dst_y;
    if (blit_w <= 0 || blit_h <= 0) return;

    u32 *fb = (u32*)xfb;
    // XFB row stride in u32 words (2 pixels per word)
    int stride32 = xfb_w / 2;

    for (int y = 0; y < blit_h; y++) {
        const u8 *src_row = rgba + y * img_w * 4;
        u32 *dst_row = fb + (dst_y + y) * stride32 + dst_x / 2;
        // process pairs of pixels
        int x = 0;
        for (; x + 1 < blit_w; x += 2) {
            u8 r0 = src_row[x*4+0], g0 = src_row[x*4+1], b0 = src_row[x*4+2], a0 = src_row[x*4+3];
            u8 r1 = src_row[x*4+4], g1 = src_row[x*4+5], b1 = src_row[x*4+6], a1 = src_row[x*4+7];
            // alpha-blend over black (background is already black/image)
            u8 y0, cb, y1, cr;
            if (a0 == 255) {
                y0 = rgb_to_y(r0,g0,b0);
                cb = rgb_to_cb(r0,g0,b0);
            } else {
                int ra = r0*a0/255, ga = g0*a0/255, ba = b0*a0/255;
                y0 = rgb_to_y(ra,ga,ba);
                cb = rgb_to_cb(ra,ga,ba);
            }
            if (a1 == 255) {
                y1 = rgb_to_y(r1,g1,b1);
                cr = rgb_to_cr(r1,g1,b1);
            } else {
                int ra = r1*a1/255, ga = g1*a1/255, ba = b1*a1/255;
                y1 = rgb_to_y(ra,ga,ba);
                cr = rgb_to_cr(ra,ga,ba);
            }
            dst_row[x/2] = ((u32)y0 << 24) | ((u32)cb << 16) | ((u32)y1 << 8) | cr;
        }
        // odd trailing pixel (img_w is odd — shouldn't happen for these assets but be safe)
        if (x < blit_w) {
            u8 r0 = src_row[x*4+0], g0 = src_row[x*4+1], b0 = src_row[x*4+2], a0 = src_row[x*4+3];
            u8 y0 = a0==255 ? rgb_to_y(r0,g0,b0) : rgb_to_y(r0*a0/255,g0*a0/255,b0*a0/255);
            u8 cb = a0==255 ? rgb_to_cb(r0,g0,b0) : rgb_to_cb(r0*a0/255,g0*a0/255,b0*a0/255);
            u32 old = dst_row[x/2];
            dst_row[x/2] = ((u32)y0 << 24) | ((u32)cb << 16) | (old & 0x0000FFFFu);
        }
    }
}

// Load an .rgba file (8-byte header: w,h big-endian u32, then w*h*4 RGBA bytes).
// Returns malloc'd pixel data (caller must free), sets *out_w/*out_h.
// Returns NULL on failure.
static u8 *load_rgba_file(const char *path, int *out_w, int *out_h) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    u8 hdr[8];
    if (fread(hdr, 1, 8, f) != 8) { fclose(f); return NULL; }
    int w = (hdr[0]<<24)|(hdr[1]<<16)|(hdr[2]<<8)|hdr[3];
    int h = (hdr[4]<<24)|(hdr[5]<<16)|(hdr[6]<<8)|hdr[7];
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) { fclose(f); return NULL; }
    size_t sz = (size_t)w * h * 4;
    u8 *data = (u8*)malloc(sz);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, sz, f) != sz) { free(data); fclose(f); return NULL; }
    fclose(f);
    *out_w = w; *out_h = h;
    return data;
}

// Fill the entire XFB with a YUY2 colour word (e.g. 0x00800080 = black).
static void fill_xfb(u32 yuy2_word) {
    u32 *p = (u32*)xfb;
    u32 n = (rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ) / 4;
    for (u32 i = 0; i < n; i++) p[i] = yuy2_word;
}

// Draw the loading splash into the XFB: back_intro.png.rgba as background,
// loading.png.rgba label centred near the bottom.
// Falls back to a deep-blue screen if assets are missing so we never show black.
static void show_loading_screen(void) {
    int xfb_w = rmode->fbWidth;
    int xfb_h = 480; // visible lines

    // Fallback: deep blue — Y=29, Cb=145, Cr=63  (R=0,G=0,B=128)
    // Packed as YUY2 word: [Y0 Cb Y1 Cr]
    fill_xfb(0x1D911D3F);

    // Background: back_intro.png — 640x480, blitted at (0,0).
    {
        int w, h;
        u8 *px = load_rgba_file("sd:/apps/frozenbubble/share/gfx/intro/back_intro.png.rgba", &w, &h);
        if (px) {
            // Centre horizontally in case XFB width differs (unlikely, but safe).
            int dx = (xfb_w - w) / 2;
            if (dx < 0) dx = 0;
            blit_rgba_to_xfb(px, w, h, dx & ~1, 0, xfb_w, xfb_h);
            free(px);
        }
    }

    // Loading label: loading.png — 99x24, centred horizontally, 20px from bottom.
    {
        int w, h;
        u8 *px = load_rgba_file("sd:/apps/frozenbubble/share/gfx/loading.png.rgba", &w, &h);
        if (px) {
            int dx = ((xfb_w - w) / 2) & ~1;
            int dy = xfb_h - h - 20;
            if (dy < 0) dy = 0;
            blit_rgba_to_xfb(px, w, h, dx, dy, xfb_w, xfb_h);
            free(px);
        }
    }

    DCFlushRange(xfb, rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

extern "C" {
    int main(int argc, char** argv);
    void* __memalign_aligned(size_t align, size_t size);
    void __exception_closeall(int exception);
}

// Try to mount the SD card.  fatInitDefault() can fail on real hardware if the
// SD slot is not yet ready when main() is entered (IOS startup latency).
// We retry up to 10 times with a ~50 ms busy-wait between attempts, then fall
// back to a direct fatMountSimple("sd", &__io_wiisd) in case the default probe
// skipped the slot for some reason.
static void mount_sd(void) {
    for (int i = 0; i < 10; i++) {
        if (fatInitDefault()) return;
        // busy-wait ~50 ms (Wii bus ~243 MHz, TB ticks at bus/4 = ~60.75 MHz)
        u64 end = gettime() + millisecs_to_ticks(50);
        while (gettime() < end) {}
    }
    // Last resort: mount the internal SD slot directly.
    fatMountSimple("sd", &__io_wiisd);
}

int main(int argc, char **argv) {
    SYS_SetPowerCallback(NULL);
    SYS_SetResetCallback(NULL);

    VIDEO_Init();
    WPAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    // YUY2: Y=0,Cb=0,Cr=0 decodes to green in BT.601 — use 0x00800080 for true black.
    {
        u32 *p = (u32*)xfb;
        u32 n = (rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ) / 4;
        for (u32 i = 0; i < n; i++) p[i] = 0x00800080;
    }
    DCFlushRange(xfb, rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    mount_sd();
    show_loading_screen();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    GameSettings::Instance()->ReadSettings();
    FrozenBubble *game = FrozenBubble::Instance();
    return game->RunForEver();
}

extern "C" void* __memalign_aligned(size_t align, size_t size) {
    static char buffer[256 * 1024] __attribute__((aligned(32)));
    return buffer;
}

void __exception_closeall(int exception) {
}
