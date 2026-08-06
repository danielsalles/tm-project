# 04 — Convenções: LH/RH, Depth, Winding, Half-Texel, Math

Este é o documento mais crítico da migração. Erros aqui não dão crash — dão cena
silenciosamente errada (picking deslocado, culling invertido, UI espelhada).

## 4.1 Decisão: manter left-handed + z∈[0,1]

O pipeline inteiro é LH (`D3DXMatrixLookAtLH`, `D3DXMatrixPerspectiveFovLH`):
`RenderDevice.cpp:1832, 1839, 1857, 1865`, `ObjectManager.cpp:688`. Código de gameplay
testa z transformado diretamente:

- Culling por tile: `vPosTransformed.z >= 0.0f && z < 1.0f` (`TMGround.cpp:3230`)
- Lens flare: aborta se `z > 1.0f` (`TMSun.cpp:118`)
- Picking ray: `v.z = 1.0f` (`RenderDevice.cpp:1872-1889`)

**Opção escolhida: matrizes D3D-style bit-idênticas em CPU e GPU.**

`glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE)` seria a solução nativa, mas é GL 4.5 /
`ARB_clip_control` — **indisponível no GL 4.1 do macOS**. Substituto: correção de z
no vertex shader.

### Projeção D3D vs GL

| | D3D (LH, ZO) | GL default (RH, NO) |
|---|---|---|
| z NDC | [0, 1] | [-1, 1] |
| clip z | [0, w] | [-w, w] |

Matriz de projeção a usar (idêntica à `D3DXMatrixPerspectiveFovLH`):

```
xScale = 1/tan(fovY/2) / aspect
yScale = 1/tan(fovY/2)
[ xScale  0       0          0      ]
[ 0       yScale  0          0      ]
[ 0       0       zf/(zf-zn) 1      ]
[ 0       0      -zn*zf/(zf-zn) 0   ]
```

Ao final de **todo vertex shader**, uma linha converte o z para o intervalo do GL:

```glsl
vec4 pos = uProj * uView * uWorld * vec4(aPos, 1.0);
pos.z = pos.z * 2.0 - pos.w;   // D3D z∈[0,w] → GL z∈[-w,w]; full depth range
gl_Position = pos;
```

Custo: 1 FMA por vértice. Benefício: todas as matrizes, o picking, os z-tests da CPU e
a depth ficam **consistentes com o código original** — zero divergência de gameplay.

(Macro prático: `#define FIX_Z(p) do { p.z = p.z * 2.0 - p.w; gl_Position = p; } while(0)`
em um header GLSL comum incluído por todos os shaders.)

### View matrix LH

`D3DXMatrixLookAtLH(eye, at, up)` — zaxis = normalize(at-eye); xaxis = normalize(cross(up,zaxis));
yaxis = cross(zaxis,xaxis). Copiar a fórmula no shim — não usar `glm::lookAt` (que é RH).

## 4.2 Winding / culling

D3D9 LH com `D3DCULL_CCW` (=3, o default do jogo) mantém triângulos CW vistos de frente.
Em GL, a janela tem origem **inferior-esquerda** (D3D: superior-esquerda). A inversão de Y
na transformação de viewport espelha o winding: o triângulo CW do D3D aparece CCW no GL.

Resultado prático (sem flip de Y na projeção):

| D3D | GL |
|---|---|
| `CULLMODE=3` (CCW — default) | `glEnable(GL_CULL_FACE); glFrontFace(GL_CCW); glCullFace(GL_BACK)` |
| `CULLMODE=2` (CW) | `glCullFace(GL_FRONT)` |
| `CULLMODE=1` (NONE) | `glDisable(GL_CULL_FACE)` |

Ou seja: **defaults do GL reproduzem o cull padrão do jogo.** O StateCache traduz 1:1.
(`CMesh.cpp:582/588` alterna CW/CCW por peça — o cache lida.)

> Se algum dia se optar por flip de Y (texturas FBO), o winding inverte de novo e o
> mapeamento acima troca. Não fazer na fase 1.

## 4.3 Row-major (D3DX) vs column-major (GL/GLM)

D3DX: row-major, vetor-linha, `v' = v * M`, concatenação `A*B` = aplica A, depois B.
GLM/GLSL: column-major, vetor-coluna, `v' = M * v`.

Duas consequências:

1. **O shim de math mantém a convenção D3DX na CPU** (row-major, `v*M`) → call-sites intactos.
2. **Upload para GPU sem transposição extra**: um `D3DXMATRIX` row-major na memória é
   byte-idêntico a um `mat4` column-major **transposto**. Como o jogo já transpõe matrizes
   ao subir para VS constants (`RenderDevice.cpp:1847-1849`: "projeção transposta → c2-c5"),
   a mesma prática vale: subir com `glUniformMatrix4fv(loc, 1, GL_FALSE, &m._11)` quando a
   matriz já foi preparada transposta, ou preparar no shim (`D3DXMatrixTranspose`) como hoje.
   Convenção fixa do projeto: **shader faz `uM * vec4(v,1)` com matriz transposta no upload**,
   centralizado num helper `SetMat4Uniform(loc, const D3DXMATRIX&)` para ninguém pensar nisso.

## 4.4 Half-texel offset

D3D9 amostra texels em coordenadas inteiras (centro do texel em k.0); o código compensa
com `±0.5/texDim` nas UVs de UI (`RenderDevice.cpp:2110-2117`) e offsets de +0.5 em
`RenderRectC` (`:1919-1928`).

**GL amostra no centro do texel (k+0.5)** — pixel-perfect sem correção. Ação: **remover**
todos os half-texel offsets ao portar os quads de UI. Remoção mecânica, mas obrigatória,
senão a UI inteira fica borrada/deslocada por meio pixel.

## 4.5 TRIANGLEFAN, TRIANGLESTRIP

- `GL_TRIANGLE_FAN` **existe** em core profile (não foi deprecado; só `GL_QUADS` foi).
  Os quads FAN de 4 vértices funcionam como estão. Mesmo assim, converter para
  `GL_TRIANGLES` indexado nos batchers (permite concatenar quads num único draw).
- `GL_TRIANGLE_STRIP` idem — terreno (`TMGround.cpp:3248`) e `RenderRectProgress2` usam.
- `DrawPrimitiveUP` → VBO dinâmico com orphaning (`glBufferData(NULL)` +
  `glMapBufferRange(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)`) — o máximo que
  4.1 oferece (sem persistent mapping, que é 4.4).

## 4.6 Shim D3DX (TMMath.h)

Com 295× `D3DXVECTOR3`, 263× `D3DXToRadian`, 116× `D3DXMATRIX`, 68× `D3DXMatrixMultiply`:
reimplementar a API, não traduzir call-sites.

```cpp
// math/TMMath.h — header-only, row-major, convenções D3DX
struct D3DXVECTOR3 { float x, y, z; /* ops */ };
struct D3DXVECTOR4 { float x, y, z, w; };
struct D3DXQUATERNION { float x, y, z, w; };
struct D3DXMATRIX { float m[4][4]; /* _11.._44 como union p/ call-sites que usam ._11 */ };

D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b);
D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX*, float, float, float);
D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX*, float yaw, float pitch, float roll);
D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX*, const D3DXVECTOR3* eye, const D3DXVECTOR3* at, const D3DXVECTOR3* up);
D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX*, float fovY, float aspect, float zn, float zf);
D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX*, float* det, const D3DXMATRIX*);
D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* m);
BOOL D3DXIntersectTri(const D3DXVECTOR3* p0, const D3DXVECTOR3* p1, const D3DXVECTOR3* p2,
                      const D3DXVECTOR3* rayPos, const D3DXVECTOR3* rayDir,
                      float* u, float* v, float* dist);  // Möller–Trumbore
D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* out, const D3DXQUATERNION* a, const D3DXQUATERNION* b, float t);
// ... ~40 funções. Total ~600 linhas. Semântica idêntica, testável contra o D3DX real.
```

Pontos de atenção do shim:
- `D3DXMATRIX._11` style member access é usado (ex.: `m_matView._11` em `TMRain.cpp:90`) —
  o union/anon struct resolve.
- `D3DXIntersectTri` do D3D9: **não** faz culling de backface (retorna hit nos dois lados) e
  tem convenção de `u,v` baricêntricos documentada — replicar, pois picking depende.
- `D3DXQuaternionRotationMatrix`/`D3DXMatrixRotationQuaternion`: direção da conversão importa
  (usados no crossfade de animação, `TMSkinMesh.cpp:544-574`).
- Testes de unidade: comparar contra valores congelados do cliente D3D real (dourados).

## 4.7 Gamma / cor

- O jogo trabalha em espaço de cor legado (sem sRGB). **Não** usar `GL_FRAMEBUFFER_SRGB`
  na fase 1 — seria uma mudança visual global. Texturas sobem como `GL_RGBA8` (não SRGB8).
- Gamma ramp global (`RenderDevice.cpp:717-745`) → ignorar no MVP; fase 2: uniform de
  brilho num fullscreen pass final.
