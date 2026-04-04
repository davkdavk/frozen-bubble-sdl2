#include "wii_input.h"
#include <ogc/lwp_queue.h>

WiiInput* WiiInput::ptrInstance = NULL;

WiiInput* WiiInput::Instance() {
    if(ptrInstance == NULL)
        ptrInstance = new WiiInput();
    return ptrInstance;
}

WiiInput::WiiInput() 
    : wii_initialized(false), connected_controllers(0), held_buttons(0), held_buttons_prev(0), wiimote_type(0), last_key(SDLK_UNKNOWN) {
}

WiiInput::~WiiInput() {
    Shutdown();
}

void WiiInput::Init() {
    if (wii_initialized) return;
    
    WPAD_Init();
    wii_initialized = true;
}

void WiiInput::Shutdown() {
    if (!wii_initialized) return;
    
    WPAD_Shutdown();
    wii_initialized = false;
}

void WiiInput::Poll() {
    if (!wii_initialized) return;
    
    WPAD_ScanPads();
    
    held_buttons_prev = held_buttons;
    held_buttons = 0;
    connected_controllers = 0;
    
    for (int i = 0; i < 4; i++) {
        u32 type;
        if (WPAD_Probe(i, &type) == WPAD_ERR_NONE) {
            connected_controllers++;
            WPADData* data = WPAD_Data(i);
            held_buttons |= data->btns_h;
            
            if (data->exp.type == WPAD_EXP_NUNCHUK) {
                wiimote_type = WPAD_EXP_NUNCHUK;
            } else if (data->exp.type == WPAD_EXP_CLASSIC) {
                wiimote_type = WPAD_EXP_CLASSIC;
            }
        }
    }
}

bool WiiInput::IsButtonPressed(WiiButton btn) {
    if (!wii_initialized) return false;
    
    switch (btn) {
        case WII_UP:       return (held_buttons & WPAD_BUTTON_UP);
        case WII_DOWN:     return (held_buttons & WPAD_BUTTON_DOWN);
        case WII_LEFT:     return (held_buttons & WPAD_BUTTON_LEFT);
        case WII_RIGHT:    return (held_buttons & WPAD_BUTTON_RIGHT);
        case WII_A:        return (held_buttons & WPAD_BUTTON_A);
        case WII_B:        return (held_buttons & WPAD_BUTTON_B);
        case WII_PLUS:     return (held_buttons & WPAD_BUTTON_PLUS);
        case WII_MINUS:    return (held_buttons & WPAD_BUTTON_MINUS);
        case WII_HOME:     return (held_buttons & WPAD_BUTTON_HOME);
        case WII_1:        return (held_buttons & WPAD_BUTTON_1);
        case WII_2:        return (held_buttons & WPAD_BUTTON_2);
        case WII_NUNCHUK_Z: return (held_buttons & WPAD_NUNCHUK_BUTTON_Z);
        case WII_NUNCHUK_C: return (held_buttons & WPAD_NUNCHUK_BUTTON_C);
        case WII_CLASSIC_A: return (held_buttons & WPAD_CLASSIC_BUTTON_A);
        case WII_CLASSIC_B: return (held_buttons & WPAD_CLASSIC_BUTTON_B);
        case WII_CLASSIC_X: return (held_buttons & WPAD_CLASSIC_BUTTON_X);
        case WII_CLASSIC_Y: return (held_buttons & WPAD_CLASSIC_BUTTON_Y);
        case WII_CLASSIC_ZL: return (held_buttons & WPAD_CLASSIC_BUTTON_ZL);
        case WII_CLASSIC_ZR: return (held_buttons & WPAD_CLASSIC_BUTTON_ZR);
        default: return false;
    }
}

bool WiiInput::IsButtonJustPressed(WiiButton btn) {
    if (!wii_initialized) return false;
    
    u32 current = held_buttons;
    u32 prev = held_buttons_prev;
    
    switch (btn) {
        case WII_UP:       return (current & WPAD_BUTTON_UP) && !(prev & WPAD_BUTTON_UP);
        case WII_DOWN:     return (current & WPAD_BUTTON_DOWN) && !(prev & WPAD_BUTTON_DOWN);
        case WII_LEFT:     return (current & WPAD_BUTTON_LEFT) && !(prev & WPAD_BUTTON_LEFT);
        case WII_RIGHT:    return (current & WPAD_BUTTON_RIGHT) && !(prev & WPAD_BUTTON_RIGHT);
        case WII_A:        return (current & WPAD_BUTTON_A) && !(prev & WPAD_BUTTON_A);
        case WII_B:        return (current & WPAD_BUTTON_B) && !(prev & WPAD_BUTTON_B);
        case WII_PLUS:     return (current & WPAD_BUTTON_PLUS) && !(prev & WPAD_BUTTON_PLUS);
        case WII_MINUS:    return (current & WPAD_BUTTON_MINUS) && !(prev & WPAD_BUTTON_MINUS);
        case WII_HOME:     return (current & WPAD_BUTTON_HOME) && !(prev & WPAD_BUTTON_HOME);
        case WII_1:        return (current & WPAD_BUTTON_1) && !(prev & WPAD_BUTTON_1);
        case WII_2:        return (current & WPAD_BUTTON_2) && !(prev & WPAD_BUTTON_2);
        default: return false;
    }
}

bool WiiInput::IsDPadUp() { return IsButtonPressed(WII_UP); }
bool WiiInput::IsDPadDown() { return IsButtonPressed(WII_DOWN); }
bool WiiInput::IsDPadLeft() { return IsButtonPressed(WII_LEFT); }
bool WiiInput::IsDPadRight() { return IsButtonPressed(WII_RIGHT); }
bool WiiInput::IsA() { return IsButtonPressed(WII_A); }
bool WiiInput::IsB() { return IsButtonPressed(WII_B); }
bool WiiInput::IsPlus() { return IsButtonPressed(WII_PLUS); }
bool WiiInput::IsMinus() { return IsButtonPressed(WII_MINUS); }
bool WiiInput::IsHome() { return IsButtonPressed(WII_HOME); }
bool WiiInput::Is1() { return IsButtonPressed(WII_1); }
bool WiiInput::Is2() { return IsButtonPressed(WII_2); }

bool WiiInput::IsMenuUp() { return IsDPadUp() || Is1(); }
bool WiiInput::IsMenuDown() { return IsDPadDown() || Is2(); }
bool WiiInput::IsMenuLeft() { return IsDPadLeft(); }
bool WiiInput::IsMenuRight() { return IsDPadRight(); }
bool WiiInput::IsMenuSelect() { return IsA() || IsPlus(); }
bool WiiInput::IsMenuBack() { return IsB() || IsMinus(); }
bool WiiInput::IsPause() { return IsHome(); }

bool WiiInput::IsAimLeft() { return IsDPadLeft(); }
bool WiiInput::IsAimRight() { return IsDPadRight(); }
bool WiiInput::IsFire() { return IsA() || Is1(); }

SDL_Keycode WiiInput::GetMappedKey() {
    if (IsMenuUp()) return SDLK_UP;
    if (IsMenuDown()) return SDLK_DOWN;
    if (IsMenuLeft()) return SDLK_LEFT;
    if (IsMenuRight()) return SDLK_RIGHT;
    if (IsMenuSelect()) return SDLK_RETURN;
    if (IsMenuBack()) return SDLK_ESCAPE;
    if (IsPause()) return SDLK_PAUSE;
    if (IsButtonJustPressed(WII_A)) {
        last_key = SDLK_RETURN;
        return SDLK_RETURN;
    }
    if (IsButtonJustPressed(WII_B)) {
        last_key = SDLK_ESCAPE;
        return SDLK_ESCAPE;
    }
    if (IsButtonJustPressed(WII_HOME)) {
        last_key = SDLK_ESCAPE;
        return SDLK_ESCAPE;
    }
    return SDLK_UNKNOWN;
}
