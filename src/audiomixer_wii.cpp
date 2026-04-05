// audiomixer_wii.cpp — AESND-based audio for Wii (no SDL_mixer)
// Music: streamed from SD card (double-buffered)
// SFX:   loaded entirely into RAM on first use

#ifdef WII

#include "audiomixer.h"
#include <aesndlib.h>
#include <fat.h>
#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <map>
#include <string>

// ── constants ────────────────────────────────────────────────────────────────

// Raw PCM format: signed 16-bit big-endian stereo @ 32000 Hz
static const u32  AUDIO_FORMAT = VOICE_STEREO16;
static const f32  AUDIO_FREQ   = (f32)VOICE_FREQ32KHZ;
static const u32  BYTES_PER_SAMPLE = 4; // 2 channels × 2 bytes

// Music streaming: two buffers of 32 KB each (≈ ~256 ms at 32 kHz stereo16)
// Larger buffers give the main thread more headroom to refill before the DSP
// exhausts a buffer, preventing glitches/stalls.
// Must be 32-byte aligned for GC/Wii DMA.
static const u32 MUSIC_BUF_SIZE = 32768;

// Number of one-shot SFX voices
static const int SFX_VOICES = 8;

// Max volume for AESND (0x00–0xFF per channel, but API uses u16 0..0x100)
static const u16 VOL_MAX = 0x100;

// ── static data ──────────────────────────────────────────────────────────────

// SFX voice pool
static AESNDPB  *sfxVoice[SFX_VOICES];
static int       sfxVoiceNext = 0; // round-robin

// SFX sample cache: name → { data ptr, byte size }
struct SfxBuf {
    void  *data;
    size_t size;
};
static std::map<std::string, SfxBuf> sfxCache;

// Music streaming state (modified from IRQ callback — keep volatile)
struct MusicState {
    FILE    *fp;
    // double-buffer: buf[0] and buf[1], each MUSIC_BUF_SIZE bytes, 32B aligned
    u8      *buf[2];
    volatile int writeBuf;  // index of buffer currently being filled by main
    volatile int playBuf;   // index being played by DSP (set from callback)
    volatile bool needFill; // main thread should fill writeBuf
    volatile bool running;
    volatile bool looping;
    AESNDPB *voice;
};
static MusicState music;

// ── helpers ───────────────────────────────────────────────────────────────────

static void *aligned_alloc32(size_t size)
{
    // memalign is available in devkitPPC libc
    return memalign(32, size);
}

// Fill buf with PCM data from music.fp; silence-pad if EOF.
// Flushes the CPU data cache after filling so the DSP DMA sees the data.
// Returns false if nothing more to play (EOF and not looping).
static bool fill_music_buf(u8 *buf, u32 len)
{
    if (!music.fp) {
        memset(buf, 0, len);
        DCFlushRange(buf, len);
        return false;
    }

    u32 total = 0;
    while (total < len) {
        size_t got = fread(buf + total, 1, len - total, music.fp);
        if (got == 0) {
            if (music.looping) {
                rewind(music.fp);
            } else {
                // silence-pad remainder
                memset(buf + total, 0, len - total);
                DCFlushRange(buf, len);
                return false;
            }
        }
        total += (u32)got;
    }
    DCFlushRange(buf, len);
    return true;
}

// ── AESND voice callback (called from DSP IRQ) ────────────────────────────────
//
// Disassembly of __dsp_requestcallback in libaesnd.a reveals the following
// sequence for a streaming voice whose buffer is exhausted:
//
//   1. Tests stream flag (bit 0x40 in voice+0x28). If set → calls callback(state=2).
//   2. After callback returns: re-reads voice+0x28.
//   3. Tests RUNNING bit (bit 0x20). If set → sets stop flag (0x00200000) and
//      calls callback(state=0).
//
// So if RUNNING (0x20) is still set when we return from the STREAM callback,
// the stop flag gets re-armed immediately AFTER our SetVoiceStop(false).
// The voice is then silently skipped on every subsequent DMA interrupt → silence.
//
// Fix: clear bit 0x20 (RUNNING) directly in voice+0x28 before returning from
// the STREAM callback.  voice+0x28 is the 11th u32 in the opaque AESNDPB struct
// (confirmed via objdump: all AESND functions access flags at pb+0x28).
// The RUNNING bit is 0x20 (set by AESND_PlayVoice via `ori r10,r0,32`).

static void music_voice_cb(AESNDPB *pb, u32 state)
{
    if (state != VOICE_STATE_STREAM) return;
    if (!music.running) return;

    // The DSP just consumed playBuf; queue writeBuf (pre-filled and flushed
    // by the main thread via PumpMusic before needFill was cleared).
    int nextPlay = music.writeBuf;
    AESND_SetVoiceBuffer(pb, music.buf[nextPlay], MUSIC_BUF_SIZE);

    // Clear the RUNNING bit (0x20) from voice+0x28 before returning.
    // __dsp_requestcallback tests this bit AFTER the STREAM callback returns;
    // if set it unconditionally re-arms the stop flag (0x00200000), killing
    // streaming.  voice+0x28 is the flags word (offset 40 bytes into AESNDPB).
    volatile u32 *flags = (volatile u32 *)((u8 *)pb + 0x28);
    *flags &= ~0x20u;

    // Swap: old playBuf becomes the new writeBuf for the main thread to refill.
    music.writeBuf = music.playBuf;
    music.playBuf  = nextPlay;
    // Signal main thread to refill writeBuf — set LAST so main sees consistent state.
    music.needFill = true;
}

// ── AudioMixer singleton ──────────────────────────────────────────────────────

AudioMixer *AudioMixer::ptrInstance = nullptr;

AudioMixer *AudioMixer::Instance()
{
    if (!ptrInstance)
        ptrInstance = new AudioMixer();
    return ptrInstance;
}

AudioMixer::AudioMixer()
{
    gameSettings = GameSettings::Instance();

    AESND_Init();

    // Allocate SFX voices
    for (int i = 0; i < SFX_VOICES; i++) {
        sfxVoice[i] = AESND_AllocateVoice(nullptr);
        if (sfxVoice[i]) {
            AESND_SetVoiceVolume(sfxVoice[i], VOL_MAX, VOL_MAX);
            AESND_SetVoiceFormat(sfxVoice[i], AUDIO_FORMAT);
            AESND_SetVoiceFrequency(sfxVoice[i], AUDIO_FREQ);
        }
    }

    // Allocate music voice (streaming)
    music.voice   = AESND_AllocateVoice(music_voice_cb);
    music.fp      = nullptr;
    music.running = false;
    music.looping = false;
    music.needFill = false;
    music.writeBuf = 0;
    music.playBuf  = 1;

    music.buf[0] = (u8 *)aligned_alloc32(MUSIC_BUF_SIZE);
    music.buf[1] = (u8 *)aligned_alloc32(MUSIC_BUF_SIZE);
    if (music.buf[0]) memset(music.buf[0], 0, MUSIC_BUF_SIZE);
    if (music.buf[1]) memset(music.buf[1], 0, MUSIC_BUF_SIZE);

    if (music.voice) {
        AESND_SetVoiceVolume(music.voice, VOL_MAX, VOL_MAX);
        AESND_SetVoiceFormat(music.voice, AUDIO_FORMAT);
        AESND_SetVoiceFrequency(music.voice, AUDIO_FREQ);
        AESND_SetVoiceStream(music.voice, true);
    }

    mixerEnabled = true;
    SDL_Log("AudioMixer: AESND initialised (%d SFX voices + 1 music voice)", SFX_VOICES);
}

AudioMixer::~AudioMixer()
{
    // Stop music
    if (music.running) {
        music.running = false;
        if (music.voice) AESND_SetVoiceStop(music.voice, true);
        if (music.fp)    { fclose(music.fp); music.fp = nullptr; }
    }

    // Free buffers
    if (music.buf[0]) { free(music.buf[0]); music.buf[0] = nullptr; }
    if (music.buf[1]) { free(music.buf[1]); music.buf[1] = nullptr; }

    // Free voices
    for (int i = 0; i < SFX_VOICES; i++) {
        if (sfxVoice[i]) { AESND_FreeVoice(sfxVoice[i]); sfxVoice[i] = nullptr; }
    }
    if (music.voice) { AESND_FreeVoice(music.voice); music.voice = nullptr; }

    // Free SFX cache
    for (auto &kv : sfxCache) {
        if (kv.second.data) free(kv.second.data);
    }
    sfxCache.clear();

    AESND_Reset();
}

void AudioMixer::Dispose()
{
    this->~AudioMixer();
}

// ── PlayMusic ─────────────────────────────────────────────────────────────────

static const struct { const char *id; const char *file; } kMusicFiles[] = {
    { "intro",   "/snd/introzik.raw"           },
    { "main1p",  "/snd/frozen-mainzik-1p.raw"  },
    { "main2p",  "/snd/frozen-mainzik-2p.raw"  },
};

void AudioMixer::StopMusic()
{
    // Silence and stop the current music immediately.
    // Called before long SD card I/O in NewGame() so the streaming callback
    // cannot loop stale buffers while the main thread is blocked.
    music.running = false;
    if (music.voice) AESND_SetVoiceStop(music.voice, true);
    if (music.fp) { fclose(music.fp); music.fp = nullptr; }
    music.needFill = false;
}

void AudioMixer::PlayMusic(const char *track)
{
    if (!mixerEnabled || !gameSettings->canPlayMusic() || haltedMixer) return;
    if (!music.voice) return;

    // Stop any current music
    music.running = false;
    AESND_SetVoiceStop(music.voice, true);
    if (music.fp) { fclose(music.fp); music.fp = nullptr; }

    // Find the file
    const char *filename = nullptr;
    for (auto &m : kMusicFiles) {
        if (strcmp(track, m.id) == 0) { filename = m.file; break; }
    }
    if (!filename) {
        SDL_LogError(1, "AudioMixer: unknown music track '%s'", track);
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), DATA_DIR "%s", filename);

    music.fp = fopen(path, "rb");
    if (!music.fp) {
        SDL_LogError(1, "AudioMixer: cannot open music '%s': %s", path, strerror(errno));
        return;
    }

    music.looping  = true;
    music.writeBuf = 0;
    music.playBuf  = 1;
    music.needFill = false;
    music.running  = true;

    // Fill both buffers before we start playing.
    fill_music_buf(music.buf[0], MUSIC_BUF_SIZE);
    fill_music_buf(music.buf[1], MUSIC_BUF_SIZE);

    // Start playing from buf[playBuf]=buf[1].
    // IMPORTANT: AESND_PlayVoice resets internal voice state (including stream flag)
    // but does NOT clear the stop flag.  We must:
    //   1. Call AESND_PlayVoice to load the first buffer and set format/freq.
    //   2. Call AESND_SetVoiceStream AFTER PlayVoice (PlayVoice clears bit 0x40).
    //   3. Call AESND_SetVoiceStop(false) to un-stop the voice (PlayVoice does not
    //      clear the stop flag set by any prior AESND_SetVoiceStop(true) call).
    AESND_PlayVoice(music.voice, AUDIO_FORMAT,
                    music.buf[music.playBuf], MUSIC_BUF_SIZE,
                    AUDIO_FREQ, 0, false);
    // Enable streaming: callback fires with VOICE_STATE_STREAM when buffer exhausted.
    AESND_SetVoiceStream(music.voice, true);
    // Un-stop: the previous SetVoiceStop(true) call leaves the stop bit set;
    // PlayVoice does not clear it, so we must explicitly clear it here.
    AESND_SetVoiceStop(music.voice, false);
}

// ── PumpMusic — call from main loop to refill streaming buffer ────────────────
// (This is a new internal helper; we hook it into MuteAll/PauseMusic as a
//  convenient point, OR the game loop can call AudioMixer::Instance()->Pump().)
// For simplicity we'll call it at the top of every PlaySFX and also expose it.

void AudioMixer::PumpMusic()
{
    if (!music.running || !music.needFill) return;
    // Fill the writeBuf (the one NOT currently being played).
    bool ok = fill_music_buf(music.buf[music.writeBuf], MUSIC_BUF_SIZE);
    // fill_music_buf already calls DCFlushRange, so the DSP will see the data.
    // Clear needFill AFTER the fill+flush so the callback never queues a
    // buffer that hasn't been flushed yet.
    music.needFill = false;
    if (!ok && !music.looping) {
        music.running = false;
    }
}

// ── PlaySFX ───────────────────────────────────────────────────────────────────

SfxBuf *AudioMixer::GetSFXBuf(const char *sfx)
{
    auto it = sfxCache.find(sfx);
    if (it != sfxCache.end()) return &it->second;

    char path[256];
    snprintf(path, sizeof(path), DATA_DIR "/snd/%s.raw", sfx);

    FILE *f = fopen(path, "rb");
    if (!f) {
        SDL_LogError(1, "AudioMixer: cannot open SFX '%s': %s", path, strerror(errno));
        sfxCache[sfx] = { nullptr, 0 };
        return &sfxCache[sfx];
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz <= 0) {
        fclose(f);
        sfxCache[sfx] = { nullptr, 0 };
        return &sfxCache[sfx];
    }

    // Allocate 32-byte aligned (required for DMA)
    void *buf = aligned_alloc32((size_t)sz);
    if (!buf) {
        fclose(f);
        sfxCache[sfx] = { nullptr, 0 };
        return &sfxCache[sfx];
    }

    fread(buf, 1, (size_t)sz, f);
    fclose(f);

    sfxCache[sfx] = { buf, (size_t)sz };
    SDL_Log("AudioMixer: loaded SFX '%s' (%ld bytes)", sfx, sz);
    return &sfxCache[sfx];
}

void AudioMixer::PlaySFX(const char *sfx)
{
    if (!mixerEnabled || !gameSettings->canPlaySFX() || haltedMixer) return;

    // Pump music streaming while we're here
    PumpMusic();

    SfxBuf *sb = GetSFXBuf(sfx);
    if (!sb || !sb->data || sb->size == 0) return;

    // Round-robin SFX voice selection
    AESNDPB *voice = sfxVoice[sfxVoiceNext];
    sfxVoiceNext = (sfxVoiceNext + 1) % SFX_VOICES;

    if (!voice) return;

    AESND_SetVoiceStop(voice, true);
    AESND_PlayVoice(voice, AUDIO_FORMAT,
                    sb->data, (u32)sb->size,
                    AUDIO_FREQ, 0, false);
    // AESND_PlayVoice does NOT clear the stop flag (bit 0x00200000 at AESNDPB+40;
    // PlayVoice mask 0xBFDFFFCF skips that bit). Without this call the voice stays
    // stopped after the first time it was stopped and never plays again.
    AESND_SetVoiceStop(voice, false);
}

// ── PauseMusic / MuteAll ──────────────────────────────────────────────────────

void AudioMixer::PauseMusic(bool enable)
{
    // enable==true means RESUME, enable==false means PAUSE (matches original)
    if (!music.voice) return;
    AESND_SetVoiceMute(music.voice, !enable);
}

void AudioMixer::MuteAll(bool enable)
{
    // enable==true means UNMUTE, enable==false means MUTE/HALT
    if (enable) {
        haltedMixer = false;
        if (music.voice) AESND_SetVoiceMute(music.voice, false);
        for (int i = 0; i < SFX_VOICES; i++)
            if (sfxVoice[i]) AESND_SetVoiceMute(sfxVoice[i], false);
    } else {
        haltedMixer = true;
        if (music.voice) AESND_SetVoiceMute(music.voice, true);
        for (int i = 0; i < SFX_VOICES; i++)
            if (sfxVoice[i]) AESND_SetVoiceMute(sfxVoice[i], true);
    }
}

#endif // WII
