#include "test_framework.h"

#include "audio/AudioEngine.h"
#include "platform/Platform.h"

using namespace tmx;

// ---- volume mapping (bit-faithful to NewApp.cpp:418/430) ----

TEST(audio, volume_slider_full) {
    EXPECT_NEAR(SoundPercentToLinear(100), 1.0f, 1e-5f);
    EXPECT_NEAR(MusicPercentToLinear(100), 1.0f, 1e-5f);
}

TEST(audio, volume_slider_zero_is_not_mute) {
    // 25*0-2500 = -2500 centi-dB = -25 dB -> 10^(-25/20)
    EXPECT_NEAR(SoundPercentToLinear(0), 0.0562341f, 1e-4f);
    // 30*0-3000 = -30 dB
    EXPECT_NEAR(MusicPercentToLinear(0), 0.0316228f, 1e-4f);
}

TEST(audio, volume_midpoint) {
    // 25*50-2500 = -1250 centi-dB = -12.5 dB
    EXPECT_NEAR(SoundPercentToLinear(50), 0.2371374f, 1e-4f);
}

TEST(audio, volume_mute_floor) {
    EXPECT_EQ(DsCentiDbToLinear(-10000), 0.0f);
    EXPECT_EQ(DsCentiDbToLinear(-20000), 0.0f);
}

// ---- soundlist parsing (dsutil.cpp:57-75) ----

TEST(audio, soundlist_parse_basic) {
    SoundList list;
    const char* text =
        "1 sound\\ambient\\amb01.wav 1\n"
        "2 sound\\ambient\\dustdrop.wav 1\n"
        "13 sound\\ambient\\dropstoneg.wav 3\n";
    EXPECT_TRUE(list.LoadFromText(text));
    EXPECT_EQ(list.count, 3);
    EXPECT_TRUE(list.present[1]);
    EXPECT_TRUE(list.present[2]);
    EXPECT_TRUE(list.present[13]);
    EXPECT_FALSE(list.present[3]);
    EXPECT_EQ(list.entries[1].path, std::string("sound/ambient/amb01.wav"));
    EXPECT_EQ(list.entries[13].channels, 3);
}

TEST(audio, soundlist_ignores_out_of_range) {
    SoundList list;
    EXPECT_FALSE(list.LoadFromText("0 sound\\x.wav 1\n600 sound\\y.wav 1\n"));
    EXPECT_EQ(list.count, 0);
}

TEST(audio, soundlist_real_file) {
    SetDataDir(TM_REPO_ROOT);
    SoundList list;
    EXPECT_TRUE(list.Load("sound/soundlist.txt"));
    EXPECT_TRUE(list.count > 100);
    // spot-check: 33 is the UI click used all over TMFieldScene.cpp
    EXPECT_TRUE(list.present[33]);
    EXPECT_TRUE(list.entries[33].path.find("sound/") == 0);
}

// ---- voice pool bookkeeping ----

TEST(audio, pool_first_free_voice) {
    VoicePool pool;
    pool.cap = 2;
    EXPECT_EQ(pool.Acquire(100), 0);
    EXPECT_EQ(pool.Acquire(200), 1);
    // both busy -> steal oldest (slot 0, started 100)
    EXPECT_EQ(pool.Acquire(300), 0);
}

TEST(audio, pool_reuse_finished) {
    VoicePool pool;
    pool.cap = 1;
    EXPECT_EQ(pool.Acquire(100), 0);
    pool.playing[0] = false; // voice finished (engine syncs from ma_sound)
    EXPECT_EQ(pool.Acquire(200), 0);
}

TEST(audio, pool_ensure_resizes) {
    VoicePool pool;
    pool.cap = 3;
    pool.Ensure();
    EXPECT_EQ(pool.startedMs.size(), (size_t)3);
    EXPECT_EQ(pool.playing.size(), (size_t)3);
}

// ---- music table (DirShow.cpp:7-23) ----

TEST(audio, music_table) {
    EXPECT_EQ(std::string(AudioEngine::MusicPath(0)), "music/login.mp3");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(2)), "music/field01.mp3");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(10)), "music/CastleWar.mp3");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(13)), "");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(14)), "");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(15)), "");
    EXPECT_EQ(std::string(AudioEngine::MusicPath(-1)), "");
}

// ---- headless engine (null backend) ----

TEST(audio, engine_null_backend_lifecycle) {
    SetDataDir(TM_REPO_ROOT);
    AudioEngine eng;
    EXPECT_TRUE(eng.InitNull());
    EXPECT_TRUE(eng.IsReady());
    EXPECT_TRUE(eng.LoadSoundList("sound/soundlist.txt"));
    // no crash on silent-ops
    eng.SetSoundVolume(80);
    eng.SetMusicVolume(60);
    eng.SetMute(true);
    eng.SetMute(false);
    EXPECT_FALSE(eng.IsSoundPlaying(33));
    eng.StopSound(33);
    EXPECT_EQ(eng.CurrentMusic(), -1);
    eng.Shutdown();
    EXPECT_FALSE(eng.IsReady());
}
