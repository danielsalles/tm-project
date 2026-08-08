#pragma once

#include <glad/gl.h>

namespace tmx {

// Dirty-checking GL state cache, mirroring RenderDevice.cpp's design (05 §5.1).
// Phase 1 subset: depth/blend/cull/alpha-test + 2 texture units.
// Fog, point lights, multitex stages arrive with their first real consumer (phase 2).
struct GLStateCache {
    bool   depthTest  = true;
    bool   depthWrite = true;
    bool   blend      = false;
    bool   cull       = true;
    GLenum depthFunc  = GL_LEQUAL;
    GLenum cullFaceMode = GL_BACK;   // GL defaults already match D3DCULL_CCW (04 §4.2)
    GLenum blendSrc   = GL_SRC_ALPHA;
    GLenum blendDst   = GL_ONE_MINUS_SRC_ALPHA;

    bool   alphaTest  = false;       // -> uAlphaTest uniform
    float  alphaRef   = 221.0f;      // 0xDD normalized to the D3D 0..255 scale (05 §5.2)

    GLuint texture[2] = { 0, 0 };
    GLuint sampler[2] = { 0, 0 };

    // Scissor test (Phase 6 — UI panel clipping).
    bool   scissorEnabled = false;
    int    scissorX = 0, scissorY = 0, scissorW = 0, scissorH = 0;

    void SetScissor(int x, int y, int w, int h);
    void DisableScissor();

    // Applies only what changed. Call once per draw, before issuing it.
    void Apply();

    // Force every cached value to "unknown" (e.g. after context loss).
    void Invalidate();

private:
    bool   m_valid = false;
    bool   c_depthTest, c_depthWrite, c_blend, c_cull;
    GLenum c_depthFunc, c_cullFaceMode, c_blendSrc, c_blendDst;
    GLuint c_texture[2], c_sampler[2];
};

}
