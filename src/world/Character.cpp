#include "world/Character.h"

#include "gl/GLRenderDevice.h"
#include "gl/SkinPipeline.h"

#include <cmath>
#include <cstring>

namespace tmx {

namespace {

// TMHuman angle for a route step: characters face the walk direction.
float AngleTowards(float dx, float dz) {
    // Model forward convention matches the trees: yaw 0 faces +X after the
    // YPR(yaw-90, -90) object transform; store the plain atan2 heading.
    return atan2f(dz, dx);
}

} // namespace

bool Character::InitLogic(CharacterAnimationCache& cache, const AniSoundData& aniSound,
                          const CharDesc& d, const int8_t* maskCells, int maskW,
                          int maskH, float maskOriginX, float maskOriginZ,
                          std::string* err) {
    m_desc = d;
    m_aniSound = &aniSound;
    m_mask.cells = maskCells;
    m_mask.width = maskW;
    m_mask.height = maskH;
    m_originX = maskOriginX;
    m_originZ = maskOriginZ;
    m_unitTimeMs = d.maxSpeed > 0.0f ? (uint32_t)(1000.0f / d.maxSpeed) : 500;

    if (!m_mesh.LoadLogic(cache, d.boneAniIndex, err))
        return false;
    m_mesh.pb.fps = 20;
    UpdateWorldMatrix();
    return true;
}

bool Character::InitMesh(CharacterAnimationCache& cache, GLTextureManager& textures,
                         std::string* err) {
    return m_mesh.LoadParts(cache, textures, m_desc.meshLook, m_desc.skinLook, err);
}

void Character::Destroy() {
    m_mesh.Destroy();
}

void Character::SetPosition(float x, float z) {
    m_x = x;
    m_z = z;
    m_routeLen = 0;
    m_height = MaskAt(x, z) * 0.1f;
    UpdateWorldMatrix();
}

int Character::MaskAt(float wx, float wz) const {
    int cx = (int)wx - (int)m_originX;
    int cz = (int)wz - (int)m_originZ;
    if (cx < 0) cx = 0;
    if (cz < 0) cz = 0;
    if (cx >= m_mask.width) cx = m_mask.width - 1;
    if (cz >= m_mask.height) cz = m_mask.height - 1;
    if (!m_mask.cells)
        return 0;
    return m_mask.cells[cx + m_mask.width * cz];
}

bool Character::SetMotion(CharMotion m, uint32_t nowMs) {
    if (m == CharMotion::None || !m_aniSound)
        return false;

    int motion = (int)m;
    const int type = m_desc.boneAniIndex;
    if (type < 0 || type >= kAniTypes)
        return false;

    // Type-specific remaps (TMHuman::SetAnimation): horse shifts attacks.
    if (type == 31 && motion >= 4 && motion <= 6)
        motion += 3;

    int cut = -1;
    uint32_t fps = 20;
    if (type < kAniHumanTypes) {
        const int cls = m_desc.classIndex & 3;
        const int tableAni = (int)m_aniSound->human[cls][type].ani[motion];
        const int weapon = m_desc.weaponIndex;
        if (weapon >= 0 && weapon < kMaxWeaponSlots && tableAni < kCharMotions)
            cut = m_mesh.Anim() ? m_mesh.Anim()->animMap[weapon][tableAni] : -1;
        fps = m_aniSound->human[cls][type].speed[motion];
    } else {
        cut = (int)m_aniSound->mob[type].ani[motion];
        fps = m_aniSound->mob[type].speed[motion];
    }
    if (cut < 0)
        return false;
    if (m_mesh.pb.cut == cut) {
        // Already playing (TMSkinMesh::SetAnimation returns 0 = no-op, not error).
        m_motion = m;
        m_mesh.pb.fps = fps;
        return true;
    }
    if (!m_mesh.SetCut(cut, fps, nowMs))
        return false;

    m_motion = m;
    return true;
}

bool Character::MoveTo(float wx, float wz, uint32_t nowMs) {
    if (!m_mask.cells)
        return false;

    int sx = (int)m_x - (int)m_originX;
    int sz = (int)m_z - (int)m_originZ;
    int tx = (int)wx - (int)m_originX;
    int tz = (int)wz - (int)m_originZ;

    uint8_t route[24];
    if (!GetRoute(m_mask, sx, sz, &tx, &tz, route, 12, 8))
        return false;

    // Materialize the route as world-space cell centers.
    m_routeLen = 0;
    int cx = sx, cz = sz;
    m_routeX[m_routeLen] = m_originX + cx + 0.5f;
    m_routeZ[m_routeLen] = m_originZ + cz + 0.5f;
    ++m_routeLen;
    for (int i = 0; i < 24 && route[i]; ++i) {
        int dx, dy;
        RouteStepDir(route[i], dx, dy);
        cx += dx;
        cz += dy;
        m_routeX[m_routeLen] = m_originX + cx + 0.5f;
        m_routeZ[m_routeLen] = m_originZ + cz + 0.5f;
        ++m_routeLen;
        if (m_routeLen >= 24)
            break;
    }
    if (m_routeLen < 2) {
        m_routeLen = 0;
        return false;
    }

    m_routeIndex = 0;
    m_segmentStartMs = nowMs;
    m_running = m_desc.maxSpeed > 2.0f;
    SetMotion(m_running ? CharMotion::Run : CharMotion::Walk, nowMs);
    m_wantAngle = AngleTowards(m_routeX[1] - m_x, m_routeZ[1] - m_z);
    return true;
}

void Character::Stop(uint32_t nowMs) {
    m_routeLen = 0;
    SetMotion(CharMotion::Stand01, nowMs);
}

void Character::FrameMove(uint32_t nowMs) {
    if (m_routeLen > 0) {
        // Consume as many whole segments as elapsed (catch-up after hitches).
        while (m_unitTimeMs > 0 && nowMs - m_segmentStartMs >= m_unitTimeMs) {
            m_segmentStartMs += m_unitTimeMs;
            ++m_routeIndex;
            if (m_routeIndex >= m_routeLen - 1) {
                m_x = m_routeX[m_routeLen - 1];
                m_z = m_routeZ[m_routeLen - 1];
                Stop(nowMs);
                break;
            }
            m_x = m_routeX[m_routeIndex];
            m_z = m_routeZ[m_routeIndex];
            m_wantAngle = AngleTowards(m_routeX[m_routeIndex + 1] - m_x,
                                       m_routeZ[m_routeIndex + 1] - m_z);
        }

        if (m_routeLen > 0 && m_routeIndex < m_routeLen - 1) {
            const float progress = m_unitTimeMs
                ? (float)(nowMs - m_segmentStartMs) / (float)m_unitTimeMs
                : 1.0f;
            const float t = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
            const float a = m_routeX[m_routeIndex];
            const float b = m_routeX[m_routeIndex + 1];
            const float c = m_routeZ[m_routeIndex];
            const float d = m_routeZ[m_routeIndex + 1];
            m_x = a + (b - a) * t;
            m_z = c + (d - c) * t;

            // Height follows the mask heights of the current segment
            // (TMHuman::FrameMove: fWantHeight = lerp(maskA, maskB) * 0.1).
            const float ha = (float)MaskAt(a, c);
            const float hb = (float)MaskAt(b, d);
            m_height = (ha + (hb - ha) * t) * 0.1f;
        }
    }

    // Gradual turn towards the wanted heading (TMHuman::FrameMove angle easing).
    float diff = m_wantAngle - m_angle;
    while (diff > 3.14159265f) diff -= 6.2831853f;
    while (diff < -3.14159265f) diff += 6.2831853f;
    if (fabsf(diff) > 0.017453292f) {
        const float step = 0.08726646f;  // ~5 deg per frame tick at 20fps
        m_angle += fabsf(diff) < step ? diff : (diff > 0.0f ? step : -step);
    }

    UpdateWorldMatrix();
}

void Character::UpdateWorldMatrix() {
    // Same object-transform convention as trees (TMSkinMesh::Render):
    // root = YPR(angle-90, -90, 0) * Scale(s) * Translation(x, h, z)
    D3DXMATRIX rot, scale, trans;
    D3DXMatrixRotationYawPitchRoll(&rot, m_angle - D3DXToRadian(90),
                                   -D3DXToRadian(90), 0.0f);
    D3DXMatrixScaling(&scale, m_desc.scale, m_desc.scale, m_desc.scale);
    D3DXMatrixTranslation(&trans, m_x, m_height, m_z);
    D3DXMatrixMultiply(&m_world, &rot, &scale);
    D3DXMatrixMultiply(&m_world, &m_world, &trans);
}

void Character::Render(SkinPipeline& pipe, GLRenderDevice& device, uint32_t nowMs) {
    m_mesh.Render(pipe, device, m_world, nowMs);
}

}
