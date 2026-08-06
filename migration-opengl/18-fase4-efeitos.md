# 18 — Fase 4: Efeitos e ambiente vivo (plano completo) — **CONCLUÍDA**

**Objetivo**: o `EffectRenderer` (batch de quads CPU-simulados, fiel ao
`TMEffectBillBoard`) + os efeitos **visíveis no viewer sem combate**: **glows de
lâmpada** (objetos 501-505 do `.dat` — flipbooks de fogo + halos), **sol com lens
flare** (TMSun, screen-space), **chuva/neve** (TMRain/TMSnow ao redor da câmera),
**criaturas ambiente skinned** (folhas caindo 311-322, borboletas 343/4/6/7,
peixes 12/344 — reusam o pipeline da Fase 3) e **highlight de mouse-over** nos
personagens. Fica a fundação para as skills na Fase 5+.

**Pré-requisito**: Fase 3 merged (`CharacterAnimationCache`, `SkinPipeline`,
`LookResolver`, FieldView com spawn de chars).

**Estado final (DoD)**:
- [x] `EffectRenderer`: batch de quads por (textura, blend) preservando ordem;
      blends EF_BRIGHT/EF_DEFAULT/SRCCOLOR-INVSRCOLOR (flare); estados do original
      (sem zwrite/fog/luz, cull off). **Estado GL direto, fora do GLStateCache**
      (ver §7 — bug do driver)
- [x] Sim de billboard bit-fiel (fades 0-4, motions 0-3, scaleVel, flipbook,
      LookCam/axis, StickGround, lifeTime) — 9 golden tests
- [x] Lâmpadas 501-505 (tabela §4, função pura `BuildLampGlow`); validado:
      fogo amarelo (501, Field1616), ghost-fire azul (503, Field0101), halo
- [x] `TMSun`: 12 flares screen-space, direção fixa (-1, 0.7·defSize, 0.3),
      hide por clima; validado no clima 0 (anel + ghosts)
- [x] `TMRain` (50) / `TMSnow` (2×200) seguindo câmera/foco; validado climas 2/3
      (incl. quirks: fade int8 do floco, respawn; desvio: seed inicial do snow
      ao redor do foco — o original converge em ~20s)
- [x] Critters: folhas/arbustos (61, fade por distância² 20-28u + uAlphaMul),
      borboletas (69/24, 3 motions), peixes (70, 3 motions + fase por peixe) —
      spawn do `.dat` (5 por record, offsets rand) + wander paramétrico
- [x] Mouse-over highlight (emissive verde via `uEmissiveAdd`) + pick por
      cilindro (`m_vecPickSize[100]` portada p/ `PickSizeTable.h`); validado
      (orc verde no hover)
- [x] 6 suítes novas (fxbillboard/fxlamp/sunflare/weatherfx/critter/charpick);
      **22/22 verdes**; `Projects/` intocado
- [ ] **KNOWN ISSUE**: peixes (fs01) renderizam deformados (CPU da simulação
      validado por teste — posição/pose sãs; defeito é GPU-side sutil, a isolar).
      Butterflies/leaves OK. Follow-up na Fase 5.

**Duração estimada**: 2 semanas (10 dias úteis).

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original |
|---|---|
| EffectRenderer + descritor de billboard | `TMEffectBillBoard.cpp` todo |
| Glows 501-505 (estáticos, do `.dat`) | `TMObjectContainer.cpp:405-530` |
| Sol/lens flare | `TMSun.cpp` (172 linhas) + `TMSky.cpp:235,323,417,467` |
| Chuva/neve | `TMRain.cpp`, `TMSnow.cpp` (~200 linhas cada) |
| Folhas/borboletas/peixes | `TMLeaf.cpp`, `TMButterFly.cpp`, `TMFish.cpp`, spawn `TMObjectContainer.cpp:140-310` |
| Mouse-over highlight + pick box | `TMFieldScene.cpp:6196`, `TMHuman.cpp:59` (m_vecPickSize) |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| Skills/combate (`TMSkill*`, MeteorStorm, Charge, MagicShield) | Precisam de alvo/dano/rede | Fase 5+ |
| **TMEffectSWSwing** (trail de arma) | O mais difícil do repo (850 linhas, ribbon 32v + 48 matrizes); sem combate não há gatilho | Fase 5 (stretch aqui se sobrar) |
| Sanc/legend glow (`TMHuman::RenderEffect`) | Billboards ancorados em ossos + regra por upgrade; cosmético de endgame | Fase 5 |
| `TMShade` (decal conformado ao terreno) / `TMEffectBillBoard2` | Usado por setas/cannons/stealth | Fase 5 (com GroundDecalRenderer) |
| Montarias (hs01) / mantuas (tipo 85) | Já carregáveis pela Fase 3; faltam regras de look | Fase 5 |
| `TMFont3` (números de dano), labels, chat bubbles | UI | Fase 5 |
| Pappus/fireflies ao redor do player (FieldScene ambiente por região) | Preso a regiões específicas + clima | Fase 5 |
| Áudio (dwSoundTable, sons de clima) | Sistema de som | Fase 6 |
| Beams (`TMEffectSpark/BillBoard3`), `TMEffectSkinMesh` skills | Casos de skill | Fase 5 |

---

## 2. EffectRenderer (fundação)

Novo `src/gl/EffectRenderer.{h,cpp}` — dono do shader `fx_quad` e do batching.
**Simulação na CPU** (idêntica ao original), GPU só desenha:

```cpp
struct FxQuad {              // uma partícula/billboard num frame
    D3DXMATRIX world;        // saída da sim (rot*scale*trans do original)
    float u0, v0, u1, v1;    // geralmente 0..1 (tex 33 inverte — ver §3)
    uint32_t color;          // BGRA (SetColor + fades)
    int textureIndex;        // EffectTextureList index (já com cycle offset)
    int blend;               // 0 = EF_DEFAULT, 1 = EF_BRIGHT
};
class EffectRenderer {
    bool Init(std::string* err);
    void Begin(GLRenderDevice& device);           // estados + shader (1x/frame)
    void Emit(const FxQuad& q);                   // acumula
    void Flush(GLRenderDevice& device, GLTextureManager& tex); // batches ordenados
};
```

- Batch = mapa ordenado por (textureIndex, blend) **preservando a ordem de emissão**
  entre grupos diferentes (flush por run, não sort global — composição importa).
- VBO dinâmico (orphaning), 4 verts × quad (pos/color/uv), `TRIANGLEFAN` → viram
  2 triângulos indexados. Sem instancing na v1 (volumes: lâmpadas ~dezenas, chuva
  50, neve 200 — batch simples resolve; instancing fica p/ Fase 5 se precisar).
- Estados (TMEffectBillBoard::Render): `blend on`, src=SRCALPHA,
  dst = ONE (EF_BRIGHT) ou INVSRCALPHA; `depthTest on, depthWrite OFF`;
  `cull OFF`; alphaTest OFF; fog/light NÃO se aplicam (shader `fx_quad` sem fog).
- Distância de corte do original: 45u (billboards), 900u (folhas). Manter por tipo.

Shader `fx_quad.vert/frag` (novos): pos/color/uv, world por uniform-vertex (CPU já
entrega mundo), `FixZ` do common.glsl. Frag: `tex × color` (ALPHAOP MODULATE p/
EF_BRIGHT é o D3DTSS_ALPHAOP=2/4 — cor já vem modulada da CPU; frag = tex*color).

## 3. Sim de billboard (port fiel)

`src/world/Billboard.h/.cpp` — struct + `FrameMove` + geração de quad, sem GL
(testável headless). Portar de `TMEffectBillBoard.cpp:190-560`:

```cpp
struct BillboardDesc {   // ctor TMEffectBillBoard(tex, life, sx, sy, sz, vel, cycleN, cycleT)
    int textureIndex; uint32_t lifeTimeMs;
    float scaleX, scaleY, scaleZ, scaleVelX = 0, scaleVelY = 0, scaleVelZ = 0;
    int cycleCount, cycleTimeMs;      // flipbook: cycleIndex = t % (ct*n) / ct
    int fade = 0;                     // 0..4
    int motion = 0;                   // m_nParticleType-1: 0..3
    int lookCam = 1;                  // 1 = vira p/ câmera; 0 = eixo fixo (m_fAxisAngle)
    int stickGround = 0;
    int animationType = 0;            // 0..3 pulsações especiais (ver fonte)
    float axisAngle = 0;
    float particleH = 0, particleV = 0, circleSpeed = 0;
    float x, y, z;
    uint32_t color;
};
struct Billboard {  // estado vivo
    BillboardDesc d; uint32_t createMs; float progress; int cycleIndex; bool dead;
    uint32_t curColor; D3DXMATRIX world;   // saída por frame
};
void BillboardFrameMove(Billboard& b, uint32_t nowMs, float camYaw, float camPitch);
```

Fórmulas exatas (do fonte):
- `cycleIndex = (t % (cycleTime*cycleCount)) / cycleTime` (guard dwMod==0→1);
  textura do frame = `textureIndex + cycleIndex`.
- `progress = t / lifeTimeMs` (0 se lifeTime==0 → estático infinito); morre quando
  `t >= lifeTime` (lifeTime>0).
- Fades: 1 = RGBA×sin(progress·π); 2 = base + (1-base)·|sin(2π·((t+200·(this%100))/3000))|
  (fase por ponteiro — nosso: fase por índice do objeto); 3 = só A×sin(progress·π);
  4 = rampa: progress<0.3 → ×3.33, senão 1−(progress−0.3)×1.428 (só alpha).
- Motions (m_nParticleType−1): 0 = sobe V·progress; 1 = +x sin(progress·π·circleSpeed)·H;
  2 = círculo (sin/cos)·H; 3 = espiral (x sin, z progress·H). Posição parte de
  vecStartPos.
- Scale: `scale += t×scaleVel` por eixo (só com lifeTime>0); animationType 1..3 =
  pulsos sin 2000/4000ms com cor cinza embutida (portar; usado por glows de NPC).
- Orientação LookCam: `YPR(π/2 − camYawH, −camPitchV, axisAngle) × Scale × Trans`
  (m_matEffect do original); StickGround soma scaleY/2 no y. `lookCam=0` → sem
  rotação de câmera (só axisAngle em Z... ver fonte linha 540+ — portar o else).
- Quad unitário: 4 verts ±0.5 (UV 0..1; textura 33 inverte V — hardcode do ctor).
- Cor BGRA no vértice (D3D diffuse); nosso FxQuad carrega o mesmo BGRA e o shader
  converte p/ float4 no formato certo (atenção à ordem de bytes, como no terrain).

## 4. Lâmpadas/tochas (501-505)

Spawn no load do `.dat` (TMObjectContainer.cpp:405-530). Tabela (lifeTime=0 =
infinito, cycleTime=80):

| Tipo | Efeito 1 | Efeito 2 (halo, EF_BRIGHT) | Efeito 3 |
|---|---|---|---|
| 501 | tex 11 (fogo), cycle 8, cor 0xEEEECC00, fade 0 | tex 2, ×2.8 scale, 0x55553300, cycle 1 | — |
| 502 | tex 61, cycle 6, fade 0 | tex 2, ×2.8, 0x55553300 | — |
| 503 | tex 101, cycle 8, 0xFF5500FF | tex 101 ×0.5, 0xFFFFFFFF, y−0.2·scale | tex 2, ×2.8, 0xFF330055 |
| 504 | tex 56, fade **2**, 0xFFFF0000 | — | — |
| 505 | tex 79, **lookCam=0, axisAngle=fAngle**, 0x33330000 | — | — |

Posição = offsetGround + (posX, fHeight, posY) do record. Sem tint extra (o tint do
terreno 501-503 já foi feito na Fase 2). Ciclo de vida: vivem para sempre; o
`m_bFrameMove` delas fica ligado (chama FrameMove todo frame p/ flipbook/fade 2).

## 5. Sol + lens flare (TMSun)

`src/world/SunFlare.{h,cpp}` (screen-space, sem mundo 3D):
- Projeção: `vecCam + lightDir` projetado por view×proj (D3DXVec3Project → nosso
  TMMath; se z>1 → atrás, não desenha). `lightDir` = direção da luz 0 atual.
- 12 flares fixos (tabela TMSun::InitObject: tex 206-213, fLoc −0.6..+0.6,
  fScale × (widthRatio×50), cores 0xAA....): posição na tela = centro + dir·fLoc·(meia
  tela); quads 2D (ortho) EF_BRIGHT. `m_fDefSize` escala global (TMSky dirige nas
  transições de clima; no viewer: 1.0 no clima 0, escondido nos demais).
- Hide rule (TMSky.cpp:237): `m_bHide = state && state < 10` → visível no sol (0)
  e noites (10+)? Portar: visível só no clima 0 (noites sem sol no viewer).
- Shader: mesmo `fx_quad` com flag screen-space (ortho) — bit do iFlags da doc 08;
  v1 pode simplesmente chamar um segundo caminho do EffectRenderer (`EmitScreen`).

## 6. Chuva e neve

`src/world/WeatherFx.{h,cpp}`:
- **TMRain**: 50 quads pequenos (0.008×0.2), cor 0x33333333, EF_DEFAULT; posições
  rand numa caixa 12×12×10 ao redor da câmera; caem com m_fSpeed[i]
  (0.08·(rand%3), 0→0.24); wrap no topo. Segue a câmera (posições relativas).
- **TMSnow**: 2 instâncias (scale 1.0 e 2.0), 200 quads total; flocos tex (ver
  ctor TMSnow.cpp), queda lenta com sway sin; mesma mecânica de caixa+wrap.
- Ativação: clima 2 → rain, 3 → snow (o viewer já tem `--weather`; select-server
  default pode cair em 3). Sem som.
- Render: pelo EffectRenderer (quads world-space, câmera-facing).

## 7. Critters (folhas/borboletas/peixes)

Reuso total da Fase 3 (`CharacterAnimationCache` + `SkinPipeline`). Spawn no load
do `.dat` (hoje pulados em FieldView.cpp:119-160):

| dwObjType | Classe | nBD/subtipo | SkinMeshType | Regras |
|---|---|---|---|---|
| 311-322 | TMLeaf | — | **61** (lf01) | 1 por record; Mesh0 = 2 se t−311≥6 senão 0; Skin0 = t−57 (≥6) ou t−55; **override Skin0=9** na região de neve (tile 26-30, 20-24); FPS 80 |
| 343 | TMButterFly | 0 | **69** | 5 por record, offsets rand(5)×0.1, altura rand(10)×0.2+fHeight |
| 4 | TMButterFly | 1 | 69 | idem; Skin0 = rand%2+3; scale 0.69; FPS 10 |
| 6 | TMButterFly | 3 | 69 | ParticleH/V ×0.5, circleSpeed = nFly+8; FPS 8 |
| 7 | TMButterFly | 2 | **24** (bd01) | scale 0.2, FPS 4, startTime = 200×nFly, H/V=5 |
| 344 | TMFish | 0 | **70** | 5 por record, offsets rand×0.05 |
| 12 | TMFish | 3 | 70 | idem |

- Wander (FrameMove de cada classe): borboleta/peixe = sin/cos paramétrico
  (m_fParticleH/V/CircleSpeed) ao redor do ponto; folha = queda lenta + sway,
  respawn no topo (ver TMLeaf.cpp:100-225 — portar as fórmulas exatas).
- Escala do skinmesh por subtipo (tabela acima); tint de terreno: TMLeaf aplica
  GroundGetColor (0 no nosso caso — preto? ver TMLeaf::Render: zera RGB e usa
  alpha do terreno... portar como no fonte: cor = (0,0,0,alphaDoTerreno)).
- Culling de distância do original: folhas >900u, borboletas têm o seu próprio.
- Novo `src/world/Critter.h/.cpp`: struct com tipo/subtipo/posição/fase + FrameMove
  + Render via `CharacterMesh` (cada critter = CharacterMesh com 1 parte; o cache
  já carrega tipos 24/61/69/70 sob demanda). Sem AniSound (cut fixo 0; FPS da tabela).

## 8. Mouse-over highlight + pick de personagem

- Pick: raio da câmera (já temos unproject do click) vs **caixa** por personagem:
  `m_vecPickSize[type]` = (raio, altura) — portar a tabela de 100 entradas
  (TMHuman.cpp:59) p/ `Character`. Teste raio×AABB vertical centrada em (x,z),
  altura [h, h+pickY]; pega o mais próximo.
- Highlight: o original pinta o material emissive verde (0x8800FF00,
  TMFieldScene.cpp:6196 + MouseMove checa `m_materials.Emissive`). Nosso skin.frag
  já tem `uEmissive` (Fase 2, piso 0.3). Adicionar `SetHighlight(bool)` no
  Character → emissive verde no render do char hovered.
- Clique direito continua andando; hover só pinta (sem seleção/ação).

## 9. Testes

| Teste | Conteúdo |
|---|---|
| `test_fxbillboard` | Golden das fórmulas: cycleIndex em t=0/79/80/160; fades 1-4 em progress conhecidos; motions 0-3; scaleVel; lifetime/dead; StickGround offset |
| `test_fxlamp` | Records sintéticos 501-505 → descritores certos (tex/cor/blend/scale; 505 axisAngle=fAngle, lookCam=0) |
| `test_sunflare` | Tabela de 12 flares bate com TMSun::InitObject; hide por clima; projeção atrás da câmera (z>1 → skip) |
| `test_weatherfx` | Rain: 50 partículas, wrap, caixa ao redor da câmera; Snow: 2 sistemas/200 quads |
| `test_critter` | Spawn rules 311-322 (Mesh0/Skin0 + override 9), 5× borboletas/peixes, tipos de skinmesh (61/69/24/70), FPS/escala por nBD |
| `test_charpick` | AABB pick: acerto no centro, miss fora do raio, mais próximo ganha |

Sem GL nova obrigatória p/ testar (sim é CPU; EffectRenderer só batcha).

## 10. Ordem de execução (TODO)

1. `fx_quad` shader + `EffectRenderer` (batch/flush/estados) — fundação
2. `Billboard` sim + testes golden (§3)
3. Lamp glows 501-505 no FieldView + screenshot Field0101 (tochas acesas)
4. `SunFlare` + screenshot clima 0 (sol + flares)
5. `WeatherFx` (rain/snow) + screenshots climas 2/3
6. `Critter` + spawn rules + wander + screenshots (Field2723 folhas / Field0101 borboletas / Field1616 peixes)
7. Pick box + highlight + screenshot hover
8. Docs (13/README/este DoD §11), CI, PR

**Critério de aceite final**: Field0101 à noite com tochas acesas e halos; sol com
flare no clima 0; chuva/neve ativos nos climas 2/3; folhas caindo em Field2723;
borboletas voando; hover pinta o orc de verde; 21+ suítes verdes.

## 11. Achados / notas de implementação

- **BUG MAIS CARO DA FASE**: o `EffectRenderer::Flush` original usava o
  `GLStateCache` compartilhado — o replay de binds de textura/sampler pelo cache
  corrompia o render do skin pipeline (chars/árvores full-fog azuis) no Metal.
  Sintoma enganoso: tudo azul (cor do fog). Bisect com env-flags (TM_NOFX,
  TM_FX_NODRAW/EARLY/EARLY2/NOAPPLY) isolou o `st.Apply()` do flush. **Fix**:
  o flush usa estado GL direto (sem cache) + `Invalidate()` ao final; documenta
  o padrão p/ renderers futuros: fora do cache, invalidar ao sair.
- **Não existe círculo de seleção** neste client: mouse-over = emissive verde no
  material (TMFieldScene.cpp:6196); o "mouserUnsel/Sel" do doc 12 é de outra
  versão.
- Billboard flipbook = índice consecutivo na EffectTextureList; UV inset
  0.02..0.98 (evitar bleeding); textura 33 inverte V (hardcode do ctor).
- Fade 2 usa `this % 100` como fase (ponteiro!) — substituído por índice estável.
- Sun flare usa direção FIXA (-1, 0.7·defSize, 0.3), não a direção da luz;
  blend SRCCOLOR/INVSRCOLOR (diferente dos demais efeitos); 12 flares com
  tabela tex 206-213 portada verbatim.
- Snow: fade por altura com quirk de wrap int8 ((char)((y-camH)*24), clamp -86);
  posições ABSOLUTAS com caixa inicial na origem do mapa — semeamos ao redor do
  foco no primeiro FrameMove (o original converge em ~20s; desvio documentado).
- Rain: streaks 0.016×0.4, só xz dos eixos da view (ficam verticais); splash
  (TMEffectBillBoard2) ficou para Fase 5.
- **TMLeaf não é folha caindo**: é foliage estática (moitas/flores/capim) com
  fade por distância² (20-28u, cull 30u) e tint do terreno. Skin0 truncado p/
  **unsigned char** pelo container (312-55 = 257 → 1!) — quirk portado.
- Borboletas: tipos 69 (bf01)/24 (bd01 passaro, dwObjType 7); 5 por record;
  offsets rand(5)×0.1; progress |sin| 20s; motion 3 = círculo 7s.
- Peixes: fase individual = `H*12 + speed*12 + 20*(this%100) + 10*Mesh0 +
  scale*10 + 5*motionType` (ponteiro substituído por 0 — fase ainda varia por
  H/speed/look randômicos); wander ±H*0.4/0.3.
- `GetEffectTexture` lazy-load funciona p/ chamar durante o frame (Metal ok
  depois do Invalidate).
- Known issue: fs01 (peixe) renderiza deformado — sim/posição validadas por
  teste; defeito GPU-side a isolar na Fase 5.

## 12. Arquivos novos / alterados

**Novos**
- `src/gl/EffectRenderer.{h,cpp}` + `src/gl/shaders/fx_quad.{vert,frag}`
- `src/world/Billboard.{h,cpp}` (sim), `src/world/SunFlare.{h,cpp}`,
  `src/world/WeatherFx.{h,cpp}`, `src/world/Critter.{h,cpp}`
- `tests/test_fxbillboard.cpp`, `test_fxlamp.cpp`, `test_sunflare.cpp`,
  `test_weatherfx.cpp`, `test_critter.cpp`, `test_charpick.cpp`

**Alterados**
- `src/scene/FieldView.{h,cpp}` — spawn de lamp glows + critters do `.dat`,
  render pass de efeitos (após sky, antes/depois chars: ordem = opacos → céu?
  verificar ordem do original: efeitos por último, sem zwrite)
- `src/platform/main.cpp` — hover/click pick de char, ativação rain/snow por clima
- `src/world/Character.{h,cpp}` — pick box + SetHighlight
- `src/gl/shaders/skin.frag` — emissive override para highlight (se necessário)
- `tests/CMakeLists.txt`, `migration-opengl/13-roadmap.md`, `migration-opengl/README.md`
