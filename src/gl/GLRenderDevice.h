#pragma once

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include "gl/GLShader.h"
#include "gl/GLStateCache.h"
#include "math/TMMath.h"

namespace tmx {

struct GLMesh;

// Skeletal render facade for phase 1 (doc 15 §3): Clear/Swap, state block presets,
// FrameData UBO updated once per frame, static-mesh draws.
// Deliberately NOT a 1:1 copy of the D3D RenderDevice's ~50 methods — consumers
// add methods as they get ported in later phases.
class GLRenderDevice {
public:
    bool Init(SDL_Window* window);
    void Shutdown();

    void BeginFrame();   // binds the scene target + uploads FrameData UBO
    void Clear(float r, float g, float b, float a);
    void EndFrame();     // resolve + blit (bright gain) + swap

    // Offscreen scene target + final blit (phase 7, doc 21 §5): the scene
    // renders into an FBO and EndFrame blits it to the backbuffer applying
    // uBright (the D3D gamma ramp was a pure linear gain). MSAA via a
    // multisample renderbuffer + resolve. Single code path — when samples<2
    // the scene renders directly into the single-sample FBO.
    bool CreateTargets(int w, int h, int samples);
    void DestroyTargets();
    int  TargetW() const { return m_targetW; }
    int  TargetH() const { return m_targetH; }
    int  MSAASamples() const { return m_samples; }
    void SetBrightGain(float gain) { m_brightGain = gain; } // bright*0.02
    float BrightGain() const { return m_brightGain; }

    // Presets (05 §5.2): 0 = UI quad, 1 = 3D scene. Only these two exist in phase 1.
    void SetRenderStateBlock(int n);

    void SetViewProj(const D3DXMATRIX& view, const D3DXMATRIX& proj);
    void SetWorldMatrix(const D3DXMATRIX& m);   // per-draw uniform, not in the UBO

    // Scene lighting (RenderDevice.cpp:92-116 defaults: two directional lights).
    void SetDirectionalLight(int i, const D3DXVECTOR3& dir, float r, float g, float b);
    void SetAmbient(float r, float g, float b, float a);

    // Linear fog, per frame (D3D FOGVERTEXMODE=3 on view depth). Defaults to disabled
    // (end = 1e9). Weather drives it from TMSky::FogList (phase 2, doc 16 §D4).
    void SetFog(float r, float g, float b, float start, float end);

    // Material emissive floor, per frame (game uses 0.3 gray: TMGround.cpp:2545-2547,
    // TMObject.cpp:106-124). Light colors follow the weather (m_colorLight) — phase 2 D4.
    void SetEmissive(float r, float g, float b);

    void DrawMesh(const GLMesh& mesh);  // textures must already be resolved into mesh.subsets

    // UI orthographic projection (Phase 6).
    void SetMatrixForUI(int screenW, int screenH);

    GLStateCache& State() { return m_state; }

    // 1x1 white texture bound when a subset/env slot has no texture.
    GLuint WhiteTexture() const { return m_whiteTex; }

    // Frame counters, for the smoke test and boot report.
    int DrawCallsThisFrame() const { return m_drawCalls; }

private:
    SDL_Window*   m_window = nullptr;
    GLShader      m_meshLit;
    GLStateCache  m_state;

    GLuint        m_uboFrame = 0;
    GLuint        m_whiteTex = 0;   // 1x1 white, bound when a subset has no texture

    struct alignas(16) FrameData {
        D3DXMATRIX view;       // memcpy'd: row-major == GLSL transpose (04 §4.3)
        D3DXMATRIX proj;
        float ambient[4];
        float lightDir[2][4];  // xyz = dir, w = enabled
        float lightColor[2][4];
        float fogColor[4];
        float emissive[4];
        float fogStart;
        float fogEnd;
        float time;
        float pad;
    } m_frame;

    GLint m_locWorld     = -1;
    GLint m_locAlphaRef  = -1;
    GLint m_locAlphaTest = -1;
    GLint m_locTex0      = -1;

    int   m_drawCalls = 0;
    bool  m_block1 = false;  // current preset == scene 3D

    // Phase 7 offscreen pipeline
    GLShader m_blit;
    GLuint   m_emptyVao  = 0;
    GLuint   m_fbo       = 0, m_fboTex  = 0, m_fboDepth  = 0;  // single-sample
    GLuint   m_msaaFbo   = 0, m_msaaColor = 0, m_msaaDepth = 0; // multisample
    int      m_targetW   = 0, m_targetH = 0, m_samples = 0;
    float    m_brightGain = 1.0f;
};

}
