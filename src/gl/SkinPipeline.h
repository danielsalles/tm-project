#pragma once

#include <glad/gl.h>
#include <string>
#include <vector>

#include "gl/GLShader.h"
#include "math/TMMath.h"
#include "world/SkinMesh.h"

namespace tmx {

class GLRenderDevice;

// Shared skinning draw pipeline: skin.vert/frag + BonePalette UBO (binding 1).
// Used by TreeRenderer (static .ani) and CharacterMesh (multi-cut characters).
class SkinPipeline {
public:
    bool Init(std::string* err);
    void Destroy();

    // Binds the shader and sets the common draw state (depth on+write, cull
    // back, alpha test with the given reference — TMTree uses 0xAA).
    void Begin(GLRenderDevice& device, float alphaRef);

    // Computes palette[i] = bindInv[i] x combined[frameId[i]] (D3D row order),
    // uploads it to the shared UBO and draws the part.
    void DrawPart(GLRenderDevice& device, GLSkinMesh& mesh, GLuint texture,
                  const std::vector<D3DXMATRIX>& combined,
                  const D3DXMATRIX& fallbackWorld, float alphaMul = 1.0f,
                  const float* emissiveAdd = nullptr);

    // Fade path (TMLeaf): SRCALPHA/INVSRCALPHA blend + no depth writes.
    void SetFadeBlend(bool on) { m_fadeBlend = on; }

private:
    GLShader m_shader;
    GLint m_locNumInfluence = -1, m_locTex0 = -1, m_locAlphaRef = -1, m_locAlphaTest = -1;
    GLuint m_uboBones = 0;
    GLint m_locAlphaMul = -1;
    GLint m_locEmissiveAdd = -1;
    bool m_fadeBlend = false;
};

}
