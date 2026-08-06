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
struct TerrainData;

// TMShade ground decal (doc 19 §5): an (N+1)² grid conforming to the terrain
// heightmap, textured with a rotated UV, fading over its lifetime. Used by
// skill impacts (lightmaps), sanc glows, arrow landing marks. Rendered with
// EF_BRIGHT additive (most) or EF_DEFAULT alpha blend, z-write off.
//
// Each decal is rebuilt per frame on the CPU (positions conform to terrain,
// color fades); this renderer uploads the grid and draws it via fx_decal.
// Direct GL state + cache Invalidate (Metal pattern, doc 18 §11).
struct DecalVertex {
    float x, y, z;       // world position (terrain-conformed)
    float r, g, b, a;    // faded tint (BGRA→RGBA float)
    float u, v;
};

class GroundDecalRenderer {
public:
    bool Init(std::string* err);
    void Destroy();

    // Build + draw one decal. verts/idx describe the (N+1)² grid already
    // conformed + faded by the caller. blend: 1 = EF_BRIGHT additive, 0 = alpha.
    void Draw(GLRenderDevice& dev, GLTextureManager& tex, int textureIndex,
              const std::vector<DecalVertex>& verts, const std::vector<uint16_t>& idx,
              int blend);

private:
    GLShader m_shader;
    GLint m_locWorld = -1, m_locTex0 = -1;
    GLuint m_vao = 0, m_vbo = 0;
};

} // namespace tmx
