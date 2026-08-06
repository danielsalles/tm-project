# 07 — Cena: Terreno, Céu, Sol, Mar, Sombras, Picking

## 7.1 Terreno (`TMGround`) — o maior draw-call offender

**Hoje**: grid 64×64 tiles por ground (arquivo `.trn`, 4096 registros de 12B,
`TMGround.cpp:2467-2683`); tile = 2×2 unidades, altura ×0.1; janela de ~31×31 tiles ao
redor da câmera; LOD por paridade (1-em-2 além de 16 tiles, `nTickX/nTickY`,
`TMGround.cpp:2835-2875`); culling CPU por canto com margem 50px (`:3218-3246`);
**1 `DrawPrimitiveUP(TRIANGLESTRIP, 2 tris)` por tile visível** (`:3248`) → centenas de
draw calls por frame. Tiles especiais: lava/água animada (170/171, 38/39, 62-65) com UV
scroll por tempo + spawn de billboards (`:2973-3122`). Vertex color `dwColor` baked por
canto + luzes estáticas carimbadas (`TMLight::InitPosition`).

**GL 4.1** (mesma semântica, outro mecanismo):

```cpp
struct GLGround {
    GLuint vao, vbo;      // 65×65 vértices estáticos (pos, normal, color, uv0, uv1) — VAO_LN2
    GLuint ebo;           // reconstruído por frame: só tiles visíveis, com step do LOD
    D3DXMATRIX world;     // Scaling(2,0.1,2)*Translation — idêntico (TMGround.cpp:2711)
};
```

- **VBO estático** por ground: posições/cores/normais dos cantos (mudam raramente —
  `SetColor` de luz dinâmica faz `glBufferSubData` no range do tile, não rebuild).
- **IBO por frame**: a mesma janela de culling de hoje decide quais tiles entram;
  índices dos tiles visíveis são emitidos num EBO dinâmico (orphan + invalidate), respeitando
  o LOD de paridade (step 2 gera 1 quad por 2×2 tiles — mesma regra, índices diferentes).
  1-2 draw calls por ground vs. centenas.
- **Por que manter janela+LOD+mask**: o picking/colisão usa a mask 128×128 (inclui os 127
  sintéticos de borda, `TMGround.cpp:2605-2675`) e o LOD por paridade afeta o que se vê.
  Trocar por frustum culling/LOD "corretos" **muda onde o jogador clica e anda**. Fase 1 =
  mesmas decisões, só o draw muda. (Fase 2 pode modernizar com validação.)
- Tiles animados: uniform `uTime` no shader de terreno + variante `COMBINE_ADD` para lava;
  os billboards de brasa continuam sendo spawnados pelo gameplay (inalterado).
- Shader: `terrain.vert/frag` — 2 samplers (tex0 tile base, tex1 blend), combiner
  MODULATE2X opcional, vertex color, fog.

## 7.2 Céu (`TMSky`)

- Dome = common mesh tipo 1 (`TMSky.cpp:190`) na posição XZ da câmera. Render: lighting off,
  fog off, `ADDSIGNED` (tex + diff − 0.5) para tingir, crossfade de clima com stage1
  `BLENDDIFFUSEALPHA` (LERP) entre texturas 67-70 (`:179-186`).
- Cores por vértice reescritas por frame no FrameMove (`:546-547`) → VBO dinâmico pequeno
  (dome tem poucos vértices) ou mover o lerp para uniforms de paleta no FS (fase 2).
- Estrelas/luas: 20+2 billboards (`:204-212`) → vão para o batcher de efeitos.
- `FrameMove` do céu comanda: clear color, fog start/end, cores das luzes, `g_ClipFar`,
  dia/noite das point lights (`TMSky.cpp:257-444`) — **lógica 100% preservada**; só os
  destinos mudam (uniforms em vez de render states).
- Shader: `sky.frag` com uniforms `uTexA, uTexB, uCrossfade, uTint` — 1 programa cobre
  todos os estados de clima (a máquina de estados `m_nState` 0/1/2/3/10/13 fica na CPU).

## 7.3 Sol (`TMSun`) — lens flare

- 12 sprites ao longo do vetor flare→centro (`TMSun.cpp:24-75`), projeção via
  `D3DXVec3Project` da direção do sol (`:99-125`), aborta se `z>1` (preservado pela
  convenção LH+ZO, 04 §4.1).
- Blend `SRCBLEND=SRCCOLOR(3) / DESTBLEND=DESTALPHA(7)` (`:131`): com backbuffer RGBA8 e
  clear alpha=1.0, `GL_SRC_COLOR/GL_DST_ALPHA` reproduz (dstA=1 → resultado dst + src·srcRGB).
  **Atenção**: no D3D o backbuffer X8 faz dstA ler como 1 — o GL precisa do alpha no
  framebuffer e do clear correto, senão o flare some (ou some demais).
- Quads screen-space: vão para o batcher 2D (mesmo path da UI, z fixo 0.8−i·0.04).

## 7.4 Mar (`TMSea`)

- Grid própria VB+IB (`TMSea.cpp:177-257`), FVF 578, animação **CPU por frame**:
  `position.y` senoidal + scroll de uv1/uv2 (`:286-345`) — lock do VB todo frame.
- GL: grid estática em VBO; **ondas e scroll no VS** (`pos.y = A*sin(uTime*f + phase(x,z))`,
  `uv += uScroll*uTime`) — elimina o lock. Fases idênticas às fórmulas atuais para paridade
  visual (a interação `IsInWater`/`GetWaterHeight` usa as mesmas fórmulas na CPU —
  `TMGround.cpp:4458-4502` — manter sincronizadas!).
- Variantes: normal (stage1 MODULATE2X, tex 3), dungeon (tex 8, sem stage1), pântano
  (blend INVSRCCOLOR/SRCCOLOR, tex 406) — 3 variantes de shader/estado; blend especial
  pântano = `GL_ONE_MINUS_SRC_COLOR/GL_SRC_COLOR`, existe em GL.
- Material com alpha 0.5-0.9 → uniform; Z write off; cull off; fog off (restaurar depois).

## 7.5 Sombras blob (`TMShade`)

- Grid (n+1)² **conformada ao heightmap** na CPU (`TMShade.cpp:106-176`), UV rotacionada,
  `DrawIndexedPrimitiveUP` (`:266-274`).
- Fase 1 (paridade): mesmo cálculo CPU em VBO dinâmico — sombras são poucas dezenas.
- Fase 2 (melhor): grid estática + heightmap como textura R16F; `pos.y = texture(uHeight, xz).r`
  no VS — zero CPU. Requer extrair o heightfield 128×128 do ground como textura por ground.

## 7.6 Clima (chuva/neve) e dust ambiente

- Hoje: 50 (chuva) / 200 (neve) quads = 1 draw cada (`TMRain.cpp:106-123`,
  `TMSnow.cpp:117-147`), billboard pelos eixos da view, cor por altura.
- GL: 1 draw instanciado por sistema (ver 08-efeitos.md — `fx_quad` com `uMode=weather`).
  Respeitar `g_pAttribute` (não cair em área coberta) na CPU como hoje.

## 7.7 Luzes dinâmicas (`TMLight`)

- Até 6 point lights por container, slots 2-7, `Range=5`, attenuation≈0 (`TMLight.cpp:17-36`).
- GL: array `uPointPos[6]/uPointColor[6]/uPointRange[6]` no UBO de frame; `uNumPointLights`
  por draw (ou por frame, dado o volume). FS: cutoff duro por range (sem atenuação real),
  igual ao D3D com att 0.001.
- Carimbo de vertex color do terreno (`GroundSetColor`, `TMLight.cpp:52-62`): inalterado —
  escreve no VBO do ground via `glBufferSubData`.
- Dia/noite liga/desliga via `TMSky::FrameMove` (`:516-541`) — lógica preservada.

## 7.8 Picking — zero mudanças

Mask 128×128, quads fixos de humanos, `D3DXIntersectTri`: tudo CPU sobre as matrizes do
shim. Com LH+ZO preservados, os resultados são bit-compatíveis (ver 04 §4.1, §4.6).

## 7.9 Streaming de grounds

`TMScene::FrameMove` (`TMScene.cpp:1373-1687`) faz attach/detach de vizinhos e costura de
bordas (`TMGround::Attach`, `TMGround.cpp:2385-2465`). Inalterado — só o objeto de render
do ground muda. Hitch de load do `.trn`: fase 2 → thread de IO + contexto GL compartilhado.
