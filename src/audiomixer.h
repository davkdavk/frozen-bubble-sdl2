#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <SDL2/SDL.h>
#ifndef WII
#include <SDL2/SDL_mixer.h>
#endif
#include <map>
#include <string>
#include <vector>

#include "gamesettings.h"

#ifdef WII
struct SfxBuf; // defined in audiomixer_wii.cpp
#endif

class AudioMixer final
{
public:
    void PlayMusic(const char *track);
    void StopMusic();
    void PlaySFX(const char *sfx);
    void PauseMusic(bool enable = false);
    void MuteAll(bool enable = false);
    bool IsHalted() { return haltedMixer; };
    // Call once per frame from main loop to keep music stream filled (Wii only)
    void PumpMusic();

    AudioMixer(const AudioMixer& obj) = delete;
    void Dispose();
    static AudioMixer* Instance();
private:
#ifdef WII
    SfxBuf* GetSFXBuf(const char *);
#else
    std::map<const char *, Mix_Chunk *> sfxFiles;
    Mix_Chunk* GetSFX(const char *);
#endif

    AudioMixer();
    ~AudioMixer();
    static AudioMixer* ptrInstance;
    GameSettings* gameSettings;

    bool mixerEnabled = true, haltedMixer = false;
#ifndef WII
    Mix_Music* curMusic;
#endif
};

#endif // AUDIOMIXER_H