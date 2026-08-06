#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math/TMMath.h"

namespace tmx {

// CPU-side mirror of a .msh skinned mesh part (CMesh::RestoreDeviceObjects,
// CMesh.cpp:814-910). Validated byte-exact against mesh/tr010101.msh.
//
// Disk layout:
//   u32 parentID, u32 id, u32 FVF, u32 sizeVertex,
//   u32 numFaceInflunce (1..4 weights/vertex), u32 numPalette,
//   u32 vertexCount, u32 faceCount (index count; /3 = triangles)
//   if numPalette: mat4 boneBindInv[numPalette], u32 boneFrameId[numPalette]
//   u8 vertices[sizeVertex * vertexCount]
//   u16 indices[faceCount]
//
// Vertex layout by numFaceInflunce N (RenderDevice.cpp VertexDecl1-4):
//   pos f3 @0, weights float[N-1] @12, boneIndices u8x4 @12+4(N-1),
//   normal f3 @12+4(N-1)+4, uv f2 after normal.
//   N=1: 36B, N=2: 40B, N=3: 44B, N=4: 48B.
struct MshData {
    uint32_t fvf = 0;
    uint32_t vsize = 0;
    uint32_t numInfluence = 0;
    uint32_t numPalette = 0;
    uint32_t numVerts = 0;

    std::vector<D3DXMATRIX> boneBindInv;   // mesh -> bone space (bind pose inverse)
    std::vector<uint32_t> boneFrameId;     // which .bon frame each palette entry uses

    std::vector<uint8_t>  vertices;
    std::vector<uint16_t> indices;

    // Vertex element offsets derived from numInfluence.
    uint32_t OffWeights() const { return 12; }
    uint32_t OffIndices() const { return 12 + 4 * (numInfluence - 1); }
    uint32_t OffNormal()  const { return OffIndices() + 4; }
    uint32_t OffUV()      const { return OffNormal() + 12; }
};

bool ParseMsh(const uint8_t* data, size_t size, MshData& out, std::string* err);

}
