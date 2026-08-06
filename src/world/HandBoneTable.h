#pragma once

#include <cstdint>

namespace tmx {

// g_dwHandIndex (TMGlobal.h:373): per BoneAni4 index, the [right,left] hand bone
// frame IDs. Used by CMesh::InitEffect to attach a TMEffectSWSwing and by our
// SwingTrail to find the weapon bone. Indices 0-19 are populated (humanoids +
// early monsters); the rest are {0,0} (no hand bone / two-handed).
inline void GetHandBones(int boneAniIndex, uint32_t& right, uint32_t& left) {
    static const uint32_t kTable[20][2] = {
        {19, 25}, {18, 24}, {15, 21}, {15, 21}, {12, 18}, {22, 28}, {23, 29},
        {20, 26}, {24, 30}, {23, 31}, {32, 17}, {22, 35}, {34, 44}, {34, 44},
        {34, 44}, {34, 44}, {34, 44}, {34, 44}, {34, 44}, {34, 44},
    };
    if (boneAniIndex < 0 || boneAniIndex >= 20) {
        right = 0; left = 0;
        return;
    }
    right = kTable[boneAniIndex][0];
    left  = kTable[boneAniIndex][1];
}

} // namespace tmx
