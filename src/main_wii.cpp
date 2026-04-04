#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>
#include <gccore.h>

#include "frozenbubble.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

extern "C" {
    int main(int argc, char** argv);
    void* __memalign_aligned(size_t align, size_t size);
    void __exception_closeall(int exception);
}

static void debug_step(const char* message) {
    printf("%s\n", message);
    fflush(stdout);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();
    usleep(500000);
}

int main(int argc, char **argv) {
    SYS_SetPowerCallback(NULL);
    SYS_SetResetCallback(NULL);

    VIDEO_Init();
    WPAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    printf("\x1b[2;0H");
    debug_step("Hello from game port");
    debug_step("Step 1: FAT init");
    printf("fatInitDefault=%d\n", fatInitDefault() ? 1 : 0);
    fflush(stdout);

    debug_step("Step 2: SDL_Init");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

    debug_step("Step 3: GameSettings");
    GameSettings::Instance()->ReadSettings();

    debug_step("Step 4: FrozenBubble::Instance");
    FrozenBubble *game = FrozenBubble::Instance();

    debug_step("Step 5: RunForEver");
    return game->RunForEver();

    while(1) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);
        VIDEO_WaitVSync();
    }

    return 0;
}

extern "C" void* __memalign_aligned(size_t align, size_t size) {
    static char buffer[256 * 1024] __attribute__((aligned(32)));
    return buffer;
}

void __exception_closeall(int exception) {
}
