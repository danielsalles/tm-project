#include "world/WeatherFx.h"

#include "gl/EffectRenderer.h"

#include <cmath>
#include <cstdlib>

namespace tmx {

int WeatherTextureIndex(int type) {
    return type == 1 ? 2 : 9;
}

void WeatherInit(WeatherSystem& w, int type, float scale) {
    w.type = type;
    w.scale = scale;
    w.started = false;
    w.drops.resize(type == 1 ? 200 : 50);
    for (auto& d : w.drops) {
        d.x = 6.0f - ((float)(rand() % 24) / 2.0f);
        d.z = 6.0f - ((float)(rand() % 24) / 2.0f);
        d.y = (float)(rand() % 10);
        if (type == 1) {
            d.speed = (float)(rand() % 3) * 0.039999999f;
            if (d.speed == 0.0f)
                d.speed = 0.23999999f;
        } else {
            d.speed = (float)(rand() % 3) * 0.079999998f;
            if (d.speed == 0.0f)
                d.speed = 0.23999999f;
        }
    }
}

void WeatherFrameMove(WeatherSystem& w, uint32_t nowMs,
                      float focusX, float focusH, float focusZ) {
    if (!w.started) {
        w.lastMs = nowMs;
        w.started = true;
        // Snow positions are ABSOLUTE and the initial box sits near the world
        // origin; the original only re-centers flakes as they fall below the
        // focus (takes ~20s). Seed around the focus immediately instead.
        if (w.type == 1) {
            for (auto& d : w.drops) {
                d.y = focusH + (float)(rand() % 10);
                d.x = (4.0f - ((float)(rand() % 32) / 3.0f)) + focusX;
                d.z = (4.0f - ((float)(rand() % 32) / 3.0f)) + focusZ;
            }
        }
        return;
    }
    const float dtMs = (float)(nowMs - w.lastMs);
    w.lastMs = nowMs;

    if (w.type == 0) {
        const float elapsed = dtMs * 0.1f;
        for (auto& d : w.drops) {
            if (d.y > focusH) {
                d.y -= d.speed * elapsed;
                continue;
            }
            // (splash BillBoard2 skipped — decal scope, Fase 5)
            d.y = focusH + 10.0f;
            d.x = 6.0f - ((float)(rand() % 24) / 2.0f);
            d.z = 6.0f - ((float)(rand() % 24) / 2.0f);
        }
    } else {
        const float elapsed = dtMs * 0.02f;
        const int n = (int)w.drops.size();
        for (int i = 0; i < n; ++i) {
            WeatherDrop& d = w.drops[(size_t)i];
            const float fSin = sinf(((float)i * 2.0f / (float)n) * 3.14159265f);
            if (d.y > focusH) {
                d.y -= d.speed * elapsed;
                d.x += fSin * 0.0099999998f;
            } else {
                d.y = (focusH + 7.0f) + ((float)(rand() % 10) * 0.5f);
                d.x = (4.0f - ((float)(rand() % 32) / 3.0f)) + focusX;
                d.z = (4.0f - ((float)(rand() % 32) / 3.0f)) + focusZ;
            }
        }
    }
}

void WeatherEmit(const WeatherSystem& w, float camX, float camZ,
                 float rightX, float rightY, float rightZ,
                 float upX, float upY, float upZ,
                 float focusH, EffectRenderer& fx) {
    if (w.type == 0) {
        // Streaks 0.016 wide (view xz) x 0.4 tall (fixed Y), color 0x33333333,
        // positions relative to the camera.
        const float s = 0.008f * 2.0f;   // quad corners are +-0.5
        for (const auto& d : w.drops) {
            FxQuad q;
            D3DXMatrixIdentity(&q.world);
            q.world._11 = rightX * s; q.world._21 = 0.0f;     q.world._31 = rightZ * s;
            q.world._12 = 0.0f;       q.world._22 = 0.4f;     q.world._32 = 0.0f;
            q.world._41 = d.x + camX;
            q.world._42 = d.y;
            q.world._43 = d.z + camZ;
            q.bgra = 0x33333333;
            q.textureIndex = WeatherTextureIndex(0);
            q.blendMode = 1;
            fx.Emit(q);
        }
    } else {
        const float s = w.scale * 0.05f * 2.0f;
        for (const auto& d : w.drops) {
            // Height fade with the original's int8 wrap quirk.
            int cCol = (int)((d.y - focusH) * 24.0f);
            if (d.y > focusH + 7.0f)
                cCol = -86;
            if (focusH > d.y)
                cCol = 0;
            const uint32_t v = (uint8_t)(int8_t)cCol;
            FxQuad q;
            D3DXMatrixIdentity(&q.world);
            q.world._11 = rightX * s; q.world._21 = rightY * s; q.world._31 = rightZ * s;
            q.world._12 = upX * s;    q.world._22 = upY * s;    q.world._32 = upZ * s;
            q.world._41 = d.x;
            q.world._42 = d.y;
            q.world._43 = d.z;
            q.bgra = v | (v << 8) | (v << 16) | (v << 24);
            q.textureIndex = WeatherTextureIndex(1);
            q.blendMode = 1;
            fx.Emit(q);
        }
    }
}

}
