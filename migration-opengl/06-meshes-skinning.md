# 06 — Meshes, Loaders e Skinning

## 6.1 Formatos de arquivo (little-endian, layouts MSVC crus)

| Formato | Conteúdo | Parser atual |
|---|---|---|
| `.msa` | mesh estática: FVF, stride, `D3DXATTRIBUTERANGE[n]` (16B cada), nomes de textura **11 bytes sem NUL**, blob IB u16, blob VB | `TMMesh.cpp:481-728` |
| `.msh` | parte skinned: 8×u32 header (bone ID, FVF, stride, nInfluences, nPalette, nVerts, nIdx), paleta de bind `nPalette×mat4`, nomes `nPalette×u32`, vértices, índices u16 | `CMesh.cpp:794-931` |
| `.bon` | pares u32 (parent, id); numBones = filesize/8 | `MeshManager.cpp:112-123` |
| `.ani` | u32 nTicks, u32 nBones, `nBones×nTicks×mat4` (matrizes locais por keyframe) | `MeshManager.cpp:124-185` |
| `MeshList.txt`, `BoneAni4.txt`, `AniSound4.txt` | índices texto | `MeshManager.cpp:53-110` |
| `ValidIndex.bin` | int[100][186] bloco | `MeshManager.cpp:86-94` |

**Armadilha do `.msa`**: quando `FVF != 322`, os vértices em disco têm stride **8 bytes menor**
que em memória — o loader expande (lê com passadas espaçadas, duplica uv0→uv1)
(`TMMesh.cpp:644-688`). Preservar essa lógica no novo loader.

**Endianness/ARM**: tudo LE nativo. No port, ler com funções LE-explícitas
(`read_u32_le`) para segurança futura em ARM (x86_64 e ARM64 são ambos LE — na prática
inócuo, mas o padding de `FileTileInfo` (12B = 5×char+u32, `TMGround.h:6-14`) e os strides
variáveis dos registros de mapa (`TMObjectContainer.cpp:85-100`) exigem cuidado de packing:
usar leitura campo-a-campo, nunca `fread` de struct.

## 6.2 Loaders GL

```cpp
struct GLMesh {                       // substitui TMMesh
    GLuint vao, vbo, ebo;
    struct Subset { uint32_t indexStart, indexCount, baseVertex; int textureIndex; } subsets[32];
    int subsetCount;
    float radius;                     // AABB/raio p/ culling (lido no load, CMesh.cpp:865-894)
    // estado mutável compartilhado do original (m_fScaleH, m_bEffect...) NÃO vai no recurso:
    // vira parâmetro por draw (ver "risco" abaixo)
};

GLMesh* LoadMsa(const char* path) {
    // fread header → attr ranges → texture names → IB blob → VB blob (com expansão stride)
    // glBufferStorage? NÃO (4.4). glBufferData(GL_STATIC_DRAW) — geometria nunca muda.
    // Criar VAO conforme FVF do arquivo (tabela 05 §5.3). Converter cores BGRA→RGBA aqui.
}
```

`GLMeshManager` preserva a semântica do `MeshManager`: lazy load por índice
(`GetCommonMesh`), cache por timestamp com evicção a cada 1200ms
(`ReleaseNotUsingMesh`, `MeshManager.cpp:377-395`), instância única compartilhada.
**Risco**: campos mutáveis na mesh compartilhada (`m_fScaleH/m_fScaleV`, escritos por
consumidores, ex.: `TMHuman.cpp:1784`) → viram uniforms por draw, nunca no VBO.

## 6.3 Skinning: o coração do port

### Hoje (revisão)

- CPU: `TMSkinMesh::FrameMove` amostra `.ani` (1 keyframe = 4 sub-ticks), lerp 3:1/1:1/1:3
  de 16 floats entre keyframes, crossfade de 10 sub-ticks entre animações (slerp de
  quaternion para esqueletos 0/1, lerp de matriz para os demais) (`TMSkinMesh.cpp:428-591`).
- CPU: `CFrame::UpdateFrames` — `m_matCombined = m_matRot * parentCombined` recursivo
  (`CFrame.cpp:279-911`). Paleta = array de **ponteiros para matrizes vivas**
  (`m_pBoneOffset[i] = &frame->m_matCombined`, `CFrame.cpp:267`).
- GPU: por parte, `mat = bind[i] × frameWorld[i] × view`, transposta, 3 regs por osso em
  `c9+3i` (`CMesh.cpp:625-631`); VS vs.1.1 faz o blend.

### Amanhã (GL 4.1)

**Cinemática 100% na CPU, inalterada.** Só muda o upload:

```cpp
struct GLSkinPart {                 // substitui CMesh/TMMesh skinned
    GLuint vao[4], vbo, ebo;        // layout por nInfluences (VAO_SKIN[n])
    D3DXMATRIX bindPalette[40];     // do .msh
    CFrame* boneLive[40];           // ponteiros vivos (igual hoje)
    int nPalette, nInfluences;
};

// Por draw (CMesh::RenderMesh → GLSkinPart::Draw):
void GLSkinPart::Draw(const D3DXMATRIX& world) {
    alignas(16) float palette[64*16];
    for (int i = 0; i < nPalette; i++) {
        D3DXMATRIX m, final;
        D3DXMatrixMultiply(&m, &bindPalette[i], &boneLive[i]->m_matCombined);
        D3DXMatrixMultiply(&final, &m, &world);          // dobra world, NÃO a view
        D3DXMatrixTranspose((D3DXMATRIX*)&palette[i*16], &final);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, uboBones);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, nPalette*64, palette);   // 1 upload por parte
    glDrawElements(GL_TRIANGLES, nIdx, GL_UNSIGNED_SHORT, 0);
}
```

Mudança de responsabilidade vs. original: a **view sai da paleta** (hoje é dobrada por draw)
e vai para o UBO de frame — mesmo resultado, menos multiplicações na CPU.

```glsl
// skinned.vert — substitui skinmesh1-8.bin (semântica inferida das constantes c0-c95)
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aWeights;   // último peso implícito: w3 = 1 - Σ
layout(location=2) in uvec4 aBones;
layout(location=3) in vec3 aNormal;
layout(location=4) in vec2 aUV;

layout(std140) uniform FrameData { mat4 uView; mat4 uProj; /* fog, lights... */ };
layout(std140) uniform Bones { mat4 uBones[64]; };         // bind×world, transpostas

uniform vec3 uLightDir;    // c1
uniform vec4 uMaterial;    // c8 (emissive+diffuse)
uniform vec4 uAmbientEm;   // c7 = ambient*0.25 + emissive

out vec2 vUV; out vec4 vColor; out float vViewZ;

void main() {
    float w3 = 1.0 - (aWeights.x + aWeights.y + aWeights.z);
    vec4 w = vec4(aWeights, w3);
    // índices além de nInfluences têm peso 0 pelo padding do loader
    mat4 skin = uBones[aBones.x]*w.x + uBones[aBones.y]*w.y
              + uBones[aBones.z]*w.z + uBones[aBones.w]*w.w;
    vec4 pos = skin * vec4(aPos, 1.0);
    vec3 nrm = normalize(mat3(skin) * aNormal);
    vColor = uAmbientEm + uMaterial * max(dot(nrm, -uLightDir), 0.0);
    vec4 view = uView * pos;
    vViewZ = view.z;
    vec4 clip = uProj * view;
    clip.z = clip.z * 2.0 - clip.w;      // fix z (04 §4.1)
    gl_Position = clip;
}
```

Notas:
- **1 influência (decl 1)**: sem campo de peso — o VAO dessa variante usa `aWeights=(1,0,0,0)`
  constante (atributo desabilitado com valor default via `glVertexAttrib3f`) e `aBones.x` real.
- Animação 61 (banco 2, `CMesh.cpp:612-623`): mesma malha, mesma técnica — os 2 bancos do
  D3D colapsam num único shader GLSL (o motivo dos 8 bins era variação de decl, não de lógica).
- **Outline/mouse-over** (2º passe com `TEXTUREFACTOR` + paleta re-escalada, `CMesh.cpp:665-754`):
  mesmo VAO, segundo draw com `uOutlineScale` e shader `outline` trivial (cor flat).
- Armas na mão (common mesh pendurada em osso, `TMSkinMesh.cpp:381-402`): sem skinning —
  draw estático com `uWorld = boneMatrix × correção` (correções em `CFrame.cpp:73-165`).

## 6.4 UV scroll por VB-lock → uniform

`TMMesh::Render` reescreve UVs locando o VB por frame (`TMMesh.cpp:78-156`); idem
`CMesh.cpp:149-166` e `TMEffectMesh.cpp:179-189`. **Proibido levar isso para GL.**
Substituto: `uv2 = uv1 + uScroll * uTime` no VS. O campo `m_cUScroll` vira uniform.
Bônus: elimina o aliasing de estado na mesh global cacheada (hoje um efeito suja a cor
da mesh para todos os outros usuários — `TMEffectCharge.cpp:156-187`).

## 6.5 Volumes e orçamento

- Personagem: 6-8 partes × (1 draw + 1 UBO upload de ≤40 ossos) — idêntico ao D3D em draws,
  mas ~10× menos trabalho de CPU por upload (1 `glBufferSubData` vs. N `SetVertexShaderConstantF`).
- Cena cheia (~100 personagens): ~800 draws skinned — ok para GL 4.1 sem instancing de
  skinned na fase 1. (Fase 2: mesma parte × mesma textura → instancing com paletas em
  textura buffer; ver 12.)
- Índices u16 preservados (`glDrawElements(GL_UNSIGNED_SHORT)`); subsets via offset/BaseVertex.
