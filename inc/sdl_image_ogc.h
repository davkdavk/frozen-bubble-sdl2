#ifndef SDL_IMAGE_OGC_H
#define SDL_IMAGE_OGC_H

#include <SDL.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_Init(x) 0
#define IMG_Quit()

SDL_Surface* IMG_Load(const char* file);
SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file);

int IMG_isPNG(SDL_RWops* src);
int IMG_isJPG(SDL_RWops* src);
int IMG_isGIF(SDL_RWops* src);
int IMG_isBMP(SDL_RWops* src);

typedef struct SDL_RWops {
    int (*seek)(struct SDL_RWops* context, int offset, int whence);
    int (*read)(struct SDL_RWops* context, void* ptr, int size, int maxnum);
    int (*write)(struct SDL_RWops* context, const void* ptr, int size, int num);
    void (*close)(struct SDL_RWops* context);
    uint32_t type;
    void *hidden;
} SDL_RWops;

SDL_RWops* SDL_RWFromFile(const char* file, const char* mode);

#ifdef __cplusplus
}
#endif

#endif
