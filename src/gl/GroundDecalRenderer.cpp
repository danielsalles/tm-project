#include "gl/GroundDecalRenderer.h"
#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "shaders_embedded.h"

namespace tmx {

bool GroundDecalRenderer::Init(std::string* err) {
    if (!m_shader.Build(kCommonGlsl, kFxDecalVert, kFxDecalFrag, err))
        return false;
    m_locWorld = m_shader.UniformLoc("uWorld");
    m_locTex0  = m_shader.UniformLoc("uTex0");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // pos, color, uv (matches DecalVertex; normals unused).
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DecalVertex),
                          (void*)offsetof(DecalVertex, x));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(DecalVertex),
                          (void*)offsetof(DecalVertex, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(DecalVertex),
                          (void*)offsetof(DecalVertex, u));
    glBindVertexArray(0);
    return true;
}

void GroundDecalRenderer::Destroy() {
    m_shader.Destroy();
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
}

void GroundDecalRenderer::Draw(GLRenderDevice& dev, GLTextureManager& tex, int textureIndex,
                               const std::vector<DecalVertex>& verts,
                               const std::vector<uint16_t>& idx, int blend) {
    if (verts.empty() || idx.empty())
        return;
    (void)dev;
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    if (blend == 1)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);

    D3DXMATRIX ident;
    D3DXMatrixIdentity(&ident);   // positions are already world-space
    glUniformMatrix4fv(m_locWorld, 1, GL_FALSE, &ident._11);

    glActiveTexture(GL_TEXTURE0);
    GLuint t = tex.GetEffectTexture(textureIndex);
    if (t == 0) t = 1;
    glBindTexture(GL_TEXTURE_2D, t);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizei)(verts.size() * sizeof(DecalVertex)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_SHORT, idx.data());
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    dev.State().Invalidate();
}

} // namespace tmx
