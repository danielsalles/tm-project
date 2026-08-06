# 01 — Auditoria do Renderer Atual (D3D9)

Consolidação do estado atual, com referências `arquivo:linha` para cada afirmação.

## 1. Estrutura geral

```
NewApp (Win32 loop, NewApp.cpp:1339 wWinMain)
 └── RenderDevice : public D3DDevice          (RenderDevice.h:11)
      ├── IDirect3D9 / IDirect3DDevice9        (D3DDevice.h:81-82)
      ├── TextureManager                       (UI[512], Effect[512], Model[2048], Env[512], Dyn[72], Guild[64])
      ├── MeshManager → TMMesh / CMesh         (MeshManager.h:48-76)
      ├── TMFont2 / TMFont3                    (GDI → textura A4R4G4B4)
      └── ID3DXSprite m_pSprite                (RenderDevice.cpp:181)
```

- Instância global: `g_pDevice` (`RenderDevice.cpp:141`). Acesso duplo em todo o código:
  wrapper com cache (`g_pDevice->SetRenderState`, cache em `RenderDevice.cpp:672-715`)
  **misturado** com acesso cru (`g_pDevice->m_pd3dDevice->SetFVF/SetTransform/SetMaterial/SetLight`,
  ex.: `CMesh.cpp:604-650`, `TMSky.cpp:168`). O cache não cobre FVF, transforms, materiais, luzes.
- Device criado com `D3D9b_SDK_VERSION` (`D3DDevice.cpp:53`) — SDK beta. **Rejeita
  hardware vertex processing**: exige SOFTWARE/MIXED VP e `MaxVertexBlendMatrices>=2`
  (`D3DDevice.cpp:994-1006`, `RenderDevice.cpp:520-523`).

## 2. O frame, ponta a ponta

`NewApp::RenderScene()` (`NewApp.cpp:663-689`) é o único dono de Begin/EndScene:

```
TestCooperativeLevel                        (device lost, NewApp.cpp:665)
SetViewPort(0,0,W,H) → recalcula projeção   (RenderDevice.cpp:341, 1837)
Lock(1)  → Clear(Z|TARGET) + BeginScene     (RenderDevice.cpp:358-406)
SetViewVector(cam, lookat)                  (D3DXMatrixLookAtLH, RenderDevice.cpp:1826)
SetRenderStateBlock(1)                      (preset 3D, RenderDevice.cpp:1630)
ObjectManager::RenderObject()               (travessia da árvore, ObjectManager.cpp:628-660)
SetRenderStateBlock(3)                      (preset UI, RenderDevice.cpp:1790)
ObjectManager::RenderControl()              (UI, ObjectManager.cpp:591-626)
Unlock(1) → EndScene + Present              (RenderDevice.cpp:408-448)
```

### Ordem de draws na field scene

`TreeNode::AddChild` insere no início da lista (`TreeNode.cpp:120-129`) → render é na
ordem inversa da inserção. Ordem efetiva (`TMScene.cpp:70-90`, `TMFieldScene.cpp:1758-1808`):

1. ItemContainer (itens dropados)
2. TMSky (dome)
3. HumanContainer (personagens)
4. GroundObjectContainer → TMGround (terreno) + TMObjectContainer (árvores/casas/mares/luzes)
5. ShadeContainer (blob shadows)
6. EffectContainer (sol, chuva/neve, billboards, skills)
7. ExtraContainer (TMFont3 — números de dano)
8. UI

**Não há sort de transparentes.** A composição correta depende dessa ordem + `ZWRITEENABLE=0`
nos efeitos. Qualquer reordenação (ex.: batching por material ingênuo) quebra a cena.

## 3. Os 4 render-state blocks (`SetRenderStateBlock`, RenderDevice.cpp:1602-1824)

| Block | Uso | Estados-chave |
|---|---|---|
| 0 (`:1604`) | Quads UI genéricos | Z on, fog off, lighting off, cull CCW, SRCALPHA/INVSRCALPHA, stage0 MODULATE tex×diffuse, filtro LINEAR, FVF 322 |
| 1 (`:1630`) | **Cena 3D** | 2 luzes direcionais + `SetLight`, ALPHATEST GREATER + ALPHAREF por GPU, ambient 0x33FFFFFF, Z LESSEQUAL+write, LIGHTING on, SPECULAR off, NORMALIZENORMALS, fog linear (start/end por `m_fSightLength`), stage0 MODULATE, stage1 DISABLE |
| 2 (`:1758`) | Fonte/texto | Z off, blend SRCALPHA/INVSRCALPHA, ALPHAREF=8, **filtro POINT**, stage0 MODULATE cor+alpha |
| 3 (`:1790`) | Painéis UI | = block 2 mas blend off e filtro LINEAR |

Decodificação dos `D3DTOP` numéricos usados no código: 1=SELECTARG1, 2=MODULATE,
4=MODULATE2X, 5=MODULATE4X, 6=ADD, 7=ADDSIGNED, 8=SUBTRACT, 11=MODULATEALPHA_ADDCOLOR,
12=LERP (BLENDDIFFUSEALPHA), 18=MODULATEINVALPHA_ADDCOLOR, 24=DOTPRODUCT3.
Exemplos reais: céu noturno usa SUBTRACT no stage0 e LERP no stage1 (`TMSky.cpp:179-186`);
itens legendary usam DOTPRODUCT3 (`TMMesh.cpp:436`).

**Quirk de hardware**: o path NVIDIA (`m_iVGAID==1`) seta `ALPHAREF=0xFF000000` em 32bpp
(`RenderDevice.cpp:1689-1703` e 9 outros sites) — efetivamente desliga o alpha test.
Comportamento a normalizar no port (usar ALPHAREF=0xDD para todos).

## 4. Formatos de vértice

### FVF clássicos (`Structures.h`)

| Struct | FVF | Campos | Stride | Uso |
|---|---|---|---|---|
| `RDLVERTEX` (`:232`) | 322 = XYZ\|DIFFUSE\|TEX1 | pos, cor, uv | 24 | efeitos, céu, clima, shade |
| `RDTLVERTEX` (`:223`) | 324 = XYZRHW\|DIFFUSE\|TEX1 | posT(rhw), cor, uv | 28 | quads UI, fontes, flares |
| `RDTLVERTEX2` (`:212`) | 580 = XYZRHW\|DIFFUSE\|TEX2 | posT, cor, uv1, uv2 | 36 | UI multi-tex |
| `RDVERTEX2` (`TMMesh.h:5`) | 530 = XYZ\|NORMAL\|TEX2 | pos, normal, uv1, uv2 | 40 | meshes c/ multitex animada |
| `RDLVERTEX2` (`:247`) | 578 = XYZ\|DIFFUSE\|TEX2 | pos, cor, uv1, uv2 | 32 | mar (`TMSea.cpp:186`) |
| `RDLNVERTEX2` (`:257`) | 594 = XYZ\|NORMAL\|DIFFUSE\|TEX2 | pos, normal, cor, uv1, uv2 | 44 | terreno (`TMGround.cpp:3216`) |

### Vertex declarations programáveis (`InitVertexShader`, RenderDevice.cpp:766-1526)

Skinned (seleção por `m_numFaceInflunce-1`, `CMesh.cpp:608-623`):

| Decl | Stride | Layout |
|---|---|---|
| 1 influência (`:773`) | 36* | pos@0 f3, **bone indices UBYTE4@12**, normal@16, uv@28 — sem peso (implícito 1.0) |
| 2 influências (`:803`) | 36 | pos@0, peso f1@12, índices@16, normal@20, uv@32 |
| 3 influências (`:839`) | 40 | pos@0, pesos f2@12, índices@20, normal@24, uv@36 |
| 4 influências (`:875`) | 44 | pos@0, pesos f3@12, índices@24, normal@28, uv@40 |

\* stride efetivo 32 em disco para 1 influência (sem campo de peso); declaração D3D reporta 36.

- **Último peso sempre implícito** (`1 − Σ`), padrão D3D XYZBn.
- Índices de osso = DWORD empacotado (4 bytes), índices na **paleta local da parte** (máx 40 ossos, `CMesh.h:25`).
- Banco secundário (decls/shaders 4-7) usado quando `m_nBoneAniIndex==61` (`CMesh.cpp:612-623`).

Criadas mas **sem shader associado** (mortas): DeclMesh, DeclSwing, DeclEfMesh, DeclBumpEquip
(com TANGENT — bump mapping nunca terminado), DeclEnvMesh, DeclWaterFallMesh, etc.
(`RenderDevice.cpp:1054-1472`). **Não portar.**

## 5. Shaders — o que existe de verdade

- **8 vertex shaders vs.1.1** (`Shader\skinmesh1..8.bin`, bytecode → `CreateVertexShader`,
  `RenderDevice.cpp:934-961`). Constantes:
  - `c0` = (1.0, power, progress, 765.01) (`CMesh.cpp:993`)
  - `c1` = direção da luz (`RenderDevice.cpp:1687`)
  - `c2-c5` = projeção transposta (`RenderDevice.cpp:1849`)
  - `c6` = fog (`CMesh.cpp:1046-1049`)
  - `c7` = ambient×0.25 + emissive (`CMesh.cpp:994-1033`)
  - `c8` = material (`CMesh.cpp:1028-1030`)
  - `c9+3i` = paleta de ossos (3 registradores por osso, matriz 4×3 transposta = `bind × frameWorld × view`, `CMesh.cpp:625-631`)
  - `c92-c95` = view inversa transposta (`CMesh.cpp:637-640`)
- **4 VS + 6 PS de efeito** (`vseffect1-4.bin`, `pseffect1-6.bin`): criados
  (`RenderDevice.cpp:1495-1571`) mas **nunca usados** — nenhum `SetPixelShader` no projeto.
  Mortos junto com JBlur. **Não portar; reimplementar do zero se quiser água/blur modernos.**
- Fallback software: se VS < 1.1, `SetSoftwareVertexProcessing(1)` (`CMesh.cpp:601`) — irrelevante em GL.

Limite prático de paleta: 96 registradores vs.1.1 → ~27 ossos por draw. Em GL 4.1, UBOs
garantem muito mais — usar `uBones[64]`.

## 6. Iluminação

- **2 direcionais globais**: light0 dir (1,-1,-1) cor `m_colorLight` (do clima, `TMSky.cpp:13-19`),
  light1 dir (0,0,1) cor `m_colorBackLight` (`RenderDevice.cpp:1632-1656`). Caso "boss":
  light0 zerada por 1 frame (`:1639-1651`).
- **Point lights**: `TMLight` usa slots 2+ (até 6 por container, `TMObjectContainer.h:9`),
  `D3DLIGHT_POINT`, attenuation 0.001 (≈nula, só Range=5 corta) (`TMLight.cpp:17-36`).
  Também **carimba vertex color do terreno** (`GroundSetColor`, `TMLight.cpp:52-62`).
- **Materiais**: `D3DMATERIAL9` por objeto (emissive 0.1-0.3, Power=0, sem specular real),
  espalhados em `TMMesh.cpp:320-334`, `TMSky.cpp:134-153`, `TMGround.cpp:2750`, etc.
- Ambient global 0x33FFFFFF; `COLORVERTEX=1`; `NORMALIZENORMALS=1`; `SPECULARENABLE=0`.

## 7. Fog

- Sempre **vertex fog LINEAR**, sem range fog (`FOGVERTEXMODE=3`, `RANGEFOGENABLE=0`).
- Cor = clear color do clima (`RenderDevice.cpp:1728`); clear: dia `0xFF335599`,
  noite `0xFF333333`, pôr-do-sol `0xFF441100`, dungeon preto (`:385-397`).
- Start/End por frame: `(m_fFogStart + sightLength) - 8` / `(m_fFogEnd + sightLength) - 15`
  (`:1725-1731`). Base vem de `TMSky::FogList[16][2]` (`TMSky.cpp:21-39`), com subaquático
  1..10 (`:501-514`).
- Efeitos/céu/mar desligam fog localmente e restauram.

## 8. Texturas

- **Formato custom `.wys`**: DDS sem o 1º byte, fourCC corrompido ('2'→DXT1, senão DXT3)
  (`TextureManager.cpp:299-310`). Loader remenda o header e chama
  `D3DXCreateTextureFromFileInMemoryEx`. Conteúdo: **DXT1/DXT3**.
- **Demais arquivos**: TGA sem os 4 primeiros bytes e sem footer (`:269-292`).
- **Color key 0xFF000000** (preto→transparente) aplicado pelo D3DX nos paths TGA
  (`:265, 542, 757, 1022`). Sem paletas 8-bit em lugar nenhum.
- Endereçamento: sempre WRAP (default; o mar depende disso — `TMSea.cpp:211-214`).
- Filtros por state block (§3); mipmaps por política `m_nMipMap` (UI=1 nível, Effect=4,
  Model=4 progressivo por VRAM, Env=4) (`TextureManager.cpp:626-669, 835-894`).
- Tudo `D3DPOOL_MANAGED` + recarga lazy pós-reset → em GL vira simplesmente "carregar uma vez".

## 9. UI, fontes, cursor

- **UI**: controles de RC binário (`TMScene::ReadRCBin`), 30 camadas de `GeomControl`
  (`ObjectManager.cpp:591-626`), desenhados num **frustum 3D raso** (`SetMatrixForUI`:
  fov 0.1, câmera (0,0,50), `RenderDevice.cpp:1854-1870`) — funcionalmente ortho.
  1 draw por quad (DrawPrimitiveUP ou `ID3DXSprite::Draw` por quad!). Objetos 3D na UI
  (ícones de item) usam block 1 + `ZFUNC=ALWAYS` (`RenderDevice.cpp:3316-3364`).
  Half-texel offset manual nas UVs (`:2110-2117`) — **não necessário em GL**.
- **Fontes**: `TMFont2` rasteriza cada string via GDI `TextOut` num DIB 32bpp e copia
  luminância→alpha para textura A4R4G4B4 512×64 (`TMFont2.cpp:177-269`). Cada string =
  1 textura re-rasterizada por `SetText`. Quebra de linha DBCS byte-a-byte (coreano).
  `LPD3DXFONT` existe mas é código morto (`RenderDevice.h:109`).
- **Cursor**: 2 modos — software (painel de UI `SCursor`, cursor Win32 oculto) ou
  hardware (`.cur` de recurso, `NewApp.cpp:224-227`). No port: sempre software (SDL cursor
  oculto) — o modo hardware morre.
- **Gamma**: `SetGammaRamp` com rampa linear × brightness (`RenderDevice.cpp:717-745`).
  Em GL vira uniform de brilho em pós-process (ou ignorar no MVP).

## 10. Picking e culling (CPU — nada muda)

- Raio: `GetPickRayVector` unproject manual (`RenderDevice.cpp:1872-1889`).
- Terreno: `D3DXIntersectTri` brute-force na mask 128×128 (`TMGround.cpp:3299-3513`).
- Humanos/itens/portões: quads fixos + intersectTri (`TMHuman.cpp:3084-3230`).
- Culling por tile: 4 cantos transformados por view×proj na CPU, teste `z∈[0,1)` + margem
  50px (`TMGround.cpp:3218-3246`).
- **Tudo isso é matemática pura sobre as próprias matrizes** — preservando as matrizes
  (decisão LH), picking/culling não mudam uma linha.

## 11. Device lost (a deletar)

Máquina completa: `Unlock` detecta `D3DERR_DEVICELOST` (`RenderDevice.cpp:443`), `Lock`
espera e chama `Reset3DEnvironment` (`:358-381` → `D3DDevice.cpp:770-800`), que dispara
`InvalidateDeviceObjects` (libera TUDO: texturas, meshes, shaders, sprite, fonte GDI,
`RenderDevice.cpp:626-670`) e `RestoreDeviceObjects` (`:525-623`). Propaga pela árvore
via `ObjectManager` (`ObjectManager.cpp:540-588`). **Em GL nada disso existe** — apagar
~500 linhas e todos os overrides `Invalidate/RestoreDeviceObjects` nas ~60 classes da árvore.

## 12. JBlur / pós-process (morto)

`JBlur.cpp` é inteiramente stub (`:12-66`). O header (`JBlur.h`) mostra o design: ping-pong
de texturas ¼/½, projeção ortho, motion blur, glow. As RT textures do `TextureManager`
(`:1446-1560`) também não têm consumidores (zero `SetRenderTarget` no projeto).
Bloom/glow declarados, nunca inicializados (`TextureManager.h:125-132`).
**Conclusão: pós-process será implementação nova sobre FBOs, guiada pelo design do JBlur.h.**
