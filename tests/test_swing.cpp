#include "test_framework.h"

#include "world/SwingTrail.h"
#include "world/SkillEffect.h"

#include <cmath>

using tmx::SwingTrail;
using tmx::SkillCtx;
using tmx::MakeSkillEffect;

namespace {
// A SegmentFn that emits a sweeping blade (base fixed, tip rotating around it).
bool SweepTip(D3DXVECTOR3& base, D3DXVECTOR3& tip) {
    static float t = 0;
    t += 0.3f;
    base = D3DXVECTOR3(0, 1, 0);
    tip = D3DXVECTOR3(cosf(t) * 1.0f, 1.0f, sinf(t) * 1.0f);
    return true;
}
SkillCtx Ctx() { SkillCtx c; return c; }
} // namespace

TEST(swing, records_segments_during_window) {
    auto tr = MakeSkillEffect<SwingTrail>(0);
    tr->Start(0, 300, SweepTip);
    // First tick seeds nothing (m_fn adds on FrameMove). Tick a few times.
    for (uint32_t t = 16; t < 300; t += 16)
        tr->FrameMove(t, Ctx());
    // After the window, the trail should still be alive (fade tail) then expire.
    EXPECT_TRUE(tr->FrameMove(350, Ctx()));
}

TEST(swing, expires_after_fade_tail) {
    auto tr = MakeSkillEffect<SwingTrail>(0);
    tr->Start(0, 200, SweepTip);
    tr->SetTrailMs(150);
    for (uint32_t t = 16; t < 200; t += 16)
        tr->FrameMove(t, Ctx());
    // Recording done at 200; fade until ~350; then expires.
    EXPECT_TRUE(tr->FrameMove(300, Ctx()));
    EXPECT_FALSE(tr->FrameMove(500, Ctx()));
}

TEST(swing, no_segments_renders_nothing) {
    // Render with <2 history segments must not crash / must early-return.
    auto tr = MakeSkillEffect<SwingTrail>(0);
    tr->Start(0, 100, SweepTip);
    tr->FrameMove(0, Ctx());   // no segment recorded yet (m_fn only adds in window ticks)
    SkillCtx c;
    tr->Render(c);   // should be a no-op without GL
}
