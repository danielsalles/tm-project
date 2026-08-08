#include "gl/UIBatcher.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

#include <cstring>
#include <cmath>

namespace tmx {

// Unit quad vertices (fx_quad expects -0.5..0.5 corners).
struct FxVertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
};

static const float kCorners[4][2] = {
    { -0.5f,  0.5f }, { 0.5f,  0.5f }, { 0.5f, -0.5f }, { -0.5f, -0.5f },
};

static inline void BgraToFloats(uint32_t bgra, float* out) {
    out[0] = (float)((bgra >> 16) & 0xFF) / 255.0f;   // R
    out[1] = (float)((bgra >> 8) & 0xFF) / 255.0f;    // G
    out[2] = (float)(bgra & 0xFF) / 255.0f;           // B
    out[3] = (float)((bgra >> 24) & 0xFF) / 255.0f;   // A
}

bool UIBatcher::Init(GLuint whiteTex, std::string* err) {
    m_whiteTex = whiteTex;

    if (!m_shader.Build(kCommonGlsl, kFxQuadVert, kFxQuadFrag, err))
        return false;
    m_locWorld       = m_shader.UniformLoc("uWorld");
    m_locScreenSpace = m_shader.UniformLoc("uScreenSpace");
    m_locScreenSize  = m_shader.UniformLoc("uScreenSize");
    m_locTex0        = m_shader.UniformLoc("uTex0");

    // Create VAO/VBO/EBO (unit quad, TRIANGLEFAN order → indexed as 2 triangles)
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    FxVertex verts[4];
    for (int i = 0; i < 4; ++i) {
        verts[i].x = kCorners[i][0];
        verts[i].y = kCorners[i][1];
        verts[i].z = 0.0f;
        verts[i].r = verts[i].g = verts[i].b = verts[i].a = 1.0f;
        verts[i].u = verts[i].v = 0.0f;
    }

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_DYNAMIC_DRAW);

    const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    const int stride = sizeof(FxVertex);
    glEnableVertexAttribArray(0);   // aPos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(2);   // aColor
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)12);
    glEnableVertexAttribArray(3);   // aUV
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)28);

    glBindVertexArray(0);
    return true;
}

void UIBatcher::Begin() {
    m_queue.clear();
}

void UIBatcher::Push(const UIQuad& q) {
    if ((int)m_queue.size() < MAX_QUADS)
        m_queue.push_back(q);
}

void UIBatcher::Flush(GLRenderDevice& device, GLTextureManager& textures,
                      int screenW, int screenH) {
    if (m_queue.empty())
        return;

    // Direct GL state (same pattern as EffectRenderer for Metal safety)
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, screenW, screenH);

    m_shader.Bind();
    glUniform1i(m_locScreenSpace, 1);
    glUniform1i(m_locTex0, 0);
    glUniform2f(m_locScreenSize, (float)screenW, (float)screenH);

    // Identity world matrix base (we'll set translation/scale per quad)
    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glActiveTexture(GL_TEXTURE0);
    // UI textures (fonts, panels) have no mipmaps — a mipmapped sampler left over
    // from the 3D scene would make them incomplete (samples return black).
    glBindSampler(0, GLSamplers::LinearNoMip());

    FxVertex vv[4];
    uint32_t lastColor = 0;
    GLuint lastTex = 0;

    for (const UIQuad& q : m_queue) {
        // Compute world matrix: translate to pixel center, scale to pixel size
        float cx = q.x + q.w * 0.5f;
        float cy = q.y + q.h * 0.5f;
        D3DXMatrixIdentity(&world);
        world._11 = q.w;   // scale X (half-width in quad coords → full width)
        world._22 = q.h;   // scale Y
        world._41 = cx;    // translate X to center
        world._42 = cy;    // translate Y to center

        // Set vertex colors
        float rgba[4];
        BgraToFloats(q.color, rgba);
        const float us[4] = { q.u0, q.u1, q.u1, q.u0 };
        // Screen pixels are y-down: the corner named LT (aPos.y=+0.5) lands on
        // the BOTTOM screen edge, so it must sample v1 (texture bottom row).
        const float vs[4] = { q.v1, q.v1, q.v0, q.v0 };
        for (int n = 0; n < 4; ++n) {
            vv[n].x = kCorners[n][0];
            vv[n].y = kCorners[n][1];
            vv[n].z = 0.0f;
            memcpy(&vv[n].r, rgba, sizeof rgba);
            vv[n].u = us[n];
            vv[n].v = vs[n];
        }

        // Bind texture
        GLuint tex = q.texHandle ? q.texHandle : m_whiteTex;
        if (q.texHandle == 0 && q.texIndex >= 0)
            tex = textures.GetUITexture(q.texIndex, 2000);
        if (tex == 0) tex = m_whiteTex;

        glBindTexture(GL_TEXTURE_2D, tex);
        glBufferData(GL_ARRAY_BUFFER, sizeof vv, vv, GL_DYNAMIC_DRAW);
        glUniformMatrix4fv(m_locWorld, 1, GL_FALSE, &world._11);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    device.State().Invalidate();
    m_queue.clear();
}

void UIBatcher::Destroy() {
    m_shader.Destroy();
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

}
