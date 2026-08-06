#pragma once

#include <cstdint>
#include <vector>

#include "math/TMMath.h"

namespace tmx {

class EffectRenderer;

// TMRain / TMSnow (TMRain.cpp, TMSnow.cpp): camera-following precipitation.
// Rain: 50 drops, RELATIVE positions (render adds the camera), texture 9,
//   fall = speed * (dt*0.1), speeds 0.08*(rand%3) (0 -> 0.24).
// Snow: 200 flakes, ABSOLUTE positions, texture 2, EF_BRIGHT, two systems
//   (scale multiplier 1.0 and 2.0 -> m_fScale = mult*0.05), per-flake height
//   fade with the original's int8 wrap quirk, sway per FrameMove call.
struct WeatherDrop {
    float x, y, z;
    float speed;
};

struct WeatherSystem {
    int type = 0;               // 0 = rain, 1 = snow
    float scale = 1.0f;         // snow multiplier (1.0 / 2.0)
    std::vector<WeatherDrop> drops;
    uint32_t lastMs = 0;
    bool started = false;
};

void WeatherInit(WeatherSystem& w, int type, float scale);

// focusX/Z/H: followed character position+height; when no character is
// focused the original uses the camera (camPos + 2.5, camY - 2 for snow) —
// the caller passes whatever applies.
void WeatherFrameMove(WeatherSystem& w, uint32_t nowMs,
                      float focusX, float focusH, float focusZ);

// Emits one FxQuad per drop/flake. right/up are the view matrix row axes
// (view._11,_21,_31 / view._12,_22,_32).
void WeatherEmit(const WeatherSystem& w, float camX, float camZ,
                 float rightX, float rightY, float rightZ,
                 float upX, float upY, float upZ,
                 float focusH, EffectRenderer& fx);

int WeatherTextureIndex(int type);   // rain 9, snow 2

}
