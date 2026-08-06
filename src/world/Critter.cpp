#include "world/Critter.h"

#include <cmath>
#include <cstdlib>

namespace tmx {

namespace {

const float kPi = 3.14159265f;

Critter MakeButterfly(int nBD, float x, float y, float z, float angle) {
    Critter c;
    c.kind = 1;
    c.skinMeshType = 69;
    c.motionType = rand() % 3;
    c.angle = angle;
    switch (nBD) {
    case 1:  // dwObjType 4
        c.skinLook0 = (int16_t)(rand() % 2 + 3);
        c.scale = 0.69f;
        c.fps = 10;
        break;
    case 2:  // dwObjType 7 -> bd01 bird
        c.skinMeshType = 24;
        c.motionType = 3;
        c.scale = 0.2f;
        c.fps = 4;
        c.particleH = 5.0f;
        c.particleV = 5.0f;
        break;
    case 3:  // dwObjType 6
        c.scale = 0.5f;
        c.fps = 8;
        c.particleH = 0.5f;   // container multiplies defaults by 0.5
        c.particleV = 1.0f;
        break;
    default: // 343, nBD 0
        c.scale = 0.5f;
        c.fps = 15;
        break;
    }
    c.x = x; c.y = y; c.z = z;
    c.curX = x; c.curY = y; c.curZ = z;
    return c;
}

Critter MakeFish(int sub, float x, float y, float z) {
    Critter c;
    c.kind = 2;
    c.skinMeshType = 70;
    c.motionType = rand() % 3;
    c.circleSpeed = ((float)(rand() % 10) * 0.1f) + 0.7f;
    c.particleH = ((float)(rand() % 7) * 0.5f) + 3.0f;
    c.particleV = 2.0f;
    c.fps = 80;
    (void)sub;
    c.x = x; c.y = y; c.z = z;
    c.curX = x; c.curY = y; c.curZ = z;
    return c;
}

} // namespace

void CritterSpawn(uint32_t dwObjType, float wx, float h, float wz,
                  std::vector<Critter>& out) {
    if (dwObjType >= 311 && dwObjType <= 322) {
        // TMLeaf (TMObjectContainer.cpp:279-305)
        Critter c;
        c.kind = 0;
        c.skinMeshType = 61;
        c.fps = 80;
        if ((int)(dwObjType - 311) >= 6) {
            c.meshLook0 = 2;
            c.skinLook0 = (int16_t)(uint8_t)(dwObjType - 57);   // original truncates to u8
        } else {
            c.meshLook0 = 0;
            // Snow-region override (tile 26-30 / 20-24).
            const int tx = (int)wx >> 7;
            const int ty = (int)wz >> 7;
            if (tx > 26 && tx < 31 && ty > 20 && ty < 25)
                c.skinLook0 = 9;
            else
                c.skinLook0 = (int16_t)(uint8_t)(dwObjType - 55);   // u8 truncation
        }
        c.x = wx; c.y = h; c.z = wz;
        c.curX = wx; c.curY = h; c.curZ = wz;
        out.push_back(c);
        return;
    }

    if (dwObjType == 343 || dwObjType == 4 || dwObjType == 6 || dwObjType == 7) {
        const int nBD = dwObjType == 343 ? 0 : dwObjType == 4 ? 1 : dwObjType == 6 ? 3 : 2;
        for (int i = 0; i < 5; ++i) {
            const float angle = dwObjType == 7 ? -1.5707963f
                                : (float)(rand() % 4) * 3.14159265f / 12.0f;
            Critter c = MakeButterfly(nBD,
                wx + ((float)(rand() % 5)) * 0.1f,
                h + ((float)(rand() % 10)) * 0.2f,
                wz + ((float)(rand() % 5)) * 0.1f, angle);
            if (dwObjType == 7 || dwObjType == 6) {
                c.circleSpeed = (float)i + 8.0f;
                if (dwObjType == 7)
                    c.startOffsetMs = 200u * (uint32_t)i;
            }
            out.push_back(c);
        }
        return;
    }

    if (dwObjType == 344 || dwObjType == 12) {
        const int sub = dwObjType == 344 ? 0 : 3;
        for (int i = 0; i < 5; ++i) {
            Critter c = MakeFish(sub,
                wx + ((float)(rand() % 5)) * 0.05f,
                h + ((float)(rand() % 10)) * 0.02f,
                wz + ((float)(rand() % 5)) * 0.05f);
            out.push_back(c);
        }
        return;
    }
}

void CritterFrameMove(Critter& c, uint32_t nowMs, float camX, float camZ) {
    const uint32_t t = nowMs + c.startOffsetMs;

    if (c.kind == 0) {
        // TMLeaf: static; alpha fades with squared camera distance.
        const float dx = c.x - camX;
        const float dz = c.z - camZ;
        const float fLen = dx * dx + dz * dz;
        if (fLen <= 400.0f)           // 20^2
            c.alpha = 1.0f;
        else
            c.alpha = 1.0f - (fLen - 400.0f) / (784.0f - 400.0f);
        if (c.alpha < 0.0f)
            c.alpha = 0.0f;
        return;
    }

    if (c.kind == 1) {
        // TMButterFly: 20s |sin| progress, 3 motion programs.
        const float fProgress = fabsf(sinf((float)(t % 20000u) / 20000.0f * kPi));
        switch (c.motionType) {
        case 1: {
            const float fCos = cosf(fProgress * kPi * c.circleSpeed);
            const float fCos2 = cosf(fProgress * 6.0f * kPi * c.circleSpeed);
            c.curY = fCos2 * c.particleV + c.y;
            c.curX = fCos * c.particleH * 0.5f + c.x;
            c.curZ = fProgress * c.particleH * 0.5f + c.z;
            break;
        }
        case 2: {
            const float fSin = sinf(fProgress * 2.0f * kPi);
            const float fCos2 = cosf(fProgress * 2.0f * kPi);
            const float fCos3 = cosf(fProgress * 6.0f * kPi * c.circleSpeed);
            c.curY = fCos3 * c.particleV + c.y;
            c.curX = fProgress * fCos2 * c.particleH * 0.5f + c.x;
            c.curZ = fProgress * fSin * c.particleH * 0.5f + c.z;
            break;
        }
        case 3: {
            const float p = (float)(t % 7000u) / 7000.0f;
            const float dir = 1.0f;   // original flips randomly at cycle restart
            const float fSin = sinf(dir * p * 2.0f * kPi);
            const float fCos = cosf(dir * p * 2.0f * kPi);
            c.curX = fCos * c.particleH * 0.5f + c.x;
            c.curZ = fSin * c.particleH * 0.5f + c.z;
            c.curY = c.y + fSin * c.particleV * 0.2f;
            break;
        }
        default:
            break;
        }
        return;
    }

    // TMFish: 40s sin progress with per-fish phase offset.
    const uint32_t dwOffset = (uint32_t)((c.particleH * 12.0f + c.circleSpeed * 12.0f)
        + (float)(20 * 0u) + 10.0f * c.meshLook0 + c.scale * 10.0f
        + (float)(5 * c.motionType));
    const float fProgress = sinf((float)((dwOffset + t) % 40000u) / 40000.0f * kPi * 2.0f);
    if (c.motionType == 0) {
        const float fSin = sinf(fProgress * kPi * c.circleSpeed + (float)dwOffset * 0.05f);
        c.curX = fProgress * c.particleH * 0.4f + c.x;
        c.curZ = fSin * c.particleH * 0.3f + c.z;
    } else if (c.motionType == 1) {
        const float fCos = cosf(fProgress * kPi * c.circleSpeed + (float)dwOffset * 0.05f);
        c.curX = fCos * c.particleH * 0.4f + c.x;
        c.curZ = fProgress * c.particleH * 0.3f + c.z;
    } else {
        const float fSinf = sinf(fProgress * 2.0f * kPi + (float)dwOffset * 0.05f);
        const float fCosf = cosf(fProgress * 2.0f * kPi + (float)dwOffset * 0.05f);
        c.curX = fCosf * c.particleH * 0.1f + c.x;
        c.curZ = fSinf * c.particleH * 0.1f + c.z;
    }
    c.curAngle = atan2f(c.curX, c.curZ);
}

}
