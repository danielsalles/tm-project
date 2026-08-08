#pragma once

// AudioEngine — DirectSound/DirectShow parity on top of miniaudio.
//
// Port of the two original sound systems (doc 21 §2):
//   * CSoundManager  (dsutil.cpp): SFX WAVs listed in sound/soundlist.txt
//     ("index path channels"), lazy decode on first use, polyphony = one
//     voice pool per entry capped at `channels`, Play picks the first free
//     voice (GetFreeBuffer) else steals the oldest.
//   * DS_SOUND_MANAGER (DirShow.cpp): BGM, 13 MP3s streamed + looped, one
//     track at a time.
//
// Volume mapping is bit-faithful to the original formulas:
//   SFX   slider 0-100 -> centi-dB  25*n - 2500   (NewApp.cpp:418)
//   Music slider 0-100 -> centi-dB  30*n - 3000   (NewApp.cpp:430)
//   DS units are hundredths of a dB: linear gain = 10^(centiDb / 2000).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace tmx {

// ---------------------------------------------------------------- pure core

constexpr int MAX_SOUNDLIST = 512; // dsutil.h:17

// DS volume is in hundredths of a decibel.
inline float DsCentiDbToLinear(int centiDb);

// Slider (0-100) -> DS centi-dB, original formulas.
inline int SoundPercentToCentiDb(int pct) { return 25 * pct - 2500; }
inline int MusicPercentToCentiDb(int pct) { return 30 * pct - 3000; }

inline float SoundPercentToLinear(int pct);
inline float MusicPercentToLinear(int pct);

struct SoundListEntry {
    std::string path;    // normalized with forward slashes
    int channels = 1;    // polyphony cap (duplicate buffers in the original)
};

// Parsed sound/soundlist.txt. Pure: no audio device needed.
struct SoundList {
    SoundListEntry entries[MAX_SOUNDLIST];
    bool present[MAX_SOUNDLIST]{};
    int count = 0;

    bool Load(const char* path);         // via OpenAsset
    bool LoadFromText(const char* text); // test hook
};

// Device-agnostic voice pool bookkeeping (first free voice, else steal the
// oldest started). The engine mirrors this state onto ma_sound instances.
struct VoicePool {
    int cap = 1;
    std::vector<uint64_t> startedMs; // 0 = never used
    std::vector<bool> playing;

    void Ensure();
    // Returns the slot to play on. `nowMs` stamps the slot.
    int Acquire(uint64_t nowMs);
};

// ------------------------------------------------------------- engine

class AudioEngine {
public:
    bool Init();          // real device
    bool InitNull();      // headless (ma_backend_null) — tests / --no-sound
    void Shutdown();
    bool IsReady() const { return m_ready; }

    bool LoadSoundList(const char* path); // "sound/soundlist.txt"

    // SFX — GetSoundAndPlay* parity (TMUtil.cpp:47-89)
    void PlaySound(int id);
    void PlaySoundIfNot(int id);
    void PlaySoundLooping(int id);
    void StopSound(int id);
    bool IsSoundPlaying(int id);

    void SetSoundVolume(int percent); // 0-100 slider
    void SetMusicVolume(int percent);
    void SetMute(bool mute);

    // BGM — 15-slot table (DirShow.cpp:7-23), indices 0-12 valid.
    void PlayMusic(int index);
    void StopMusic();
    int  CurrentMusic() const { return m_bgmIndex; }
    static const char* MusicPath(int index);

private:
    struct Sfx {
        std::string path;
        int channels = 1;
        VoicePool pool;
        std::vector<void*> voices; // ma_sound*
        bool loadFailed = false;
    };

    bool EnsureVoice(int id, int slot);
    void SyncPool(int id);

    Sfx m_sfx[MAX_SOUNDLIST];
    bool m_present[MAX_SOUNDLIST]{};

    void* m_engine = nullptr;    // ma_engine*
    void* m_context = nullptr;   // ma_context* (null-backend init only)
    void* m_sfxGroup = nullptr;  // ma_sound_group*
    void* m_bgmGroup = nullptr;  // ma_sound_group*
    void* m_bgm = nullptr;       // ma_sound*
    int m_bgmIndex = -1;

    int m_soundPct = 100;
    int m_musicPct = 100;
    bool m_muted = false;
    bool m_ready = false;
};

// ------------------------------------------------------- inline impls

inline float DsCentiDbToLinear(int centiDb) {
    if (centiDb <= -10000)
        return 0.0f;
    if (centiDb >= 0)
        return 1.0f;
    return powf(10.0f, (centiDb / 100.0f) / 20.0f);
}

inline float SoundPercentToLinear(int pct) {
    if (pct <= 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return DsCentiDbToLinear(SoundPercentToCentiDb(pct));
}

inline float MusicPercentToLinear(int pct) {
    if (pct <= 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return DsCentiDbToLinear(MusicPercentToCentiDb(pct));
}

}
