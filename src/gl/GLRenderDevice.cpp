#include "gl/GLRenderDevice.h"

#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

#include <cstring>

namespace tmx {

bool GLRenderDevice::Init(SDL_Window* window) {
    m_window = window;

    GLSamplers::Init();

    std::string logStr;
    if (!m_meshLit.Build(kCommonGlsl, kMeshLitVert, kMeshLitFrag, &logStr)) {
        Log("mesh_lit build failed:\n%s", logStr.c_str());
        return false;
    }

    m_locWorld     = m_meshLit.UniformLoc("uWorld");
    m_locAlphaRef  = m_meshLit.UniformLoc("uAlphaRef");
    m_locAlphaTest = m_meshLit.UniformLoc("uAlphaTest");
    m_locTex0      = m_meshLit.UniformLoc("uTex0");

    glGenBuffers(1, &m_uboFrame);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboFrame);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_uboFrame);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // default texture for subsets without one — sampling an unbound texture is
    // undefined (and Metal logs warnings about it)
    glGenTextures(1, &m_whiteTex);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    const uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    D3DXMatrixIdentity(&m_frame.view);
    D3DXMatrixIdentity(&m_frame.proj);
    SetAmbient(1.0f, 1.0f, 1.0f, 1.0f);
    SetFog(0.0f, 0.0f, 0.0f, 0.0f, 1e9f);   // disabled by default
    SetEmissive(0.3f, 0.3f, 0.3f);          // game material floor (non-voodoo path)
    m_frame.lightDir[0][3] = 0.0f;
    m_frame.lightDir[1][3] = 0.0f;

    return true;
}

void GLRenderDevice::Shutdown() {
    m_meshLit.Destroy();
    if (m_uboFrame) {
        glDeleteBuffers(1, &m_uboFrame);
        m_uboFrame = 0;
    }
    if (m_whiteTex) {
        glDeleteTextures(1, &m_whiteTex);
        m_whiteTex = 0;
    }
    GLSamplers::Destroy();
}

void GLRenderDevice::SetViewProj(const D3DXMATRIX& view, const D3DXMATRIX& proj) {
    m_frame.view = view;
    m_frame.proj = proj;
}

void GLRenderDevice::SetDirectionalLight(int i, const D3DXVECTOR3& dir,
                                         float r, float g, float b) {
    if (i < 0 || i > 1)
        return;
    m_frame.lightDir[i][0] = dir.x;
    m_frame.lightDir[i][1] = dir.y;
    m_frame.lightDir[i][2] = dir.z;
    m_frame.lightDir[i][3] = 1.0f;
    m_frame.lightColor[i][0] = r;
    m_frame.lightColor[i][1] = g;
    m_frame.lightColor[i][2] = b;
    m_frame.lightColor[i][3] = 1.0f;
}

void GLRenderDevice::SetAmbient(float r, float g, float b, float a) {
    m_frame.ambient[0] = r;
    m_frame.ambient[1] = g;
    m_frame.ambient[2] = b;
    m_frame.ambient[3] = a;
}

void GLRenderDevice::SetFog(float r, float g, float b, float start, float end) {
    m_frame.fogColor[0] = r;
    m_frame.fogColor[1] = g;
    m_frame.fogColor[2] = b;
    m_frame.fogColor[3] = 1.0f;
    m_frame.fogStart = start;
    m_frame.fogEnd = end;
}

void GLRenderDevice::SetEmissive(float r, float g, float b) {
    m_frame.emissive[0] = r;
    m_frame.emissive[1] = g;
    m_frame.emissive[2] = b;
    m_frame.emissive[3] = 1.0f;
}

void GLRenderDevice::SetMatrixForUI(int screenW, int screenH) {
    // Orthographic projection: (0,0) top-left, (W,H) bottom-right.
    // Replaces the D3D9 shallow perspective (fov=0.1, z=50) — visually equivalent.
    D3DXMATRIX proj;
    D3DXMatrixIdentity(&proj);
    proj._11 = 2.0f / (float)screenW;
    proj._22 = 2.0f / (float)screenH;
    proj._33 = -1.0f;
    proj._41 = -1.0f;
    proj._42 = -1.0f;
    proj._44 = 1.0f;
    m_frame.proj = proj;
}

void GLRenderDevice::BeginFrame() {
    m_drawCalls = 0;
    m_frame.time = GetTicks() / 1000.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_uboFrame);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameData), &m_frame);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderDevice::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderDevice::EndFrame() {
    SDL_GL_SwapWindow(m_window);
}

void GLRenderDevice::SetRenderStateBlock(int n) {
    m_block1 = (n == 1);
    if (n == 1) {
        // 3D scene (05 §5.2): depth LEQUAL+write, blend off, alpha test 0xDD, cull BACK.
        m_state.depthTest = true;
        m_state.depthWrite = true;
        m_state.depthFunc = GL_LEQUAL;
        m_state.blend = false;
        m_state.cull = true;
        m_state.cullFaceMode = GL_BACK;
        m_state.alphaTest = true;
        m_state.alphaRef = 221.0f; // 0xDD — the non-NVIDIA path, visually correct (05 §5.2)
        m_state.sampler[0] = GLSamplers::LinearMip();
    } else {
        // UI quad: depth test on/write off, blend SRCALPHA/INVSRCALPHA, cull off.
        m_state.depthTest = true;
        m_state.depthWrite = false;
        m_state.depthFunc = GL_LEQUAL;
        m_state.blend = true;
        m_state.blendSrc = GL_SRC_ALPHA;
        m_state.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        m_state.cull = false;
        m_state.alphaTest = false;
        m_state.sampler[0] = GLSamplers::LinearNoMip();
    }
}

void GLRenderDevice::SetWorldMatrix(const D3DXMATRIX& m) {
    m_meshLit.Bind();
    // Row-major D3DXMATRIX in memory == column-major transpose GLSL wants (04 §4.3).
    m_meshLit.SetMat4(m_locWorld, &m._11);
}

void GLRenderDevice::DrawMesh(const GLMesh& mesh) {
    m_meshLit.Bind();
    glUniform1f(m_locAlphaRef, m_state.alphaRef);
    glUniform1i(m_locAlphaTest, m_state.alphaTest ? 1 : 0);
    glUniform1i(m_locTex0, 0);

    glBindVertexArray(mesh.vao);
    for (int i = 0; i < mesh.subsetCount; ++i) {
        m_state.texture[0] = mesh.subsets[i].textureIndex > 0
            ? (GLuint)mesh.subsets[i].textureIndex : m_whiteTex;
        // TMObject::Render: alpha-flagged textures get ALPHAREF 0xAA + alpha blend
        // (SRCALPHA/INVSRCALPHA); 'N' textures stay opaque cutout at the block's ref.
        const char flag = mesh.subsets[i].alphaFlag;
        if (m_block1) {
            if (flag != 'N') {
                m_state.blend = true;
                m_state.blendSrc = GL_SRC_ALPHA;
                m_state.blendDst = GL_ONE_MINUS_SRC_ALPHA;
                m_state.alphaTest = true;
                m_state.alphaRef = 170.0f;   // 0xAA
            } else {
                m_state.blend = false;
                m_state.alphaTest = true;
                m_state.alphaRef = 221.0f;   // 0xDD (block-1 default)
            }
        }
        m_state.Apply();
        glUniform1f(m_locAlphaRef, m_state.alphaRef);
        glUniform1i(m_locAlphaTest, m_state.alphaTest ? 1 : 0);
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.subsets[i].indexCount,
                       GL_UNSIGNED_SHORT,
                       (void*)(uintptr_t)(mesh.subsets[i].indexStart * 2));
        ++m_drawCalls;
    }
    glBindVertexArray(0);
}

}
