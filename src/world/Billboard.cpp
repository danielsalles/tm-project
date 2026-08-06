#include "world/Billboard.h"

#include <cmath>

namespace tmx {

namespace {

const float kPi = 3.1415927f;

} // namespace

void BillboardFrameMove(Billboard& b, uint32_t nowMs, float camYawH, float camPitchV,
                        uint32_t phaseSeed) {
    if (!b.started) {
        b.createMs = nowMs;
        b.started = true;
    }

    const uint32_t t = nowMs >= b.createMs ? nowMs - b.createMs : b.d.lifeTimeMs;

    if (b.d.lifeTimeMs && (int)b.d.lifeTimeMs <= (int)t) {
        b.dead = true;
        return;
    }

    // Flipbook frame.
    uint32_t dwMod = (uint32_t)b.d.cycleTimeMs * (uint32_t)b.d.cycleCount;
    if (!dwMod)
        dwMod = 1;
    b.cycleIndex = b.d.cycleTimeMs
        ? (int)(t % dwMod / (uint32_t)b.d.cycleTimeMs)
        : 0;

    // Progress.
    if (b.d.lifeTimeMs)
        b.progress = (float)t / (float)b.d.lifeTimeMs;
    if (b.progress < 0.0f)
        b.progress = 0.0099999998f;
    if (b.progress > 1.0f)
        b.progress = 1.0f;

    const uint32_t dwA = (b.d.bgra & 0xFF000000u) >> 24;
    const uint32_t dwR = (b.d.bgra & 0x00FF0000u) >> 16;
    const uint32_t dwG = (b.d.bgra & 0x0000FF00u) >> 8;
    const uint32_t dwB = b.d.bgra & 0xFF;

    // Fade programs (TMEffectBillBoard.cpp:258-313).
    switch (b.d.fade) {
    case 1: {
        const float a = sinf(b.progress * kPi);
        const uint32_t cA = (uint32_t)(dwA * a), cR = (uint32_t)(dwR * a);
        const uint32_t cG = (uint32_t)(dwG * a), cB = (uint32_t)(dwB * a);
        b.curBgra = cB | (cG << 8) | (cR << 16) | (cA << 24);
        break;
    }
    case 2: {
        const float fOther = 1.0f - b.d.baseAlpha;
        const float fVel = (float)((nowMs + 200u * phaseSeed) % 3000u) / 3000.0f;
        const float m = fOther * fabsf(sinf(fVel * kPi * 2.0f)) + b.d.baseAlpha;
        const uint32_t cA = (uint32_t)(dwA * m), cR = (uint32_t)(dwR * m);
        const uint32_t cG = (uint32_t)(dwG * m), cB = (uint32_t)(dwB * m);
        b.curBgra = cB | (cG << 8) | (cR << 16) | (cA << 24);
        break;
    }
    case 3: {
        const uint32_t cA = (uint32_t)(dwA * sinf(b.progress * kPi));
        b.curBgra = dwB | (dwG << 8) | (dwR << 16) | (cA << 24);
        break;
    }
    case 4: {
        float p;
        if (b.progress >= 0.30000001f)
            p = 1.0f - ((b.progress - 0.30000001f) * 1.428f);
        else
            p = b.progress * 3.3299999f;
        const uint32_t cA = (uint32_t)(dwA * p);
        b.curBgra = dwB | (dwG << 8) | (dwR << 16) | (cA << 24);
        break;
    }
    default:
        b.curBgra = b.d.bgra;
        break;
    }

    // Motion programs (switch (m_nParticleType - 1), TMEffectBillBoard.cpp:315-345).
    const float sx = b.d.x, sy = b.d.y, sz = b.d.z;
    switch (b.d.motion) {
    case 0:
        b.curY = b.progress * b.d.particleV + sy;
        b.curX = sx;
        b.curZ = sz;
        break;
    case 1: {
        const float s = sinf(b.progress * kPi * b.d.circleSpeed);
        b.curY = b.progress * b.d.particleV + sy;
        b.curX = s * b.d.particleH + sx;
        b.curZ = sz;
        break;
    }
    case 2: {
        const float s = sinf(b.progress * kPi * b.d.circleSpeed);
        const float c = cosf(b.progress * kPi * b.d.circleSpeed);
        b.curY = b.progress * b.d.particleV + sy;
        b.curX = s * b.d.particleH + sx;
        b.curZ = c * b.d.particleH + sz;
        break;
    }
    case 3: {
        const float s = sinf(b.progress * kPi * b.d.circleSpeed);
        b.curY = b.progress * b.d.particleV + sy;
        b.curX = s * b.d.particleH + sx;
        b.curZ = b.progress * b.d.particleH + sz;
        break;
    }
    default:
        b.curX = sx;
        b.curY = sy;
        b.curZ = sz;
        break;
    }

    // Scale (velocity only applies with a finite lifetime).
    float vx = b.d.scaleX, vy = b.d.scaleY, vz = b.d.scaleZ;
    if (b.d.lifeTimeMs) {
        vx += (float)t * b.d.scaleVelX;
        vy += (float)t * b.d.scaleVelY;
        vz += (float)t * b.d.scaleVelZ;
    }

    // Pulse animation types (bake into scale + gray color).
    if (b.d.lookCam == 1 && b.d.animationType == 1) {
        const float s = sinf(((float)(nowMs % 2000u) / 2000.0f) * kPi);
        vx += s * 0.2f;
        vy += s * 0.2f;
        const uint32_t one = 255u - (uint32_t)(155.0f * s);
        b.curBgra = one | (one << 8) | (one << 16) | (one << 24);
    } else if (b.d.lookCam == 1 && (b.d.animationType == 2 || b.d.animationType == 3)) {
        const float ft = (float)(nowMs % 4000u) / 4000.0f;
        const float s = sinf(ft * kPi);
        vx += s * 3.0f;
        vy += s * 3.0f;
        const uint32_t one = 255u - (uint32_t)(255.0f * s);
        b.curBgra = one | (one << 8) | (one << 16) | (one << 24);
        // animationType 2 -> 1 at cycle end handled by the caller state if needed
    }

    // Orientation + world matrix.
    D3DXMATRIX rot, scale, trans;
    D3DXMatrixScaling(&scale, vx, vy, vz);
    const float ty = b.d.stickGround == 1 ? b.curY + vy * 0.5f : b.curY;
    D3DXMatrixTranslation(&trans, b.curX, ty, b.curZ);

    if (b.d.lookCam == 1) {
        const float fAngle = -camPitchV;
        // Original: RotationX then YPR overwrites — net result is the YPR below.
        D3DXMatrixRotationYawPitchRoll(&rot, 1.5707964f - camYawH, fAngle,
                                       b.d.axisAngle);
    } else {
        D3DXMatrixRotationY(&rot, b.d.axisAngle);
    }
    D3DXMatrixMultiply(&b.world, &rot, &scale);
    D3DXMatrixMultiply(&b.world, &b.world, &trans);
}

}
