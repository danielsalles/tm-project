#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

#include "gl/GLShader.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

struct UIQuad {
    float x, y, w, h;       // position/size in screen pixels
    float u0, v0, u1, v1;   // UVs in the texture
    uint32_t color;          // ARGB (0xAARRGGBB)
    int texIndex;            // UI texture index (-1 = no texture)
    GLuint texHandle = 0;    // direct GL texture handle (overrides texIndex if non-zero)
    int layer;               // 0-29 (preserved, not reordered)
};

class UIBatcher {
public:
    static constexpr int MAX_QUADS = 16384;

    bool Init(GLuint whiteTex, std::string* err);
    void Destroy();

    void Begin();
    void Push(const UIQuad& q);
    void Flush(GLRenderDevice& device, GLTextureManager& textures,
               int screenW, int screenH);

    bool IsEmpty() const { return m_queue.empty(); }
    size_t Pending() const { return m_queue.size(); }

private:
    GLShader m_shader;
    GLint m_locWorld      = -1;
    GLint m_locScreenSpace = -1;
    GLint m_locScreenSize  = -1;
    GLint m_locTex0        = -1;

    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_whiteTex = 0;
    std::vector<UIQuad> m_queue;
};

}
