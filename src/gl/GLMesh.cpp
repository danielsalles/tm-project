#include "gl/GLMesh.h"

#include <cstring>
#include <cstdio>

namespace tmx {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t ReadU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

// Texture name on disk: 11 bytes, may carry a path prefix ("dir\name.ext") and is
// not NUL-terminated. The original strips the path and the extension
// (TMMesh.cpp:524-543). Same here.
std::string CleanTextureName(const uint8_t* raw11) {
    char buf[12];
    memcpy(buf, raw11, 11);
    buf[11] = '\0';

    const char* start = buf;
    for (char* p = buf; *p; ++p) {
        if (*p == '\\' || *p == '/')
            start = p + 1;
    }

    std::string name = start;
    size_t dot = name.find('.');
    if (dot != std::string::npos)
        name.resize(dot);
    return name;
}

} // namespace

bool ParseMsa(const uint8_t* data, size_t size, MsaData& out, std::string* err) {
    auto fail = [&](const char* msg) {
        if (err) *err = msg;
        return false;
    };

    size_t pos = 0;
    auto need = [&](size_t n) { return pos + n <= size; };

    if (!need(12))
        return fail("truncated header");
    out.fvf = ReadU32(data + pos);        pos += 4;
    out.fileStride = ReadU32(data + pos); pos += 4;
    uint32_t attCount = ReadU32(data + pos); pos += 4;

    if (attCount == 0 || attCount > 32)
        return fail("attr count out of range");
    if (out.fileStride == 0 || out.fileStride > 64)
        return fail("vertex stride out of range");

    out.subsets.resize(attCount);
    for (uint32_t i = 0; i < attCount; ++i) {
        if (!need(20))
            return fail("truncated attr ranges");
        out.subsets[i].attribId    = ReadU32(data + pos + 0);
        out.subsets[i].faceStart   = ReadU32(data + pos + 4);
        out.subsets[i].faceCount   = ReadU32(data + pos + 8);
        out.subsets[i].vertexStart = ReadU32(data + pos + 12);
        out.subsets[i].vertexCount = ReadU32(data + pos + 16);
        pos += 20;
    }

    out.textureNames.resize(attCount);
    for (uint32_t i = 0; i < attCount; ++i) {
        if (!need(11))
            return fail("truncated texture names");
        out.textureNames[i] = CleanTextureName(data + pos);
        pos += 11;
    }

    if (!need(4))
        return fail("truncated IB size");
    uint32_t ibBytes = ReadU32(data + pos); pos += 4;
    if (ibBytes % 2 != 0 || !need(ibBytes))
        return fail("truncated IB blob");
    out.indices.resize(ibBytes / 2);
    for (size_t i = 0; i < out.indices.size(); ++i)
        out.indices[i] = ReadU16(data + pos + i * 2);
    pos += ibBytes;

    if (!need(4))
        return fail("truncated VB size");
    uint32_t vbBytes = ReadU32(data + pos); pos += 4;
    if (!need(vbBytes))
        return fail("truncated VB blob");

    uint32_t nVerts = out.fileStride ? vbBytes / out.fileStride : 0;
    if (nVerts == 0)
        return fail("empty vertex buffer");

    out.memStride = out.fileStride;
    out.vertices.resize((size_t)nVerts * out.fileStride);
    memcpy(out.vertices.data(), data + pos, (size_t)nVerts * out.fileStride);

    if (out.fvf != 322) {
        // Expand: disk stride is 8 bytes shorter; FVF gains +256 (TEX1) like the
        // original (m_dwFVF += 256), so disk fvf 274 becomes memory fvf 530 and
        // uv1 = uv0. The uv copy keeps the original's nVerts-1 loop bound
        // (TMMesh.cpp:688-696) — last vertex keeps zeroed uv1.
        std::vector<uint8_t> expanded((size_t)nVerts * (out.fileStride + 8), 0);
        for (uint32_t i = 0; i < nVerts; ++i)
            memcpy(expanded.data() + (size_t)i * (out.fileStride + 8),
                   out.vertices.data() + (size_t)i * out.fileStride, out.fileStride);
        out.memStride = out.fileStride + 8;
        out.fvf += 256;

        if (out.fvf == 530) {
            float* fv = reinterpret_cast<float*>(expanded.data());
            uint32_t memFloats = out.memStride >> 2;
            for (uint32_t i = 0; i + 1 < nVerts; ++i) {
                fv[i * memFloats + 8] = fv[i * memFloats + 6];
                fv[i * memFloats + 9] = fv[i * memFloats + 7];
            }
        }
        out.vertices = std::move(expanded);
    }
    pos += vbBytes;

    return true;
}

bool GLMesh::Upload(const MsaData& data) {
    Destroy();

    if (data.indices.empty() || data.vertices.empty())
        return false;

    // BGRA -> RGBA color conversion (D3DCOLOR is B,G,R,A in little-endian memory).
    // Color offset within the vertex depends on the FVF layout (05 §5.3):
    //   322: pos(12) + color @12
    //   530 (+8 expansion): pos(12) + normal(12) + uv0 @24 ... no color
    std::vector<uint8_t> verts = data.vertices;
    if (data.fvf == 322) {
        for (size_t off = 12; off + 3 < verts.size(); off += data.memStride)
            std::swap(verts[off], verts[off + 2]);
    }

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)verts.size(), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(data.indices.size() * 2),
                 data.indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = (GLsizei)data.memStride;
    if (data.fvf == 322) {
        // VAO_L: pos f3@0, color u8x4 norm@12, uv0 f2@16
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)12);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)16);
        // constant normal (up) — mesh_lit expects the attribute
        glVertexAttrib3f(1, 0.0f, 1.0f, 0.0f);
        // constant white is NOT forced: aColor comes from the stream
    } else if (data.fvf == 530) {
        // VAO_N2: pos f3@0, normal f3@12, uv0 f2@24, uv1 f2@32
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)12);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)24);
        glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        // Unknown layout for this phase: fail loudly instead of guessing (doc 15 §6)
        glBindVertexArray(0);
        Destroy();
        return false;
    }

    glBindVertexArray(0);

    subsetCount = (int)data.subsets.size();
    if (subsetCount > 32)
        subsetCount = 32;
    for (int i = 0; i < subsetCount; ++i) {
        subsets[i].indexStart = data.subsets[i].faceStart * 3;
        subsets[i].indexCount = data.subsets[i].faceCount * 3;
        subsets[i].textureIndex = -1;
        subsets[i].alphaFlag = 'N';
        textureNames[i] = data.textureNames[i];
    }
    return true;
}

void GLMesh::Destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    subsetCount = 0;
}

}
