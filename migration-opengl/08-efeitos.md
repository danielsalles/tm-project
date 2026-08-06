# 08 — Sistema de Efeitos

## 8.1 Realidade atual

~45 classes `TMEffect*`/`TMSkill*`. Simulação 100% CPU, paramétrica (posição = f(progresso),
17 programas de movimento em `TMEffectBillBoard.cpp:313-458`; fades paramétricos,
`:258-311`). **1 draw call por partícula** — `DrawPrimitiveUP(TRIANGLEFAN)`, 4 vértices
FVF 322. Neve = 200 draws; `TMSkillMeteorStorm` emite 4 billboards/frame; `TMEffectSWSwing`
reconstrói ribbon de 32 vértices por frame com slerp de 48 matrizes (`:605-746`).

Blend modes (`EEFFECT_ALPHATYPE`, `TMEffect.h:6-12`):

| Tipo | D3D | GL |
|---|---|---|
| `EF_BRIGHT` (aditivo) | SRCALPHA / ONE | `glBlendFunc(GL_SRC_ALPHA, GL_ONE)` |
| `EF_DEFAULT` | SRCALPHA / INVSRCALPHA | `(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` |
| `EF_ALPHA` | SRCALPHA / DESTCOLOR | `(GL_SRC_ALPHA, GL_DST_COLOR)` |
| especial meteoro | SRCCOLOR / INVSRCCOLOR | `(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR)` |

Estados auxiliares universais nos efeitos: `ZWRITEENABLE=0` → `glDepthMask(GL_FALSE)`;
`LIGHTING=0`, `FOGENABLE=0` → shaders `fx_*` sem luz/fog; `CULLMODE=1` → cull off;
`ALPHATESTENABLE=0` durante o draw.

## 8.2 Arquitetura GL: 7 renderers

```
EffectSystem (ordem preservada — efeitos não são reordenados!)
├── QuadBatchRenderer      ← 90% dos efeitos (A/B/E)
├── BeamRenderer           ← BillBoard3, Spark, Spark2 (pode fundir ao QuadBatch)
├── TrailRenderer          ← SWSwing
├── GroundDecalRenderer    ← TMShade, anéis (BillBoard2)
├── MeshEffectRenderer     ← TMMesh-based (Charge, MagicShield, MeteorStorm, Start...)
└── SkinEffectRenderer     ← TMEffectSkinMesh, TMButterFly (reusa pipeline skinned, 06)
```

### QuadBatchRenderer (o coração)

```glsl
// fx_quad.vert — instanced billboards
#version 410 core
layout(location=0) in vec2 aCorner;      // quad unitário (-0.5..0.5), VBO estático
// atributos por instância (divisor 1):
layout(location=1) in vec3 iCenter;
layout(location=2) in vec2 iScale;
layout(location=3) in vec4 iColor;
layout(location=4) in float iFrame;      // índice na TEXTURE_2D_ARRAY (flipbook)
layout(location=5) in float iRot;
layout(location=6) in uint  iFlags;      // bit0: screen-space; bit1: ground-plane

uniform vec3 uCamRight, uCamUp;          // extraídos da view 1×/frame
uniform mat4 uViewProj;

void main() {
    vec2 c = aCorner * iScale;
    if (iRot != 0.0) c = mat2(cos(iRot),-sin(iRot), sin(iRot),cos(iRot)) * c;
    vec3 world = iCenter + uCamRight*c.x + uCamUp*c.y;   // billboard cilíndrico/esférico
    // iFlags&1: quad 2D de tela (BillBoard4) — iCenter já em pixels, usa ortho
    vec4 clip = (iFlags & 1u) != 0u ? uOrtho * vec4(iCenter.xy + c, 0.0, 1.0)
                                    : uViewProj * vec4(world, 1.0);
    clip.z = clip.z * 2.0 - clip.w;
    gl_Position = clip;
    vUV = aCorner + 0.5; vFrame = iFrame; vColor = iColor;
}
```

- **Flipbook**: as texturas de efeito (`m_stEffectTextureList[512]`, quadros consecutivos =
  frames) viram `GL_TEXTURE_2D_ARRAY` agrupado por sequência → `iFrame` indexa a camada.
  Elimina `GetEffectTexture(idx + cycle)` e o bind por partícula (`TMEffectBillBoard.cpp:155`).
- **Simulação continua na CPU** (volumes pequenos, modelos paramétricos já existem) — CPU
  escreve o array de instâncias por frame com orphaning. Movimentos/fades/motion types
  ficam **bit-idênticos** ao original.
- Neve: 200 draws → 1. Chuva: 50 → 1. Meteor storm: N → 1.
- **Ordenação**: o sistema drena a árvore de efeitos na ordem atual (mesma travessia do
  `ObjectManager::RenderObject`) e emite instâncias nessa ordem; flush **por blend-mode +
  textura** respeitando a sequência (um efeito blend-normal entre dois aditivos vira 3
  batches — correto, pois a ordem de composição importa).

## 8.3 Tabela de porte dos efeitos

| Efeito | Renderer destino | Nota |
|---|---|---|
| TMEffectBillBoard / 2 / 4 | QuadBatch / GroundDecal / QuadBatch(2D) | trivial |
| TMEffectParticle, TMSkill{Fire,Poison,Heal,Cure,HolyTouch,Haste,SpeedUp,HeavenDust,SlowSlash,Bash,Flash}, TMEffectLevelUp, TMSkill{ThunderBolt,Judgement,Explosion2} | compositores → saem de graça com os primitivos | trivial |
| TMEffectGold, TMEffectFirework, TMSkillSnow | stubs — nada a portar | zero |
| TMEffectBillBoard3, TMSkill{MagicArrow,DoubleSwing,FreezeBlade,IceSpear,TownPortal}, TMEffect{Start,Mesh,MeshRotate}, TMSnow, TMRain, TMDust | QuadBatch/MeshEffect | VB-lock→uniform, UV scroll→uTime |
| TMEffectCharge, TMSkillMagicShield, TMEffectDust, TMButterFly | MeshEffect / SkinEffect | cuidado: cor via VB-lock na mesh **global cacheada** — vira uniform por instância |
| TMShade | GroundDecal | decal conformado ao heightmap (07 §7.5) |
| TMEffectSpark / Spark2 | Beam | jitter por frame + ancoragem em osso (`FindFrame`, matrizes vivas — expor `TMSkinMesh::GetBoneMatrix(id)`) |
| TMSkillMeteorStorm | QuadBatch + MeshEffect | 7 níveis, blend SRCCOLOR especial, destrutor que spawna ~20 objetos |
| TMEffectSkinMesh / TMSkillSpChange | SkinEffect | pipeline skinned dentro de efeito; 10 motion types |
| **TMEffectSWSwing** | **Trail** | **o mais difícil do repositório**: ribbon 32 vértices, histórico 48 matrizes + slerp, 6 subsistemas anexados, tabela de escala por arma (~850 linhas). Portar por último. |

## 8.4 Interface mínima que o gameplay precisa

Para não reescrever os 45 arquivos, expor na fachada algo morfologicamente igual ao que
eles chamam hoje:

```cpp
// Substitui o padrão SetTransform + SetTexture + DrawPrimitiveUP(TRIANGLEFAN, 2, verts, 24)
void GLRenderDevice::DrawQuad(const RDLVERTEX v[4], int textureIndex, int blendMode);
void GLRenderDevice::DrawStrip(const RDLVERTEX* v, int count, int textureIndex, int blendMode);
void GLRenderDevice::DrawMeshEffect(GLMesh* mesh, const D3DXMATRIX& world, uint32_t color, int blendMode);
```

Cada `TMEffect*::Render()` mantém sua lógica de estados e só troca as primitivas — diff
cirúrgico por arquivo, revisável um a um. A otimização em batch acontece **dentro** do
renderer, invisível ao gameplay.

## 8.5 Estimativa de shaders

7 programas cobrem tudo (`fx_quad`, `fx_beam`, `fx_trail`, `fx_ground_decal`,
`fx_mesh_unlit`, `fx_mesh_skinned`, `fx_screen_quad`) + variantes de conveniência ≈ 9-10
programas. Código de referência em `11-shaders.md`.
