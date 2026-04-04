#include <gccore.h>
#include <wiiuse/wpad.h>
#include <string.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    VIDEO_Init();
    WPAD_Init();
    
    rmode = VIDEO_GetPreferredMode(NULL);
    
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode == VI_PAL) VIDEO_WaitVSync();
    
    printf("\n\nHello Wii!\n");
    printf("If you see this, display works.\n");
    
    void *fb2 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    u16 *pixels = (u16 *)xfb;
    for (u32 i = 0; i < rmode->fbWidth * rmode->xfbHeight; i++) {
        pixels[i] = 0x001F;
    }
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    
    while (1) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
        VIDEO_WaitVSync();
    }
    
    return 0;
}
