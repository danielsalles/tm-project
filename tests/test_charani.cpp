#include "test_framework.h"

#include "world/CharacterAnimation.h"
#include "platform/Platform.h"

#include <cstdio>
#include <cstring>
#include <vector>

using tmx::CharacterAnimation;
using tmx::CharPlayback;
using tmx::CharSetAnimation;
using tmx::LoadCharacterAnimation;
using tmx::LoadValidIndex;
using tmx::SampleCharacter;

namespace {

uint32_t AniTicks(const char* path) {
    FILE* f = tmx::OpenAsset(path, "rb");
    if (!f)
        return 0;
    uint8_t h[8];
    bool ok = fread(h, 1, 8, f) == 8;
    fclose(f);
    if (!ok)
        return 0;
    return (uint32_t)h[0] | ((uint32_t)h[1] << 8) | ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
}

}

TEST(charani, validindex_loads) {
    tmx::SetDataDir(TM_REPO_ROOT);
    std::vector<int32_t> vi;
    std::string err;
    if (!LoadValidIndex("Mesh/ValidIndex.bin", vi, &err)) {
        printf("      (ValidIndex.bin ausente — pulando)\n");
        return;
    }
    EXPECT_EQ(vi.size(), 100u * 186u);
    // Row 0 (ch01) starts at 100; row 63 (tr01) starts at 100 too.
    EXPECT_EQ(vi[0], 100);
    EXPECT_EQ(vi[63 * 186], 100);
}

TEST(charani, ch01_loads_all_cuts) {
    tmx::SetDataDir(TM_REPO_ROOT);
    std::vector<int32_t> vi;
    std::string err;
    if (!LoadValidIndex("Mesh/ValidIndex.bin", vi, &err)) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }

    CharacterAnimation a;
    if (!LoadCharacterAnimation(0, "mesh\\ch01", 186, vi.data(), true, a, &err)) {
        printf("      (sem ch01 — pulando: %s)\n", err.c_str());
        return;
    }

    // All 186 declared files exist in this build.
    EXPECT_EQ(a.numCuts, 186);
    EXPECT_EQ(a.numBones, 47u);          // ch01.bon = 376B / 8
    EXPECT_EQ(a.base.frames.size(), 47u);
    EXPECT_TRUE(!a.quats.empty());
    EXPECT_EQ(a.quats.size(), a.mats.size());

    // Cut 0 = ch010101.ani: numAniCut matches the file header; offsets accumulate.
    const uint32_t t0 = AniTicks("mesh\\ch010101.ani");
    const uint32_t t1 = AniTicks("mesh\\ch010102.ani");
    EXPECT_TRUE(t0 > 0 && t1 > 0);
    if (t0 == 0 || t1 == 0)
        return;
    EXPECT_EQ(a.numAniCut[0], t0);
    EXPECT_EQ(a.numAniCut[1], t1);
    EXPECT_EQ(a.cutTickOffset[0], 0u);
    EXPECT_EQ(a.cutTickOffset[1], t0);
    EXPECT_EQ(a.mats.size(), (size_t)a.numBones * (a.cutTickOffset[185] + a.numAniCut[185]));

    // animMap: weapon 0 motions 0/1/2 -> cuts 0/1/2 (files 101/102/103).
    EXPECT_EQ(a.animMap[0][0], 0);
    EXPECT_EQ(a.animMap[0][1], 1);
    EXPECT_EQ(a.animMap[0][2], 2);
    // Last row0 entry (nI=1610 -> nArrayIndex 1611): weapon 15, motion 10, last cut.
    EXPECT_EQ(a.animMap[15][10], 185);
    // Weapons >= 12 inherit die/dead/levelup from weapon 11.
    EXPECT_EQ(a.animMap[12][11], a.animMap[11][11]);
    EXPECT_EQ(a.animMap[15][14], a.animMap[11][14]);

    printf("      ch01: %d cuts, %u bones, %zu matrices\n",
           a.numCuts, a.numBones, a.mats.size());
}

TEST(charani, ch01_sampling_and_crossfade) {
    tmx::SetDataDir(TM_REPO_ROOT);
    std::vector<int32_t> vi;
    std::string err;
    if (!LoadValidIndex("Mesh/ValidIndex.bin", vi, &err)) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    CharacterAnimation a;
    if (!LoadCharacterAnimation(0, "mesh\\ch01", 186, vi.data(), true, a, &err)) {
        printf("      (sem ch01 — pulando)\n");
        return;
    }

    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);

    // t=0, no crossfade: matRot == first tick of cut 0 for every bone.
    tmx::CharacterPose pose;
    pose.Resize(a.base.frames.size());
    CharPlayback pb;
    pb.fps = 20;
    SampleCharacter(a, pose, pb, 0, world);
    for (uint32_t j = 0; j < a.numBones; ++j) {
        const float* got = &pose.matRot[j]._11;
        const float* want = &a.mats[j]._11;
        for (int k = 0; k < 16; ++k)
            EXPECT_TRUE(got[k] == want[k]);
    }

    // Sub-step 1 of 4: (3*cur + next)/4.
    SampleCharacter(a, pose, pb, 20, world);
    {
        const float* o = &pose.matRot[1]._11;
        const float* cur = &a.mats[a.numBones * 0 + 1]._11;
        const float* nxt = &a.mats[a.numBones * 1 + 1]._11;
        for (int k = 0; k < 16; ++k)
            EXPECT_TRUE(fabsf(o[k] - (3.0f * cur[k] + nxt[k]) / 4.0f) < 1e-5f);
    }

    // Tick wrap inside the cut: at the last sub-steps, next wraps to cut start.
    const uint32_t cut0Ticks = a.numAniCut[0];
    const uint32_t endMs = (4 * cut0Ticks - 1) * 20;  // last sub-step of last tick
    SampleCharacter(a, pose, pb, endMs, world);
    {
        const float* o = &pose.matRot[1]._11;
        const float* cur = &a.mats[a.numBones * (cut0Ticks - 1) + 1]._11;
        const float* nxt = &a.mats[1]._11;  // wrapped to cut 0 tick 0
        for (int k = 0; k < 16; ++k)
            EXPECT_TRUE(fabsf(o[k] - (cur[k] + 3.0f * nxt[k]) / 4.0f) < 1e-5f);
    }

    // Cut switch at t=2000: frozen pose = old cut's tick at switch moment.
    const uint32_t switchMs = 2000;
    const uint32_t oldTick = (switchMs / 20) / 4 % cut0Ticks;
    EXPECT_TRUE(CharSetAnimation(a, pb, 1, 20, switchMs));
    EXPECT_EQ(pb.tickLastFlat, oldTick);
    EXPECT_TRUE(pb.blending);

    // Crossfade step 0 (dwOffset=0): pure old pose (slerp t=1 -> old quat,
    // translation = old). Rotation may differ numerically from matrix copy, so
    // compare translations exactly and rotation loosely via quats.
    SampleCharacter(a, pose, pb, switchMs, world);
    {
        const float* o = &pose.matRot[1]._41;
        const float* oldM = &a.mats[a.numBones * oldTick + 1]._41;
        for (int k = 0; k < 3; ++k)
            EXPECT_TRUE(fabsf(o[k] - oldM[k]) < 1e-4f);
    }
    // Crossfade over: dwOffset=10 -> tick 2, sub-step 2 -> (tick2 + tick3)/2.
    SampleCharacter(a, pose, pb, switchMs + 10 * 20, world);
    {
        const float* o = &pose.matRot[1]._11;
        const float* t2 = &a.mats[a.numBones * (a.cutTickOffset[1] + 2) + 1]._11;
        const float* t3 = &a.mats[a.numBones * (a.cutTickOffset[1] + 3) + 1]._11;
        for (int k = 0; k < 16; ++k)
            EXPECT_TRUE(fabsf(o[k] - (t2[k] + t3[k]) / 2.0f) < 1e-4f);
    }
}

TEST(charani, monster_no_quats_no_animmap) {
    tmx::SetDataDir(TM_REPO_ROOT);
    std::vector<int32_t> vi;
    std::string err;
    if (!LoadValidIndex("Mesh/ValidIndex.bin", vi, &err)) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    // or01 = index 2, 55 declared cuts.
    CharacterAnimation a;
    if (!LoadCharacterAnimation(2, "mesh\\or01", 55, vi.data() + 2 * 186, false, a, &err)) {
        printf("      (sem or01 — pulando: %s)\n", err.c_str());
        return;
    }
    EXPECT_TRUE(a.numCuts > 0);
    EXPECT_TRUE(a.quats.empty());
    EXPECT_EQ(a.animMap[0][0], -1);
    // Row 2 first values are 100..106,108...: file or010101.ani must exist.
    EXPECT_EQ(a.numAniCut[0], AniTicks("mesh\\or010101.ani"));
    printf("      or01: %d cuts, %u bones\n", a.numCuts, a.numBones);
}
