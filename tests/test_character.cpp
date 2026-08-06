#include "test_framework.h"

#include "world/AniSound.h"
#include "world/Character.h"
#include "platform/Platform.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using tmx::AniSoundData;
using tmx::Character;
using tmx::CharacterAnimationCache;
using tmx::CharDesc;
using tmx::CharMotion;
using tmx::ParseAniSound;

namespace {

std::string ReadText(const char* path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;
    return ss.str();
}

std::string ReadTextAsset(const char* rel, bool& ok) {
    FILE* f = tmx::OpenAsset(rel, "rb");
    if (!f) { ok = false; return {}; }
    std::ostringstream ss;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        ss.write(buf, (std::streamsize)n);
    fclose(f);
    ok = true;
    return ss.str();
}

struct Fixture {
    AniSoundData ani;
    CharacterAnimationCache cache;
    std::vector<int8_t> mask;   // 128x128 flat at height 10
    bool ok = false;

    Fixture() {
        tmx::SetDataDir(TM_REPO_ROOT);
        bool okA = false, okB = false;
        const std::string aniTxt = ReadText(TM_REPO_ROOT "/AniSound4.txt", okA);
        const std::string boneTxt = ReadTextAsset("Mesh/BoneAni4.txt", okB);
        if (!okA || !okB)
            return;
        std::string err;
        if (!ParseAniSound(aniTxt, ani, &err))
            return;
        if (!cache.Init(boneTxt, &err))
            return;
        mask.assign(128 * 128, 10);
        ok = true;
    }
};

}

TEST(character, humanoid_motion_mapping) {
    Fixture f;
    if (!f.ok) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    Character c;
    CharDesc d;              // ch01, class 0 (TK), weapon 0, all-zero look
    std::string err;
    if (!c.InitLogic(f.cache, f.ani, d, f.mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem ch01 — pulando: %s)\n", err.c_str());
        return;
    }

    // STAND01: human table ani 0 -> animMap[0][0] = cut 0.
    EXPECT_TRUE(c.SetMotion(CharMotion::Stand01, 0));
    EXPECT_EQ(c.Mesh().pb.cut, 0);
    EXPECT_EQ(c.Mesh().pb.fps, 20u);   // Knight STAND01 speed

    // WALK: table ani 2 -> cut 2, fps 13.
    EXPECT_TRUE(c.SetMotion(CharMotion::Walk, 100));
    EXPECT_EQ(c.Mesh().pb.cut, 2);
    EXPECT_EQ(c.Mesh().pb.fps, 13u);

    // Same motion again: no-op success, keeps playing.
    EXPECT_TRUE(c.SetMotion(CharMotion::Walk, 200));
    EXPECT_EQ(c.Mesh().pb.cut, 2);
}

TEST(character, monster_motion_direct_cut) {
    Fixture f;
    if (!f.ok) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    Character c;
    CharDesc d;
    d.boneAniIndex = 2;      // or01
    std::string err;
    if (!c.InitLogic(f.cache, f.ani, d, f.mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem or01 — pulando: %s)\n", err.c_str());
        return;
    }
    // orc STAND01: ani 0 (direct cut index), speed 20.
    EXPECT_TRUE(c.SetMotion(CharMotion::Stand01, 0));
    EXPECT_EQ(c.Mesh().pb.cut, 0);
    // orc WALK: ani 1.
    EXPECT_TRUE(c.SetMotion(CharMotion::Walk, 100));
    EXPECT_EQ(c.Mesh().pb.cut, 1);
}

TEST(character, route_walk_advances_and_stops) {
    Fixture f;
    if (!f.ok) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    Character c;
    CharDesc d;
    d.maxSpeed = 2.0f;       // walk: 500ms per cell
    std::string err;
    if (!c.InitLogic(f.cache, f.ani, d, f.mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem ch01 — pulando)\n");
        return;
    }

    c.SetPosition(10.5f, 10.5f);
    EXPECT_TRUE(c.MoveTo(15.5f, 10.5f, 0));    // 5 cells east
    EXPECT_TRUE(c.Moving());
    EXPECT_EQ(c.Motion(), CharMotion::Walk);

    // Half-way through the first segment.
    c.FrameMove(250);
    EXPECT_TRUE(c.Moving());
    EXPECT_TRUE(fabsf(c.X() - 11.0f) < 0.01f);

    // Finish: 5 segments * 500ms.
    c.FrameMove(2500 + 1);
    EXPECT_TRUE(!c.Moving());
    EXPECT_TRUE(fabsf(c.X() - 15.5f) < 0.01f);
    EXPECT_TRUE(fabsf(c.Z() - 10.5f) < 0.01f);
    EXPECT_EQ(c.Motion(), CharMotion::Stand01);
    // Flat mask height 10 -> y = 1.0.
    EXPECT_TRUE(fabsf(c.Height() - 1.0f) < 0.01f);
}

TEST(character, run_uses_run_motion_and_faster) {
    Fixture f;
    if (!f.ok) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    Character c;
    CharDesc d;
    d.maxSpeed = 4.0f;       // run: 250ms per cell
    std::string err;
    if (!c.InitLogic(f.cache, f.ani, d, f.mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem ch01 — pulando)\n");
        return;
    }
    c.SetPosition(10.5f, 10.5f);
    EXPECT_TRUE(c.MoveTo(13.5f, 10.5f, 0));
    EXPECT_EQ(c.Motion(), CharMotion::Run);
    c.FrameMove(750 + 1);    // 3 segments * 250ms
    EXPECT_TRUE(!c.Moving());
    EXPECT_TRUE(fabsf(c.X() - 13.5f) < 0.01f);
}

TEST(character, heading_turns_gradually) {
    Fixture f;
    if (!f.ok) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    Character c;
    CharDesc d;
    std::string err;
    if (!c.InitLogic(f.cache, f.ani, d, f.mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem ch01 — pulando)\n");
        return;
    }
    c.SetPosition(10.5f, 10.5f);
    c.SetAngle(3.14159265f);          // facing west
    EXPECT_TRUE(c.MoveTo(14.5f, 10.5f, 0));   // target east
    const float before = c.Angle();
    c.FrameMove(16);
    // Turned towards east (angle decreasing from pi towards 0) but not instantly.
    EXPECT_TRUE(c.Angle() < before);
    EXPECT_TRUE(c.Angle() > 0.5f);
}
