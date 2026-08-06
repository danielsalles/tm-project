#include "world/SeaSurface.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "math/TMMath.h"

#include <cmath>
#include <cstring>

namespace tmx {

bool SeaSurface::Init(int gridX, int gridY, float x, float height, float z,
                      std::string* err) {
    if (gridX <= 0 || gridY <= 0 || gridX > 64 || gridY > 64) {
        if (err) *err = "sea: implausible grid";
        return false;
    }
    m_gridX = gridX;
    m_gridY = gridY;
    m_x = x;
    m_h = height;
    m_z = z;

    const int vx = gridX + 1, vy = gridY + 1;
    m_vertices.resize((size_t)vx * vy * 32);
    for (int nY = 0; nY < vy; ++nY) {
        for (int nX = 0; nX < vx; ++nX) {
            uint8_t* vp = m_vertices.data() + ((size_t)nX + nY * vx) * 32;
            float* pos = (float*)vp;
            pos[0] = (nX - gridX / 2) * 2.0f;   // local; world applied via uWorld
            pos[1] = 0.0f;
            pos[2] = (nY - gridY / 2) * 2.0f;
            const uint32_t white = 0xFFFFFFFF;
            memcpy(vp + 12, &white, 4);
            float* uv0 = (float*)(vp + 16);
            uv0[0] = nX / 2.0f;
            uv0[1] = nY / 2.0f;
            float* uv1 = (float*)(vp + 24);
            uv1[0] = nX / 12.0f;
            uv1[1] = nY / 12.0f;
        }
    }

    std::vector<uint16_t> indices;
    indices.reserve((size_t)gridX * gridY * 6);
    for (int nY = 0; nY < gridY; ++nY) {
        for (int nX = 0; nX < gridX; ++nX) {
            const uint16_t a = (uint16_t)(nX + nY * vx);
            const uint16_t b = (uint16_t)(nX + (nY + 1) * vx);
            const uint16_t c = (uint16_t)(nX + 1 + nY * vx);
            const uint16_t d = (uint16_t)(nX + 1 + (nY + 1) * vx);
            // Original emits (a,b,c)(b,d,c) in D3D y-down space; reversed for GL.
            indices.push_back(a); indices.push_back(c); indices.push_back(b);
            indices.push_back(b); indices.push_back(c); indices.push_back(d);
        }
    }
    m_indexCount = (int)indices.size();

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_vertices.size(), m_vertices.data(), GL_DYNAMIC_DRAW);
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * 2), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void SeaSurface::FrameMove(float timeSec) {
    if (!m_vbo)
        return;
    // TMSea::FrameMove, non-dungeon/non-fog branch (TMSea.cpp:318-340):
    // tv2 scrolls on a 12s cycle; y = sin(nX*pi/2 + t) * 0.05 - 0.1.
    const uint32_t ms = (uint32_t)(timeSec * 1000.0f);
    const float t12 = (float)(ms % 12000) / 12000.0f;
    const float t6  = (float)(ms % 12000) / 6000.0f;   // nIndex0/6000 (12s window)

    const int vx = m_gridX + 1, vy = m_gridY + 1;
    for (int nY = 0; nY < vy; ++nY) {
        for (int nX = 0; nX < vx; ++nX) {
            uint8_t* vp = m_vertices.data() + ((size_t)nX + nY * vx) * 32;
            float* pos = (float*)vp;
            pos[1] = sinf((nX * 3.1415927f / 2.0f) + t6 * 3.1415927f * 2.0f) * 0.05f - 0.1f;
            float* uv1 = (float*)(vp + 24);
            uv1[1] = nY / 3.0f + t12;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)m_vertices.size(), m_vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SeaSurface::Render(GLRenderDevice& device, GLTextureManager& textures,
                        const GLShader& shader, GLint locWorld, GLint locTex0, GLint locTex1) {
    if (!m_vao)
        return;

    GLStateCache& st = device.State();
    st.depthTest = true;
    st.depthWrite = true;
    st.depthFunc = GL_LEQUAL;
    st.blend = true;
    st.blendSrc = GL_SRC_COLOR;   // D3D SRCBLEND=SRCCOLOR, DESTBLEND=DESTALPHA(=1)
    st.blendDst = GL_ONE;
    st.cull = false;              // CULLMODE 1
    st.alphaTest = false;
    st.sampler[0] = GLSamplers::LinearMip();
    st.sampler[1] = GLSamplers::LinearMip();

    D3DXMATRIX world;
    D3DXMatrixTranslation(&world, m_x, m_h, m_z);
    shader.Bind();
    shader.SetMat4(locWorld, &world._11);
    glUniform1i(locTex0, 0);
    glUniform1i(locTex1, 1);

    // Effect list 406 = Effect\fog2.wys on both stages (TMSea.cpp:139-141).
    st.texture[0] = textures.GetEffectTexture(406);
    st.texture[1] = st.texture[0];
    st.Apply();

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

void SeaSurface::Destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    m_vao = m_vbo = m_ebo = 0;
}

}
