#pragma once

#include <cstdint>
#include <vector>

#include "gl/EffectRenderer.h"
#include "math/TMMath.h"

namespace tmx {

// TMSun port (TMSun.cpp): the sun itself + a 12-entry lens flare, all
// screen-space quads projected from the light direction. Blend is
// SRCCOLOR/INVSRCOLOR (D3D SRCBLEND=3/DESTBLEND=7).
struct SunFlareEntry {
    int      texIndex;
    float    loc;      // position along the flare line (-0.6 .. +0.6)
    float    scale;    // half-size in pixels * widthRatio*50
    uint32_t bgra;
};

// Builds the 12-entry table (TMSun::InitObject). widthRatio = screenW/800-ish
// (RenderDevice::m_fWidthRatio); pass 1.0 for the reference width.
void SunFlareBuildTable(float widthRatio, SunFlareEntry out[12]);

// Computes the 12 quads for this frame. Returns false when the sun is behind
// the camera (projected z > 1). The anchor is camera + (-1, 0.7*defSize, 0.3)
// (TMSun's fixed m_vFlareDirection); defSize scales everything (weather fade).
bool SunFlareCompute(const SunFlareEntry table[12], float camX, float camY, float camZ,
                     const D3DXMATRIX& view, const D3DXMATRIX& proj,
                     int screenW, int screenH, float defSize, FxQuad out[12]);

}
