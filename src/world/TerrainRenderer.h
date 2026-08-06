#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

#include "gl/GLShader.h"

namespace tmx {

class GLRenderDevice;
struct TerrainData;

// GPU-side ground: builds one static VBO/EBO from TerrainData, bucketed by
// (mode, front texture, back texture) so a 64x64 ground renders in ~tens of
// draw calls instead of the original's 4096 DrawPrimitiveUP (TMGround::Render).
//
// Vertex = VAO_LN2 (FVF 594): pos f3 @0, normal f3 @12, color u8x4 @24,
// uv0 f2 @28, uv1 f2 @36 — 44 bytes (05 §5.3). Positions are baked to world
// space at build time (x*2+offX, h*0.1, y*2+offY); the shader uses identity world.
//
// Texture indices are EnvTextureList3.bin slots (front = byTileIndex + 10,
// back = byBackTileIndex + 256; lava/water override the back slot) and are
// resolved to GL ids by the caller at render time.
class TerrainRenderer {
public:
    struct Batch {
        int      mode;          // 0 = normal (stage1 off), 1 = modulate2x (lava/water)
        int      texFront;      // env list index
        int      texBack;       // env list index (mode 1 only)
        bool     lavaScroll;    // mode 1 lava: uv1 scrolls with time
        uint32_t indexStart;    // into the shared EBO
        uint32_t indexCount;
    };

    // Builds the terrain shader (needs a live GL context).
    bool Init(std::string* err);
    bool Build(const TerrainData& terrain, std::string* err);
    void Destroy();

    // envTextureFn: resolves an env list index to a GL texture id (0 = white).
    void Render(GLRenderDevice& device, GLuint (*envTextureFn)(int, void*), void* ctx);

    int BatchCount() const { return (int)m_batches.size(); }
    int VertexCount() const { return (int)(m_vertices.size() / 44); }

    // Re-uploads only the color channel from TerrainData (lamp tint, phase 2 D8).
    void RefreshColors(const TerrainData& terrain);

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLShader m_shader;
    GLint m_locTex0 = -1, m_locTex1 = -1, m_locModulate2X = -1, m_locLavaScroll = -1;
    std::vector<uint8_t> m_vertices;   // kept CPU-side for RefreshColors
    std::vector<Batch>   m_batches;
    bool m_built = false;
};

}
