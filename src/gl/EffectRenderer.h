#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

#include "gl/GLShader.h"
#include "math/TMMath.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

// Batched effect quads (TMEffectBillBoard render path). Simulation stays on the
// CPU (world matrix per quad); this renderer only batches draws by
// (texture, blend) preserving emission order BETWEEN groups — composition order
// matters for alpha (08 §8.2).
struct FxQuad {
    D3DXMATRIX world;       // full transform of the unit quad
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    uint32_t bgra = 0xFFFFFFFF;   // D3D diffuse dword (converted at emit)
    int textureIndex = -1;        // EffectTextureList index
    int blendMode = 0;            // 0 EF_DEFAULT, 1 EF_BRIGHT, 2 SRCCOLOR/INVSRCOLOR
    bool screenSpace = false;     // 2D pixels (sun flare); world carries px offset
};

class EffectRenderer {
public:
    bool Init(std::string* err);
    void Destroy();

    void Emit(const FxQuad& q) { m_quads.push_back(q); }
    size_t Pending() const { return m_quads.size(); }
    void Clear() { m_quads.clear(); }

    // Draws everything accumulated since the last Flush. screenW/H only used by
    // screen-space quads.
    void Flush(GLRenderDevice& device, GLTextureManager& textures,
               int screenW, int screenH);

    // Immediate world-space triangle strip (weapon trail / beam). Vertices carry
    // their own world position (uWorld = identity); drawn right away with the
    // same shader/state as quads. Used by SwingTrail before Flush.
    struct WorldVertex {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };
    void DrawWorldStrip(const WorldVertex* verts, int vertCount,
                        GLTextureManager& textures, int textureIndex, int blendMode);

private:
    void EnsureBuffers();

    GLShader m_shader;
    GLint m_locWorld = -1, m_locScreenSpace = -1, m_locScreenSize = -1, m_locTex0 = -1;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    std::vector<FxQuad> m_quads;
};

}
