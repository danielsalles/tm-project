// mesh_lit.vert — static meshes, combiner #1 (MODULATE tex x diffuse).
// Per-vertex gouraud lighting like the D3D9 fixed pipeline (SPECULARENABLE=0 in the game).
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;    // BGRA->RGBA already converted at load time (05 §5.3)
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;                  // transposed at upload, like the UBO matrices

out vec2 vUV;
out vec4 vColor;

void main() {
    vec3 n = normalize(mat3(uWorld) * aNormal);
    vec4 lit = uAmbient;
    for (int i = 0; i < 2; ++i)
        lit += uLightColor[i] * max(dot(n, -uLightDir[i].xyz), 0.0) * uLightDir[i].w;
    vColor = lit * aColor;
    vUV = aUV;
    gl_Position = FixZ(uProj * uView * uWorld * vec4(aPos, 1.0));
}
