#include "test_framework.h"

#include "world/Critter.h"
#include "world/CharacterAnimation.h"
#include "platform/Platform.h"

#include <vector>

#include <cmath>
#include <cstdio>

using tmx::Critter;
using tmx::CritterFrameMove;
using tmx::CritterSpawn;

TEST(critter, leaf_look_rules) {
    std::vector<Critter> v;
    // 311-316: Mesh0 0, Skin0 = (u8)(type-55); non-snow region here (x/z small).
    CritterSpawn(311, 10.0f, 1.0f, 20.0f, v);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].skinMeshType, 61);
    EXPECT_EQ(v[0].meshLook0, 0);
    EXPECT_EQ(v[0].skinLook0, (int16_t)(uint8_t)(311 - 55));   // 256 -> 0
    EXPECT_EQ(v[0].fps, 80u);

    // 317+: Mesh0 2, Skin0 = (u8)(type-57).
    v.clear();
    CritterSpawn(318, 10.0f, 1.0f, 20.0f, v);
    EXPECT_EQ(v[0].meshLook0, 2);
    EXPECT_EQ(v[0].skinLook0, (int16_t)(uint8_t)(318 - 57));   // 261 -> 5
}

TEST(critter, leaf_snow_region_override) {
    std::vector<Critter> v;
    // Tile 27,21 is inside the snow region (26-30, 20-24): world = tile*128+64.
    CritterSpawn(312, 27.0f * 128.0f + 64.0f, 0.0f, 21.0f * 128.0f + 64.0f, v);
    EXPECT_EQ(v[0].skinLook0, 9);
}

TEST(critter, butterfly_spawns_five) {
    std::vector<Critter> v;
    CritterSpawn(343, 100.0f, 2.0f, 100.0f, v);
    EXPECT_EQ(v.size(), 5u);
    for (const auto& c : v) {
        EXPECT_EQ(c.skinMeshType, 69);
        EXPECT_EQ(c.fps, 15u);
        EXPECT_TRUE(c.x >= 100.0f && c.x <= 100.5f);
    }
    // Type 7 -> bird (bd01), motion 3, scale 0.2.
    v.clear();
    CritterSpawn(7, 0.0f, 0.0f, 0.0f, v);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0].skinMeshType, 24);
    EXPECT_EQ(v[0].motionType, 3);
    EXPECT_TRUE(v[0].scale < 0.21f && v[0].scale > 0.19f);
}

TEST(critter, fish_spawns_five_type70) {
    std::vector<Critter> v;
    CritterSpawn(344, 50.0f, -3.0f, 60.0f, v);
    EXPECT_EQ(v.size(), 5u);
    for (const auto& c : v) {
        EXPECT_EQ(c.skinMeshType, 70);
        EXPECT_TRUE(c.circleSpeed >= 0.7f && c.circleSpeed <= 1.6f);
        EXPECT_TRUE(c.particleH >= 3.0f && c.particleH <= 6.5f);
    }
}

TEST(critter, leaf_fades_with_camera_distance) {
    std::vector<Critter> v;
    CritterSpawn(311, 0.0f, 0.0f, 0.0f, v);
    Critter& c = v[0];
    // Camera at 10u: full alpha.
    CritterFrameMove(c, 0, 10.0f, 0.0f);
    EXPECT_TRUE(c.alpha > 0.99f);
    // Camera at 24u (squared 576): mid fade = 1 - (576-400)/384.
    CritterFrameMove(c, 0, 24.0f, 0.0f);
    const float want = 1.0f - (576.0f - 400.0f) / 384.0f;
    EXPECT_TRUE(fabsf(c.alpha - want) < 1e-4f);
    // Camera at 30u+: fully faded.
    CritterFrameMove(c, 0, 35.0f, 0.0f);
    EXPECT_TRUE(c.alpha <= 0.0f);
}

TEST(critter, butterfly_wander_stays_near_spawn) {
    std::vector<Critter> v;
    CritterSpawn(343, 100.0f, 2.0f, 100.0f, v);
    Critter& c = v[0];
    c.particleH = 1.0f;
    c.particleV = 2.0f;
    c.circleSpeed = 6.0f;
    float maxD = 0.0f;
    for (uint32_t t = 0; t < 20000; t += 250) {
        CritterFrameMove(c, t, 0.0f, 0.0f);
        const float dx = c.curX - c.x, dz = c.curZ - c.z;
        const float d = sqrtf(dx * dx + dz * dz);
        if (d > maxD) maxD = d;
    }
    EXPECT_TRUE(maxD < 2.0f);   // bounded wander (H=1 -> <= ~1.0)
    EXPECT_TRUE(maxD > 0.01f);  // but it does move
}

