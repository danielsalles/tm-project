#include "gl/SkinPipeline.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"

#include "shaders_embedded.h"

namespace tmx {

bool SkinPipeline::Init(std::string* err) {
    if (!m_shader.Build(kCommonGlsl, kSkinVert, kSkinFrag, err))
        return false;
    m_locNumInfluence = m_shader.UniformLoc("uNumInfluence");
    m_locTex0         = m_shader.UniformLoc("uTex0");
    m_locAlphaRef     = m_shader.UniformLoc("uAlphaRef");
    m_locAlphaTest    = m_shader.UniformLoc("uAlphaTest");
    m_locAlphaMul     = m_shader.UniformLoc("uAlphaMul");
    m_locEmissiveAdd  = m_shader.UniformLoc("uEmissiveAdd");

    GLuint blockIdx = glGetUniformBlockIndex(m_shader.Program(), "BonePalette");
    if (blockIdx == GL_INVALID_INDEX) {
        if (err) *err = "skin shader: no BonePalette block";
        return false;
    }
    glUniformBlockBinding(m_shader.Program(), blockIdx, 1);

    glGenBuffers(1, &m_uboBones);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboBones);
    glBufferData(GL_UNIFORM_BUFFER, kMaxBones * 64, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_uboBones);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void SkinPipeline::Begin(GLRenderDevice& device, float alphaRef) {
    GLStateCache& st = device.State();
    st.depthTest = true;
    st.depthWrite = true;
    st.depthFunc = GL_LEQUAL;
    st.blend = false;
    st.cull = true;
    st.cullFaceMode = GL_BACK;
    st.alphaTest = true;
    st.alphaRef = alphaRef;
    st.sampler[0] = GLSamplers::LinearMip();

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);
    glUniform1f(m_locAlphaRef, alphaRef);
    glUniform1i(m_locAlphaTest, 1);
}

void SkinPipeline::DrawPart(GLRenderDevice& device, GLSkinMesh& mesh, GLuint texture,
                            const std::vector<D3DXMATRIX>& combined,
                            const D3DXMATRIX& fallbackWorld, float alphaMul,
                            const float* emissiveAdd) {
    D3DXMATRIX palette[kMaxBones];
    for (uint32_t i = 0; i < mesh.numPalette; ++i) {
        const uint32_t fid = mesh.boneFrameId[i];
        const D3DXMATRIX& comb = fid < combined.size() ? combined[fid] : fallbackWorld;
        D3DXMatrixMultiply(&palette[i], &mesh.boneBindInv[i], &comb);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboBones);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, mesh.numPalette * 64, palette);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUniform1i(m_locNumInfluence, (int)mesh.numInfluence);
    glUniform1f(m_locAlphaMul, alphaMul);
    if (emissiveAdd)
        glUniform3f(m_locEmissiveAdd, emissiveAdd[0], emissiveAdd[1], emissiveAdd[2]);
    else
        glUniform3f(m_locEmissiveAdd, 0.0f, 0.0f, 0.0f);
    GLStateCache& st = device.State();
    st.texture[0] = texture;
    if (m_fadeBlend) {
        st.blend = true;
        st.blendSrc = GL_SRC_ALPHA;
        st.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        st.depthWrite = false;
    }
    st.Apply();
    if (m_fadeBlend) {   // restore for the next normal draw
        st.blend = false;
        st.depthWrite = true;
    }

    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

void SkinPipeline::Destroy() {
    m_shader.Destroy();
    if (m_uboBones) {
        glDeleteBuffers(1, &m_uboBones);
        m_uboBones = 0;
    }
}

}
