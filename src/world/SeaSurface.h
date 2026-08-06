#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

#include "gl/GLShader.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

// Procedural sea plane (port of TMSea, non-dungeon path only — TMSea.cpp:117-176,
// 180-246, 286-340). The mesh is GENERATED (no file): a (gridX+1)x(gridY+1) grid
// with 2 UV sets, animated on the CPU each frame like the original does
// (sine wave on Y + uv scroll).
//
// Look: tex0 x tex1 x 2 (stage0 MODULATE, stage1 MODULATE2X) with the fog2 effect
// texture, blended srcColor x dst (D3D SRCBLEND=SRCCOLOR, DESTBLEND=DESTALPHA with
// no alpha in the backbuffer = dst factor 1) — a brightening shimmer over the water.
// No fog, no cull, no alpha test.
class SeaSurface {
public:
    // gridX = nMaskIndex/2, gridY = nTextureSetIndex/2 from the .dat record;
    // pos = world center (ground offset + local), height = fHeight.
    bool Init(int gridX, int gridY, float x, float height, float z, std::string* err);

    // CPU wave/uv animation, ported from TMSea::FrameMove (non-dungeon branch).
    void FrameMove(float timeSec);

    void Render(GLRenderDevice& device, GLTextureManager& textures,
                const GLShader& shader, GLint locWorld, GLint locTex0, GLint locTex1);
    void Destroy();

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    int m_gridX = 0, m_gridY = 0;
    int m_indexCount = 0;
    float m_x = 0, m_h = 0, m_z = 0;
    std::vector<uint8_t> m_vertices;   // 32B stride: pos f3, color u32, uv0 f2, uv1 f2
};

}
