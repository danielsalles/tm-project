#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

namespace tmx {

// CPU-side mirror of an .msa file, exactly as TMMesh::LoadMsa reads it
// (TMMesh.cpp:481-728). Kept separate from the GPU object so tests can
// validate parsing without a GL context.
struct MsaData {
    uint32_t fvf = 0;
    uint32_t fileStride = 0;              // stride on disk
    uint32_t memStride = 0;               // stride in memory (fileStride, or +8 when fvf != 322)

    struct AttrRange {                    // D3DXATTRIBUTERANGE, 20 bytes on disk
        uint32_t attribId;
        uint32_t faceStart;
        uint32_t faceCount;
        uint32_t vertexStart;
        uint32_t vertexCount;
    };
    std::vector<AttrRange> subsets;
    std::vector<std::string> textureNames; // per subset: 11 raw bytes, path-stripped, ext-stripped

    std::vector<uint16_t> indices;
    std::vector<uint8_t> vertices;         // nVerts * memStride, uv1 duplicated from uv0 (fvf 530)

    uint32_t NumVerts() const { return memStride ? (uint32_t)(vertices.size() / memStride) : 0; }
};

// Parses an .msa blob. Replicates the original's quirks on purpose:
//  - texture names are 11 bytes, NOT NUL-terminated on disk
//  - fvf != 322: vertices on disk are 8 bytes shorter than in memory; the loader
//    expands, adds +256 to the FVF (TEX1) like the original, and duplicates
//    uv0 -> uv1 (disk fvf 274 -> memory fvf 530), including the original's
//    nVerts-1 loop bound (last vertex keeps zeroed uv1)
bool ParseMsa(const uint8_t* data, size_t size, MsaData& out, std::string* err);

// GPU mesh: VAO picked from the file's FVF (05 §5.3 table), static buffers,
// colors converted BGRA -> RGBA at load (option (a) of 05 §5.3).
struct GLMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    struct Subset {
        uint32_t indexStart;   // first index (attrRange.faceStart * 3)
        uint32_t indexCount;   // attrRange.faceCount * 3
        int      textureIndex; // resolved by the caller via textureNames
        char     alphaFlag;    // list cAlpha: 'N' opaque, 'C' cutout, 'A'/'a' blend
    };
    Subset subsets[32];
    int subsetCount = 0;
    std::string textureNames[32];

    bool Upload(const MsaData& data);
    void Destroy();
};

}
