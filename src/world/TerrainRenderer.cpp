#include "world/TerrainRenderer.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLShader.h"
#include "gl/GLTexture.h"
#include "world/TerrainData.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace tmx {

namespace {

// BGRA (D3DCOLOR in memory) -> RGBA, keeping alpha (shore translucency).
uint32_t BgraToRgba(uint32_t c) {
    const uint32_t b = c & 0xFF, g = (c >> 8) & 0xFF, r = (c >> 16) & 0xFF, a = (c >> 24) & 0xFF;
    return r | (g << 8) | (b << 16) | (a << 24);
}

struct BatchKey {
    int  mode;
    int  texFront;
    int  texBack;
    bool lavaScroll;
    bool operator<(const BatchKey& o) const {
        if (mode != o.mode) return mode < o.mode;
        if (texFront != o.texFront) return texFront < o.texFront;
        if (texBack != o.texBack) return texBack < o.texBack;
        return lavaScroll < o.lavaScroll;
    }
};

} // namespace

bool TerrainRenderer::Init(std::string* err) {
    if (!m_shader.Build(kCommonGlsl, kTerrainVert, kTerrainFrag, err))
        return false;
    m_locTex0        = m_shader.UniformLoc("uTex0");
    m_locTex1        = m_shader.UniformLoc("uTex1");
    m_locModulate2X  = m_shader.UniformLoc("uModulate2X");
    m_locLavaScroll  = m_shader.UniformLoc("uLavaScroll");
    return true;
}

bool TerrainRenderer::Build(const TerrainData& terrain, std::string* err) {
    if (m_built) {
        if (err) *err = "terrain: already built";
        return false;
    }

    const float offX = terrain.OffsetX();
    const float offY = terrain.OffsetY();

    // 4 verts per quad, 63x63 quads; indices 6 per quad.
    m_vertices.resize((size_t)63 * 63 * 4 * 44);
    std::vector<uint16_t> indices;
    indices.reserve((size_t)63 * 63 * 6);

    // Per-bucket index runs.
    std::map<BatchKey, std::vector<uint16_t>> bucketIdx;

    uint16_t vbase = 0;
    uint8_t* vp = m_vertices.data();

    for (int y = 0; y < 63; ++y) {
        for (int x = 0; x < 63; ++x) {
            const TerrainTile& t = terrain.tiles[x + (y << 6)];
            const int nIndex = t.tileIndex + 10;

            BatchKey key{ 0, nIndex, 0, false };
            if (nIndex == 38 || nIndex == 39) {           // lava: tex1 = env 344, scroll
                key.mode = 1;
                key.texBack = 344;
                key.lavaScroll = true;
            } else if (nIndex >= 62 && nIndex <= 65) {    // water: tex1 = env nIndex+286
                key.mode = 1;
                key.texBack = nIndex + 286;
            }

            // Corner order matches the original's tristrip:
            // 0=(x,y) 1=(x,y+1) 2=(x+1,y) 3=(x+1,y+1)
            const int cx[4] = { x, x, x + 1, x + 1 };
            const int cy[4] = { y, y + 1, y, y + 1 };
            for (int k = 0; k < 4; ++k) {
                const TerrainTile& c = terrain.tiles[cx[k] + (cy[k] << 6)];
                float* pos = (float*)(vp);
                pos[0] = cx[k] * TerrainData::kWorldScale + offX;
                pos[1] = c.height * TerrainData::kHeightScale;
                pos[2] = cy[k] * TerrainData::kWorldScale + offY;
                memcpy(vp + 12, terrain.normals[cx[k] + (cy[k] << 6)], 12);
                const uint32_t rgba = BgraToRgba(c.color);
                memcpy(vp + 24, &rgba, 4);
                float* uv0 = (float*)(vp + 28);
                uv0[0] = kTileCoordList[t.tileCoord & 7][k][0];
                uv0[1] = kTileCoordList[t.tileCoord & 7][k][1];
                float* uv1 = (float*)(vp + 36);
                if (key.lavaScroll) {
                    // Lava animates tv2 = base + time (TMGround.cpp:3101-3106); the
                    // scroll is applied in the VS via uTime, base pattern here.
                    static const float fx[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
                    static const float fy[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
                    uv1[0] = fx[k];
                    uv1[1] = fy[k];
                } else if (key.mode == 1) {
                    uv1[0] = uv0[0];   // water 62-65: uv1 = uv0
                    uv1[1] = uv0[1];
                } else {
                    uv1[0] = kBackTileCoordList[t.backTileCoord & 31][k][0];
                    uv1[1] = kBackTileCoordList[t.backTileCoord & 31][k][1];
                }
                vp += 44;
            }

            std::vector<uint16_t>& dst = bucketIdx[key];
            // Original tri order (0,1,2)/(3,2,1): D3D CW-front == our GL front
            // (glFrontFace(GL_CW) set at context init).
            dst.push_back(vbase + 0);
            dst.push_back(vbase + 1);
            dst.push_back(vbase + 2);
            dst.push_back(vbase + 3);
            dst.push_back(vbase + 2);
            dst.push_back(vbase + 1);
            vbase += 4;
        }
    }

    // Flatten buckets into one EBO; record per-batch ranges.
    m_batches.clear();
    for (auto& [key, idx] : bucketIdx) {
        Batch b;
        b.mode = key.mode;
        b.texFront = key.texFront;
        b.texBack = key.texBack;
        b.lavaScroll = key.lavaScroll;
        b.indexStart = (uint32_t)indices.size();
        b.indexCount = (uint32_t)idx.size();
        indices.insert(indices.end(), idx.begin(), idx.end());
        m_batches.push_back(b);
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_vertices.size(), m_vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * 2), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 44, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 44, (void*)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 44, (void*)24);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 44, (void*)28);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 44, (void*)36);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_built = true;
    return true;
}

void TerrainRenderer::Destroy() {
    m_shader.Destroy();
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    m_vao = m_vbo = m_ebo = 0;
    m_batches.clear();
    m_vertices.clear();
    m_built = false;
}

void TerrainRenderer::Render(GLRenderDevice& device, GLuint (*envTextureFn)(int, void*), void* ctx) {
    if (!m_built)
        return;

    GLStateCache& st = device.State();
    // Ground: depth on+write, blend SRCALPHA/INVSRCALPHA (shore alpha), no alpha test,
    // cull BACK (TMGround::Render state setup, TMGround.cpp:2735-2748).
    st.depthTest = true;
    st.depthWrite = true;
    st.depthFunc = GL_LEQUAL;
    st.blend = true;
    st.blendSrc = GL_SRC_ALPHA;
    st.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    st.cull = true;
    st.cullFaceMode = GL_BACK;
    st.alphaTest = false;
    st.sampler[0] = GLSamplers::LinearMip();
    st.sampler[1] = GLSamplers::LinearMip();

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);
    glUniform1i(m_locTex1, 1);

    glBindVertexArray(m_vao);
    for (const Batch& b : m_batches) {
        st.texture[0] = envTextureFn(b.texFront, ctx);
        st.texture[1] = b.mode ? envTextureFn(b.texBack, ctx) : 0;
        st.Apply();
        glUniform1i(m_locModulate2X, b.mode);
        glUniform1i(m_locLavaScroll, b.lavaScroll ? 1 : 0);
        glDrawElements(GL_TRIANGLES, (GLsizei)b.indexCount, GL_UNSIGNED_SHORT,
                       (void*)(uintptr_t)(b.indexStart * 2));
    }
    glBindVertexArray(0);
}

void TerrainRenderer::RefreshColors(const TerrainData& terrain) {
    if (!m_built)
        return;
    // Rewrite only the color bytes (offset 24 of each 44-byte vertex).
    uint8_t* vp = m_vertices.data();
    for (int y = 0; y < 63; ++y) {
        for (int x = 0; x < 63; ++x) {
            const int cx[4] = { x, x, x + 1, x + 1 };
            const int cy[4] = { y, y + 1, y, y + 1 };
            for (int k = 0; k < 4; ++k) {
                const uint32_t rgba = BgraToRgba(terrain.tiles[cx[k] + (cy[k] << 6)].color);
                memcpy(vp + 24, &rgba, 4);
                vp += 44;
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)m_vertices.size(), m_vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

}
