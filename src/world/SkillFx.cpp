#include "world/SkillFx.h"
#include "gl/EffectRenderer.h"

#include <cmath>

namespace tmx {

namespace {
const float kPi = 3.1415927f;
}

// ---------------------------------------------------------------------------
// SkillGlow
// ---------------------------------------------------------------------------

SkillGlow::SkillGlow(float x, float y, float z, int textureIndex, uint32_t lifeMs,
                     float scale, uint32_t bgra, int blendMode)
    : m_blend(blendMode) {
    BillboardDesc& d = m_bill.d;
    d.textureIndex = textureIndex;
    d.lifeTimeMs   = lifeMs;
    d.scaleX = d.scaleY = d.scaleZ = scale;
    d.fade        = 1;          // RGBA x sin(progress*pi): full -> fade out
    d.lookCam     = 1;
    d.cycleCount  = 1;
    d.cycleTimeMs = 80;
    d.x = x; d.y = y; d.z = z;
    d.bgra        = bgra;
    m_bill.createMs = 0;
    m_bill.curBgra  = bgra;
}

bool SkillGlow::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    BillboardFrameMove(m_bill, nowMs, ctx.camYawH, ctx.camPitchV, 0);
    if (m_bill.dead)
        return false;
    return true;
}

void SkillGlow::Render(const SkillCtx& ctx) {
    if (ctx.fx)
        ctx.fx->Emit(BillboardToQuad(m_bill, m_blend));
}

// ---------------------------------------------------------------------------
// SkillBurst
// ---------------------------------------------------------------------------

SkillBurst::SkillBurst(float x, float y, float z, int textureIndex, uint32_t lifeMs,
                       int count, float radius, float scale, uint32_t bgra)
    : m_x(x), m_y(y), m_z(z), m_radius(radius), m_life(lifeMs) {
    m_parts.reserve(count);
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.ang = (kPi * 2.0f * i) / count;
        BillboardDesc& d = p.bill.d;
        d.textureIndex = textureIndex;
        d.lifeTimeMs   = lifeMs;
        d.scaleX = d.scaleY = d.scaleZ = scale;
        d.fade        = 1;
        d.lookCam     = 1;
        d.cycleCount  = 1;
        d.cycleTimeMs = 80;
        d.x = x; d.y = y; d.z = z;
        d.bgra        = bgra;
        p.bill.createMs = 0;
        p.bill.curBgra  = bgra;
        m_parts.push_back(p);
    }
}

bool SkillBurst::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    const uint32_t elapsed = nowMs - StartTime();
    if (m_life && elapsed >= m_life)
        return false;
    // Expanding ring: push each particle outward as progress grows, so the
    // splash reads as a spreading shockwave (TMEffectBillBoard2 behavior).
    const float progress = m_life ? (float)elapsed / (float)m_life : 0.0f;
    for (auto& p : m_parts) {
        p.bill.d.x = m_x + std::cos(p.ang) * m_radius * progress;
        p.bill.d.z = m_z + std::sin(p.ang) * m_radius * progress;
        p.bill.d.y = m_y;
        BillboardFrameMove(p.bill, nowMs, ctx.camYawH, ctx.camPitchV, 0);
    }
    return true;
}

void SkillBurst::Render(const SkillCtx& ctx) {
    if (!ctx.fx) return;
    for (auto& p : m_parts)
        if (!p.bill.dead)
            ctx.fx->Emit(BillboardToQuad(p.bill, 1));
}

} // namespace tmx
