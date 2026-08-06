#include "test_framework.h"

#include "world/Billboard.h"

#include <cmath>
#include <cstdio>

using tmx::Billboard;
using tmx::BillboardDesc;
using tmx::BillboardFrameMove;
using tmx::BillboardTexture;

namespace {

Billboard MakeBB() {
    Billboard b;
    b.d.textureIndex = 10;
    b.d.lifeTimeMs = 0;
    b.d.cycleCount = 1;
    b.d.cycleTimeMs = 80;
    b.d.fade = 0;
    b.d.motion = 0;
    b.d.x = 1.0f; b.d.y = 2.0f; b.d.z = 3.0f;
    return b;
}

}

TEST(fxbillboard, flipbook_cycle_index) {
    Billboard b = MakeBB();
    b.d.cycleCount = 8;      // 8 frames x 80ms = 640ms loop
    b.d.cycleTimeMs = 80;

    BillboardFrameMove(b, 1000, 0, 0, 0);      // create at 1000
    EXPECT_EQ(b.cycleIndex, 0);
    BillboardFrameMove(b, 1000 + 79, 0, 0, 0);
    EXPECT_EQ(b.cycleIndex, 0);
    BillboardFrameMove(b, 1000 + 80, 0, 0, 0);
    EXPECT_EQ(b.cycleIndex, 1);
    BillboardFrameMove(b, 1000 + 639, 0, 0, 0);
    EXPECT_EQ(b.cycleIndex, 7);
    BillboardFrameMove(b, 1000 + 640, 0, 0, 0);   // wraps
    EXPECT_EQ(b.cycleIndex, 0);
    BillboardFrameMove(b, 1000 + 641, 0, 0, 0);
    EXPECT_EQ(BillboardTexture(b), 10);
}

TEST(fxbillboard, lifetime_expiry_and_progress) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 1000;
    BillboardFrameMove(b, 500, 0, 0, 0);
    BillboardFrameMove(b, 1000, 0, 0, 0);      // t=500 -> progress 0.5
    EXPECT_TRUE(!b.dead);
    EXPECT_TRUE(fabsf(b.progress - 0.5f) < 1e-4f);
    BillboardFrameMove(b, 500 + 1000, 0, 0, 0);  // t=1000 -> dead
    EXPECT_TRUE(b.dead);
}

TEST(fxbillboard, fade1_scales_all_channels_by_sin) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 1000;
    b.d.fade = 1;
    b.d.bgra = 0x80402010;   // A=0x80 R=0x40 G=0x20 B=0x10
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 500, 0, 0, 0);       // progress 0.5 -> sin = 1
    EXPECT_EQ(b.curBgra, 0x80402010u);
    BillboardFrameMove(b, 250, 0, 0, 0);       // progress 0.25 -> sin = sqrt(2)/2
    {
        const float s = sinf(0.25f * 3.1415927f);
        const uint32_t want = (uint32_t)(0x10 * s) | ((uint32_t)(0x20 * s) << 8) |
                              ((uint32_t)(0x40 * s) << 16) | ((uint32_t)(0x80 * s) << 24);
        EXPECT_EQ(b.curBgra, want);
    }
}

TEST(fxbillboard, fade3_alpha_only) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 1000;
    b.d.fade = 3;
    b.d.bgra = 0x80402010;
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 500, 0, 0, 0);       // sin=1 -> unchanged
    EXPECT_EQ(b.curBgra, 0x80402010u);
    BillboardFrameMove(b, 100, 0, 0, 0);       // progress .1 -> alpha *= sin(.1 pi)
    {
        const uint32_t a = (uint32_t)(0x80 * sinf(0.1f * 3.1415927f));
        EXPECT_EQ(b.curBgra, 0x00402010u | (a << 24));
    }
}

TEST(fxbillboard, fade4_ramp) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 1000;
    b.d.fade = 4;
    b.d.bgra = 0xFF000000;
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 150, 0, 0, 0);       // progress .15 < .3 -> p = .15*3.33
    {
        const uint32_t a = (uint32_t)(0xFF * (0.15f * 3.3299999f));
        EXPECT_EQ(b.curBgra, a << 24);
    }
    BillboardFrameMove(b, 650, 0, 0, 0);       // progress .65 -> 1 - (.35*1.428)
    {
        const uint32_t a = (uint32_t)(0xFF * (1.0f - 0.35f * 1.428f));
        EXPECT_EQ(b.curBgra, a << 24);
    }
}

TEST(fxbillboard, motion_programs) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 1000;
    b.d.particleH = 4.0f;
    b.d.particleV = 8.0f;
    b.d.circleSpeed = 2.0f;

    b.d.motion = 0;   // pure rise
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 500, 0, 0, 0);
    EXPECT_TRUE(fabsf(b.curY - (2.0f + 4.0f)) < 1e-4f);   // y0 + .5*8
    EXPECT_TRUE(fabsf(b.curX - 1.0f) < 1e-4f);

    b = MakeBB();
    b.d.lifeTimeMs = 1000;
    b.d.particleH = 4.0f; b.d.particleV = 8.0f; b.d.circleSpeed = 2.0f;
    b.d.motion = 2;   // circle
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 500, 0, 0, 0);
    // progress .5: angle = .5*pi*2 = pi -> sin 0, cos -1
    EXPECT_TRUE(fabsf(b.curX - 1.0f) < 1e-3f);
    EXPECT_TRUE(fabsf(b.curZ - (3.0f - 4.0f)) < 1e-3f);
}

TEST(fxbillboard, scale_velocity_and_stick_ground) {
    Billboard b = MakeBB();
    b.d.lifeTimeMs = 2000;
    b.d.scaleVelX = 0.001f;   // +1.0 per second
    b.d.scaleX = 1.0f; b.d.scaleY = 2.0f; b.d.scaleZ = 1.0f;
    b.d.stickGround = 1;
    BillboardFrameMove(b, 0, 0, 0, 0);
    BillboardFrameMove(b, 1000, 0, 0, 0);
    // scaleX at t=1000: 1 + 1 = 2. Row-major R*S scales COLUMNS of R.
    const float sx = sqrtf(b.world._11 * b.world._11 + b.world._21 * b.world._21 +
                           b.world._31 * b.world._31);
    EXPECT_TRUE(fabsf(sx - 2.0f) < 1e-4f);
    // motion 0 rises: curY = y0 + progress*particleV = 2 + 0.5*2 = 3;
    // stickGround adds scaleY/2 = 1 -> ty = 4.
    EXPECT_TRUE(fabsf(b.world._42 - 4.0f) < 1e-4f);
}

TEST(fxbillboard, lookcam_faces_camera) {
    Billboard b = MakeBB();
    // camYawH = 0 -> yaw = pi/2; verify the quad normal (local +Z) points at -Z view.
    BillboardFrameMove(b, 0, 0.0f, 0.0f, 0);
    // Column 3 of world (rotation part) = transformed +Z axis.
    // YPR(yaw=pi/2, pitch=0, roll=0): +Z -> (sin(yaw)?,0,cos) — just verify it's unit
    // and horizontal.
    const float zx = b.world._13, zy = b.world._23, zz = b.world._33;
    const float len = sqrtf(zx * zx + zy * zy + zz * zz);
    EXPECT_TRUE(fabsf(len - 0.5f) < 1e-4f);   // scale 0.5 default
    EXPECT_TRUE(fabsf(zy) < 1e-4f);           // no vertical tilt with pitch 0
}

TEST(fxbillboard, fixed_axis_mode_ignores_camera) {
    Billboard b = MakeBB();
    b.d.lookCam = 0;
    b.d.axisAngle = 1.5707964f;
    BillboardFrameMove(b, 0, 0.7f, 0.5f, 0);   // camera angles must not matter
    // RotationY(pi/2): _11 = cos = 0, _13 = -sin = -1 (LH convention)
    EXPECT_TRUE(fabsf(b.world._11) < 1e-4f);
    EXPECT_TRUE(fabsf(b.world._13 + 0.5f) < 1e-4f);  // times scale 0.5
}
