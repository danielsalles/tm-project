#include "world/SkillFx.h"
#include "world/Character.h"
#include "gl/EffectRenderer.h"
#include "gl/SkillMeshRenderer.h"
#include "gl/GroundDecalRenderer.h"
#include "gl/GLMesh.h"
#include "world/TerrainData.h"

#include <cmath>
#include <vector>

namespace tmx {

namespace {
const float kPi = 3.1415927f;

inline void BgraToFloats(uint32_t bgra, float* out) {
    out[0] = (float)((bgra >> 16) & 0xFF) / 255.0f;   // R
    out[1] = (float)((bgra >> 8) & 0xFF) / 255.0f;    // G
    out[2] = (float)(bgra & 0xFF) / 255.0f;           // B
    out[3] = (float)((bgra >> 24) & 0xFF) / 255.0f;   // A
}
} // namespace

// TMEffectMesh type programs (TMEffectMesh.cpp:200-260). Pure so tests verify
// the scale/angle curves without GL.
void SkillMeshTypeAnim(int type, float progress, float baseScaleH, float baseScaleV,
                       float baseAngle, float* outScaleH, float* outScaleV,
                       float* outAngle) {
    float sh = baseScaleH, sv = baseScaleV, ang = baseAngle;
    switch (type) {
        case 2: sv = (sh * progress) * 3.0f; ang = progress * kPi; break;
        case 3: ang = progress * kPi; break;
        case 4:
            if (progress >= 0.2f) {
                const float s = sinf((progress - 0.2f) * kPi * 0.5f);
                sh = (s + 1.5f) * baseScaleH;
                sv = (s + 1.5f) * baseScaleV;
            } else {
                sh = (progress * 5.0f + 0.5f) * baseScaleH;
                sv = (progress * 5.0f + 0.5f) * baseScaleV;
            }
            break;
        default: break;   // 0,1,5 = static / texture override only
    }
    *outScaleH = sh; *outScaleV = sv; *outAngle = ang;
}

float DecalFade(float progress, int fi) {
    return fi ? fabsf(sinf(progress * kPi)) : fabsf(cosf(progress * kPi * 0.5f));
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

// ---------------------------------------------------------------------------
// SkillMeshFx (TMEffectMesh)
// ---------------------------------------------------------------------------

SkillMeshFx::SkillMeshFx(float x, float y, float z, const SkillMeshDesc& d)
    : m_d(d), m_x(x), m_y(y), m_z(z) {}

bool SkillMeshFx::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    (void)ctx;
    const uint32_t elapsed = nowMs - StartTime();
    m_progress = m_d.cycleTime ? (float)(elapsed % m_d.cycleTime) / (float)m_d.cycleTime : 0.0f;
    if (m_d.lifeTime && elapsed > m_d.lifeTime)
        return false;
    return true;
}

void SkillMeshFx::Render(const SkillCtx& ctx) {
    if (!ctx.skillMesh || !ctx.getMesh || !ctx.device || !ctx.textures)
        return;
    GLMesh* mesh = ctx.getMesh(m_d.meshIndex);
    if (!mesh) return;

    int texOverride = m_d.textureIndex;
    float sh, sv, ang;
    SkillMeshTypeAnim(m_d.type, m_progress, m_d.scaleH, m_d.scaleV, m_d.angle, &sh, &sv, &ang);

    D3DXMATRIX rot, scale, trans, world;
    D3DXMatrixRotationYawPitchRoll(&rot, ang, m_d.angle2, m_d.angle3);
    D3DXMatrixScaling(&scale, sh, sv, sh);
    D3DXMatrixTranslation(&trans, m_x, m_y, m_z);
    D3DXMatrixMultiply(&world, &scale, &rot);
    D3DXMatrixMultiply(&world, &world, &trans);

    float tint[4];
    BgraToFloats(m_d.bgra, tint);
    ctx.skillMesh->Draw(*ctx.device, *ctx.textures, *mesh, world, tint, m_d.blend, texOverride);
}

// ---------------------------------------------------------------------------
// GroundDecalFx (TMShade)
// ---------------------------------------------------------------------------

GroundDecalFx::GroundDecalFx(float x, float z, const GroundDecalDesc& d)
    : m_d(d), m_x(x), m_z(z) {}

bool GroundDecalFx::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    (void)ctx;
    if (m_d.lifeTime) {
        const uint32_t elapsed = nowMs - StartTime();
        if (elapsed >= m_d.lifeTime)
            return false;
        const float prog = (float)elapsed / (float)m_d.lifeTime;
        m_curFade = DecalFade(prog, m_d.fi);
    } else {
        m_curFade = 1.0f;
    }
    return true;
}

void GroundDecalFx::Render(const SkillCtx& ctx) {
    if (!ctx.decal || !ctx.terrain || !ctx.device || !ctx.textures)
        return;
    const int N = m_d.gridNum < 2 ? 2 : m_d.gridNum;
    const int nX = (int)(m_x / 2.0f) - N / 2;
    const int nY = (int)(m_z / 2.0f) - N / 2;
    const float ca = cosf(m_d.angle), sa = sinf(m_d.angle);

    float base[4];
    BgraToFloats(m_d.bgra, base);

    std::vector<DecalVertex> verts((size_t)(N + 1) * (N + 1));
    for (int gy = 0; gy <= N; ++gy) {
        for (int gx = 0; gx <= N; ++gx) {
            const float wx = (float)(gx + nX) * 2.0f;
            const float wz = (float)(gy + nY) * 2.0f;
            const float wy = TerrainGetHeight(*ctx.terrain, wx, wz) + 0.05f;
            const float fU = (m_x - wx) / (float)(N * 2);
            const float fV = -(m_z - wz) / (float)(N * 2);
            DecalVertex v;
            v.x = wx; v.y = wy; v.z = wz;
            v.r = base[0] * m_curFade;
            v.g = base[1] * m_curFade;
            v.b = base[2] * m_curFade;
            v.a = base[3] * m_curFade;
            v.u = (-ca * fU - sa * fV) - 0.5f;
            v.v = (sa * fU - ca * fV) - 0.5f;
            verts[gx + gy * (N + 1)] = v;
        }
    }
    // Soft edge (TMShade::FrameMove): zero the last 3 verts' contribution.
    if (N >= 3) {
        const int last = (N + 1) * (N + 1);
        verts[last - 1].a = 0.0f;
        verts[last - 2].a = 0.0f;
        verts[last - 1 - (N + 1)].a = 0.0f;
    }

    std::vector<uint16_t> idx((size_t)6 * N * N);
    int k = 0;
    for (int gy = 0; gy < N; ++gy) {
        for (int gx = 0; gx < N; ++gx) {
            const int a = gx + gy * (N + 1);
            const int b = gx + (gy + 1) * (N + 1);
            idx[k++] = (uint16_t)a;
            idx[k++] = (uint16_t)b;
            idx[k++] = (uint16_t)(a + 1);
            idx[k++] = (uint16_t)b;
            idx[k++] = (uint16_t)(b + 1);
            idx[k++] = (uint16_t)(a + 1);
        }
    }
    ctx.decal->Draw(*ctx.device, *ctx.textures, m_d.textureIndex, verts, idx, m_d.blend);
}

// ---------------------------------------------------------------------------
// Projectile (TMArrow-lite)
// ---------------------------------------------------------------------------

Projectile::Projectile(const ProjectileDesc& d)
    : m_d(d) {
    D3DXVECTOR3 diff(d.targetX - d.startX, d.targetY - d.startY, d.targetZ - d.startZ);
    const float len = D3DXVec3Length(&diff);
    m_life = (uint32_t)(100.0f * len);   // 100ms per unit (TMArrow base factor)
    if (m_life < 1) m_life = 1;
    if (m_life > 5000) m_life = 5000;
    m_cur = D3DXVECTOR3(d.startX, d.startY, d.startZ);
}

bool Projectile::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    const uint32_t elapsed = nowMs - StartTime();
    m_progress = m_life ? (float)elapsed / (float)m_life : 1.0f;
    if (m_progress >= 1.0f) {
        if (!m_impacted) {
            m_impacted = true;
            SpawnImpact(nowMs, ctx);
        }
        return false;   // projectile itself is done (impact children live on)
    }
    m_cur.x = m_d.startX + (m_d.targetX - m_d.startX) * m_progress;
    m_cur.z = m_d.startZ + (m_d.targetZ - m_d.startZ) * m_progress;
    m_cur.y = m_d.startY + (m_d.targetY - m_d.startY) * m_progress;
    if (m_d.arc)
        m_cur.y += sinf(m_progress * kPi * 4.0f) * 0.1f;   // type 152 hop
    return true;
}

void Projectile::Render(const SkillCtx& ctx) {
    if (!ctx.fx) return;
    // Trail billboard at the current position (stickGround offset -0.5 like the
    // original TMArrow spawn).
    Billboard b;
    BillboardDesc& d = b.d;
    d.textureIndex = m_d.trailTex;
    d.lifeTimeMs = 1000;
    d.scaleX = d.scaleY = d.scaleZ = 0.3f;
    d.fade = 1;
    d.lookCam = 1;
    d.x = m_cur.x; d.y = m_cur.y - 0.5f; d.z = m_cur.z;
    d.bgra = m_d.trailBgra;
    b.createMs = StartTime();
    b.curBgra = m_d.trailBgra;
    BillboardFrameMove(b, StartTime() + (uint32_t)(m_progress * m_life),
                       ctx.camYawH, ctx.camPitchV, 0);
    ctx.fx->Emit(BillboardToQuad(b, 1));
}

void Projectile::SpawnImpact(uint32_t nowMs, const SkillCtx& ctx) {
    if (!ctx.host) return;
    const float y = m_d.targetY;
    // Mesh ring (TMEffectMesh type 4 expanding).
    if (m_d.impactMeshIndex >= 0 && ctx.skillMesh) {
        SkillMeshDesc md;
        md.meshIndex = m_d.impactMeshIndex;
        md.bgra = m_d.impactBgra;
        md.type = 4;
        md.textureIndex = m_d.impactTexIndex;
        md.lifeTime = 200;
        md.cycleTime = 200;
        md.blend = 1;
        ctx.host->Add(MakeSkillEffect<SkillMeshFx>(nowMs, m_d.targetX, y, m_d.targetZ, md));
    }
    // Ground lightmap decal.
    if (ctx.decal && ctx.terrain) {
        GroundDecalDesc dd;
        dd.gridNum = 7;
        dd.textureIndex = m_d.impactDecalTex;
        dd.bgra = m_d.impactBgra;
        dd.blend = 1;
        dd.lifeTime = 200;
        ctx.host->Add(MakeSkillEffect<GroundDecalFx>(nowMs, m_d.targetX, m_d.targetZ, dd));
    }
    // Splash burst.
    ctx.host->Add(std::make_unique<SkillBurst>(m_d.targetX, y, m_d.targetZ,
        m_d.trailTex, 600, m_d.splashCount, m_d.splashRadius, 0.5f, m_d.impactBgra));
}

// ---------------------------------------------------------------------------
// MeteorStorm
// ---------------------------------------------------------------------------

MeteorStorm::MeteorStorm(float x, float y, float z, int level, uint32_t nowMs)
    : m_x(x), m_y(y), m_z(z), m_level(level) {
    m_strikes = 3 + level * 2;          // L1=5, L6=15
    m_life = (uint32_t)(300 + level * 120);
    m_perStrike = m_life / (uint32_t)m_strikes;
}

bool MeteorStorm::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    const uint32_t elapsed = nowMs - StartTime();
    if (elapsed > m_life + 400)
        return false;
    // Schedule the next scattered strike.
    if (m_nextStrike < m_strikes && elapsed >= m_nextStrike * (uint64_t)m_perStrike) {
        ++m_nextStrike;
        if (ctx.host) {
            const float ang = (float)(rand() % 628) * 0.01f;
            const float rad = (float)(rand() % (m_level + 1)) * 0.6f;
            const float sx = m_x + cosf(ang) * rad;
            const float sz = m_z + sinf(ang) * rad;
            // Falling glow then impact burst + lightmap.
            ctx.host->Add(std::make_unique<SkillGlow>(sx, m_y + 3.0f, sz, 11,
                400, 1.2f, 0xFFFF7711));
            ctx.host->Add(std::make_unique<SkillBurst>(sx, m_y, sz, 11,
                600, 14, 1.4f, 0.6f, 0xFFAA3300));
            if (ctx.decal && ctx.terrain) {
                GroundDecalDesc dd;
                dd.gridNum = 4; dd.textureIndex = 7;
                dd.bgra = 0xFFFF7711; dd.blend = 1; dd.lifeTime = 600;
                ctx.host->Add(MakeSkillEffect<GroundDecalFx>(nowMs, sx, sz, dd));
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// MagicShield (buff)
// ---------------------------------------------------------------------------

MagicShield::MagicShield(float x, float y, float z, uint32_t lifeMs, uint32_t bgra)
    : m_x(x), m_y(y), m_z(z), m_life(lifeMs), m_bgra(bgra) {}

bool MagicShield::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    (void)ctx;
    m_nowMs = nowMs;
    return m_life == 0 || nowMs - StartTime() < m_life;
}

void MagicShield::Render(const SkillCtx& ctx) {
    if (!ctx.fx) return;
    // Orbiting ring of 6 billboards (TMSkillMagicShield shell, tex 56).
    const float spin = (m_nowMs % 2000) * (kPi / 1000.0f);   // full turn / 2s
    for (int i = 0; i < 6; ++i) {
        const float ang = spin + kPi * 2.0f * i / 6.0f;
        Billboard b;
        BillboardDesc& d = b.d;
        d.textureIndex = 56;
        d.lifeTimeMs = 0;
        d.scaleX = d.scaleY = d.scaleZ = 0.5f;
        d.fade = 0;
        d.lookCam = 1;
        d.x = m_x + cosf(ang) * 0.8f;
        d.y = m_y + 0.5f + sinf(ang * 2.0f) * 0.3f;
        d.z = m_z + sinf(ang) * 0.8f;
        d.bgra = m_bgra;
        b.createMs = StartTime();
        b.curBgra = m_bgra;
        BillboardFrameMove(b, m_nowMs, ctx.camYawH, ctx.camPitchV, (uint32_t)i);
        ctx.fx->Emit(BillboardToQuad(b, 1));
    }
}

// ---------------------------------------------------------------------------
// MonsterAmbient (RenderEffect x15)
// ---------------------------------------------------------------------------

MonsterAmbient::MonsterAmbient(Character* c, int monsterClass, uint32_t lifeMs)
    : m_char(c), m_class(monsterClass), m_life(lifeMs), m_lastSpawn(0),
      m_tex(0), m_bgra(0xFFFFFFFF), m_interval(100), m_h(1.0f), m_v(-3.0f) {
    // Representative subset of TMHuman::RenderEffect_* (doc 19 §10). Each maps
    // a monster class to its ambient billboard recipe; unknown classes get a
    // faint default dust.
    switch (monsterClass) {
        case 39:  // Khepra — red sand falling
            m_tex = 0; m_bgra = 0xFFFF7777; m_interval = 100; m_v = -3.0f; break;
        case 16:  // BoneDragon / EmeraldDragon — embers
            m_tex = 11; m_bgra = 0xFFAA3300; m_interval = 80; m_v = 1.0f; break;
        case 32:  // Golem — dust
            m_tex = 56; m_bgra = 0xFFAA8855; m_interval = 120; m_v = 0.5f; break;
        case 30:  // Minotauros — puff
            m_tex = 0; m_bgra = 0xFF886644; m_interval = 100; m_v = -1.0f; break;
        case 23:  // Hydra — mist
            m_tex = 60; m_bgra = 0xFF5577AA; m_interval = 150; m_v = 0.8f; break;
        default:  // generic faint dust
            m_tex = 56; m_bgra = 0xFF444444; m_interval = 200; m_v = -1.0f; break;
    }
}

bool MonsterAmbient::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    if (m_life && nowMs - StartTime() >= m_life)
        return false;
    if (!m_char || !ctx.fx)
        return true;
    if (nowMs - m_lastSpawn < m_interval)
        return true;
    m_lastSpawn = nowMs;

    // Spawn near the character's head/torso (m_vecTempPos[0] approx).
    const float cx = m_char->X();
    const float cy = m_char->Height() + 1.0f;
    const float cz = m_char->Z();
    const float jx = (float)(rand() % 10 - 5) * 0.06f;
    const float jz = (float)(rand() % 10 - 5) * 0.06f;
    Billboard b;
    BillboardDesc& d = b.d;
    d.textureIndex = m_tex;
    d.lifeTimeMs = 1500;
    d.scaleX = d.scaleY = d.scaleZ = 0.25f;
    d.fade = 1;
    d.lookCam = 1;
    d.cycleCount = 1; d.cycleTimeMs = 80;
    d.x = cx + jx; d.y = cy; d.z = cz + jz;
    d.bgra = m_bgra;
    d.particleH = m_h; d.particleV = m_v;
    d.motion = (m_v < 0) ? 0 : 1;   // rise or sway
    b.createMs = nowMs;
    b.curBgra = m_bgra;
    BillboardFrameMove(b, nowMs, ctx.camYawH, ctx.camPitchV, 0);
    ctx.fx->Emit(BillboardToQuad(b, 1));
    return true;
}

} // namespace tmx
