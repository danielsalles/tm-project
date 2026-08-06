#include "test_framework.h"

#include "world/SkillFx.h"
#include "world/SkillEffect.h"

#include <cmath>

using tmx::SkillMeshTypeAnim;
using tmx::DecalFade;
using tmx::SkillMeshFx;
using tmx::SkillMeshDesc;
using tmx::GroundDecalFx;
using tmx::GroundDecalDesc;
using tmx::MakeSkillEffect;
using tmx::SkillCtx;

namespace { const float kPi = 3.1415927f; }

TEST(skillmesh, type0_static_keeps_base) {
    float sh, sv, ang;
    SkillMeshTypeAnim(0, 0.5f, 1.0f, 1.0f, 0.7f, &sh, &sv, &ang);
    EXPECT_NEAR(sh, 1.0f, 1e-4f);
    EXPECT_NEAR(sv, 1.0f, 1e-4f);
    EXPECT_NEAR(ang, 0.7f, 1e-4f);
}

TEST(skillmesh, type2_expands_and_rotates) {
    // type 2: scaleV = scaleH*progress*3, angle = progress*pi
    float sh, sv, ang;
    SkillMeshTypeAnim(2, 0.5f, 1.0f, 1.0f, 0.0f, &sh, &sv, &ang);
    EXPECT_NEAR(sh, 1.0f, 1e-4f);
    EXPECT_NEAR(sv, 1.5f, 1e-4f);          // 1.0 * 0.5 * 3
    EXPECT_NEAR(ang, kPi * 0.5f, 1e-4f);
}

TEST(skillmesh, type3_rotation_only) {
    float sh, sv, ang;
    SkillMeshTypeAnim(3, 0.25f, 1.0f, 1.0f, 0.0f, &sh, &sv, &ang);
    EXPECT_NEAR(ang, kPi * 0.25f, 1e-4f);
    EXPECT_NEAR(sh, 1.0f, 1e-4f);
}

TEST(skillmesh, type4_grow_then_settle) {
    float sh1, sv1, a1;
    SkillMeshTypeAnim(4, 0.1f, 1.0f, 1.0f, 0.0f, &sh1, &sv1, &a1);
    EXPECT_NEAR(sh1, 1.0f, 1e-3f);          // 0.1*5+0.5 = 1.0
    float sh2, sv2, a2;
    SkillMeshTypeAnim(4, 1.0f, 1.0f, 1.0f, 0.0f, &sh2, &sv2, &a2);
    // progress 1.0 -> sin((0.8)*pi/2)=sin(0.4pi) ~= 0.951; (s+1.5)
    EXPECT_NEAR(sh2, (sinf(0.8f * kPi * 0.5f) + 1.5f), 1e-3f);
}

TEST(skillmesh, lifetime_expires) {
    SkillCtx c;
    SkillMeshDesc d; d.meshIndex = 0; d.lifeTime = 500; d.cycleTime = 200;
    auto fx = MakeSkillEffect<SkillMeshFx>(0, 0.0f, 0.0f, 0.0f, d);
    EXPECT_TRUE(fx->FrameMove(0, c));
    EXPECT_TRUE(fx->FrameMove(499, c));
    EXPECT_FALSE(fx->FrameMove(501, c));
}

TEST(skillmesh, progress_wraps_at_cycletime) {
    SkillCtx c;
    SkillMeshDesc d; d.meshIndex = 0; d.lifeTime = 0; d.cycleTime = 200;
    auto fx = MakeSkillEffect<SkillMeshFx>(0, 0.0f, 0.0f, 0.0f, d);
    fx->FrameMove(0, c);
    // progress = (elapsed % cycle)/cycle
    EXPECT_TRUE(fx->FrameMove(250, c));     // lifeTime 0 = never expires
}

TEST(decal, fade_curves) {
    // fi=1: |sin(progress*pi)| — 0 at 0/1, 1 at 0.5
    EXPECT_NEAR(DecalFade(0.0f, 1), 0.0f, 1e-4f);
    EXPECT_NEAR(DecalFade(0.5f, 1), 1.0f, 1e-4f);
    EXPECT_NEAR(DecalFade(1.0f, 1), 0.0f, 1e-4f);
    // fi=0: |cos(progress*pi/2)| — 1 at 0, 0 at 1
    EXPECT_NEAR(DecalFade(0.0f, 0), 1.0f, 1e-4f);
    EXPECT_NEAR(DecalFade(1.0f, 0), 0.0f, 1e-4f);
}

TEST(decal, lifetime_expires) {
    SkillCtx c;
    GroundDecalDesc d; d.gridNum = 4; d.lifeTime = 800;
    auto fx = MakeSkillEffect<GroundDecalFx>(0, 10.0f, 20.0f, d);
    EXPECT_TRUE(fx->FrameMove(0, c));
    EXPECT_TRUE(fx->FrameMove(799, c));
    EXPECT_FALSE(fx->FrameMove(801, c));
}
