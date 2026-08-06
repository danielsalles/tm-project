#pragma once

#include <cstdint>
#include <vector>

#include "world/Billboard.h"

namespace tmx {

// Lamp/torch glow billboards for .dat objects 501-505
// (TMObjectContainer.cpp:405-530). Every lamp billboard is EF_BRIGHT, static
// (lifeTime 0), 80ms flipbook cycles. `scale` replaces the original's
// fScaleH/fScaleV (absent from this build's 28-byte records).
void BuildLampGlow(uint32_t dwObjType, float x, float y, float z, float angle,
                   float scale, std::vector<Billboard>& out);

}
