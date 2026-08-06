#include "gl/SkillMeshRenderer.h"
#include "gl/GLRenderDevice.h"
#include "gl/GLMesh.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "shaders_embedded.h"

namespace tmx {

bool SkillMeshRenderer::Init(std::string* err) {
    if (!m_shader.Build(kCommonGlsl, kFxSkillmeshVert, kFxSkillmeshFrag, err))
        return false;
    m_locWorld      = m_shader.UniformLoc("uWorld");
    m_locTex0       = m_shader.UniformLoc("uTex0");
    m_locColor      = m_shader.UniformLoc("uColor");
    m_locAlphaTest  = m_shader.UniformLoc("uAlphaTest");
    m_locAlphaRef   = m_shader.UniformLoc("uAlphaRef");
    return true;
}

void SkillMeshRenderer::Destroy() {
    m_shader.Destroy();
}

void SkillMeshRenderer::Draw(GLRenderDevice& dev, GLTextureManager& textures,
                             const GLMesh& mesh, const D3DXMATRIX& world,
                             const float tint[4], int blend, int texOverride) {
    (void)dev;
    // Direct GL state (Metal pattern): no cache replay, invalidate at the end.
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    if (blend == 1)   // EF_BRIGHT additive
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else              // EF_DEFAULT alpha
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);
    glUniform4fv(m_locColor, 1, tint);
    glUniform1i(m_locAlphaTest, 0);
    glUniform1f(m_locAlphaRef, 0.0f);
    glUniformMatrix4fv(m_locWorld, 1, GL_FALSE, &world._11);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(mesh.vao);
    for (int i = 0; i < mesh.subsetCount; ++i) {
        GLuint tex = (GLuint)mesh.subsets[i].textureIndex;
        if (texOverride >= 0)
            tex = textures.GetEffectTexture(texOverride);
        if (tex == 0)
            tex = 1;   // avoid binding 0 — fall back to a non-zero handle
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.subsets[i].indexCount,
                       GL_UNSIGNED_SHORT,
                       (void*)(uintptr_t)(mesh.subsets[i].indexStart * 2));
    }
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    dev.State().Invalidate();
}

} // namespace tmx
