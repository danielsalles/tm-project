#include "test_framework.h"

#include "world/LampFx.h"

using tmx::Billboard;
using tmx::BuildLampGlow;

TEST(fxlamp, type501_fire_and_halo) {
    std::vector<Billboard> v;
    BuildLampGlow(501, 10.0f, 2.0f, 20.0f, 0.0f, 0.5f, v);
    EXPECT_EQ(v.size(), 2u);
    // Flame: Fire01 flipbook, 8 frames, warm yellow, no fade.
    EXPECT_EQ(v[0].d.textureIndex, 11);
    EXPECT_EQ(v[0].d.cycleCount, 8);
    EXPECT_EQ(v[0].d.bgra, 0xEEEECC00u);
    EXPECT_EQ(v[0].d.fade, 0);
    EXPECT_TRUE(fabsf(v[0].d.x - 10.0f) < 1e-5f && fabsf(v[0].d.z - 20.0f) < 1e-5f);
    // Halo: bright texture 2, 2.8x scale.
    EXPECT_EQ(v[1].d.textureIndex, 2);
    EXPECT_TRUE(fabsf(v[1].d.scaleX - 1.4f) < 1e-5f);
    EXPECT_EQ(v[1].d.bgra, 0x55553300u);
}

TEST(fxlamp, type503_ghostfire_three_parts) {
    std::vector<Billboard> v;
    BuildLampGlow(503, 0.0f, 4.5f, 0.0f, 0.0f, 0.5f, v);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0].d.textureIndex, 101);
    EXPECT_EQ(v[0].d.bgra, 0xFF5500FFu);
    // Second ghost-flame is smaller and offset down.
    EXPECT_EQ(v[1].d.textureIndex, 101);
    EXPECT_TRUE(v[1].d.y < v[0].d.y);
    EXPECT_EQ(v[2].d.textureIndex, 2);
    EXPECT_EQ(v[2].d.bgra, 0xFF330055u);
}

TEST(fxlamp, type504_flickers_with_fade2) {
    std::vector<Billboard> v;
    BuildLampGlow(504, 0.0f, 1.5f, 0.0f, 0.0f, 0.5f, v);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].d.textureIndex, 56);
    EXPECT_EQ(v[0].d.fade, 2);
    EXPECT_EQ(v[0].d.bgra, 0xFFFF0000u);
}

TEST(fxlamp, type505_directional_uses_record_angle) {
    std::vector<Billboard> v;
    BuildLampGlow(505, 0.0f, 0.0f, 0.0f, 1.25f, 0.5f, v);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].d.textureIndex, 79);
    EXPECT_EQ(v[0].d.lookCam, 0);              // fixed orientation
    EXPECT_TRUE(fabsf(v[0].d.axisAngle - 1.25f) < 1e-5f);
}

TEST(fxlamp, unknown_type_emits_nothing) {
    std::vector<Billboard> v;
    BuildLampGlow(999, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, v);
    EXPECT_EQ(v.size(), 0u);
}
