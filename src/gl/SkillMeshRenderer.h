#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>

#include "gl/GLShader.h"
#include "math/TMMath.h"

namespace tmx {

class GLRenderDevice;
struct GLMesh;
class GLTextureManager;

// Draws a static common mesh as a combat VFX (TMEffectMesh render path). The
// original disables lighting + fog and locks the vertex buffer to overwrite
// every vertex's diffuse with a flat tint; here the tint is a uniform. Blend is
// EF_BRIGHT (SRC_ALPHA/ONE, additive) by default — most skill meshes are glows.
// Uses DIRECT GL state + cache Invalidate (the Metal pattern, doc 18 §11) so it
// never corrupts the skin pipeline's cached sampler/texture binds.
class SkillMeshRenderer {
public:
    bool Init(std::string* err);
    void Destroy();

    // Draw `mesh` (already uploaded) as a skill VFX.
    //   tint     — RGBA floats 0..1 (the flat vertex-color override)
    //   blend    — 0 = alpha (SRC_ALPHA/ONE_MINUS_SRC_ALPHA), 1 = EF_BRIGHT additive
    //   texOverride — effect-texture index >=0 to override the mesh's own textures
    void Draw(GLRenderDevice& dev, GLTextureManager& textures, const GLMesh& mesh,
              const D3DXMATRIX& world, const float tint[4], int blend, int texOverride);

private:
    GLShader m_shader;
    GLint m_locWorld = -1, m_locTex0 = -1, m_locColor = -1;
    GLint m_locAlphaTest = -1, m_locAlphaRef = -1;
};

} // namespace tmx
