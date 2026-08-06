#include "test_framework.h"

#include "world/SkinMesh.h"
#include "world/BoneAnimation.h"
#include "platform/Platform.h"

#include <cstdio>
#include <cstring>
#include <vector>

using tmx::MshData;

namespace {

void U32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v & 0xFF));
    b.push_back((uint8_t)((v >> 8) & 0xFF));
    b.push_back((uint8_t)((v >> 16) & 0xFF));
    b.push_back((uint8_t)((v >> 24) & 0xFF));
}

// Synthetic .msh: 8xu32 header, palette (mats + ids), verts, u16 indices.
std::vector<uint8_t> BuildMsh(uint32_t nInfluence, uint32_t nPalette,
                              uint32_t nVerts, uint32_t nIndices) {
    const uint32_t vsize = 36 + 4 * (nInfluence - 1);
    std::vector<uint8_t> b;
    U32(b, 0xFFFFFFFF);       // parentID
    U32(b, 4);                // id
    U32(b, 4376);             // fvf (informational here)
    U32(b, vsize);
    U32(b, nInfluence);
    U32(b, nPalette);
    U32(b, nVerts);
    U32(b, nIndices);
    for (uint32_t m = 0; m < nPalette; ++m)
        for (int k = 0; k < 16; ++k)
            U32(b, k == 0 ? 0x3F800000 : 0);   // ~identity diagonal start
    for (uint32_t i = 0; i < nPalette; ++i)
        U32(b, i + 1);                          // frame ids
    b.resize(b.size() + (size_t)nVerts * vsize, 0);   // zero verts
    for (uint32_t i = 0; i < nIndices; ++i) {
        b.push_back((uint8_t)(i & 0xFF));
        b.push_back((uint8_t)((i >> 8) & 0xFF));
    }
    return b;
}

std::vector<uint8_t> ReadFile(const char* path, bool& ok) {
    ok = false;
    FILE* f = fopen(path, "rb");
    if (!f)
        return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> b((size_t)sz);
    ok = fread(b.data(), 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    return b;
}

} // namespace

TEST(msh, parses_synthetic) {
    auto blob = BuildMsh(2, 7, 100, 300);
    MshData d;
    std::string err;
    EXPECT_TRUE(tmx::ParseMsh(blob.data(), blob.size(), d, &err));
    EXPECT_EQ(d.vsize, 40u);
    EXPECT_EQ(d.numInfluence, 2u);
    EXPECT_EQ(d.numPalette, 7u);
    EXPECT_EQ(d.boneFrameId[6], 7u);
    EXPECT_EQ(d.vertices.size(), 100u * 40u);
    EXPECT_EQ(d.indices.size(), 300u);
    EXPECT_EQ(d.indices[299], 299);
    // Layout offsets for N=2: weights@12, indices@16, normal@20, uv@32
    EXPECT_EQ(d.OffWeights(), 12u);
    EXPECT_EQ(d.OffIndices(), 16u);
    EXPECT_EQ(d.OffNormal(), 20u);
    EXPECT_EQ(d.OffUV(), 32u);
}

TEST(msh, high_palette_not_clamped) {
    // Regression (phase 5 fs01 fish fix): meshes with numPalette > 40 must be
    // accepted and keep the full palette. The old GLSkinMesh::Upload clamped to
    // 40, deforming high-bone meshes (fs01, big monsters). The parser must not
    // reject palettes up to kMaxBones (64).
    auto blob = BuildMsh(4, 50, 4, 6);
    MshData d;
    std::string err;
    EXPECT_TRUE(tmx::ParseMsh(blob.data(), blob.size(), d, &err));
    EXPECT_EQ(d.numPalette, 50u);
    EXPECT_EQ(d.boneFrameId[49], 50u);
}

TEST(msh, stride_table_matches_vertexdecl) {
    // N=1..4 -> 36/40/44/48 (RenderDevice.cpp VertexDecl1-4)
    for (uint32_t n = 1; n <= 4; ++n) {
        auto blob = BuildMsh(n, 0, 3, 3);
        MshData d;
        EXPECT_TRUE(tmx::ParseMsh(blob.data(), blob.size(), d, nullptr));
        EXPECT_EQ(d.vsize, 36u + 4u * (n - 1u));
    }
}

TEST(msh, rejects_garbage) {
    MshData d;
    auto bad = BuildMsh(5, 0, 3, 3);       // influence out of range
    EXPECT_FALSE(tmx::ParseMsh(bad.data(), bad.size(), d, nullptr));
    bad = BuildMsh(2, 0, 3, 3);
    bad[12] = 99;                          // vsize mismatch
    EXPECT_FALSE(tmx::ParseMsh(bad.data(), bad.size(), d, nullptr));
    auto trunc = BuildMsh(2, 7, 100, 300);
    trunc.resize(trunc.size() - 10);
    EXPECT_FALSE(tmx::ParseMsh(trunc.data(), trunc.size(), d, nullptr));
}

TEST(msh, real_tree_part_parses) {
    bool ok = false;
    auto blob = ReadFile(TM_REPO_ROOT "/mesh/tr010101.msh", ok);
    if (!ok) {
        printf("      [skip] mesh/tr010101.msh not present\n");
        return;
    }
    MshData d;
    std::string err;
    EXPECT_TRUE(tmx::ParseMsh(blob.data(), blob.size(), d, &err));
    EXPECT_EQ(d.numInfluence, 2u);
    EXPECT_EQ(d.numPalette, 7u);
    EXPECT_EQ(d.numVerts, 1426u);
    EXPECT_EQ(d.indices.size(), 1602u);
    // Bone frame ids must be valid frame ids of tr01.bon (0..19)
    for (uint32_t id : d.boneFrameId)
        EXPECT_TRUE(id < 20);
}

TEST(bonani, real_tree_loads_and_samples) {
    std::string err;
    tmx::BoneAniSet set;
    tmx::SetDataDir(TM_REPO_ROOT);   // assets live at the repo root, not build/
    // mesh\tr01: 20 frames, 15 ticks, root 0 (validated in doc 16 §2.5-2.6)
    if (!tmx::LoadBoneAni("mesh\\tr01", 101, set, &err)) {
        printf("      [skip] %s\n", err.c_str());
        return;
    }
    EXPECT_EQ(set.frames.size(), 20u);
    EXPECT_EQ(set.numTicks, 15);
    EXPECT_EQ(set.numBones, 20);
    EXPECT_EQ((int)set.frames[0].children.size(), 7);  // trunk branches

    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    tmx::SampleBoneAni(set, 0.0f, world);
    // combined[0] == world; children get tick-0 poses multiplied down
    EXPECT_TRUE(std::fabs(set.combined[0]._11 - 1.0f) < 1e-6f);
    // pose actually changes over time (trees sway): sample far ahead and compare
    D3DXMATRIX before = set.combined[set.numBones - 1];
    tmx::SampleBoneAni(set, 1600.0f, world);  // 1600ms = 2 ticks at 80fps*4
    const D3DXMATRIX& after = set.combined[set.numBones - 1];
    float diff = std::fabs(before._11 - after._11) + std::fabs(before._12 - after._12) +
                 std::fabs(before._13 - after._13);
    EXPECT_TRUE(diff > 1e-6f);
}
