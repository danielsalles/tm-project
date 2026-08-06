# 16 — Fase 2: Mundo 3D (plano completo) — **CONCLUÍDA**

**Objetivo**: renderizar o mundo estático/ambiente do jogo completo em GL — **terreno**
(`.trn`, blend de 2 tiles por alpha de vértice, cor de vértice como lightmap baked),
**céu** (domo `sky001.msa` + estados de clima + fog), **mar** (grid procedural animado),
e **objetos animados skinned** (árvores, casas, navios, floats — `TMSkinMesh`:
`.bon` + `.ani` + `.msh` + `.wyt`, skinning LBS em GLSL), mais **picking/altura de
terreno** (matemática pura). Cena alvo: select-server (Field2723) completa + `FieldView`
com câmera livre para validar qualquer mapa (Field0101 tem árvores/casas; Field1616 tem
mar/floats).

**Estado final (DoD)**:
- [x] Loader `.trn` (FileTileInfo 12B × 4096, header nome+pos) + normais por vértice +
      `m_pMaskData` 128×128; teste com blob sintético + parse do `Field2723.trn` real
- [x] Shader `terrain`: tiles normais single-texture; `MODULATE2X` só lava/água
      (**correção**: sem LERP por alpha — ver §2.1); batches por par de texturas
- [x] `EnvTextureList3.bin` — **layout real: 2048 × 528B (A/B)** — + UVs rotacionados
      + tiles especiais (código pronto; assets deste build não têm lava/água — ver §7)
- [x] Céu: domo `sky001.msa`, texturas `sky01-04.wys` por clima (por NOME — índices do
      código vazado não batem com este build), `FogList`, clear color, light colors
- [x] Mar: grid procedural, onda sin + scroll, shimmer fog2 (SRC_COLOR/ONE)
- [x] Loader `.wyt` (stb_image) + `ModelTextureList` (também 528B)
- [x] Loaders `.bon`/`.ani`/`.msh` + testes sintéticos + parse real de `tr010101.msh`
- [x] Skinning GLSL: LBS 1-4 pesos, UBO BonePalette (binding 1), 80 FPS × 4 sub-steps;
      palmeiras de Field0101 balançando
- [x] Roteamento completo: Tree (skinned), House/Ship/Float (static .msa), Sea (2)
- [x] `GroundGetColor`/`GroundSetColor` (quirks preservados: /256, branch morto) +
      tint de lâmpadas 501-503
- [x] `GetHeight`/`GetPickPos` + testes; clique direito imprime posição no viewer
- [x] `glFrontFace(GL_CW)` global (D3D CW-on-screen) — corrige paredes unilaterais
- [x] CI verde nos 3 OS; `Projects/` intocado

**Duração estimada**: 2 semanas (10 dias úteis).

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original |
|---|---|
| `.trn` loader + TMGround render (tiles, blend, cor de vértice) | `TMGround.cpp:2467-2540, 2685-3265` |
| `EnvTextureList3.bin` (lê só os primeiros 512 registros do arquivo!) | `TextureManager.cpp:980-997` |
| Céu (TMSky) + clima 0-3 + noite (+10) + fog + clear color | `TMSky.cpp` todo |
| Mar (TMSea) — grid procedural + ondas | `TMSea.cpp:180-340` |
| TMSkinMesh: `.bon`/`.ani`/`.msh`/`.wyt`, skinning 1-4 pesos | `TMSkinMesh.cpp`, `CMesh.cpp:814-915`, `MeshManager.cpp:95-185` |
| TMTree/TMHouse/TMShip/TMFloat (roteamento por dwObjType) | `TMObjectContainer.cpp:320-400` |
| Picking de terreno (GetHeight/GetPickPos) | `TMGround.cpp:3299-3380, 3515+` |
| GroundGetColor/GroundSetColor (tint dinâmico de tiles) | `TMScene.cpp:1918, 1972` |
| Câmera livre para FieldView | novo (nossa) |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| TMLeaf (311-322) = **folhas caindo** (partículas, NÃO árvores) | São `TMEffect`/`TMSkinMesh` tipo 61 ambiente; 715 na Field2723 | Fase 4 |
| TMButterFly (343,4,6,7), TMFish (344,12) | Partículas ambiente | Fase 4 |
| Sol/lua/estrelas (TMEffectBillBoard) | Precisam do sistema de billboards | Fase 4 (stretch: sol como quad único) |
| Glow billboards das lâmpadas (501-505) | Billboards; **mas o tint no terreno (GroundSetColor) ENTRA** | Fase 4 |
| TMShade (decal radial de luz) | Efeito | Fase 4 |
| Blob shadows | Fazem parte do render de TMHuman/TMObject móvel | Fase 3 |
| Combiners env-map (REFLECTIONMAP) | Só alguns meshes | Fase 3/4 |
| Câmera de gameplay (segue personagem), UI, rede | — | Fases 3/5/6 |

> **Correções aos docs anteriores** (descobertas desta pesquisa, já refletidas aqui):
> 1. Doc 13 listava "árvores e casas" como se fossem meshes estáticas — na real usam
>    `TMSkinMesh` (mesma pipeline de personagens). A Fase 2 portando TMSkinMesh deixa a
>    Fase 3 (personagens) quase de graça no lado de render.
> 2. Doc 13 listava "blob shadows" na Fase 2 — pertencem à Fase 3 (são desenhados pelo
>    dono do objeto, não pelo mundo).
> 3. Os 715 "Leaf" pulados na Fase 1 são partículas de folha caindo (TMLeaf), não
>    vegetação — a Fase 1 já renderiza as árvores **estáticas** (pinheiros win*.msa).

---

## 2. Formatos validados contra assets reais (referência de implementação)

### 2.1 `.trn` — terreno (`TMGround::LoadTileMap`)

```
u8    nameLen
char  name[nameLen]
u8    posX            // índice do ground no mundo (SetPos/SetAttatchEnable)
u8    posY
FileTileInfo tiles[4096]   // 12B cada, 64×64; total 49152B
```

`FileTileInfo` (Structures.h): `{ i8 cHeight; u8 byTileIndex; u8 byTileCoord; u8 byBackTileIndex; u8 byBackTileCoord; u32 dwColor }`

- Altura do vértice = `cHeight * 0.1f`; posição mundo: `(x*2, h, y*2) + offset`
  (offset = `(posX<<6)*2`, `(posY<<6)*2`).
- Normais: `GetNormalInGround(x,y)` a partir das alturas vizinhas; bordas copiam o vizinho
  (loops de clamp em `LoadTileMap` — portar igual).
- `dwColor` = lightmap baked por vértice. **Correção (validado no código + render)**:
  o blend de tiles NÃO é LERP por alpha — o stage1 é `DISABLE` para tiles normais e
  `MODULATE2X` só para lava (nIndex 38/39) e água (62-65) (`TMGround.cpp:3086-3121`).
  As transições suaves entre tiles vêm "de fábrica" na arte dos próprios tiles
  (`TileNNNNN.wys` inclui variantes pré-blendadas; `byTileCoord` rotaciona).
  O alpha de `dwColor` dá translucidez de margem (SRCALPHA/INVSRCALPHA).
- Iluminação do terreno (fixed pipe): `final = tex × (dwColor × Σluzes + emissive 0.3)`
  — ambient da cena × material ambient(=0) não contribui; emissive é ADITIVO
  (`TMGround.cpp:2540-2547`, block 1 `D3DRS_AMBIENT=0x33FFFFFF` com matAmb 0).
- Winding: os triângulos do pick quad `(0,1,2)/(3,2,1)` do original leem CW (back face)
  no window space y-up do GL — emitir a ordem invertida `(0,2,1)/(3,1,2)`.
- `byTileCoord`/`byBackTileCoord` indexam `TileCoordList[8][4][2]`/`BackTileCoordList[32][4][2]`
  (rotações/variantes de UV — tabelas estáticas em TMGround.cpp, copiar literais).
- Tiles especiais: lava 38/39 (stage 1 = env tex 344, scroll por tempo), água 62-65
  (env tex `nIndex+286`, UV espelhado), senão stage1 disable.
- Render original: 1 `DrawPrimitiveUP` por tile (FVF 594 = XYZ|NORMAL|DIFFUSE|TEX2,
  44B/vértice, tristrip de 4 vértices) com frustum check por tile. **Nós**: agrupar por
  par (tex0, tex1) em VBO dinâmico — mapas usam poucos pares (~dezenas).
- `m_pMaskData[128][128]` (2×2 por tile) e `m_pVAttrData` — derivados no load; mask é
  usada por picking/colisão/HeightMapData da cena.
- Checksum anti-hack (`m_nCheckSum[bPosY][bPosX]`, cdata.bin): **não portar** (é
  proteção de integridade online; logar e ignorar).

### 2.2 `EnvTextureList3.bin` / `stTextureListInfo`

```
struct stTextureListInfo { char szFileName[255]; char cAlpha; u32 dwLastUsedTime; u32 dwShowTime; }  // 264B
```
- **Layout real deste client build (validado byte-a-byte)**: 2048 entradas de **528 bytes**
  = dois registros de 264B por entrada (A = ativo, B = resíduo de nome curto). Entrada `i`
  em `i*528`; bate com a 1ª coluna do `EnvTextureList3.txt`.
- ⚠️ O `TextureManager.cpp` do vazamento lê `512 × 264B` contíguos — incompatível com este
  asset (leria metades B intercaladas). O cliente shipped usa o stride 528 (ou lê o .txt).
- `cAlpha` real no bin = 'E' (o txt diz 'N' — desatualizado). Tiles não usam alpha de
  textura no shader; o blend de margem vem do alpha de `dwColor`.
- Texturas `.wys` (DXT) e **`.wyt`** (ex.: idx 256 = `Tile26262.wyt`) — dispatch por
  extensão no loader.
- Este build não tem `Tile00002.wys`/`Tile00003.wys` (referenciados pela Field2723) →
  textura 0 = preto, mesmo comportamento do NULL do D3D9.

### 2.3 `.wyt` — textura não-comprimida (skinned meshes, minimap)

```
char magic[4] = "WT10"
// resto = TGA completo MENOS o footer de 18B ("TRUEVISION-XFILE.\0")
```
- Validado em `mesh/cbt054.wyt`: após "WT10" vem header TGA type 2 (BGR 24bpp não
  comprimido), 512×512. O original re-anexa o footer e delega ao D3DX.
- **Nós**: `stbi_load_from_memory(data+4, size-4)` (stb já vendored) → GL_RGB(A)8.
  Atenção ao flip vertical (TGA bottom-left origin, descriptor bit 5).

### 2.4 `.msh` — mesh skinned (`CMesh::RestoreDeviceObjects`)

```
u32 parentID
u32 id
u32 FVF              // define layout (ver VertexDecl1-4 em RenderDevice.cpp:850+)
u32 sizeVertex
u32 numFaceInflunce  // 1..4 = quantos pesos por vértice
u32 numPalette       // ossos usados por ESTA parte (≤40)
u32 vertexCount
u32 faceCount        // = índices, /3 = triângulos
if (numPalette) { mat4 boneOffset[numPalette]; u32 boneName[numPalette]; }
u8  vertices[sizeVertex * vertexCount]
u16 indices[faceCount]
```
- Layout do vértice (VertexDecl, numFaceInflunce N): pos FLOAT3 @0; blendweight
  FLOAT(N-1) @12; blendindices D3DCOLOR @12+4(N-1) (4 bytes packed); normal FLOAT3;
  uv FLOAT2. Strides: N=1→?, N=2→?, N=3→44B... **confirmar com hexdump na D6** (como
  fizemos com o attr range do .msa na Fase 1 — não confiar no papel).
- Várias partes (numParts) por personagem/objeto, nomes `mesh\trNNNNNN.msh` derivados
  do look (TMSkinMesh.cpp:167+). Para árvores/casas o look é fixo por dwObjType
  (InitLook).

### 2.5 `.bon` — hierarquia de ossos (MeshManager)

```
(u32 childID, u32 parentID) * numBone   // numBone = filesize / 8
```
- Parent 0xFFFFFFFF = root. Frames CFrame montam a árvore; `matCombined` por frame.

### 2.6 `.ani` — animação (por tipo de animação)

```
u32 numAniTick    // ticks totais (duração; 80 FPS de amostragem)
u32 numAniFrame
mat4 matAnimation[numAniFrame * numBone]   // pose por frame por osso
```
- Lista mestra: `Mesh\MeshList.txt`? **NÃO** — bone anis vêm de uma lista própria
  (`m_BoneAnimationList`, MAX_BONE_ANIMATION_LIST) carregada por fscanf
  `"%d %d %d %s"` (id, numAniTypes, numParts, szAniName) — **localizar o arquivo
  da lista na D6** (provável `mesh\BoneAnimationList.txt` ou similar; grep no load).
- Interpolação: original guarda também quaternions (`matQuaternion`) derivados das
  matrizes; FrameMove avança frame por `m_dwFPS` (árvores = 80).

### 2.7 Skinning no original (referência para o shader GLSL)

O original usa **vs_1_1 compilado** (`Shader\skinmesh1..8.bin`, índice =
`numFaceInflunce-1`, e `+3` para boneAniIndex 61=folha). Não executamos bytecode D3D,
mas a matemática é LBS padrão; constantes: c9.. = 3 regs por osso (mat3x4 transposta
de `boneMatrix[i] * boneOffset[i] * view`), c1 = dir de luz (-1,1,1) normalizado,
c92.. = viewInv transposta. Se precisar bit-exact, os .bin vs_1_1 são disassembláveis
(formato simples) — provavelmente desnecessário.

**Nosso shader `skin.vert`**: UBO `BonePalette` (binding 1) com `mat4 bones[40]`;
atributos pos/normal/uv + `weights` + `indices` (GLubyte4 → ivec4); final =
Σ wᵢ × (bones[idxᵢ] × pos); iluminação gouraud igual ao mesh_lit; fog. Última
componente de peso = 1 - Σ (padrão para N pesos com N-1 armazenados).

---

## 3. Arquitetura nova (arquivos)

```
src/world/TerrainData.h/.cpp     // .trn parse, normais, mask, TileCoordList (tabelas)
src/world/TerrainRenderer.h/.cpp // batches por par de texturas, VBO dinâmico, tint dinâmico
src/world/SkyDome.h/.cpp         // sky001.msa + clima + fog + clear color
src/world/SeaSurface.h/.cpp      // grid procedural + onda sin + alpha
src/world/BoneAnimation.h/.cpp   // .bon + .ani + árvore de frames + sampling
src/world/SkinMesh.h/.cpp        // .msh parse + VAOs skinned + palette UBO upload
src/gl/shaders/terrain.vert/.frag
src/gl/shaders/sky.vert/.frag
src/gl/shaders/sea.vert/.frag
src/gl/shaders/skin.vert/.frag
src/scene/FieldView.h/.cpp       // cena de validação: --map FieldXXYY, câmera livre
tests/test_trn.cpp test_envtex.cpp test_wyt.cpp test_msh.cpp test_bonani.cpp test_picking.cpp
```

`GLRenderDevice`: estender `FrameData` UBO com `fogColor/fogStart/fogEnd` (fog é por
frame/cena, não por draw) + `emissiveLight` (m_colorLight do clima). GLTextureManager:
adicionar decode `.wyt` e lookups por nome para Model/Env lists.

---

## 4. Cronograma (10 dias)

### D1 — `.trn` loader + dados derivados
- `TerrainData`: parse (§2.1), alturas ×0.1, normais (portar `GetNormalInGround` +
  clamps de borda), mask 128×128, VAttr.
- `test_trn`: blob sintético mínimo + **parse do Field2723.trn real** no CI (assets
  estão no runner? NÃO — assets ficam fora do repo. Teste real roda local-only com
  guard `access()` igual ao glsmoke; sintético roda no CI).
- Saída: dump de estatísticas (min/max altura, tiles únicos) comparado com expectativa.

### D2 — Terrain renderer + FieldView
- `terrain.vert/.frag`: 2 UVs, vertex color (dwColor) como material diffuse, 2 luzes
  direcionais (as do RenderDevice ctor), FixZ, fog (ainda constante).
- Batching: agrupar tiles por (texFront, texBack) em VBO dinâmico por frame (ou
  pré-montar VBO estático por par no load — mapa é estático! Só dwColor muda com
  lâmpadas → VBO de cor separado, dinâmico, re-upload parcial).
- `FieldView` com câmera livre (WASD + mouse, nossa — não a TMCamera do jogo).
- Saída: screenshot terreno Field2723 texturizado (sem blend ainda = artefatos ok).

### D3 — Blend de tiles + EnvTextureList + tiles especiais
- `EnvTextureList3.bin` loader (512 regs de 264B; `.wys` via GLTextureManager da F1).
- TileCoordList/BackTileCoordList (copiar tabelas literais), blend por alpha de vértice.
- Lava 38/39 (scroll temporal), água 62-65. `test_envtex` sintético.
- Saída: screenshot Field2723 com blend correto (estradas/transições suaves).

### D4 — Céu + clima + fog real
- `SkyDome`: GetCommonMesh(1) = `mesh\sky001.msa` via GLMesh; texturas env 67-70
  por clima; render sem depth-write, seguindo a câmera, escala 0.5, y=-5.
- `SetWeatherState(0-3, +10 noite)`: LightVal[4] → m_colorLight (vai pro UBO),
  FogList[16][2] → fog start/end, m_dwClearColor → glClearColor.
- Fog no `terrain` e no `mesh_lit` (FS, fator por distância, como o D3D linear fog).
- Clima default da cena select-server: `time.wDay % 4` (igual original) — override por
  `--weather N` para testar.
- Saída: 4 screenshots (sol/nuvens/chuva/neve + noite).

### D5 — Mar
- `SeaSurface`: grid procedural (§1 TMSea::InitObject — (gridX+1)×(gridY+1) vértices,
  FVF 578: pos/diffuse/uv1/uv2), índices u16, textura env 3.
- Onda: `pos.y = sin(x*π/2 + t*π*2)*0.05 - 0.1` (portar FrameMove §2; variantes
  dungeon/fog ficam logadas como skip).
- Alpha blend + UV scroll. Validar em Field1616 (3 mares) e Field2922 (4 mares).
- Saída: screenshot mar em Field1616.

### D6 — Loaders skinned: `.wyt`, `.bon`, `.ani`, `.msh`
- `.wyt` via stb (§2.3) + ModelTextureList lookup por nome; `test_wyt` sintético.
- `.bon` → árvore de frames (CFrame-lite: id/parent/matCombined); `.ani` → arrays de
  matrizes por frame; `.msh` → partes com palette (§2.4). **Hexdump real primeiro** —
  confirmar strides/FVF como fizemos na Fase 1.
- `test_msh`/`test_bonani` sintéticos + parse real de `mesh\tr*.msh` local.
- Saída: árvore de Field0101 em pose frame 0 (sem skinning ainda — render como estático
  com bones em identidade, pode parecer "em T-pose de árvore").

### D7 — Skinning GLSL + animação
- `skin.vert` (§2.7) + UBO BonePalette binding 1 (mat4[40], orphan por draw).
- Sampling de animação a 80 FPS (m_dwFPS das árvores), loop por numAniTick.
- Objetos: TMTree (331-342, 351-378) renderizando animado em Field0101.
- Saída: `--frames 30` sequência mostrando balanço; screenshot no PR.

### D8 — Roteamento completo + tint de terreno
- ObjectFile: despachar por faixas (§1 "Entra"): Tree/House(cHouseType table)/Ship/
  Float/Sea; contadores de skip para o que fica de fora (leaf/butterfly/fish/glow).
- GroundSetColor (lâmpadas 501-503 alteram dwColor de tiles próximos — re-upload da
  região do VBO de cor) + GroundGetColor alimentando emissive dos objetos.
- Saída: Field0101 completa: terreno + céu + árvores + casas + lâmpadas tingindo chão.

### D9 — Picking + polish
- `GetHeight` (bilinear nos 4 cantos do tile) + `GetPickPos` (ray-tri sobre quads da
  mask, portar TMGround.cpp:3299+) — matemática pura, `test_picking` com casos
  calculados à mão do Field2723.trn real.
- Câmera livre final (scroll zoom, clamp), `--map`, `--weather`, `--shot`.
- Saída: picking clicando no chão imprime coordenada no console (validação manual).

### D10 — DoD, docs, PR
- Revisar DoD item a item; screenshots lado-a-lado (original Windows se disponível).
- Atualizar docs: 03 (audit — correções leaf/shadow), 06 (.msa attr 20B já corrigido;
  adicionar .msh), 07 (world — marcar implementado), 13 (escopo fase 2 ajustado), 16
  (este doc — marcar feito), README do migration-opengl.
- PR `feature/port-phase2` → main.

---

## 5. Riscos

| Risco | Mitigação |
|---|---|
| `.msh` layout real divergir do VertexDecl no papel (igual ao attr range do .msa) | Hexdump + parse de `tr*.msh` reais na D6 **antes** de escrever o renderer; ajustar tabela §2.4 no doc |
| Arquivo da lista de bone-animações (`m_BoneAnimationList`) não localizado de cara | grep `fscanf` no MeshManager::InitMeshManager; nome deve estar em TMPaths.h ou hardcoded |
| Skinning GLSL diferente do vs_1_1 em detalhes (ordem boneMatrix×boneOffset×view) | Testar com árvore real: se explodir/esticar, transpor; último recurso = disassemblar skinmesh1.bin (vs_1_1 é trivial de decodificar) |
| Performance do terreno (4096 tiles) | Batch por par de texturas; VBO estático de geometria + VBO dinâmico só de cor; medir com `--frames` |
| Fog gouraud vs por-pixel | D3D9 fixed fog é por-vértice (FOGVERTEXMODE linear); fazer no FS por distância — visualmente superior e mais simples; aceitar diferença sutil |
| `.ani` de árvores pode ter quirks (numAniCut por tipo) | Portar a struct de leitura exata do MeshManager.cpp:159-184 |
| Mar dungeon/fog variants (bFog, m_bDungeon) | Só o caminho comum entra; variantes logam skip (consistente com política da F1) |
| `dwColor` alpha=0 em tiles sem backtile | Original ainda seta stage1 com tex 256+idx; confirmar visualmente que modulação fica idêntica (blend com alpha 0 = tile frontal puro) |

## 6. Fora de escopo mas anotar para fases futuras

- Fase 3 ganha de graça: TMSkinMesh render (personagens = mesmo pipeline + TMHuman
  logic + seleção de look/anim + blob shadow). Resta: routing de cena, montarias,
  efeitos de equipamento (RenderSkinMeshEffect), mantuas (m_matMantua).
- Shaders `vseffect*.bin`/`pseffect*.bin` (8 efeitos) — Fase 4; mesma técnica de
  disassembly se precisar bit-exact.
- `AttributeMap.dat`/`cdata.bin` (checksum/anti-hack) — ignorar no port single-player.

## 7. Achados da execução (pós-implementação)

1. **Winding global**: D3D9 front = CW **na tela** (framebuffer y-down); GL default
   CCW (window y-up). Como não flipamos Y, `glFrontFace(GL_CW)` iguala a semântica
   (`D3DCULL_CCW` ≡ cull `GL_BACK`). Sem isso, meshes unilaterais (muralhas) ficam
   invertidas. Aplicado no main + glsmoke; ordens de índice do terreno/mar são as
   originais (sem reversão).
2. **Listas de texturas são 528B/entrada (A+B)**: `MeshTextureList.bin` (3048),
   `EnvTextureList3.bin` (2048), `EffectTextureList.bin` (600) — todos stride 528,
   metade A = `stTextureListInfo` ativa. O código vazado lê stride 264 — incompatível
   com este build (índices numéricos como `67+clima` do TMSky não funcionam; usar
   lookup por nome: `sky0N.wys`).
3. **Nomes de textura no .msa podem ter extensão errada** (`.tga` legado) — lookup
   por stem (sem extensão), igual ao truque do `GetModelTextureIndex` original.
4. **Tiles lava/água não existem neste build**: nenhum `.trn` usa byTileIndex 28-29
   (lava) ou 52-55 (água) — os paths MODULATE2X estão implementados mas sem cobertura
   visual aqui. `Tile00002/3.wys` ausentes → manchas pretas na Field2723 são fiéis
   ao asset (textura nula = preto no D3D9 também).
5. **Alpha de objetos estáticos**: `TMObject::Render` usa ALPHAREF 0xAA **+ blend**
   para texturas com cAlpha != 'N' (não é só cutout) — corrigido no DrawMesh por subset.
6. **`TerrainGetColor`**: `/256.0f` (não 255) e o terceiro branch `else if (nX==63)`
   duplicado (morto) — borda inferior lê zeros (preto). Ambos preservados.
