#ifndef AUDIOMIXER_OGC_H
#define AUDIOMIXER_OGC_H

#include <asndlib.h>
#include <mad.h>
#include <string>
#include <map>

#define Mix_Chunk void
#define Mix_Music void

class AudioMixerOgc {
public:
    static AudioMixerOgc* Instance();
    
    void Init();
    void Shutdown();
    void PlaySFX(const char* sfx);
    void PlayMusic(const char* track);
    void StopMusic();
    void PauseMusic(bool enable);
    void MuteAll(bool enable);
    void SetVolume(int volume);
    
private:
    AudioMixerOgc();
    ~AudioMixerOgc();
    static AudioMixerOgc* ptrInstance;
    
    bool initialized;
    bool music_playing;
    bool sfx_enabled;
    bool music_enabled;
    int volume;
    
    std::map<std::string, void*> loaded_sfx;
};

#define MIX_DEFAULT_FREQUENCY 48000
#define MIX_DEFAULT_FORMAT 0
#define MIX_CHANNELS 16

#define Mix_FadingMusic() false
#define Mix_PlayingMusic() false
#define Mix_FadeOutMusic(x) 0
#define Mix_PlayMusic(x, y) 0
#define Mix_HaltMusic() 0
#define Mix_HaltChannel(x) 0
#define Mix_OpenAudio(a, b, c, d) 0
#define Mix_CloseAudio() 
#define Mix_FreeChunk(x)
#define Mix_FreeMusic(x)
#define Mix_Volume(x, y) 100

#define Mix_LoadWAV(x) NULL
#define Mix_LoadMUS(x) NULL
#define Mix_PlayChannel(a, b, c) -1

#define Mix_ResumeMusic()
#define Mix_PauseMusic()

int Mix_OpenAudio(int freq, int format, int channels, int chunksize);
void Mix_CloseAudio(void);
Mix_Chunk* Mix_LoadWAV(const char* file);
void Mix_FreeChunk(Mix_Chunk* chunk);
int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops);
void Mix_HaltChannel(int channel);

#endif
