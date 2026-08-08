#include "audio/AudioEngine.h"

#include "platform/Platform.h"

#include <miniaudio.h>

#include <cstring>

namespace tmx {

// ------------------------------------------------------------- SoundList

bool SoundList::Load(const char* path) {
    FILE* fp = OpenAsset(path, "rt");
    if (!fp)
        return false;
    std::string text;
    char buf[512];
    while (fgets(buf, sizeof buf, fp))
        text += buf;
    fclose(fp);
    return LoadFromText(text.c_str());
}

bool SoundList::LoadFromText(const char* text) {
    // Original parser (dsutil.cpp:64-72): fscanf "%d %s %d" triples; lines
    // look like `1 sound\ambient\amb01.wav 1`. Backslashes normalized.
    if (!text)
        return false;
    const char* p = text;
    int parsed = 0;
    while (*p) {
        // skip whitespace/newlines between records
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            ++p;
        if (!*p)
            break;

        int index = 0;
        if (sscanf(p, "%d", &index) != 1)
            break;
        // advance past the number
        while (*p && *p != ' ' && *p != '\t')
            ++p;
        while (*p == ' ' || *p == '\t')
            ++p;

        char file[256]{};
        int channels = 1;
        if (sscanf(p, "%255s %d", file, &channels) < 1)
            break;
        // advance past path (+ optional channels token if present on line)
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            ++p;
        while (*p == ' ' || *p == '\t')
            ++p;
        int dummy = 0;
        if (sscanf(p, "%d", &dummy) == 1) {
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
                ++p;
        }

        if (index > 0 && index < MAX_SOUNDLIST) {
            std::string norm = file;
            for (auto& c : norm) {
                if (c == '\\')
                    c = '/';
            }
            entries[index].path = norm;
            entries[index].channels = channels > 0 ? channels : 1;
            present[index] = true;
            ++parsed;
        }
    }
    count = parsed;
    return parsed > 0;
}

// ------------------------------------------------------------- VoicePool

void VoicePool::Ensure() {
    if ((int)startedMs.size() < cap) {
        startedMs.resize(cap, 0);
        playing.resize(cap, false);
    }
}

int VoicePool::Acquire(uint64_t nowMs) {
    Ensure();
    int oldest = 0;
    for (int i = 0; i < cap; ++i) {
        if (!playing[i]) {
            startedMs[i] = nowMs;
            playing[i] = true;
            return i;
        }
        if (startedMs[i] < startedMs[oldest])
            oldest = i;
    }
    startedMs[oldest] = nowMs;
    return oldest; // steal: caller restarts the voice; stays "playing"
}

// ------------------------------------------------------------- AudioEngine

static const char* kMusicPaths[15] = {
    "music/login.mp3",
    "music/town01.mp3",
    "music/field01.mp3",
    "music/town02.mp3",
    "music/field02.mp3",
    "music/dungeon01.mp3",
    "music/kingdom.mp3",
    "music/dungeon02.mp3",
    "music/town03.mp3",
    "music/field03.mp3",
    "music/CastleWar.mp3",
    "music/kepra.mp3",
    "music/khepraBoss.mp3",
    "",
    "",
};

const char* AudioEngine::MusicPath(int index) {
    if (index < 0 || index >= 15)
        return "";
    return kMusicPaths[index];
}

bool AudioEngine::Init() {
    if (m_ready)
        return true;
    ma_engine* engine = new ma_engine();
    if (ma_engine_init(nullptr, engine) != MA_SUCCESS) {
        Log("audio: ma_engine_init failed — running silent");
        delete engine;
        return false;
    }
    m_engine = engine;

    ma_sound_group* sfx = new ma_sound_group();
    ma_sound_group* bgm = new ma_sound_group();
    ma_sound_group_init(engine, 0, nullptr, sfx);
    ma_sound_group_init(engine, 0, nullptr, bgm);
    m_sfxGroup = sfx;
    m_bgmGroup = bgm;

    m_ready = true;
    SetSoundVolume(m_soundPct);
    SetMusicVolume(m_musicPct);
    return true;
}

bool AudioEngine::InitNull() {
    if (m_ready)
        return true;
    ma_context* ctx = new ma_context();
    ma_backend backends[] = { ma_backend_null };
    if (ma_context_init(backends, 1, nullptr, ctx) != MA_SUCCESS) {
        delete ctx;
        return false;
    }
    ma_engine_config cfg = ma_engine_config_init();
    cfg.pContext = ctx;
    ma_engine* engine = new ma_engine();
    if (ma_engine_init(&cfg, engine) != MA_SUCCESS) {
        ma_context_uninit(ctx);
        delete ctx;
        delete engine;
        return false;
    }
    m_context = ctx;
    m_engine = engine;

    ma_sound_group* sfx = new ma_sound_group();
    ma_sound_group* bgm = new ma_sound_group();
    ma_sound_group_init(engine, 0, nullptr, sfx);
    ma_sound_group_init(engine, 0, nullptr, bgm);
    m_sfxGroup = sfx;
    m_bgmGroup = bgm;

    m_ready = true;
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_ready)
        return;
    StopMusic();
    ma_engine* engine = (ma_engine*)m_engine;
    for (int i = 0; i < MAX_SOUNDLIST; ++i) {
        for (void* v : m_sfx[i].voices) {
            ma_sound_uninit((ma_sound*)v);
            delete (ma_sound*)v;
        }
        m_sfx[i].voices.clear();
    }
    if (m_sfxGroup) {
        ma_sound_group_uninit((ma_sound_group*)m_sfxGroup);
        delete (ma_sound_group*)m_sfxGroup;
        m_sfxGroup = nullptr;
    }
    if (m_bgmGroup) {
        ma_sound_group_uninit((ma_sound_group*)m_bgmGroup);
        delete (ma_sound_group*)m_bgmGroup;
        m_bgmGroup = nullptr;
    }
    ma_engine_uninit(engine);
    delete engine;
    m_engine = nullptr;
    if (m_context) {
        ma_context_uninit((ma_context*)m_context);
        delete (ma_context*)m_context;
        m_context = nullptr;
    }
    m_ready = false;
}

bool AudioEngine::LoadSoundList(const char* path) {
    SoundList list;
    if (!list.Load(path)) {
        Log("audio: soundlist not found (%s) — SFX disabled", path);
        return false;
    }
    for (int i = 0; i < MAX_SOUNDLIST; ++i) {
        if (list.present[i]) {
            m_sfx[i].path = list.entries[i].path;
            m_sfx[i].channels = list.entries[i].channels;
            m_sfx[i].pool.cap = list.entries[i].channels;
            m_present[i] = true;
        }
    }
    Log("audio: soundlist loaded (%d entries)", list.count);
    return true;
}

bool AudioEngine::EnsureVoice(int id, int slot) {
    Sfx& s = m_sfx[id];
    s.pool.Ensure();
    while ((int)s.voices.size() <= slot) {
        std::string full = ResolveAssetPath(s.path.c_str());
        if (full.empty()) {
            if (!s.loadFailed) {
                Log("audio: missing %s (id %d)", s.path.c_str(), id);
                s.loadFailed = true;
            }
            return false;
        }
        ma_sound* snd = new ma_sound();
        ma_result r = ma_sound_init_from_file((ma_engine*)m_engine, full.c_str(),
                                              MA_SOUND_FLAG_DECODE,
                                              (ma_sound_group*)m_sfxGroup,
                                              nullptr, snd);
        if (r != MA_SUCCESS) {
            delete snd;
            if (!s.loadFailed) {
                Log("audio: decode failed %s (id %d)", s.path.c_str(), id);
                s.loadFailed = true;
            }
            return false;
        }
        s.voices.push_back(snd);
    }
    return true;
}

void AudioEngine::SyncPool(int id) {
    Sfx& s = m_sfx[id];
    for (size_t i = 0; i < s.voices.size(); ++i)
        s.pool.playing[i] = ma_sound_is_playing((ma_sound*)s.voices[i]) == MA_TRUE;
}

void AudioEngine::PlaySound(int id) {
    if (!m_ready || m_muted || id <= 0 || id >= MAX_SOUNDLIST || !m_present[id])
        return;
    Sfx& s = m_sfx[id];
    SyncPool(id);
    int slot = s.pool.Acquire(GetTicks());
    if (!EnsureVoice(id, slot))
        return;
    ma_sound* snd = (ma_sound*)s.voices[slot];
    ma_sound_set_looping(snd, MA_FALSE);
    ma_sound_stop(snd);
    ma_sound_seek_to_pcm_frame(snd, 0);
    ma_sound_start(snd);
}

void AudioEngine::PlaySoundIfNot(int id) {
    if (!m_ready || m_muted || id <= 0 || id >= MAX_SOUNDLIST || !m_present[id])
        return;
    if (IsSoundPlaying(id))
        return;
    PlaySound(id);
}

void AudioEngine::PlaySoundLooping(int id) {
    if (!m_ready || m_muted || id <= 0 || id >= MAX_SOUNDLIST || !m_present[id])
        return;
    Sfx& s = m_sfx[id];
    SyncPool(id);
    int slot = s.pool.Acquire(GetTicks());
    if (!EnsureVoice(id, slot))
        return;
    ma_sound* snd = (ma_sound*)s.voices[slot];
    ma_sound_set_looping(snd, MA_TRUE);
    ma_sound_stop(snd);
    ma_sound_seek_to_pcm_frame(snd, 0);
    ma_sound_start(snd);
}

void AudioEngine::StopSound(int id) {
    if (!m_ready || id <= 0 || id >= MAX_SOUNDLIST)
        return;
    Sfx& s = m_sfx[id];
    for (void* v : s.voices)
        ma_sound_stop((ma_sound*)v);
    for (size_t i = 0; i < s.pool.playing.size(); ++i)
        s.pool.playing[i] = false;
}

bool AudioEngine::IsSoundPlaying(int id) {
    if (!m_ready || id <= 0 || id >= MAX_SOUNDLIST)
        return false;
    for (void* v : m_sfx[id].voices) {
        if (ma_sound_is_playing((ma_sound*)v))
            return true;
    }
    return false;
}

void AudioEngine::SetSoundVolume(int percent) {
    m_soundPct = percent;
    if (m_ready && m_sfxGroup)
        ma_sound_group_set_volume((ma_sound_group*)m_sfxGroup, SoundPercentToLinear(percent));
}

void AudioEngine::SetMusicVolume(int percent) {
    m_musicPct = percent;
    if (m_ready && m_bgmGroup)
        ma_sound_group_set_volume((ma_sound_group*)m_bgmGroup, MusicPercentToLinear(percent));
}

void AudioEngine::SetMute(bool mute) {
    m_muted = mute;
    if (!m_ready)
        return;
    // Original: volume -10000 kills SFX at GetSoundData level; BGM keeps its
    // own switch. Here mute zeroes both groups.
    ma_sound_group_set_volume((ma_sound_group*)m_sfxGroup,
                              mute ? 0.0f : SoundPercentToLinear(m_soundPct));
    ma_sound_group_set_volume((ma_sound_group*)m_bgmGroup,
                              mute ? 0.0f : MusicPercentToLinear(m_musicPct));
}

void AudioEngine::PlayMusic(int index) {
    if (!m_ready || index < 0 || index >= 15 || !kMusicPaths[index][0])
        return;
    if (m_bgmIndex == index && m_bgm)
        return; // already on this track (DirectShow kept it running)
    StopMusic();

    std::string full = ResolveAssetPath(kMusicPaths[index]);
    if (full.empty()) {
        Log("audio: missing music %s", kMusicPaths[index]);
        return;
    }
    ma_sound* snd = new ma_sound();
    if (ma_sound_init_from_file((ma_engine*)m_engine, full.c_str(),
                                MA_SOUND_FLAG_STREAM,
                                (ma_sound_group*)m_bgmGroup,
                                nullptr, snd) != MA_SUCCESS) {
        delete snd;
        Log("audio: music decode failed %s", kMusicPaths[index]);
        return;
    }
    ma_sound_set_looping(snd, MA_TRUE);
    ma_sound_start(snd);
    m_bgm = snd;
    m_bgmIndex = index;
}

void AudioEngine::StopMusic() {
    if (m_bgm) {
        ma_sound_uninit((ma_sound*)m_bgm);
        delete (ma_sound*)m_bgm;
        m_bgm = nullptr;
    }
    m_bgmIndex = -1;
}

}
