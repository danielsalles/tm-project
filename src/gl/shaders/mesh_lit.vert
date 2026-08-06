// mesh_lit.vert — static meshes, combiner #1 (MODULATE tex x diffuse).
// Per-vertex gouraud lighting like the D3D9 fixed pipeline (SPECULARENABLE=0 in the game).
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;    // BGRA->RGBA already converted at load time (05 §5.3)
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;                  // transposed at upload, like the UBO matrices

out vec2 vUV;
out vec4 vColor;
out float vFogDepth;

void main() {
    vec3 n = normalize(mat3(uWorld) * aNormal);
    vec3 lightSum = vec3(0.0);
    for (int i = 0; i < 2; ++i)
        lightSum += uLightColor[i].rgb * max(dot(n, -uLightDir[i].xyz), 0.0) * uLightDir[i].w;
    // D3D fixed pipe: ambient*matAmb(=0) + lightSum*diffuse(=vertex color) + emissive
    vColor = vec4(aColor.rgb * lightSum + uEmissive.rgb, aColor.a);
    vUV = aUV;
    vec4 viewPos = uView * uWorld * vec4(aPos, 1.0);
    vFogDepth = viewPos.z;           // LH view space: forward is +z
    gl_Position = FixZ(uProj * viewPos);
}
