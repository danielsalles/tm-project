# 02 — Arquitetura-Alvo (OpenGL 4.1 Core)

## Princípio norteador

**Portar fielmente primeiro, modernizar depois.** O jogo tem dezenas de dependências
implícitas (ordem de draws, blend modes, z-tests na CPU). A arquitetura abaixo mantém a
ordem de render e a semântica de estado do D3D, mas troca o mecanismo. As otimizações
estão em `12-modernizacoes.md` e são fase 2.

## Camadas

```
┌─────────────────────────────────────────────────────────┐
│ Gameplay (inalterado): TMHuman, TMObject, TMEffect*,    │
│ TMFieldScene, ObjectManager, TreeNode...                │
│  → cada Render() chama o device como hoje               │
├─────────────────────────────────────────────────────────┤
│ GLRenderDevice  (substitui RenderDevice/D3DDevice)      │
│  • mesma interface pública (SetRenderStateBlock,        │
│    SetTexture, SetTransform, RenderRect*, SetLight...)  │
│  • traduz estados D3D → GL na hora                      │
│  • StateCache + ShaderLibrary + Batchers                │
├──────────────────┬──────────────────────────────────────┤
│ GLStateCache     │ GLShaderLibrary                      │
│ (blend/depth/    │ (10-14 programas GLSL,               │
│  cull/samplers)  │  variantes por #define)              │
├──────────────────┴──────────────────────────────────────┤
│ GLTextureManager │ GLMeshManager │ GLFontRenderer       │
│ (.wys/TGA→GL,    │ (.msa/.msh →  │ (stb_truetype,       │
│  DXT, color key) │  VAO+VBO+EBO) │  cache de strings)   │
├─────────────────────────────────────────────────────────┤
│ TMMath (shim D3DX: D3DXMATRIX, D3DXVec3*, LH)           │
├─────────────────────────────────────────────────────────┤
│ glad2 + SDL3 + OpenGL 4.1 core                          │
└─────────────────────────────────────────────────────────┘
```

## Decisões estruturais

### 2.1 Manter a fachada `RenderDevice`

A tentação de reescrever tudo "do jeito certo" morre nos ~200 call-sites espalhados.
Em vez disso: a classe `GLRenderDevice` implementa **a mesma interface** que os Render()
de gameplay já chamam. Cada método traduz:

| Método atual | Tradução GL |
|---|---|
| `SetRenderStateBlock(n)` | Aplica preset: bind do programa GLSL correspondente + estado fixo |
| `SetRenderState(k,v)` | Atualiza `GLStateCache` (aplica na hora, com dirty-check como hoje) |
| `SetTextureStageState(s,k,v)` | Atualiza `TextureStageState[s][k]` → usado para **selecionar variante de shader** no próximo draw |
| `SetTexture(s,tex)` | `glBindTextureUnit(s, tex)` (4.1: bind ao unit ativo) |
| `SetTransform(WORLD/VIEW/PROJ)` | Grava em `m_matWorld/View/Proj`; sobe para UBO/uniform no draw |
| `SetMaterial` / `SetLight` / `LightEnable` | Escreve em struct `LightingState` → UBO |
| `RenderRect*(...)` | Empurra quad no `UIBatcher` (não desenha na hora) |
| `Lock(1)`/`Unlock(1)` | `Clear` + início de frame / flush de batchers + `SDL_GL_SwapWindow` |

Os `DrawPrimitive*` diretos em `m_pd3dDevice` são o único vazamento — ~60 sites
(`TMGround`, `TMEffect*`, `CMesh`, `TMMesh`). Estratégia: adicionar métodos equivalentes
em `GLRenderDevice` (`DrawUP(fvf, prim, verts, count, stride)`) e trocar os call-sites
mecanicamente. O vazamento de `SetFVF/SetTransform/SetMaterial/SetLight` diretos
(`CMesh.cpp:604-650` etc.) vira chamada à fachada — mecânico também.

### 2.2 Texture stage combiners → seleção de shader

O padrão D3D "seta stage state → draw" vira "seta stage state → no draw, resolve o programa":

```cpp
struct CombinerKey {
    uint8_t colorOp[2];   // stage0, stage1 (D3DTOP)
    uint8_t alphaOp[2];
    uint8_t colorArg1[2]; // TEXTURE/CURRENT/DIFFUSE/TFACTOR
    // ...
};
// GLRenderDevice::Draw* → ShaderLibrary.Get(MakeKey(currentStageState, lighting, fog))
```

Na prática só ~12 combinações são usadas de verdade (mapeadas em `05-renderdevice.md` §4).
Cada uma vira uma variante compilada de um uber-shader via `#define`, com cache
`unordered_map<CombinerKey, Program>`.

### 2.3 Estado fixo (blend/depth/cull) não é shader

Blend modes, depth func, cull, alpha-test-as-discard: tudo estado de pipeline GL, setado
direto pelo StateCache. `ALPHATESTENABLE+ALPHAREF` vira uniform `uAlphaRef` + `discard`
no FS (GL não tem alpha test fixo).

Blend factors D3D→GL (todos têm equivalente — inclusive os "exóticos"):

| D3D | GL |
|---|---|
| ONE (2) | `GL_ONE` |
| SRCCOLOR (3) | `GL_SRC_COLOR` |
| INVSRCCOLOR (4) | `GL_ONE_MINUS_SRC_COLOR` |
| SRCALPHA (5) | `GL_SRC_ALPHA` |
| INVSRCALPHA (6) | `GL_ONE_MINUS_SRC_ALPHA` |
| DESTALPHA (7) | `GL_DST_ALPHA` — **atenção**: exige backbuffer RGBA8 e clear alpha=1.0 |
| DESTCOLOR (9) | `GL_DST_COLOR` |

### 2.4 Draw call flow por frame (paridade com hoje)

```
BeginFrame:  glClear(cor do clima) | depth
  bind UBO_Frame { uView, uProj, uFog, uLights[8], uAmbient, uTime }
RenderObject():
  TMSky       → prog_sky (combiner ADDSIGNED/LERP), VBO do dome
  TMHuman     → prog_skinned, UBO_Bones por parte, 6-8 draws
  TMGround    → prog_terrain, 1 VBO/ground + IBO reconstruído por frame
  Objetos     → prog_mesh (MODULATE/MODULATE2X variants), VBOs cacheados
  Shades      → prog_decal
  Efeitos     → fx_* batchers (flush ordenado — ver 08-efeitos.md)
  UI          → ui_batch (ortho), flush por troca de textura
SwapBuffers
```

### 2.5 O que some da árvore de classes

- Todos os `RestoreDeviceObjects()`/`InvalidateDeviceObjects()` (~60 classes) → deletar
  ou esvaziar. Recursos GL vivem no manager e nunca se perdem.
- `D3DEnumeration`, `D3DSettings`, `ConfirmDevice` → deletar (GL detecta nada; 4.1 é o contrato).
- `JBlur`, shaders `.bin`, `D3DUtil`, paths de shader binário → deletar.
- `m_bVoodoo/m_bSavage/m_iVGAID`, paths de 16bpp, `GetAvailableTextureMem` → deletar.

### 2.6 Multithreading

Não na fase 1. GL 4.1 single-context; o jogo é single-threaded no render (a thread de
rede só alimenta filas). Manter. Futuro: loader thread com contexto compartilhado
(SDL_GL sharelists) para streaming de `.trn`/meshes sem hitch.
