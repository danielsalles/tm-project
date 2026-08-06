#include "test_framework.h"

#include "world/SkillEffect.h"
#include "world/SkillFx.h"
#include "gl/EffectRenderer.h"

#include <memory>

using tmx::SkillEffect;
using tmx::SkillCtx;
using tmx::EffectContainer;
using tmx::SkillGlow;
using tmx::SkillBurst;
using tmx::MakeSkillEffect;

namespace {

// Recording mock effect for container-mechanics tests. Counts tick/render/cull
// calls and reports a programmable alive/visible state so we can assert sweeps.
class MockFx : public SkillEffect {
public:
    MockFx(bool expiresImmediately, bool visible)
        : m_alive(!expiresImmediately), m_visible(visible) {}
    int  ticks = 0, renders = 0;
    void forceExpire() { m_alive = false; }
    bool FrameMove(uint32_t, const SkillCtx&) override { ++ticks; return m_alive; }
    void Render(const SkillCtx&) override { ++renders; }
    bool IsVisible(const SkillCtx&) const override { return m_visible; }
private:
    bool m_alive, m_visible;
};

SkillCtx Ctx() {
    SkillCtx c;
    c.camYawH = 0; c.camPitchV = 0;
    return c;
}

} // namespace

TEST(spawneffect, add_and_count) {
    EffectContainer ec;
    EXPECT_EQ(ec.Count(), 0u);
    ec.Add(std::make_unique<MockFx>(false, true));
    ec.Add(std::make_unique<MockFx>(false, true));
    EXPECT_EQ(ec.Count(), 2u);
}

TEST(spawneffect, tick_keeps_alive_effects) {
    EffectContainer ec;
    auto keep = std::make_unique<MockFx>(false, true);
    MockFx* kp = keep.get();
    ec.Add(std::move(keep));
    ec.FrameMove(0, Ctx());
    EXPECT_EQ(ec.Count(), 1u);
    EXPECT_EQ(kp->ticks, 1);
}

TEST(spawneffect, expired_effects_removed) {
    EffectContainer ec;
    ec.Add(std::make_unique<MockFx>(true, true));   // expires immediately
    ec.Add(std::make_unique<MockFx>(false, true));  // stays
    ec.FrameMove(0, Ctx());
    EXPECT_EQ(ec.Count(), 1u);
}

TEST(spawneffect, render_skips_invisible) {
    EffectContainer ec;
    auto vis = std::make_unique<MockFx>(false, true);
    auto inv = std::make_unique<MockFx>(false, false);
    MockFx* visP = vis.get();
    MockFx* invP = inv.get();
    ec.Add(std::move(vis));
    ec.Add(std::move(inv));
    ec.Render(Ctx());
    EXPECT_EQ(visP->renders, 1);
    EXPECT_EQ(invP->renders, 0);
}

TEST(spawneffect, children_appended_during_tick_survive) {
    // A FrameMove that adds a child to the same container must not be lost when
    // the parent sweep finishes (doc 19 §2 — index loop + survivor list).
    EffectContainer ec;
    class Parent : public SkillEffect {
    public:
        EffectContainer* host; bool spawned = false;
        bool FrameMove(uint32_t now, const SkillCtx& c) override {
            if (!spawned && host) { host->Add(std::make_unique<MockFx>(false, true)); spawned = true; }
            return true;
        }
    };
    auto p = std::make_unique<Parent>();
    p->host = &ec;
    ec.Add(std::move(p));
    ec.FrameMove(0, Ctx());
    EXPECT_EQ(ec.Count(), 2u);   // parent + child
}

TEST(spawneffect, skillglow_emits_quad_per_frame) {
    // Emit() only pushes to a vector; no GL context needed, so an uninitialized
    // EffectRenderer doubles as a headless emit sink.
    tmx::EffectRenderer fx;
    SkillCtx c = Ctx();
    c.fx = &fx;
    auto g = MakeSkillEffect<SkillGlow>(0, 5.0f, 10.0f, 20.0f, 56, 700, 0.8f, 0xFFFFFFFFu);
    g->FrameMove(0, c);
    EXPECT_EQ(fx.Pending(), 0u);
    g->Render(c);
    EXPECT_EQ(fx.Pending(), 1u);
}

TEST(spawneffect, skillglow_expires_at_lifetime) {
    SkillCtx c = Ctx();
    auto g = MakeSkillEffect<SkillGlow>(0, 0, 0, 0, 56, 700, 0.8f, 0xFFFFFFFFu);
    EXPECT_TRUE(g->FrameMove(0, c));       // t=0 alive
    EXPECT_TRUE(g->FrameMove(699, c));     // just before expiry
    EXPECT_FALSE(g->FrameMove(701, c));    // past lifetime -> dead
}

TEST(spawneffect, skillburst_expands_and_emits_count) {
    tmx::EffectRenderer fx;
    SkillCtx c = Ctx();
    c.fx = &fx;
    auto b = MakeSkillEffect<SkillBurst>(0, 0.0f, 0.0f, 0.0f, 8, 600, 10, 1.0f, 0.5f, 0xFFFFFFFFu);
    b->FrameMove(0, c);    // progress 0
    b->Render(c);
    EXPECT_EQ(fx.Pending(), 10u);
    fx.Clear();
    b->FrameMove(600, c);  // past lifetime -> should be dead
    EXPECT_FALSE(b->FrameMove(601, c));
}
