#include "gl/EffectRenderer.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"

#include "shaders_embedded.h"

#include <cstring>
#include <vector>

namespace tmx {

namespace {

struct FxVertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
};

inline void BgraToFloats(uint32_t bgra, float* out) {
    out[0] = (float)((bgra >> 16) & 0xFF) / 255.0f;   // R
    out[1] = (float)((bgra >> 8) & 0xFF) / 255.0f;    // G
    out[2] = (float)(bgra & 0xFF) / 255.0f;           // B
    out[3] = (float)((bgra >> 24) & 0xFF) / 255.0f;   // A
}

// Unit quad corners, TRIANGLEFAN order of the original (lt, rt, rb, lb).
const float kCorners[4][2] = {
    { -0.5f,  0.5f }, { 0.5f,  0.5f }, { 0.5f, -0.5f }, { -0.5f, -0.5f },
};

} // namespace

bool EffectRenderer::Init(std::string* err) {
    if (!m_shader.Build(kCommonGlsl, kFxQuadVert, kFxQuadFrag, err))
        return false;
    m_locWorld       = m_shader.UniformLoc("uWorld");
    m_locScreenSpace = m_shader.UniformLoc("uScreenSpace");
    m_locScreenSize  = m_shader.UniformLoc("uScreenSize");
    m_locTex0        = m_shader.UniformLoc("uTex0");
    EnsureBuffers();
    return true;
}

void EffectRenderer::EnsureBuffers() {
    if (m_vao)
        return;
    // Unit quad, TRIANGLEFAN order of the original (lt, rt, rb, lb).
    FxVertex verts[4];
    for (int i = 0; i < 4; ++i) {
        verts[i].x = kCorners[i][0];
        verts[i].y = kCorners[i][1];
        verts[i].z = 0.0f;
        verts[i].r = verts[i].g = verts[i].b = verts[i].a = 1.0f;
        verts[i].u = verts[i].v = 0.0f;
    }
    const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    const int stride = (int)sizeof(FxVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)12);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)28);
    glBindVertexArray(0);
}

void EffectRenderer::Flush(GLRenderDevice& device, GLTextureManager& textures,
                           int screenW, int screenH) {
    if (m_quads.empty())
        return;
    EnsureBuffers();

    // Direct GL state, bypassing the shared GLStateCache (the Metal driver
    // misbehaves when the cache replays texture/sampler binds for the fx
    // shader and the skin pipeline afterwards). The cache is invalidated at
    // the end so the next scene draw re-applies everything.
    (void)device;
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);
    glUniform2f(m_locScreenSize, (float)screenW, (float)screenH);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glActiveTexture(GL_TEXTURE0);

    int lastTex = -2, lastBlend = -1;
    bool lastScreen = false;
    bool firstGroup = true;

    for (const FxQuad& q : m_quads) {
        if (firstGroup || q.textureIndex != lastTex ||
            q.blendMode != lastBlend || q.screenSpace != lastScreen) {
            firstGroup = false;
            lastTex = q.textureIndex;
            lastBlend = q.blendMode;
            lastScreen = q.screenSpace;

            if (q.blendMode == 2)           // TMSun flare (SRCCOLOR/INVSRCOLOR)
                glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
            else if (q.blendMode == 1)      // EF_BRIGHT
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else                            // EF_DEFAULT
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glBindTexture(GL_TEXTURE_2D,
                          q.textureIndex >= 0 ? textures.GetEffectTexture(q.textureIndex) : 0);
            glUniform1i(m_locScreenSpace, q.screenSpace ? 1 : 0);
        }

        FxVertex vv[4];
        float rgba[4];
        BgraToFloats(q.bgra, rgba);
        const float us[4] = { q.u0, q.u1, q.u1, q.u0 };
        const float vs[4] = { q.v0, q.v0, q.v1, q.v1 };  // lt,rt,rb,lb
        for (int n = 0; n < 4; ++n) {
            vv[n].x = kCorners[n][0];
            vv[n].y = kCorners[n][1];
            vv[n].z = 0.0f;
            memcpy(&vv[n].r, rgba, sizeof rgba);
            vv[n].u = us[n];
            vv[n].v = vs[n];
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof vv, vv, GL_DYNAMIC_DRAW);
        glUniformMatrix4fv(m_locWorld, 1, GL_FALSE, &q.world._11);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    device.State().Invalidate();
    m_quads.clear();
}

void EffectRenderer::Destroy() {
    m_shader.Destroy();
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_vao = m_vbo = m_ebo = 0;
}

}
