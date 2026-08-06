#include "test_framework.h"

#include "world/SunFlare.h"

#include <cmath>
#include <cstdio>

using tmx::FxQuad;
using tmx::SunFlareBuildTable;
using tmx::SunFlareCompute;
using tmx::SunFlareEntry;

TEST(sunflare, table_matches_original) {
    SunFlareEntry t[12];
    SunFlareBuildTable(1.0f, t);   // reference width: fRatio = 50
    EXPECT_EQ(t[0].texIndex, 206);
    EXPECT_EQ(t[11].texIndex, 208);
    EXPECT_TRUE(fabsf(t[0].loc + 0.6f) < 1e-6f);
    EXPECT_TRUE(fabsf(t[11].loc - 0.6f) < 1e-6f);
    EXPECT_TRUE(fabsf(t[0].scale - 0.4f * 50.0f) < 1e-4f);
    EXPECT_TRUE(fabsf(t[7].scale - 10.6f * 50.0f) < 1e-3f);
    EXPECT_EQ(t[3].bgra, 0xFFFFFFu);
    EXPECT_EQ(t[0].bgra, 0xAA8888u);
}

TEST(sunflare, behind_camera_returns_false) {
    SunFlareEntry t[12];
    SunFlareBuildTable(1.0f, t);
    // Camera at origin looking +Z (identity view, simple LH proj).
    D3DXMATRIX view, proj;
    D3DXMatrixIdentity(&view);
    D3DXMatrixPerspectiveFovLH(&proj, 0.78f, 4.0f / 3.0f, 1.0f, 100.0f);
    FxQuad out[12];
    // Sun anchor = cam + (-1, 0.7, 0.3) — roughly ahead (+Z) => visible.
    EXPECT_TRUE(SunFlareCompute(t, 0, 0, 0, view, proj, 800, 600, 1.0f, out));
    // Rotate view 180° (sun now behind): yaw pi about Y.
    D3DXMatrixRotationY(&view, 3.14159265f);
    EXPECT_TRUE(!SunFlareCompute(t, 0, 0, 0, view, proj, 800, 600, 1.0f, out));
}

TEST(sunflare, quads_are_screenspace_with_flare_blend) {
    SunFlareEntry t[12];
    SunFlareBuildTable(1.0f, t);
    D3DXMATRIX view, proj;
    D3DXMatrixIdentity(&view);
    D3DXMatrixPerspectiveFovLH(&proj, 0.78f, 4.0f / 3.0f, 1.0f, 100.0f);
    FxQuad out[12];
    if (!SunFlareCompute(t, 0, 0, 0, view, proj, 800, 600, 1.0f, out)) {
        printf("      (sun fora da tela — pulando checagem de quads)\n");
        return;
    }
    for (int i = 0; i < 12; ++i) {
        EXPECT_TRUE(out[i].screenSpace);
        EXPECT_EQ(out[i].blendMode, 2);
        EXPECT_EQ(out[i].textureIndex, t[i].texIndex);
    }
    // Central flare (loc 0) sits exactly at the projected anchor.
    EXPECT_TRUE(fabsf(out[3].world._41 - out[4].world._41) < 1e-4f);
    // Negative-loc flares on the opposite side of the anchor vs positive.
    const float dxNeg = out[0].world._41 - out[3].world._41;
    const float dxPos = out[10].world._41 - out[3].world._41;
    EXPECT_TRUE((dxNeg < 0) != (dxPos < 0) || fabsf(dxNeg) < 1e-4f);
}
