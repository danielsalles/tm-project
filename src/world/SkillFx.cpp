#include "world/SkillFx.h"
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

} // namespace tmx
