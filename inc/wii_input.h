#ifndef WII_INPUT_H
#define WII_INPUT_H

#include <ogc/lwp_queue.h>
#include <wiiuse/wpad.h>
#include "sdl_ogc_shim.h"

enum WiiButton {
    WII_NONE = 0,
    WII_UP,
    WII_DOWN,
    WII_LEFT,
    WII_RIGHT,
    WII_A,
    WII_B,
    WII_PLUS,
    WII_MINUS,
    WII_HOME,
    WII_1,
    WII_2,
    WII_NUNCHUK_C,
    WII_NUNCHUK_Z,
    WII_CLASSIC_A,
    WII_CLASSIC_B,
    WII_CLASSIC_X,
    WII_CLASSIC_Y,
    WII_CLASSIC_ZL,
    WII_CLASSIC_ZR,
    WII_CLASSIC_L,
    WII_CLASSIC_R,
    WII_CLASSIC_PLUS,
    WII_CLASSIC_MINUS,
    WII_CLASSIC_HOME
};

class WiiInput {
public:
    static WiiInput* Instance();
    
    void Init();
    void Shutdown();
    void Poll();
    
    bool IsButtonPressed(WiiButton btn);
    bool IsButtonJustPressed(WiiButton btn);
    
    bool IsDPadUp();
    bool IsDPadDown();
    bool IsDPadLeft();
    bool IsDPadRight();
    bool IsA();
    bool IsB();
    bool IsPlus();
    bool IsMinus();
    bool IsHome();
    bool Is1();
    bool Is2();
    
    bool IsMenuUp();
    bool IsMenuDown();
    bool IsMenuLeft();
    bool IsMenuRight();
    bool IsMenuSelect();
    bool IsMenuBack();
    bool IsPause();
    
    bool IsAimLeft();
    bool IsAimRight();
    bool IsFire();
    
    SDL_Keycode GetMappedKey();
    
    int connected_controllers;
    u32 held_buttons;
    u32 held_buttons_prev;
    s32 wiimote_type;
    
private:
    WiiInput();
    ~WiiInput();
    static WiiInput* ptrInstance;
    
    bool wii_initialized;
    
    SDL_Keycode last_key;
};

#endif
