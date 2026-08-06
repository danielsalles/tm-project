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
    if (out.numPalette > kMaxBones)
        return fail("msh: palette over kMaxBones");

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

bool GLSkinMesh::Upload(const MshData& data) {
    Destroy();
    if (data.numVerts == 0 || data.indices.empty())
        return false;

    numInfluence = data.numInfluence;
    numPalette = data.numPalette;
    if (numPalette > 40)
        numPalette = 40;
    for (uint32_t i = 0; i < numPalette; ++i) {
        boneBindInv[i] = data.boneBindInv[i];
        boneFrameId[i] = data.boneFrameId[i];
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data.vertices.size(), data.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(data.indices.size() * 2), data.indices.data(), GL_STATIC_DRAW);

    const uint32_t stride = data.vsize;
    glEnableVertexAttribArray(0);   // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // weights: N-1 floats at 12; the shader reads a vec4 — pad behavior differs
    // per N, so upload what exists and let the VS use only the first N-1.
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)data.OffWeights());
    glEnableVertexAttribArray(6);   // bone indices: 4 x u8
    glVertexAttribIPointer(6, 4, GL_UNSIGNED_BYTE, stride, (void*)(uintptr_t)data.OffIndices());
    glEnableVertexAttribArray(1);   // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)data.OffNormal());
    glEnableVertexAttribArray(3);   // uv
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)data.OffUV());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    indexCount = (int)data.indices.size();
    return true;
}

void GLSkinMesh::Destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

}
