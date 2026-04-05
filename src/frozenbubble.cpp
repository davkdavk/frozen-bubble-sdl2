#include "frozenbubble.h"

#ifdef WII
#include <stdio.h>
#include <unistd.h>
#include <gccore.h>
#endif

#define FB_DEBUG_STEP(msg) do { } while (0)

FrozenBubble *FrozenBubble::ptrInstance = NULL;

const char *formatTime(int time){
    int h = int(time/3600.0);
    int m = int((time-h*3600)/60.0);
    int s = int((time-h*3600)-(m*60));

    char *fm = new char[128];
    if (h > 0) sprintf(fm, "%dh ", h);
    if (m > 0) {
        if (h > 0) sprintf(fm + strlen(fm), "%02dm ", m);
        else sprintf(fm, "%dm ", m);
    }
    if (s > 0) {
        if (m > 0) sprintf(fm + strlen(fm), "%02ds", s);
        else sprintf(fm, "%ds", s); 
    }
    return fm;
}

FrozenBubble *FrozenBubble::Instance()
{
    if(ptrInstance == NULL)
        ptrInstance = new FrozenBubble();
    return ptrInstance;
}

FrozenBubble::FrozenBubble() : IsGameQuit(false), IsGamePause(false), window(nullptr), renderer(nullptr), gameOptions(nullptr), audMixer(nullptr), mainMenu(nullptr), mainGame(nullptr), hiscoreManager(nullptr) {
    FB_DEBUG_STEP("FB ctor: settings");
    gameOptions = GameSettings::Instance();
    gameOptions->ReadSettings();

    SDL_Point resolution = gameOptions->curResolution();
    Uint32 fullscreen = gameOptions->fullscreenMode() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

    FB_DEBUG_STEP("FB ctor: window");
    window = SDL_CreateWindow("Frozen-Bubble: SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, resolution.x, resolution.y, fullscreen);
    if(gameOptions->linearScaling) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    if(!window) {
        IsGameQuit = true;
        std::cout << "Failed to create window: " << SDL_GetError() << std::endl;
    }

    FB_DEBUG_STEP("FB ctor: icon");
    SDL_Surface *icon = SDL_LoadBMP(DATA_DIR "/gfx/pinguins/window_icon_penguin.bmp");
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);

    FB_DEBUG_STEP("FB ctor: renderer");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, 640, 480);

    if(!renderer) {
        IsGameQuit = true;
        std::cout << "Failed to create renderer: " << SDL_GetError() << std::endl;
    }

    FB_DEBUG_STEP("FB ctor: ttf");
    if( TTF_Init() == -1 )
    {
        IsGameQuit = true;
        std::cout << "Failed to initialise SDL_ttf: " << SDL_GetError() << std::endl;
    }

    FB_DEBUG_STEP("FB ctor: audio");
    audMixer = AudioMixer::Instance();
    FB_DEBUG_STEP("FB ctor: highscores");
    hiscoreManager = HighscoreManager::Instance(renderer);

    FB_DEBUG_STEP("FB ctor: effects");
    init_effects((char*)DATA_DIR);
    FB_DEBUG_STEP("FB ctor: mainmenu");
    mainMenu = new MainMenu(renderer);
    FB_DEBUG_STEP("FB ctor: bubblegame");
    mainGame = new BubbleGame(renderer);

    FB_DEBUG_STEP("FB ctor: done");

}

FrozenBubble::~FrozenBubble() {
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if(window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    hiscoreManager->Dispose();
    audMixer->Dispose();
    gameOptions->Dispose();

    TTF_Quit();
    IMG_Quit();
    Mix_Quit();
    SDL_Quit();
}

uint8_t FrozenBubble::RunForEver()
{
    // on init, try playing one of these songs depending on the current state:
#ifndef WII
    if(currentState == TitleScreen) audMixer->PlayMusic("intro");
#endif
    //else if (currentState == MainGame) mainGame->NewGame({false, 1, false});

    float framerate = 60;
#ifndef WII
    float frametime = 1/framerate * 1000;
#else
    (void)framerate;
#endif

    unsigned int ticks = SDL_GetTicks(), lasttick = ticks;
#ifndef WII
    float elapsed = 0;
#endif

    while(!IsGameQuit) {
        lasttick = ticks;
        ticks = SDL_GetTicks();
#ifndef WII
        elapsed = ticks - lasttick;
#endif

        // handle input
        SDL_Event e;
        while (SDL_PollEvent (&e)) {
            HandleInput(&e);
        }

        // render
        if(!IsGamePause) {
            SDL_RenderClear(renderer);
            if (currentState == TitleScreen) mainMenu->Render();
            else if (currentState == MainGame) mainGame->Render();
            else if (currentState == Highscores) {
                if (hiscoreManager->lastState == 1) mainGame->Render();
                hiscoreManager->RenderScoreScreen();
            }
            SDL_RenderPresent(renderer);
        }
        else {
            if (currentState == MainGame){
                mainGame->RenderPaused();
                SDL_RenderPresent(renderer);
            }
        }
        // On Wii, VIDEO_WaitVSync() inside SDL_RenderPresent already paces the
        // frame to ~16.7ms. Adding SDL_Delay on top doubles the frame time to
        // ~32ms (31fps). Skip the delay on Wii and let vsync do the pacing.
#ifndef WII
        if(elapsed < frametime) {
            SDL_Delay(frametime - elapsed);
        }
#endif
    }
    if (startTime != 0) addictedTime += SDL_GetTicks() - startTime;
    if(addictedTime != 0) printf("Addicted for %s, %d bubbles were launched.", formatTime(addictedTime/1000), totalBubbles);
#ifdef WII
    // On Wii, return safely to HBC rather than invoking the destructor and
    // going through the C++ atexit/exit path, which triggers ISI crashes when
    // libogc callbacks fire against already-freed GX/video memory.
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
    // Should not be reached, but satisfy the compiler:
    return 0;
#endif
    this->~FrozenBubble();
    return 0;
}

void FrozenBubble::HandleInput(SDL_Event *e) {
    switch(e->type) {
        case SDL_WINDOWEVENT:
            switch (e->window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                {
                    IsGameQuit = true;
                    break;
                }
            }
            break;
        case SDL_KEYDOWN:
            if(e->key.repeat) break;
            switch(e->key.keysym.sym) {
                case SDLK_F12:
                    gameOptions->SetValue("GFX:Fullscreen", "");
                    SDL_SetWindowFullscreen(window, gameOptions->fullscreenMode() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    break;
                case SDLK_PAUSE:
                    CallGamePause();
                    if (currentState == MainGame) {
                        if (!mainGame->playedPause) mainGame->playedPause = false;
                    }
                    break;
                case SDLK_ESCAPE:
                    // Home button: return to main menu from anywhere; quit from title screen.
                    if (currentState == MainGame) {
                        CallMenuReturn();
                    } else {
                        IsGameQuit = true;
                    }
                    break;
            }
            break;
    }

    if (IsGamePause) return;
    if(currentState == Highscores) {
        hiscoreManager->HandleInput(e);
        return;
    }
    if(currentState == TitleScreen) mainMenu->HandleInput(e);
    if(currentState == MainGame) mainGame->HandleInput(e);
}
