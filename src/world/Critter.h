#pragma once

#include <cstdint>
#include <vector>

namespace tmx {

// Ambient critters (TMLeaf / TMButterFly / TMFish). Spawn rules come from
// TMObjectContainer.cpp:140-310; movement is parametric sin/cos wandering.
struct Critter {
    int kind = 0;            // 0 leaf, 1 butterfly, 2 fish
    int skinMeshType = 61;   // BoneAni4 index (61 lf01, 69 bt01?, 24 bd01, 70 fish)
    int16_t meshLook0 = 0;
    int16_t skinLook0 = 0;
    float scale = 1.0f;
    uint32_t fps = 80;
    int motionType = 0;
    float particleH = 1.0f, particleV = 2.0f, circleSpeed = 6.0f;
    uint32_t startOffsetMs = 0;
    float x = 0, y = 0, z = 0;   // spawn (start) position
    float angle = 0.0f;

    // Runtime
    float curX = 0, curY = 0, curZ = 0;
    float curAngle = 0.0f;
    float alpha = 1.0f;          // leaf distance fade
};

// Appends critters for one .dat record (butterflies/fish spawn 5 per record
// with the original's random offsets; leaves 1). wx/wz/h = world position.
void CritterSpawn(uint32_t dwObjType, float wx, float h, float wz,
                  std::vector<Critter>& out);

// Wander + leaf fade. camX/camZ = camera for the leaf distance fade (cull 30u,
// fade 20-28u, squared distances like the original).
void CritterFrameMove(Critter& c, uint32_t nowMs, float camX, float camZ);

}
