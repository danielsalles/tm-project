# 11 — Catálogo de Shaders GLSL (410 core)

Todos os shaders. `#version 410 core` em todos. Convenções fixas:
- Matrizes sobem **transpostas** (04 §4.3) — shader faz `M * v`.
- Todo VS termina com o fix de z: `clip.z = clip.z * 2.0 - clip.w;` (04 §4.1).
- Cores de vértice convertidas BGRA→RGBA no load (05 §5.3).
- Alpha test: `if (uAlphaTest > 0 && vColor.a * tex.a * 255.0 <= uAlphaRef) discard;`

Header comum (`common.glsl`, via `#include` do nosso preprocessor ou concatenação no load):

```glsl
layout(std140) uniform FrameData {           // binding 0
    mat4 uView; mat4 uProj;
    vec4 uAmbient;
    vec4 uLightDir[2];  vec4 uLightColor[2];
    vec4 uPointPos[6];  vec4 uPointColor[6]; float uPointRange[6]; int uNumPointLights;
    vec4 uFogColor; float uFogStart; float uFogEnd; int uFogEnabled;
    float uTime;
};
float applyFog(inout vec3 color, float viewZ) {
    if (uFogEnabled == 0) return 1.0;
    float f = clamp((viewZ - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    color = mix(color, uFogColor.rgb, f);
    return f;
}
vec3 d3dLight(vec3 N, vec3 matDiffuse) {     // fidelidade FFP (05 §5.5)
    vec3 c = uAmbient.rgb * matDiffuse;
    for (int i = 0; i < 2; i++)
        if (uLightDir[i].w > 0.5)
            c += max(dot(N, -uLightDir[i].xyz), 0.0) * uLightColor[i].rgb * matDiffuse;
    return c;
}
```

## Inventário (10 programas)

| # | Programa | Substitui | Variantes (#define) |
|---|---|---|---|
| 1 | `skinned` | skinmesh1-8.bin | `NUM_INFLUENCES 1..4` (4 variantes) |
| 2 | `mesh_lit` | block 1 + FFP | `COMBINE_*` (05 §5.4: ~10 variantes), `VERTEX_COLOR` |
| 3 | `terrain` | TMGround FVF 594 | `COMBINE_MODULATE2X`, `TILE_ADD` (lava/água) |
| 4 | `sky` | TMSky stages | `CROSSFADE` |
| 5 | `water` | TMSea | `VARIANT_NORMAL/DUNGEON/SWAMP` |
| 6 | `ui_quad` | blocks 0/2/3 + sprite | `SECOND_TEXTURE`, `NO_TEXTURE`, `POINT_FILTER` via sampler |
| 7 | `font` | block 2 texto | — (sampler NEAREST) |
| 8 | `fx_quad` | todos os billboards | `GROUND_PLANE`, `SCREEN_SPACE` |
| 9 | `fx_trail` | SWSwing | — |
| 10 | `fx_mesh_unlit` | efeitos de mesh | `UV_SCROLL` |
| + | `outline` | mouse-over glow (CMesh.cpp:723-754) | — (cor flat + paleta escalada) |
| + | `fx_ground_decal` | TMShade | — |
| + | `post_*` (fase 2) | JBlur redesign | blur/bright/composite |

## Referência: os 3 mais importantes

### `mesh_lit.vert` (substitui o FFP da cena)

```glsl
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;     // OPT_VERTEX_COLOR
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;       // OPT_TWO_UV

uniform mat4 uWorld;
uniform vec4 uMaterial;                // diffuse (alpha = opacidade)
uniform vec4 uEmissive;
uniform vec2 uUVScroll;                // TMMesh cMulti (06 §6.4)

out vec4 vColor; out vec2 vUV0; out vec2 vUV1; out float vViewZ;

void main() {
    vec4 world = uWorld * vec4(aPos, 1.0);
    vec4 view  = uView * world;
    vec3 N = normalize(mat3(uWorld) * aNormal);
    vColor.rgb = d3dLight(N, uMaterial.rgb) + uEmissive.rgb;
    vColor.a = uMaterial.a;
#ifdef VERTEX_COLOR
    vColor *= aColor;
#endif
    vUV0 = aUV0 + uUVScroll * uTime;
    vUV1 = aUV1;
    vViewZ = view.z;
    vec4 clip = uProj * view;
    clip.z = clip.z * 2.0 - clip.w;
    gl_Position = clip;
}
```

### `mesh_lit.frag`

```glsl
#version 410 core
uniform sampler2D uTex0; uniform sampler2D uTex1;
uniform float uAlphaRef; uniform int uAlphaTest;
uniform int uStage1Enabled;
in vec4 vColor; in vec2 vUV0; in vec2 vUV1; in float vViewZ;
out vec4 fragColor;

void main() {
    vec4 t0 = texture(uTex0, vUV0);
#if defined(COMBINE_MODULATE)
    vec4 c = t0 * vColor;
#elif defined(COMBINE_MODULATE2X)
    vec4 c = t0 * vColor * 2.0;
#elif defined(COMBINE_ADDSIGNED)
    vec4 c = vec4(t0.rgb + vColor.rgb - 0.5, t0.a);
#elif defined(COMBINE_ADD)
    vec4 c = vec4(t0.rgb + vColor.rgb, t0.a);
#elif defined(COMBINE_SELECTARG1)
    vec4 c = t0;
#endif
    if (uStage1Enabled != 0) {
        vec4 t1 = texture(uTex1, vUV1);
        c *= t1;                       // pares reais usam MODULATE no stage1 (05 §5.4)
    }
    if (uAlphaTest != 0 && c.a * 255.0 <= uAlphaRef) discard;
    applyFog(c.rgb, vViewZ);
    fragColor = c;
}
```

### `skinned.vert` — ver 06 §6.3 (código completo lá).

### `fx_quad.vert` — ver 08 §8.2 (código completo lá).

### `ui_quad.vert`

```glsl
#version 410 core
layout(location=0) in vec2 aPos;       // pixels lógicos (0..800)
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUV;
uniform mat4 uOrtho;                   // ortho(0, 800*ratioX, 600*ratioY, 0)
out vec4 vColor; out vec2 vUV;
void main() {                          // sem fix de z: ortho em NDC direto, z=0
    gl_Position = uOrtho * vec4(aPos, 0.0, 1.0);
    vColor = aColor; vUV = aUV;
}
```

## Compilação e cache

```cpp
class GLShaderLibrary {
    GLuint Get(ShaderID id, uint32_t defineMask);   // cache: (id, mask) → program
    // load: lê shaders/foo.vert + foo.frag, prepend common.glsl + #defines, compila,
    // glGetProgramBinary? NÃO (cache em disco é opcional fase 2; compile é <100ms total)
    // hot-reload em dev: file watcher → recompile (modo debug)
};
```

Erros de compilação: log com arquivo/linha (`glGetShaderInfoLog`) + fallback magenta —
essencial durante o port, quando cada combiner novo encontrado em runtime precisa de variante.

## Validação contra os .bin originais

Os 8 `skinmesh*.bin` são bytecode D3D9 — não dá para decompilar confiavelmente, mas a
semântica está totalmente determinada pelas constantes documentadas (01 §5) e pelo visual
in-game. Estratégia: screenshot de referência (personagem parado/andando/atacando no
cliente D3D) vs. cliente GL, mesmo ângulo — comparação visual em RenderDoc lado a lado.
