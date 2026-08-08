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

// TMArrow-lite (doc 19 §7): a projectile traveling start -> target, emitting a
// trail billboard each frame and spawning an impact composition on arrival.
// impactTex/mesh/decal drive the landing burst (mesh ring + ground lightmap +
// splash burst). type=152 arc adds a sine hop on Y.
struct ProjectileDesc {
    float startX, startY, startZ;
    float targetX, targetY, targetZ;
    int   trailTex = 0;        // trail billboard texture (effect index)
    uint32_t trailBgra = 0xFFAAAAEE;
    int   impactMeshIndex = -1;   // -1 = no mesh ring
    int   impactTexIndex  = 229;  // mesh texture override
    int   impactDecalTex  = 118;  // ground lightmap texture
    uint32_t impactBgra = 0x80FFFFFF;
    int   splashCount = 10;
    float splashRadius = 1.0f;
    bool  arc = false;         // type 152 hop
};

// TMArrow 13-tipos data-table (phase 7, doc 21 §8): (type, level, color) ->
// visual/timing parameters, ported from the switch in TMArrow.cpp:45-145.
// Mesh indices point at MeshList models (the original renders the arrow mesh
// during travel; our trail-based renderer keeps the trail visual and uses the
// table for lifetime/sound/beam — mesh-index rendering is cosmetic, deferred).
struct ArrowTypeInfo {
    int   meshIndex = 800;      // 800 = default wooden arrow
    int   lifePerUnitMs = 50;   // lifetime = lifePerUnit * distance (clamped 1..5000)
    int   fixedLifeMs = 0;      // >0 overrides the distance formula (types 10002/3)
    float beamSize = 0.0f;      // 0 = no beam trail (types 104/105)
    uint32_t beamColor = 0x00777777;
    int   soundId = 0;          // fire sound (0 = silent)
    int   impactSoundId = 24;   // 26 for some hits, 24 default (TMArrow.cpp:674-678)
};
ArrowTypeInfo GetArrowTypeInfo(int type, int level, int color);
class Projectile : public SkillEffect {
public:
    Projectile(const ProjectileDesc& d);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    void SpawnImpact(uint32_t nowMs, const SkillCtx& ctx);
    ProjectileDesc m_d;
    D3DXVECTOR3 m_cur;
    float m_progress = 0.0f;
    uint32_t m_life = 1;
    bool m_impacted = false;
};

// TMSkillMeteorStorm-lite (doc 19 §8): rains `strikes` impacts scattered
// around the target over the lifetime. Each strike = a falling glow + an
// impact burst (fire billboard ring + ground lightmap). EF_BRIGHT orange.
class MeteorStorm : public SkillEffect {
public:
    MeteorStorm(float x, float y, float z, int level, uint32_t nowMs);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
private:
    float m_x, m_y, m_z;
    int m_level;
    int m_strikes;
    int m_nextStrike = 0;
    uint32_t m_life;
    uint32_t m_perStrike;
};

// TMSkillMagicShield-lite (doc 19 §8 buff): a persistent orbiting glow around a
// world position. Stays until its lifetime (buffs set a long life / are
// toggled). Renders a rotating ring of billboards.
class MagicShield : public SkillEffect {
public:
    MagicShield(float x, float y, float z, uint32_t lifeMs, uint32_t bgra);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;
private:
    float m_x, m_y, m_z;
    uint32_t m_life, m_bgra;
    uint32_t m_nowMs = 0;
};

// TMHuman::RenderEffect x15 (doc 19 §10): per-monster-class ambient billboard
// spawns (Khepra sand, dragon embers, golem dust, ...) anchored near the
// character. Table-driven; each class maps to a texture/color/interval. The
// effect stays attached for its lifetime (or forever when lifeMs=0) and emits
// billboards into ctx.fx each interval.
class Character;
class MonsterAmbient : public SkillEffect {
public:
    MonsterAmbient(Character* c, int monsterClass, uint32_t lifeMs);
    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override { (void)ctx; }
private:
    Character* m_char;
    int m_class;
    uint32_t m_life, m_lastSpawn;
    // Per-class recipe (ported from RenderEffect_*: Khepra/BoneDragon/Golem/etc.)
    int   m_tex;
    uint32_t m_bgra;
    uint32_t m_interval;
    float m_h, m_v;   // particle H/V drift
};

} // namespace tmx
