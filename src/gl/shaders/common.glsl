// common.glsl — concatenated into every shader by GLShader (after the #version line).
// Keeps LH + z in [0,1] D3D conventions: matrices are D3D-style, uploaded transposed
// (a row-major D3DXMATRIX memcpy'd into a std140 mat4 slot IS the transpose — 04 §4.3).

// Function, not a macro: a macro argument can't contain vec4(...) commas.
vec4 FixZ(vec4 p) { p.z = p.z * 2.0 - p.w; return p; }

// Note: no "binding=" in the layout — that's GLSL 4.20; we're on 4.10.
// The binding point is set from C++ via glUniformBlockBinding (GLShader::Build).
layout(std140) uniform FrameData {
    mat4  uView;
    mat4  uProj;
    vec4  uAmbient;                     // scene ambient (D3DRS_AMBIENT) — material ambient is 0 in the game paths
    vec4  uLightDir[2];                 // xyz = direction, w = enabled
    vec4  uLightColor[2];               // weather-driven (m_colorLight / m_colorBackLight)
    vec4  uFogColor;                    // rgb used; a unused
    vec4  uEmissive;                    // material emissive floor (0.3 gray in game paths)
    float uFogStart;                    // linear fog in view-space depth (D3D FOGVERTEXMODE=3)
    float uFogEnd;                      // default 1e9 = effectively disabled
    float uTime;                        // seconds
    float uPad0;
};

float FogFactor(float viewDepth) {
    return clamp((uFogEnd - viewDepth) / (uFogEnd - uFogStart), 0.0, 1.0);
}
