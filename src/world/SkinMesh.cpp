#include "world/SkinMesh.h"

#include <cstring>

namespace tmx {

namespace {
uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
} // namespace

bool ParseMsh(const uint8_t* data, size_t size, MshData& out, std::string* err) {
    auto fail = [&](const char* msg) {
        if (err) *err = msg;
        return false;
    };
    if (!data || size < 32)
        return fail("msh: too small");

    out.fvf          = ReadU32(data + 8);
    out.vsize        = ReadU32(data + 12);
    out.numInfluence = ReadU32(data + 16);
    out.numPalette   = ReadU32(data + 20);
    out.numVerts     = ReadU32(data + 24);
    const uint32_t faceCount = ReadU32(data + 28);   // index count (3 per tri)

    if (out.numInfluence < 1 || out.numInfluence > 4)
        return fail("msh: numFaceInflunce outside 1..4");
    if (out.numPalette > 40)
        return fail("msh: palette over 40");

    // The declared stride must match the layout table (VertexDecl1-4).
    const uint32_t expectStride = 36 + 4 * (out.numInfluence - 1);
    if (out.vsize != expectStride)
        return fail("msh: stride/layout mismatch");
    if (out.numVerts == 0 || out.numVerts > 65535)
        return fail("msh: implausible vertex count");

    size_t pos = 32;
    if (out.numPalette) {
        if (size < pos + (size_t)out.numPalette * 68)
            return fail("msh: truncated palette");
        out.boneBindInv.resize(out.numPalette);
        memcpy(out.boneBindInv.data(), data + pos, out.numPalette * 64);
        pos += out.numPalette * 64;
        out.boneFrameId.resize(out.numPalette);
        for (uint32_t i = 0; i < out.numPalette; ++i)
            out.boneFrameId[i] = ReadU32(data + pos + i * 4);
        pos += out.numPalette * 4;
    }

    if (size < pos + (size_t)out.numVerts * out.vsize)
        return fail("msh: truncated vertices");
    out.vertices.resize((size_t)out.numVerts * out.vsize);
    memcpy(out.vertices.data(), data + pos, out.vertices.size());
    pos += out.vertices.size();

    if (size < pos + (size_t)faceCount * 2)
        return fail("msh: truncated indices");
    out.indices.resize(faceCount);
    for (uint32_t i = 0; i < faceCount; ++i)
        out.indices[i] = (uint16_t)(data[pos + i * 2] | (data[pos + i * 2 + 1] << 8));

    return true;
}

}
