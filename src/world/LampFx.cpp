#include "world/LampFx.h"

namespace tmx {

void BuildLampGlow(uint32_t dwObjType, float x, float y, float z, float angle,
                   float scale, std::vector<Billboard>& out) {
    auto add = [&](int tex, float mul, uint32_t bgra, int cycles, int fade,
                   bool lookCam, float axisAngle, float dy) {
        Billboard b;
        b.d.textureIndex = tex;
        b.d.lifeTimeMs = 0;
        b.d.scaleX = scale * mul;
        b.d.scaleY = scale * mul;
        b.d.scaleZ = scale * mul;
        b.d.cycleCount = cycles;
        b.d.cycleTimeMs = 80;
        b.d.fade = fade;
        b.d.motion = 0;
        b.d.particleV = 0.0f;    // static flames don't rise
        b.d.lookCam = lookCam ? 1 : 0;
        b.d.axisAngle = axisAngle;
        b.d.x = x;
        b.d.y = y + dy;
        b.d.z = z;
        b.d.bgra = bgra;
        out.push_back(b);
    };

    switch (dwObjType) {
    case 501:
        add(11, 1.0f, 0xEEEECC00, 8, 0, true, 0.0f, 0.0f);
        add(2, 2.8f, 0x55553300, 1, 0, true, 0.0f, 0.0f);
        break;
    case 502:
        add(61, 1.0f, 0xFFFFFFFF, 6, 0, true, 0.0f, 0.0f);
        add(2, 2.8f, 0x55553300, 1, 0, true, 0.0f, 0.0f);
        break;
    case 503:
        add(101, 1.0f, 0xFF5500FF, 8, 0, true, 0.0f, 0.0f);
        add(101, 0.5f, 0xFFFFFFFF, 8, 0, true, 0.0f, -scale * 0.2f);
        add(2, 2.8f, 0xFF330055, 1, 0, true, 0.0f, 0.0f);
        break;
    case 504:
        add(56, 1.0f, 0xFFFF0000, 1, 2, true, 0.0f, 0.0f);
        break;
    case 505:
        add(79, 1.0f, 0x33330000, 1, 0, false, angle, 0.0f);
        break;
    default:
        break;
    }
}

}
