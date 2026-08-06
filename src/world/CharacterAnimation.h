#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math/TMMath.h"
#include "world/BoneAnimation.h"

namespace tmx {

// Character/monster animation set (MeshManager::LoadBoneAnimationList,
// MeshManager.cpp:95-340). Unlike trees (single .ani), these types try
// numAniTypeCount files `prefix%04d.ani` (index from ValidIndex.bin + 1);
// each existing file is one "cut" and all cuts are concatenated into a single
// matrix array. Humanoids (BoneAni4 index 0/1, ch01/ch02) also precompute a
// quaternion per matrix for slerp crossfades.
constexpr int kMaxWeaponSlots = 60;  // m_sAnimationArray 2nd dimension
constexpr int kCharMotions = 56;     // 28 normal + 28 mounted

struct CharacterAnimation {
    BoneAniSet base;                        // skeleton + runtime pose buffers
    uint32_t numBones = 0;

    std::vector<uint32_t> numAniCut;        // ticks per loaded cut
    std::vector<uint32_t> cutTickOffset;    // prefix sums (m_nAniBaseIndex)
    std::vector<D3DXMATRIX> mats;           // [sum(ticks) * numBones]
    std::vector<D3DXQUATERNION> quats;      // humanoids only, same layout as mats
    int numCuts = 0;
    bool dwModMinus2 = false;               // boneAniIndex 49 quirk

    // [weapon][motion] -> cut index; -1 = none. Humanoids only.
    int16_t animMap[kMaxWeaponSlots][kCharMotions];
};

// Mesh/ValidIndex.bin: [100][186] int32 row-major.
bool LoadValidIndex(const char* path, std::vector<int32_t>& out, std::string* err);

// Loads skeleton (.bon) + every existing cut. `validRow` = 186 entries for this
// BoneAni4 index. humanoid = (boneAniIndex 0 or 1): builds quats + animMap with
// the original TK-BM / cross-class fallback quirks.
bool LoadCharacterAnimation(int boneAniIndex, const char* prefix, int numAniTypeCount,
                            const int32_t* validRow, bool humanoid,
                            CharacterAnimation& out, std::string* err);

// Per-instance runtime pose (one per character; the CharacterAnimation data
// itself is shared between instances of the same type).
struct CharacterPose {
    std::vector<D3DXMATRIX> matRot;    // per frame
    std::vector<D3DXMATRIX> combined;  // world-space, down the tree

    void Resize(size_t n) {
        matRot.resize(n);
        combined.resize(n);
        for (size_t i = 0; i < n; ++i)
            D3DXMatrixIdentity(&matRot[i]);
    }
};

// Playback state for one animated character instance.
struct CharPlayback {
    int cut = 0;                 // m_nAniIndex
    int lastCut = 0;             // m_nAniIndexLast
    uint32_t fps = 30;           // m_dwFPS (ms per sub-step; from AniSound speed)
    uint32_t switchTimeMs = 0;   // m_dwStartOffset
    uint32_t tickLastFlat = 0;   // frozen pose of the previous cut (matrix units)
    bool blending = false;       // m_nAniIndexLast != 0
};

// TMSkinMesh::SetAnimation: switches cut, freezing the current pose for the
// 10-step crossfade. Returns false if cut >= numCuts.
bool CharSetAnimation(const CharacterAnimation& anim, CharPlayback& pb, int cut,
                      uint32_t fps, uint32_t nowMs);

// TMSkinMesh::FrameMove sampling: tick = (t/fps)/4 within the cut (4 sub-steps,
// fixed 1/4-1/2-3/4 blends), 10-step crossfade after a cut switch (slerp for
// humanoids, matrix lerp otherwise), then combined = rot x parent down the tree
// with rootWorld as the root frame's transform. `pose` must be Resize()d to the
// skeleton's frame count.
void SampleCharacter(const CharacterAnimation& anim, CharacterPose& pose,
                     CharPlayback& pb, uint32_t nowMs, const D3DXMATRIX& rootWorld);

}
