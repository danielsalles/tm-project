#pragma once

#include <cstdint>
#include "world/SkillEffect.h"
#include "world/Billboard.h"

namespace tmx {

// Minimal combat VFX built from the Phase-4 billboard sim. These are the first
// concrete SkillEffects (doc 19 §2 validation): they animate Billboard structs
// on the CPU and emit FxQuads into ctx.fx, proving the CLI -> container ->
// billboard -> EffectRenderer pipeline end-to-end. Richer renderers (mesh/
// decal/swing) are added in later steps.

// Single fading glow at a point (the core of Explosion2 / Bash center light,
// TMSkillExplosion2.cpp ctor). One billboard, fade 1, EF_BRIGHT.
class SkillGlow : public SkillEffect {
public:
    SkillGlow(float x, float y, float z, int textureIndex, uint32_t lifeMs,
              float scale, uint32_t bgra, int blendMode = 1);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    Billboard m_bill;
    int       m_blend;
};

// Expanding ring of billboards (TMEffectBillBoard2 / the splash of Bash /
// MagicArrow impact). Spawns `count` particles spread on a circle that grows
// with progress; each fades over the lifetime. EF_BRIGHT.
class SkillBurst : public SkillEffect {
public:
    SkillBurst(float x, float y, float z, int textureIndex, uint32_t lifeMs,
               int count, float radius, float scale, uint32_t bgra);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    struct Particle { Billboard bill; float ang; };
    std::vector<Particle> m_parts;
    float m_x, m_y, m_z, m_radius;
    uint32_t m_life;
};

} // namespace tmx
