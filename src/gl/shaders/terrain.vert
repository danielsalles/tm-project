// terrain.vert — ground tiles (VAO_LN2, FVF 594 equivalent).
// Per-vertex gouraud lighting with dwColor as the diffuse material (D3D COLORVERTEX +
// DIFFUSEMATERIALSOURCE=color), matching the original's baked-light look.
layout(location=0) in vec3 aPos;      // world-space (baked at build: x*2+ox, h*0.1, y*2+oy)
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;    // dwColor, BGRA->RGBA at build; a = shore translucency
layout(location=3) in vec2 aUV0;      // TileCoordList[byTileCoord]
layout(location=4) in vec2 aUV1;      // BackTileCoordList[byBackTileCoord] (or animated lava UV)

out vec2 vUV0;
out vec2 vUV1;
out vec4 vColor;
out float vFogDepth;

uniform int uLavaScroll;  // lava tiles: tv2 += fract(time * 0.1) (TMGround.cpp:3101-3106)

void main() {
    vec3 n = normalize(aNormal);
    vec3 lightSum = vec3(0.0);
    for (int i = 0; i < 2; ++i)
        lightSum += uLightColor[i].rgb * max(dot(n, -uLightDir[i].xyz), 0.0) * uLightDir[i].w;
    // D3D fixed pipe: ambient*matAmb(=0) + lightSum*diffuse(=vertex color) + emissive
    vColor = vec4(aColor.rgb * lightSum + uEmissive.rgb, aColor.a);
    vUV0 = aUV0;
    vUV1 = aUV1;
    if (uLavaScroll != 0)
        vUV1.y += fract(uTime * 0.1);   // servertime%10000/10000 -> 1 cycle / 10s
    vec4 viewPos = uView * vec4(aPos, 1.0);
    vFogDepth = viewPos.z;            // LH view space: forward is +z
    gl_Position = FixZ(uProj * viewPos);
}
