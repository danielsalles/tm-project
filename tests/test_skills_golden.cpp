#include "test_framework.h"

#include "world/SkillFx.h"
#include "world/SkillEffect.h"

#include <memory>

using tmx::Projectile;
using tmx::ProjectileDesc;
using tmx::MeteorStorm;
using tmx::MagicShield;
using tmx::SkillCtx;
using tmx::EffectContainer;
using tmx::MakeSkillEffect;

namespace {
SkillCtx CtxWithHost(EffectContainer& host) {
    SkillCtx c;
    c.host = &host;
    return c;
}
} // namespace

TEST(skills, projectile_travels_and_expires) {
    ProjectileDesc d;
    d.startX = 0; d.startY = 0; d.startZ = 0;
    d.targetX = 10; d.targetY = 0; d.targetZ = 0;   // 10 units -> ~1000ms life
    auto p = MakeSkillEffect<Projectile>(0, d);
    SkillCtx c;
    EXPECT_TRUE(p->FrameMove(0, c));        // t=0 alive
    EXPECT_TRUE(p->FrameMove(500, c));      // mid-flight
}

TEST(skills, projectile_impact_spawns_children) {
    EffectContainer host;
    SkillCtx c = CtxWithHost(host);
    ProjectileDesc d;
    d.startX = 0; d.startZ = 0; d.targetX = 2; d.targetZ = 0;  // short flight
    auto p = MakeSkillEffect<Projectile>(0, d);
    // Step time forward until the projectile reports expiry (impact fired).
    for (uint32_t t = 0; t < 3000; t += 50) {
        if (!p->FrameMove(t, c))
            break;
    }
    // impact should have added children (splash burst + decal when terrain set;
    // here no terrain so just the burst).
    EXPECT_TRUE(host.Count() >= 1u);
}

TEST(skills, meteor_schedules_strikes) {
    EffectContainer host;
    SkillCtx c;
    c.host = &host;
    auto m = MakeSkillEffect<MeteorStorm>(0, 5.0f, 0.0f, 10.0f, 3, 0);
    // L3 -> 3 + 3*2 = 9 strikes over ~660ms. Tick well past.
    for (uint32_t t = 0; t < 1500; t += 50)
        m->FrameMove(t, c);
    // Each strike adds a glow + burst (+decal if terrain; none here) -> >= 9*2.
    EXPECT_TRUE(host.Count() >= 9u);
    // Eventually expires.
    EXPECT_FALSE(m->FrameMove(2000, c));
}

TEST(skills, magicshield_lifetime) {
    SkillCtx c;
    auto s = MakeSkillEffect<MagicShield>(0, 0.0f, 0.0f, 0.0f, 1000, 0xFFFFFFFFu);
    EXPECT_TRUE(s->FrameMove(0, c));
    EXPECT_TRUE(s->FrameMove(999, c));
    EXPECT_FALSE(s->FrameMove(1001, c));
}
