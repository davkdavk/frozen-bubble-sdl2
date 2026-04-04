#ifndef SDL_OGC_IMPL_H
#define SDL_OGC_IMPL_H

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

typedef struct SDL_Renderer {
    int width;
    int height;
    int logical_width;
    int logical_height;
    uint8_t draw_r, draw_g, draw_b, draw_a;
    int blend_mode;
    GXRModeObj* vmode;
    void* fifo_buffer;
    void* frame_buffer[2];
    int current_fb;
    bool video_active;
    void* back_buffer;
    GXTexObj back_buffer_tex;
    bool back_buffer_dirty;
} SDL_Renderer;

typedef struct SDL_Texture {
    uint32_t format;
    int access;
    int w;
    int h;
    int blend_mode;
    int texId;
    SDL_Renderer* renderer;
    GXTexObj texObj;
    void* buffer;
} SDL_Texture;

#endif
