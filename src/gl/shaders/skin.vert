// skin.vert — linear blend skinning, port of the vs_1_1 Shader\skinmeshN.bin pair
// (CMesh::Render, CMesh.cpp:600-660). Palette entries are bindInv x combined
// (world space); view/proj come from the FrameData UBO like every other shader.
// Weights: N-1 stored per vertex, last = 1 - sum (D3D9 XYZB convention).
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=5) in vec4  aWeights;
layout(location=6) in uvec4 aBones;

uniform int uNumInfluence;   // 1..4

layout(std140) uniform BonePalette {
    mat4 uBones[64];
};

out vec2 vUV;
out vec4 vColor;
out float vFogDepth;

void main() {
    // N-1 weights stored; last = 1 - sum(stored). Lanes >= uNumInfluence are
    // garbage in the stream and must not contribute.
    float w[4];
    float acc = 0.0;
    for (int i = 0; i < 4; ++i)
        w[i] = 0.0;
    for (int i = 0; i < uNumInfluence - 1; ++i) {
        w[i] = aWeights[i];
        acc += aWeights[i];
    }
    w[uNumInfluence - 1] = 1.0 - acc;

    vec4 pos = vec4(0.0);
    vec3 nrm = vec3(0.0);
    for (int i = 0; i < uNumInfluence; ++i) {
        mat4 b = uBones[aBones[i]];
        pos += w[i] * (b * vec4(aPos, 1.0));
        nrm += w[i] * (mat3(b) * aNormal);
    }
    pos.w = 1.0;

    vec3 n = normalize(nrm);
    vec3 lightSum = vec3(0.0);
    for (int i = 0; i < 2; ++i)
        lightSum += uLightColor[i].rgb * max(dot(n, -uLightDir[i].xyz), 0.0) * uLightDir[i].w;
    vColor = vec4(lightSum + uEmissive.rgb, 1.0);
    vUV = aUV;
    vec4 viewPos = uView * pos;
    vFogDepth = viewPos.z;
    gl_Position = FixZ(uProj * viewPos);
}
