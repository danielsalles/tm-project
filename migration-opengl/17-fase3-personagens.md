# 17 — Fase 3: Personagens (plano completo) — **CONCLUÍDA**

**Objetivo**: personagens skinned multi-parte no mundo GL — **o personagem próprio
controlável** (click-to-move com rota + altura de terreno + câmera follow), **NPCs e
monstros** parados/andando, com o sistema completo de **looks** (8 partes por
`LOOK_INFO`), **armas como partes skinned**, **animações por arma×moção**
(`m_sAnimationArray` + `AniSound4.txt`) e interpolação **slerp** (humanos) /
**lerp de matriz** (monstros/árvores). Cena alvo: Field0101 com char próprio andando
pela cidade + spawns de demo (guarda humano, orc, javali, cavalo).

**Pré-requisito**: Fase 2 merged (skinning pipeline `SkinMesh`/`BoneAnimation`,
`TreeRenderer`, FieldView, picking, `glFrontFace(GL_CW)`).

**Estado final (DoD)**:
- [x] `BoneAnimation` estendido p/ múltiplos `.ani` por tipo (`ch01NNNN.ani` via
      `ValidIndex.bin`), cuts concatenados + `numAniCut[]` + `m_sAnimationArray`
      com fallbacks do original; quaternion table p/ índices 0/1 (ch01/ch02)
      → `CharacterAnimation.cpp` (ch01: 186 cuts, 47 ossos; or01: 55 cuts, 35 ossos)
- [x] `MAX_BONES` 40→**64** no shader/UBO (ch01 tem **47 ossos** — ver §7)
- [x] `LookResolver`: look→(mesh,texture) por parte, incl. tabela de exceções
      (`ch010218→ch010214`, `ch02x3x→ch01xx30`, `mt01*`, `tr13-17→tr130101`,
      `bm010102→mi010105`); testes unitários das exceções
- [x] `CharacterMesh`: 8 partes (`Face/Helm/Coat/Pants/Gloves/Boots/Right/Left`)
      compartilhando 1 esqueleto + `SkinPipeline` (UBO extraído do TreeRenderer);
      render multi-parte com fog/luz
- [x] `AniSound4.txt` parser → `g_MobAniTable` (motion→aniIndex + speed; **sem som**)
      — **51 seções** neste build (tipos 13-19/52/58 ausentes; tipo 59 nome EUC-KR)
- [x] `Character` runtime: `SetAnimation(ECHAR_MOTION)` simplificado (humanos via
      `m_sAnimationArray[type][weapon][table]`, monstros via `dwAniTable` direto),
      `FrameMove` de rota (lerp por segmento, altura = máscara×0.1, giro gradual,
      catch-up de segmentos após hitch)
- [x] `BASE_GetRoute` portado (greedy, códigos numpad 50-57, tolerância MH=8) + testes
- [x] Click-direito = andar até o ponto (pick → rota → walk/run); teclas de demo
      1-6 ataques / 0 die / 8 stand / 9 levelup / R walk-run; câmera **follow**
      (roda = zoom 3-14, arrastar = orbitar, F alterna free-fly)
- [x] Spawns de demo via CLI: `--spawn type,x,z` (repetível; orc/wolfbear validados);
      presets: P cicla arma (0-11), C cicla classe (TK/Foema/BM/Hunter); `--weapon N`,
      `--walkto x y` (automação), `--nochar`, `--follow/--nofollow`
- [x] Screenshots de validação (idle/walk/ataque; humano + monstro) + CI verde
- [x] `Projects/` intocado

**Duração estimada**: 2 semanas (10 dias úteis).

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original |
|---|---|
| Multi-`.ani` por tipo + cuts + `m_sAnimationArray[type][weapon][motion]` | `MeshManager.cpp:95-270` |
| Quaternion table (slerp) p/ ch01/ch02 | `MeshManager.cpp:147-160`, `TMSkinMesh.cpp` FrameMove |
| `AniSound4.txt` → `g_MobAniTable[60]` (dwAniTable/dwSpeed; ignorar dwSoundTable) | `ObjectManager.cpp:859-907` |
| Look→arquivos (8 partes + exceções) | `TMSkinMesh.cpp:160-330` |
| `Character` (pos/ângulo/altura/rota/animação/render) | `TMHuman.cpp:1886-2400, 2832-2920, 5824-6230`, `TMSkinMesh.cpp` |
| `BASE_GetRoute` | `Basedef.cpp:859-960` |
| Câmera follow simplificada | inspiração: `TMFieldScene` camera (m_fCameraLength/Yaw) |
| NPC/monstro parado ou patrulhando (sem IA) | `TMNPC.cpp` (só o caminho visual) |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| Rede/pacotes (`OnPacket*`, MSG_Action) | Sem servidor; movimento é local por rota | Fase 5+ |
| Sanctus/legend glow (`SetColorMaterial`, `RenderEffect`, attach effects) | Sistema de efeitos/billboards | Fase 4 |
| Swing trails (`m_pSwingEffect`) | Efeitos | Fase 4 |
| Mantuas (SkinMeshType 85) + montarias (hs01, tipo 31) | Cosmético; render já suporta — ligar depois | Fase 4 (stretch aqui se sobrar tempo) |
| Labels de nome/HP/chat, minimapa, party | UI | Fase 5 |
| Sons de passos/ataque (dwSoundTable) | Áudio | Fase 6 |
| Colisão char×char, combate/dano/skills | Gameplay/rede | Fase 5+ |
| `m_cShadow` (stealth) / `m_cHide` / die-sink na parede | Lógica de jogo | Fase 5 |
| Círculo de seleção sob o char (FieldScene meshes 10/12/14) | Decal .msa simples — **stretch** desta fase | Fase 4 |

> **Correção do roadmap**: não existe "blob shadow" (`mesh\sh.msh`) neste client —
> `sh01` (BoneAniIndex 60) é legado sem uso; `m_pShadow` do TMHuman é billboard
> vermelho de stealth. Sombra de personagem = ausente no original (remover do escopo).

---

## 2. Sistema de assets de personagem

### 2.1 Múltiplos `.ani` por tipo (diferente das árvores!)

Árvores: 1 arquivo `prefix%04d.ani` com todos os cuts. **Personagens (ch01/ch02) e
monstros**: `BoneAni4.txt` declara `numAniTypeCount` (ch01 = **186**); o loader tenta
cada `prefix%04d.ani` com índice de `ValidIndex.bin`; **cada arquivo existente = 1 cut**
(`numAniCut[dwFileIndex] = numTicks` daquele arquivo), e todas as matrizes são
**concatenadas** num único array `matAnimation` (offset acumulado). `numAniFrame`
(segundo dword) = nº de ossos.

Para ch01/ch02 (índices 0/1): pré-computa também `matQuaternion[]`
(`D3DXQuaternionRotationMatrix` por matriz) — humanos interpolam rotação via **slerp**
de quaternion + lerp de translação; demais tipos usam lerp de matriz (já temos).

`m_sAnimationArray[nClass][nWeapon][nMotion]`:
- `nArrayIndex = ValidIndex+1`; `nWeapon = nArrayIndex/100 - 1`; `nAnimation = nArrayIndex%100 - 1`;
  valor = `dwFileIndex` (índice sequencial do cut carregado).
- Regras de fallback do original (`MeshManager.cpp:195-265`): TK-BM (copiar anim
  anterior p/ motions 4-9 vazios), cópia cruzada entre classes (índice 11/12/14),
  default p/ classe 0 quando vazio. **Portar literalmente** (testes com valores reais).

Estruturas alvo (estender `src/world/BoneAnimation.h`):
```cpp
struct CharacterAnimation {           // por BoneAniIndex
    Skeleton skeleton;                 // .bon (reuso)
    std::vector<uint32_t> numAniCut;   // ticks por cut (na ordem carregada)
    std::vector<glm::mat4> mats;       // todos os cuts concatenados [Σticks × bones]
    std::vector<glm::quat> quats;      // só p/ índices 0/1 (mesmo layout)
    uint32_t numBones = 0;
    int16_t animMap[/*weapon*/][/*motion*/] // m_sAnimationArray[type]
};
bool LoadCharacterAnimation(animIndex, validIndex[186], CharacterAnimation& out);
```

### 2.2 `AniSound4.txt` → tabela de moções

Formato por tipo (`ObjectManager.cpp:859`): header `[Nome] tipo`; tipos < 2 (humanos):
28 linhas `NOME a0 s0 a1 s1 a2 s2 a3 s3 snd` (4 classes: ani+speed cada); tipos ≥ 2:
`NOME ani speed snd`. Montar `g_MobAniTable[60]{dwAniTable[56], dwSpeed[56]}` +
`g_MobAniTableEx[4][...]` p/ humanos. Parse tolerante (arquivo na raiz do repo).

`ECHAR_MOTION` (Enums.h:35): 28 valores (STAND01=0 … PUNEND=27). Mapeamento final:
`aniIndex = m_sAnimationArray[type][weapon][ g_MobAniTableEx[class][type].dwAniTable[motion] ]`
(monstros: `dwAniTable[motion]` direto como cut index — verificar no SetAnimation
qual ramo cada tipo usa; o `+28` no original é a tabela de "em montaria").

### 2.3 Look → nomes de arquivo (por parte i = 0..7)

`LOOK_INFO` = 8 pares `(Mesh_i, Skin_i)` (shorts). Regra base
(`TMSkinMesh.cpp:167-190`), com `bExpand=0` no nosso escopo:

```
mesh  = prefix + %02d(i+1) + %02d(Mesh_i + 1)                 → .msh
tex   = prefix + %02d(i+1) + %02d((Skin_i & 0xFFF) + Mesh_i + 1) → .wyt
```

Exceções (portar como tabela, com teste por linha):
- tipos 45/46/53/54: mesh e tex fixos em `%02d01` (ignoram look)
- `God2Exception(i)`: textura usa parte **1** no lugar de i+1 (cosmetic god2 — mapear a
  lista de exceções do original; se envolver nCos/cosmético, stub = regra base)
- `ch010218.msh + ch010219.wyt` → tex vira `ch010214.wyt`
- `MantleException(tex)` / `mt010124/132-137.wyt` → mesh vira `mt010131.msh`/`mt010124.msh`
- tex `ch02?3?.wyt` com `[10]=='1'|'4'|'5'` → `ch010130/ch010430/ch010530.wyt`
- `ch020315.wyt`→`ch020314.wyt`; `bm010102.wyt`→`mi010105.wyt`;
  `tr13..tr17`→`tr130101.wyt`; `tr190101`→`tr180101.wyt`

**Fallback de arquivo ausente**: original não checa (crasha ou parte invisível).
Nossa regra: parte ausente = não renderizada + warn no log (sem throw) — preciso p/
looks arbitrários de demo.

Semântica das partes humanas (`HUMAN_LOOKINFO`, Structures.h:296):
`0=Face 1=Helm 2=Coat 3=Pants 4=Gloves 5=Boots 6=Right 7=Left` — **armas são partes
skinned no mesmo esqueleto** (não há attach de .msa em bone para humanos neste build).

### 2.4 CharacterMesh (render)

- 1 `Skeleton` + 1 `CharacterAnimation` por tipo (cache por BoneAniIndex, como
  `TreeAnimationCache`); **8 GLSkinMesh** (partes) compartilhando o mesmo UBO de
  paleta (1 upload por personagem por frame, não 8).
- Sampling: tick dentro do cut atual; humanos slerp (quats) + lerp (trans);
  monstros lerp de matriz (reuso do caminho das árvores). `m_dwFPS` por moção vem de
  `dwSpeed` da AniSound4 (validar semântica exata em `TMSkinMesh::FrameMove` —
  registrar achado no §7).
- Palete por osso: `bindInv[i] × combined[frameId[i]]` (igual Fase 2); root frame
  recebe o world do personagem (YPR(fYaw−90°?) — alinhar com a convenção já validada
  nas árvores; o `.ani` do osso 0 **é usado** em personagens — diferente das árvores!
  Ver `TMSkinMesh::FrameMove`: `matRot` do root = world; confirmar se o cut anima o
  root também. Teste visual decide.)
- Estados GL: mesmo caminho `mesh_lit`+skinning (fog/luz do SkyDome), cAlpha por
  parte (algumas partes têm alpha), ZWrite on, cull back (CW global).
- Escala: `m_fScale` (alguns tipos 20/24 alteram altura/velocidade — stub 1.0).

---

## 3. Character runtime (movimento)

Portar o núcleo de `TMHuman::FrameMove` (TMHuman.cpp:2090-2260), sem UI/efeitos:

- Estado: `m_vecPosition` (x,z), `m_fHeight`, `m_fAngle`/`m_fWantAngle`,
  `m_vecRouteBuffer[≤24]` + índice, `m_dwStartMoveTime`, `dwUnitTime = 1000/m_fMaxSpeed`
  (walk 2.0; run quando `m_fMaxSpeed > 2`).
- Por frame: `fProgressRate = (now - start)/unitTime`; pos = lerp(route[i], route[i+1]);
  avança segmento; altura = lerp(mask[a], mask[b]) × 0.1 (`GroundGetMask` — Fase 2);
  giro: `m_fAngle → m_fWantAngle` a taxa fixa (fElapsedAngleToTime × fDAngle);
  animação WALK/RUN conforme speed; chegou → STAND01.
- `BASE_GetRoute(x,y,&tx,&ty,route,distance=12,mask, MH=8)` (Basedef.cpp:859):
  greedy numpad (50-57), anda célula a célula na **máscara 128×128** (atenção: coords
  do route são em células de máscara? verificar conversão `*2` no caller —
  FieldScene.cpp:16147) com tolerância de altura MH; para no alvo ou no 1º bloqueio.
  Retorna também `targetx/y` efetivos (alvo "alcançável").
- Coordenadas: máscara é 128×128 para terreno 64×64 tiles → célula = 0.5u. O route
  opera em células; posição final em unidades de mundo = célula/2 (+0.25 centro).
  Validar com teste usando Field0101 real.

Input (FieldView): click-direito → `TerrainPick` (Fase 2) → célula → rota → anda.
Tecla **R** toggle walk/run. Teclas **1-6** ataques, **0** morrer/reviver (demo).
Sem colisão com objetos/casas (original também não tem char×obj; só máscara).

---

## 4. Câmera follow

Modo novo no FieldView (`--follow`, default on quando há char próprio):
- Câmera atrás do char: `cam = charPos + R(yaw)·(0, h, +dist)`, lookAt charPos+1.2u;
  yaw orbita com arrastar botão **esquerdo** (liberado vira look-around atual);
  roda = dist 3..14u; pitch deriva de dist (como o original: quanto mais longe, mais alto).
- Suavização simples (lerp 10%/frame p/ pos).
- Sem colisão de câmera com parede (original tem; stretch).
- Tecla **F** alterna follow ↔ free-fly (modo atual preservado).

---

## 5. Spawns de demo

CLI (FieldView):
```
--char class,weapon,face,helm,coat,pants,gloves,boots,right,left   (look do char próprio;
        default = preset Transknight sem elmo, espada 1H)
--spawn type,x,z,meshlook0..7[,skinlook0..7]   (repetível; monstros: type=2 or01 etc.)
--preset N    cicla presets das 4 classes × armas (tecla P in-game)
```
Presets (descobrir valores bons visualmente; registrar no §7): Transknight/Foema/
BeastMaster/Hunter × weapon 0..N. NPCs: humano estático (STAND01) + orc patrulhando
quadrado de 4 células (demo de rota de monstro: mesmo Character com type 2,
`dwAniTable` direto).

---

## 6. Testes

| Teste | Conteúdo |
|---|---|
| `test_anisound` | parse AniSound4.txt real: `[Knight] 0`, STAND01=(0,20)×4; 60 tipos lidos |
| `test_charani` | ch01: nº de cuts == arquivos existentes em ValidIndex; `numAniCut` bate com header de cada .ani; `animMap[0][0..3]` = valores esperados; amostragem slerp no tick 0 == matriz do arquivo |
| `test_look` | regra base + **cada linha** da tabela de exceções; ausência → fallback |
| `test_route` | máscara sintética: reto, diagonal, bloqueado (para antes), alvo inalcançável (target corrigido); conversão célula↔mundo |
| `test_character` | headless: SetAnimation muda cut/loop; FrameMove avança 1 célula em dwUnitTime; altura segue máscara; chega ao alvo e para (STAND) |
| glsmoke | inalterado |

CI: os testes de assets continuam condicionados a `TM_REPO_ROOT` (pulam na CI).

---

## 7. Achados / notas de implementação

- **ch01.bon = 47 ossos** (376B/8) — `MAX_BONES` subiu para **64** (`skin.vert`,
  UBO, `ParseMsh`, `GLSkinMesh`). or01=35, hs01=37.
- **AniSound4.txt tem 51 seções** (não 60): tipos 13-19, 52, 58 ausentes; o tipo
  59 tem nome em EUC-KR (bytes não-ASCII). Humanos (0/1) têm 4 colunas de classe;
  monstros uma coluna `ani speed sound`.
- `m_dwFPS = dwSpeed[motion]` direto (Knight STAND/WALK = 20/13). Casos especiais
  por tipo (20/24 ajustes de FPS/altura) ficaram fora — ver TMHuman.cpp:5867-6200.
- **Crossfade de cuts = 10 sub-steps** (10×fps ms): humanos slerp de quat + lerp de
  translação (`_41.._43`), monstros lerp de matriz; pose antiga congela no tick do
  switch (`m_dwTickLast`). Sem sub-step smoothing durante o crossfade (comportamento
  do original — TMSkinMesh.cpp:530-580).
- Sub-steps do tick: pesos fixos (3:1, 1:1, 1:3); wrap do "next" no EndEdge
  (4×dwMod−3) aponta para o tick 0 do cut.
- Quirk `boneAniIndex==49` (KONKONG): dwMod−=2 — flag `dwModMinus2` no loader.
- `SetMotion` para o cut já corrente = **no-op sucesso** (o original retorna 0 do
  SetAnimation mas mantém a animação; tratado no Character).
- **Monstros com look 0 têm partes ausentes** (or01: só partes 1-2 existem em disco;
  or0103xx+ não existem neste build) — partes faltantes são puladas com warn (o
  original não checa). Visual do orc validado (guerreiro tigre corcunda = idle certo).
- Armas de humanos são partes skinned 6/7 (Right/Left): look 0 = desarmado
  (ch010701.msh não existe) — `--weapon N` troca o CONJUNTO de animações; a malha da
  arma vem do look (Mesh6/Mesh7).
- `m_sAnimationArray` = [6 classes][60 weapons][56 motions]; fallbacks portados:
  TK-BM 4→9 e 25→29 fill-forward, weapons ≥12 herdam die/dead/levelup do weapon 11,
  nArrayIndex 138 (ch01) / 137 (ch02) copiam a linha weapon-0 sobre tudo.
- `CharacterPose` é por instância (matRot/combined); `CharacterAnimation` é
  compartilhada por tipo via `CharacterAnimationCache`.
- `SkinPipeline` (shader skin + UBO BonePalette binding 1) extraído do TreeRenderer;
  TreeRenderer e CharacterMesh compartilham o mesmo caminho de draw.
- **Correção do roadmap**: não existe "blob shadow" (`mesh\sh.msh`) neste client —
  `sh01` (BoneAniIndex 60) é legado sem uso; `m_pShadow` do TMHuman é billboard
  vermelho de stealth. Sombra de personagem = ausente no original.

---

## 8. Arquivos novos / alterados

**Novos**
- `src/world/CharacterAnimation.h/.cpp` — loader multi-.ani + cuts + animMap + quats
- `src/world/AniSound.h/.cpp` — parser AniSound4.txt → tabelas
- `src/world/LookResolver.h/.cpp` — regra + exceções (função pura)
- `src/world/CharacterMesh.h/.cpp` — 8 partes, UBO compartilhado, slerp/lerp
- `src/world/Character.h/.cpp` — runtime (rota/altura/ângulo/animação/render)
- `src/world/Route.h/.cpp` — BASE_GetRoute port
- `tests/test_anisound.cpp`, `test_charani.cpp`, `test_look.cpp`, `test_route.cpp`,
  `test_character.cpp`

**Alterados**
- `src/gl/shaders/skin.vert` + `src/gl/GLSkinMesh.*` + `src/world/SkinMesh.cpp` —
  MAX_BONES 64; checagem numPalette
- `src/world/BoneAnimation.h/.cpp` — expor loader p/ reuso; quats
- `src/scene/FieldView.h/.cpp` — spawn char/NPCs, input move/anim, câmera follow,
  CLI `--char/--spawn/--preset/--follow`
- `src/TMMath.h` — quat (from-matrix, slerp, normalize) se ainda não existir
- `src/main.cpp` — flags novas
- `tests/CMakeLists.txt`, `migration-opengl/13-roadmap.md`, `migration-opengl/README.md`

---

## 9. Ordem de execução (TODO)

1. **MAX_BONES 64** + ParseMsh relaxado (desbloqueia tudo; 30 min)
2. `AniSound` parser + teste (tabela de moções pronta cedo)
3. `CharacterAnimation` loader (multi-.ani + cuts + animMap + quats) + teste c/ ch01 real
4. `LookResolver` + testes de exceções
5. `CharacterMesh` (8 partes, 1 UBO) + render headless screenshot (char parado T-pose→idle)
6. `Route` port + testes
7. `Character` runtime (walk/run/altura/ângulo) + teste headless
8. FieldView: char próprio + click-to-move + câmera follow + teclas demo
9. `--spawn` monstros/NPC + presets 4 classes (tecla P)
10. Validação visual (screenshots idle/walk/atk humano+orc), ajustes de FPS/altura
11. Docs (13/README/este DoD), CI, PR

**Critério de aceite final**: `./build/TMProject --map Field0101 --follow` abre com o
char na praça de Armia; click-direito anda (contornando terreno alto), R corre,
1-6 atacam, P troca preset; orc patrulha; `./build/TMProject --map Field0101 --spawn
2,140,180 --shot f3.bmp --frames 80` gera screenshot com orc idle; CI verde 3 OS.
