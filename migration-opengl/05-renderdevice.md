# 05 — RenderDevice → GL: Estados, Blocos, FVF→VAO, Fog, Luzes

## 5.1 GLStateCache

Espelha o design atual (`RenderDevice.cpp:672-715`) — dirty-check antes de tocar a API:

```cpp
struct GLStateCache {
    // render states
    bool depthTest, depthWrite, blend, cull, scissor;
    GLenum depthFunc, cullFace;
    GLenum blendSrcRGB, blendDstRGB, blendSrcA, blendDstA;
    GLenum blendEquation;          // sempre FUNC_ADD (jogo não usa subtract de blend)
    // texture stage state (virtual — alimenta seleção de shader, não GL)
    struct Stage { uint32_t colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2,
                           texCoordIndex, textureTransformFlags; } stage[2];
    // sampler por unit
    struct Sampler { GLenum min, mag, wrapS, wrapT; } sampler[4];
    GLuint texture[8];
    float alphaRef; bool alphaTest;   // → uniform uAlphaRef + discard no FS
    // fog/luzes/material → UBO (ver 5.5)
};
```

`Apply()` chamado uma vez por draw; diffs aplicados na ordem (depth → blend → cull → samplers).

### Tabela render state D3D → GL

| D3DRS | GL |
|---|---|
| `ZENABLE` | `GL_DEPTH_TEST` enable/disable |
| `ZWRITEENABLE` | `glDepthMask` |
| `ZFUNC` 4 (LESSEQUAL) / 8 (ALWAYS) | `glDepthFunc(GL_LEQUAL/GL_ALWAYS)` |
| `ALPHABLENDENABLE` | `GL_BLEND` |
| `SRCBLEND/DESTBLEND` | `glBlendFunc` (tabela em 02 §2.3) |
| `CULLMODE` 1/2/3 | ver 04 §4.2 (3→BACK default, 2→FRONT, 1→off) |
| `ALPHATESTENABLE/ALPHAFUNC/ALPHAREF` | uniform `uAlphaRef`, `uAlphaTest`; FS: `if (a*255.0 < uAlphaRef) discard;` (ALPHAFUNC é sempre GREATER(7) ou ALWAYS(8) no jogo) |
| `FOGENABLE, FOGCOLOR, FOGVERTEXMODE, FOGSTART/END` | uniforms `uFog{color,start,end,enabled}` — fog calculado no shader |
| `LIGHTING, SPECULARENABLE, AMBIENT, NORMALIZENORMALS, COLORVERTEX, *MATERIALSOURCE` | uniforms + defines de variante (ver 5.5) |
| `SHADEMODE` GOURAUD/FLAT | Gouraud = interpoladores smooth (default); o FLAT do outline de mouse-over (`CMesh.cpp:723-754`) vira variante de shader de outline |
| `DITHERENABLE` | `GL_DITHER` (default on; manter) |
| `FILLMODE` wireframe debug | `glPolygonMode(GL_LINE)` |
| `MULTISAMPLEANTIALIAS` | atributo do contexto na criação (MSAA 4x opcional) |
| `STENCILENABLE, CLIPPING, VERTEXBLEND, INDEXEDVERTEXBLENDENABLE, RANGEFOGENABLE` | sem equivalente necessário — ignorar (stencil nunca é ligado; vertex blend não é usado — skinning é por shader) |
| `TEXTUREFACTOR` (outline glow, `CMesh.cpp:723`) | uniform `uTFactor` |
| `SetSoftwareVertexProcessing` | não existe — deletar |

## 5.2 Os 4 state blocks → presets

Cada `SetRenderStateBlock(n)` vira: aplica o preset no StateCache + define o programa base:

| Block | Preset GL | Programa base |
|---|---|---|
| 0 — UI quad | depth test on/write off, blend SRCALPHA/INVSRCALPHA, cull off, sampler LINEAR | `ui_quad` |
| 1 — Cena 3D | depth LEQUAL+write, blend off (alpha test on, ref 0xDD), cull BACK, LINEAR_MIPMAP_LINEAR | `mesh_lit` (variantes por combiner) |
| 2 — Fonte | depth off, blend SRCALPHA/INVSRCALPHA, alpha ref 8, **NEAREST** | `font` |
| 3 — Painéis | depth off, blend off, alpha ref 8, LINEAR | `ui_quad` |

Detalhes fiéis ao original estão em `01-auditoria.md` §3. Notas:
- `ALPHAREF` normalizar para **0xDD** em todos os GPUs (o path NVIDIA 0xFF000000 desligava
  o teste — comportamento visualmente divergente por hardware; escolher 0xDD, o path não-NVIDIA,
  que é o visual correto/majoritário).
- O segundo estágio (stage1) default é DISABLE — o FS de `mesh_lit` tem `uStage1Enabled`.

## 5.3 FVF / Vertex Decls → VAOs

8 layouts cobrem 100% do jogo (fontes: `Structures.h`, `RenderDevice.cpp:766-1526`):

| ID | Origem | Attribs GL | Stride | Uso |
|---|---|---|---|---|
| VAO_L | FVF 322 | pos f3@0, color u8x4 norm@12, uv0 f2@16 | 24 | efeitos, céu, clima |
| VAO_TL | FVF 324 | pos f3@0(+rhw@12), color@16, uv0@20 | 28 | UI (pré-transformado — ver nota) |
| VAO_TL2 | FVF 580 | posT, color, uv0@20, uv1@28 | 36 | UI multi-tex |
| VAO_N2 | FVF 530 | pos f3, normal f3@12, uv0@24, uv1@32 | 40 | meshes multitex |
| VAO_L2 | FVF 578 | pos, color, uv0, uv1 | 32 | mar |
| VAO_LN2 | FVF 594 | pos, normal@12, color@24, uv0@28, uv1@36 | 44 | terreno |
| VAO_SKIN[n] | decls 1-4 | pos@0, pesos f(n-1)@12, **índices u8x4 int**, normal, uv | 32/36/40/44 | skinned |
| VAO_POS | DeclEquipShadow | pos@0 | 12 | shadow pass (futuro) |

Convenções:
- Cor `D3DCOLOR` (DWORD BGRA na memória!) → `glVertexAttribPointer(loc, 4, GL_UNSIGNED_BYTE,
  GL_TRUE, stride, off)`. **BGRA**: D3D `D3DCOLOR` em little-endian é B,G,R,A na memória.
  Duas opções: (a) converter BGRA→RGBA na carga dos VBs; (b) ler como está e reordenar no
  VS (`aColor.bgra`). Recomendo (a) — feito uma vez no loader, shaders ficam limpos.
- Índices de osso: `glVertexAttribIPointer(loc, 4, GL_UNSIGNED_BYTE, ...)` (integer attrib),
  shader lê `uvec4` — índices diretos na paleta UBO.
- Vértices RHW de UI: **não** usar como posição clip direta. Converter no batcher:
  `x_ndc = x/800*2-1; y_ndc = 1-y/600*2` (ou projeção ortho equivalente). O campo rhw é
  sempre 1.0 no jogo (`RenderDevice.cpp:206-280`) — ignorar/dividir por ele por segurança.

## 5.4 Combinadores de texture stage → variantes de shader

Levantamento real dos pares (stage0, stage1) usados:

| # | COLOROP s0 | COLOROP s1 | Onde | FS equivalente |
|---|---|---|---|---|
| 1 | MODULATE tex×diffuse | disable | default 3D (block 1) | `c = tex * vColor` |
| 2 | MODULATE tex×current | disable | UI/fonte | `c = tex * uColor` |
| 3 | SELECTARG1 tex | disable | sky dia, overlays | `c = tex` |
| 4 | MODULATE2X | MODULATE (tex1) | terreno água 38/39 (`TMGround.cpp:3086`), mar (`TMSea.cpp:105`) | `c = tex0*2 * tex1 * vColor` |
| 5 | ADDSIGNED (tex+diff-0.5) | LERP(diffuse.a) | sky crossfade (`TMSky.cpp:179-186`) | `c = mix(tex0+diff-0.5, tex1..., blendA)` |
| 6 | ADD | disable | lava/água animada (`TMGround.cpp:2986`) | `c = tex + vColor` |
| 7 | DOTPRODUCT3 | disable | itens legendary (`TMMesh.cpp:436`) | `c.rgb = dot(n,l)` baked |
| 8 | MODULATE | MODULATE tex1 | dust multitex (`TMEffectDust.cpp:283`) | `c = tex0 * tex1 * vColor` |
| 9 | MODULATEALPHA_ADDCOLOR | disable | env-map objetos (`TMObject.cpp:287`) | `c = tex + vColor.rgb*tex.a` |
| 10 | TEXTURETRANSFORMFLAGS + TCI_CAMERASPACEREFLECTIONVECTOR | idem | env-map (`TMObject.cpp:284-295`) | VS: `uv1 = reflect(viewDir,n).xy` |

Implementação: um único `mesh.frag` com `#ifdef COMBINE_*` por linha, compilado em ~10
variantes cacheadas por `CombinerKey` (02 §2.2). Alpha/blend/fog são uniformes, não variantes.

## 5.5 Iluminação e fog (UBO por frame)

```glsl
// binding 0 — atualizado 1×/frame
layout(std140) uniform FrameData {
    mat4  uView;        // transposta no upload (04 §4.3)
    mat4  uProj;
    vec4  uAmbient;                        // D3DRS_AMBIENT × material.ambient
    vec4  uLightDir[2];                    // xyz=dir, w=enabled
    vec4  uLightColor[2];
    vec4  uPointPos[6];   vec4 uPointColor[6];  float uPointRange[6];
    int   uNumPointLights;
    vec4  uFogColor; float uFogStart, uFogEnd; int uFogEnabled;
    float uTime;        // UV scroll, ondas do mar, efeitos
};
```

Fidelidade ao modelo D3D9 fixed-function (replicar no FS/VS):

```
cor = emissive + ambient*matAmbient
    + Σ_dir  max(dot(n, -l), 0) * (lightDiffuse * matDiffuse)      // luzes 0-1
    + Σ_point (dot>0 por range cutoff) ...                          // luzes 2-7, attenuation≈0 → só cutoff por Range
specular: DESLIGADO (SPECULARENABLE=0) — não implementar
NORMALIZENORMALS → normalize() no VS
COLORVERTEX: vertex diffuse multiplica material quando D3DRS_COLORVERTEX — o jogo seta
  DIFFUSEMATERIALSOURCE=MATERIAL no block 1, mas terreno usa dwColor como difusa via
  FVF com DIFFUSE + material branco (TMGround.cpp:2538). Modelo: vColor sempre multiplica.
```

Fog (vertex fog do D3D = por distância z de view, linear, sem range):

```glsl
float fogF = clamp((vViewZ - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
fragColor.rgb = mix(fragColor.rgb, uFogColor.rgb, fogF * uFogEnabled);
```

Fazer **por pixel** (qualidade igual ou melhor que o vertex fog original; o vértice do
terreno é denso o suficiente para não haver divergência visível).

## 5.6 Picking — inalterado

`GetPickRayVector`, `D3DXIntersectTri` e todos os testes ficam na CPU com as mesmas
matrizes do shim. Nenhuma mudança (04 §4.1 garante consistência).
