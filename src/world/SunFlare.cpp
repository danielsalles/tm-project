#include "world/SunFlare.h"

namespace tmx {

void SunFlareBuildTable(float widthRatio, SunFlareEntry out[12]) {
    // TMSun::InitObject (verbatim table).
    static const int      kTex[12] = { 206, 212, 212, 211, 210, 211, 212, 213, 210, 209, 206, 208 };
    static const float    kLoc[12] = { -0.6f, -0.4f, -0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        0.1f, 0.2f, 0.3f, 0.6f };
    static const float    kScale[12] = { 0.4f, 0.23f, 0.09f, 1.6f, 0.93f, 2.8f, 9.43f, 10.6f,
                                         0.42f, 0.64f, 0.73f, 1.56f };
    static const uint32_t kCol[12] = { 0xAA8888u, 0xAAAA22u, 0xAA22AAu, 0xFFFFFFu, 0xAAAAAAu,
                                       0xAAAAAAu, 0xAAAAAAu, 0xFFFFFFu, 0xAAAAAAu, 0xAAAAAAu,
                                       0xAAAAAAu, 0xAAAAAAu };
    const float fRatio = widthRatio * 50.0f;
    for (int i = 0; i < 12; ++i) {
        out[i].texIndex = kTex[i];
        out[i].loc = kLoc[i];
        out[i].scale = kScale[i] * fRatio;
        out[i].bgra = kCol[i];
    }
}

bool SunFlareCompute(const SunFlareEntry table[12], float camX, float camY, float camZ,
                     const D3DXMATRIX& view, const D3DXMATRIX& proj,
                     int screenW, int screenH, float defSize, FxQuad out[12]) {
    // TMSun's fixed flare direction; y scales with defSize (TMSun.cpp:137).
    D3DXVECTOR3 light(camX - 1.0f, camY + defSize * 0.7f, camZ + 0.3f);

    D3DVIEWPORT9 vp;
    vp.X = 0; vp.Y = 0;
    vp.Width = (unsigned long)screenW;
    vp.Height = (unsigned long)screenH;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    D3DXVECTOR3 flarePos;
    D3DXVec3Project(&flarePos, &light, &vp, &proj, &view, &world);
    if (flarePos.z > 1.0f)
        return false;

    const float cx = (float)(screenW / 2) - flarePos.x;
    const float cy = (float)(screenH / 2) - flarePos.y;

    for (int i = 0; i < 12; ++i) {
        const float px = cx * table[i].loc + flarePos.x;
        const float py = cy * table[i].loc + flarePos.y;
        const float half = table[i].scale * defSize;
        FxQuad& q = out[i];
        D3DXMatrixIdentity(&q.world);
        q.world._11 = half * 2.0f;       // fx_quad screen path: aPos(+-0.5) * size
        q.world._22 = half * 2.0f;
        q.world._41 = px;
        q.world._42 = py;
        q.u0 = 0.0f; q.v0 = 0.0f;        // original uses -1..0 with wrap; 0..1 == same
        q.u1 = 1.0f; q.v1 = 1.0f;
        q.bgra = table[i].bgra;
        q.textureIndex = table[i].texIndex;
        q.blendMode = 2;                 // SRCCOLOR / INVSRCOLOR
        q.screenSpace = true;
    }
    return true;
}

}
