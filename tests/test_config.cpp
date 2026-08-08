#include "test_framework.h"

#include "platform/GameConfig.h"
#include "platform/Platform.h"

#include <cstdio>

using namespace tmx;

TEST(config, defaults_match_original) {
    GameConfig c; // NewApp.cpp:149-166 else-branch
    EXPECT_EQ(c.version, 7000);
    EXPECT_EQ(c.ResIndex(), 7);
    EXPECT_EQ(c.Sound(), 0);
    EXPECT_EQ(c.Music(), 0);
    EXPECT_EQ(c.Bright(), 57);
    EXPECT_TRUE(c.Windowed());
    EXPECT_FALSE(c.CameraRotInverted());
    EXPECT_TRUE(c.QuarterView());
}

TEST(config, bright_gain_is_linear) {
    // ramp[i] = bright*0.02*i (RenderDevice.cpp:296) — pure gain, not pow.
    GameConfig c;
    c.SetBright(50);
    EXPECT_NEAR(c.BrightGain(), 1.0f, 1e-6f);   // identity
    c.SetBright(57);
    EXPECT_NEAR(c.BrightGain(), 1.14f, 1e-6f);  // original default
    c.SetBright(0);
    EXPECT_NEAR(c.BrightGain(), 0.0f, 1e-6f);
}

TEST(config, resolution_table) {
    int w = 0, h = 0;
    GameConfig::ResolutionWH(2, w, h);
    EXPECT_EQ(w, 1024);
    EXPECT_EQ(h, 768);
    GameConfig::ResolutionWH(10, w, h);
    EXPECT_EQ(w, 3200);
    EXPECT_EQ(h, 2400);
    GameConfig::ResolutionWH(99, w, h); // out of range -> 1024x768
    EXPECT_EQ(w, 1024);
    EXPECT_EQ(h, 768);
}

TEST(config, roundtrip_bytes) {
    GameConfig a;
    a.SetResIndex(4);
    a.SetSound(40);
    a.SetMusic(40);
    a.SetBright(51);
    a.slot[4] = -1; // preserved verbatim

    const char* path = "test_config_roundtrip.bin";
    EXPECT_TRUE(a.Save(path));

    GameConfig b;
    EXPECT_TRUE(b.Load(path));
    EXPECT_EQ(b.version, a.version);
    for (int i = 0; i < GameConfig::kSlotCount; ++i)
        EXPECT_EQ(b.slot[i], a.slot[i]);
    remove(path);
}

TEST(config, repo_config_bin_parses) {
    // Golden file: the real Config.bin shipped at the repo root.
    // bytes: 1e29 0400 0200 2800 2800 ffff 3300 0200 ...
    SetDataDir(TM_REPO_ROOT);
    GameConfig c;
    EXPECT_TRUE(c.Load("Config.bin"));
    EXPECT_EQ(c.version, 0x291e);
    EXPECT_EQ(c.ResIndex(), 4);
    EXPECT_EQ(c.Sound(), 40);
    EXPECT_EQ(c.Music(), 40);
    EXPECT_EQ(c.slot[4], -1);
    EXPECT_EQ(c.Bright(), 51);
}
