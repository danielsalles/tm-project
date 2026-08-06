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

// TMEffectMesh (doc 19 §3): a static common mesh drawn as a skill VFX with a
// flat tint, no lighting/fog. type 0-5 drives per-progress scale/angle; texture
// override replaces the mesh's own textures. EF_BRIGHT additive by default.
struct SkillMeshDesc {
    int      meshIndex;
    uint32_t bgra        = 0xFFFFFFFF;
    float    angle = 0, angle2 = 0, angle3 = 1.5707964f;
    float    scaleH = 1.0f, scaleV = 1.0f;
    int      type = 0;            // 0..5 animation program
    int      textureIndex = -1;   // override; -1 = use mesh textures
    uint32_t lifeTime = 0;
    uint32_t cycleTime = 1000;
    int      blend = 1;           // 1 EF_BRIGHT, 0 EF_DEFAULT
};
class SkillMeshFx : public SkillEffect {
public:
    SkillMeshFx(float x, float y, float z, const SkillMeshDesc& d);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    SkillMeshDesc m_d;
    float m_x, m_y, m_z;
    float m_progress = 0.0f;
};

// TMShade (doc 19 §5): a terrain-conforming decal. gridNum = N (resolution);
// the (N+1)² grid is snapped to the terrain 2-unit grid, lifted to terrain
// height, UV-rotated by angle, color-faded over the lifetime.
struct GroundDecalDesc {
    int      gridNum = 4;
    int      textureIndex = 7;
    float    angle = 0.0f;
    uint32_t bgra = 0xFFFFFFFF;
    int      blend = 1;          // EF_BRIGHT
    int      fade = 0;           // 0 = post-multiply, 1 = pre-multiply
    int      fi = 1;             // fade curve: 1 = sin, 0 = cos
    uint32_t lifeTime = 0;
};
class GroundDecalFx : public SkillEffect {
public:
    GroundDecalFx(float x, float z, const GroundDecalDesc& d);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    GroundDecalDesc m_d;
    float m_x, m_z;
    float m_curFade = 1.0f;
};

// Type animation for SkillMeshFx (TMEffectMesh::Render types 0-5), extracted as
// a pure function so tests can verify scale/angle curves headless.
void SkillMeshTypeAnim(int type, float progress, float baseScaleH, float baseScaleV,
                       float baseAngle, float* outScaleH, float* outScaleV,
                       float* outAngle);

// TMShade fade curve: fi=1 -> |sin(progress*pi)|, fi=0 -> |cos(progress*pi/2)|.
float DecalFade(float progress, int fi);

} // namespace tmx
