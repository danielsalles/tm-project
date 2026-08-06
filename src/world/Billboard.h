#pragma once

#include <cstdint>

#include "math/TMMath.h"

namespace tmx {

// TMEffectBillBoard simulation (TMEffectBillBoard.cpp:190-575), CPU-only and
// GL-free so tests can run headless. A Billboard produces, per frame, a world
// matrix + color + flipbook frame; the EffectRenderer draws it.
struct BillboardDesc {
    int textureIndex = 0;        // EffectTextureList base index
    uint32_t lifeTimeMs = 0;     // 0 = infinite (static effects)
    float scaleX = 0.5f, scaleY = 0.5f, scaleZ = 0.5f;
    float scaleVelX = 0.0f, scaleVelY = 0.0f, scaleVelZ = 0.0f;
    int cycleCount = 1;          // flipbook frames (consecutive list indices)
    int cycleTimeMs = 80;        // ms per frame
    int fade = 1;                // 0..4 (original default is 1)
    int motion = 0;              // m_nParticleType - 1: 0 rise, 1 sway, 2 circle, 3 spiral
    int lookCam = 1;             // 1 = face camera; 0 = fixed (axisAngle about Y)
    int stickGround = 0;         // lift by scaleY/2
    int animationType = 0;       // 0..3 pulse programs (2s/4s sin, gray)
    float axisAngle = 0.0f;      // roll (lookCam) or Y rotation (fixed)
    float particleH = 1.0f, particleV = 2.0f, circleSpeed = 6.0f;
    float baseAlpha = 0.7f;      // fade 2 floor
    float x = 0, y = 0, z = 0;
    uint32_t bgra = 0xFFFFFFFF;
    bool invertUV = false;       // texture 33 ctor quirk (tu 0..1, tv flipped)
};

struct Billboard {
    BillboardDesc d;
    uint32_t createMs = 0;
    bool started = false;
    bool dead = false;
    float progress = 0.0f;
    int cycleIndex = 0;
    uint32_t curBgra = 0xFFFFFFFF;
    float curX = 0, curY = 0, curZ = 0;   // after motion program
    D3DXMATRIX world;                     // unit-quad transform
};

// One frame of simulation. camYawH/camPitchV are the game's horizon/vertical
// camera angles. phaseSeed replaces the original's `this % 100` fade-2 phase
// (pass a stable per-object index).
void BillboardFrameMove(Billboard& b, uint32_t nowMs, float camYawH, float camPitchV,
                        uint32_t phaseSeed);

// Texture index of the current flipbook frame.
inline int BillboardTexture(const Billboard& b) { return b.d.textureIndex + b.cycleIndex; }

// Builds the EffectRenderer quad for a billboard (UV inset 0.02..0.98 avoids
// bleeding, doc 18 §3). blendMode: 1 = EF_BRIGHT, 0 = EF_DEFAULT. Defined in
// the .cpp so the sim header stays GL-free.
struct FxQuad;
FxQuad BillboardToQuad(const Billboard& b, int blendMode = 1);

}
