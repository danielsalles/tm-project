#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math/TMMath.h"

namespace tmx {

// Bone animation set for one BoneAni4.txt entry (e.g. mesh\tr01):
//   .bon = (u32 parentID, u32 frameID) pairs — frame tree (TMSkinMesh::RestoreDeviceObjects)
//   .ani = u32 numTicks, u32 numBones, mat4[numTicks * numBones]
//          indexed [tick * numBones + frameID] (MeshManager.cpp:159-184;
//          note the header names are swapped in the original: "numAniFrame" is
//          the bone count and the first dword ("buffer"/numAniCut) is the tick count)
struct BoneAniSet {
    struct Frame {
        int id = 0;
        int parent = -1;             // -1 = root's child (parent 0xFFFFFFFF in file)
        std::vector<int> children;   // frame ids
    };

    std::vector<Frame> frames;       // indexed by frame id
    int rootId = 0;

    int numTicks = 0;
    int numBones = 0;
    std::vector<D3DXMATRIX> ticks;   // [tick * numBones + frameId]

    // Per-frame runtime pose (matRot) and world-space combined matrices.
    std::vector<D3DXMATRIX> matRot;
    std::vector<D3DXMATRIX> combined;
};

// Loads mesh\<prefix>.bon + mesh\<prefix><aniFileIndex>.ani.
// aniFileIndex = ValidIndex.bin value + 1, formatted %04d — trees use 101
// (-> tr010101.ani). Only the first animation is loaded (trees have one).
bool LoadBoneAni(const char* prefix, int aniFileIndex, BoneAniSet& out, std::string* err);

// Advances the pose: original sampling (TMSkinMesh::FrameMove) at FPS=80:
// tick = (t_ms/80)/4 % numTicks, with a 4-step component-wise matrix lerp between
// tick and tick+1. Then combined = rot x parentCombined down the tree, with
// rootWorld as the root frame's matRot (object transform).
void SampleBoneAni(BoneAniSet& set, float timeMs, const D3DXMATRIX& rootWorld);

}
