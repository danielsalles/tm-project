#include "test_framework.h"

#include "world/WeatherFx.h"

#include <cmath>
#include <cstdio>

using tmx::WeatherFrameMove;
using tmx::WeatherInit;
using tmx::WeatherSystem;
using tmx::WeatherTextureIndex;

TEST(weatherfx, rain_particles_and_speeds) {
    WeatherSystem w;
    WeatherInit(w, 0, 1.0f);
    EXPECT_EQ(w.drops.size(), 50u);
    for (const auto& d : w.drops) {
        EXPECT_TRUE(d.x >= -6.0f && d.x <= 6.0f);
        EXPECT_TRUE(d.z >= -6.0f && d.z <= 6.0f);
        EXPECT_TRUE(d.y >= 0.0f && d.y < 10.0f);
        EXPECT_TRUE(d.speed > 0.0f);
    }
    EXPECT_EQ(WeatherTextureIndex(0), 9);
}

TEST(weatherfx, rain_falls_and_respawns) {
    WeatherSystem w;
    WeatherInit(w, 0, 1.0f);
    WeatherFrameMove(w, 1000, 50.0f, 1.0f, 60.0f);   // start
    // All drops below/equal focus height 1.0 must respawn at focusH+10.
    WeatherFrameMove(w, 1000 + 16, 50.0f, 1.0f, 60.0f);
    for (const auto& d : w.drops)
        EXPECT_TRUE(d.y > 1.0f);
    // A drop that falls past the focus height respawns at focusH + 10.
    WeatherSystem w2;
    WeatherInit(w2, 0, 1.0f);
    WeatherFrameMove(w2, 0, 0.0f, 5.0f, 0.0f);
    w2.drops[0].y = 5.1f;
    w2.drops[0].speed = 1.0f;
    WeatherFrameMove(w2, 1000, 0.0f, 5.0f, 0.0f);   // falls past the focus height
    WeatherFrameMove(w2, 1000, 0.0f, 5.0f, 0.0f);   // next frame: respawn
    EXPECT_TRUE(w2.drops[0].y == 15.0f);             // respawned at 5+10
}

TEST(weatherfx, snow_counts_and_absolute_seed) {
    WeatherSystem w;
    WeatherInit(w, 1, 2.0f);
    EXPECT_EQ(w.drops.size(), 200u);
    EXPECT_EQ(WeatherTextureIndex(1), 2);
    // First FrameMove seeds absolute positions around the focus.
    WeatherFrameMove(w, 100, 3520.0f, 1.0f, 3008.0f);
    for (const auto& d : w.drops) {
        EXPECT_TRUE(d.x > 3500.0f && d.x < 3540.0f);
        EXPECT_TRUE(d.z > 2990.0f && d.z < 3028.0f);
        EXPECT_TRUE(d.y >= 1.0f && d.y <= 11.0f);
    }
}

TEST(weatherfx, snow_respawn_above_focus) {
    WeatherSystem w;
    WeatherInit(w, 1, 1.0f);
    WeatherFrameMove(w, 0, 100.0f, 2.0f, 100.0f);
    w.drops[0].y = 1.0f;   // below focusH -> respawn
    WeatherFrameMove(w, 16, 100.0f, 2.0f, 100.0f);
    EXPECT_TRUE(w.drops[0].y >= 9.0f && w.drops[0].y <= 14.0f);   // 2+7+0..4.5
    EXPECT_TRUE(w.drops[0].x > 90.0f && w.drops[0].x < 110.0f);
}
