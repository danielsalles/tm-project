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
    vec4  uAmbient;                     // scene ambient * material ambient
    vec4  uLightDir[2];                 // xyz = direction, w = enabled
    vec4  uLightColor[2];
    float uTime;                        // seconds
    float uPad0;
    float uPad1;
    float uPad2;
};
