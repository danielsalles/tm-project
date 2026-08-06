# 09 — UI e Fontes

## 9.1 UI: de draw-por-quad para batcher ordenado

**Hoje**: 30 camadas de `GeomControl` drenadas por `ObjectManager::RenderControl`
(`ObjectManager.cpp:591-626`); projeção perspectiva rasa (`SetMatrixForUI`, fov 0.1,
câmera (0,0,50), `RenderDevice.cpp:1854-1870`); **1 draw por quad** (DrawPrimitiveUP ou
1 `ID3DXSprite::Begin/Draw/End` por retângulo, `RenderDevice.cpp:1905-1907`); coordenadas
lógicas 800×600 escaladas (`m_fWidthRatio/m_fHeightRatio`, `RenderDevice.cpp:64-65`);
half-texel offsets manuais (a remover, 04 §4.4).

**GL 4.1 — UIBatcher**:

```cpp
struct UIQuad { float x, y, w, h; float u0, v0, u1, v1; uint32_t color; int texIndex; };
class UIBatcher {
    std::vector<UIQuad> queue;             // ordem de chegada = ordem de composição
    void Push(...);                        // chamado por RenderRectC/RenderRectTex/...
    void Flush();                          // fim de frame: agrupa runs por textura
    // 1 VBO dinâmico (orphan), 1 draw por run de textura; depth test OFF
};
```

- Projeção: **`glm::ortho(0, 800, 600, 0, -1, 1)`** lógica + escala por ratio — substitui
  o frustum perspectiva raso sem mudança visual. Y-flip na ortho (origem topo-esquerda
  como o D3D/Win32 espera).
- **Preservar a ordem das 30 camadas e a inserção** (inclui `SCursor` na camada 29,
  `SControlContainer.cpp:307-308`). Não reordenar por textura globalmente — agrupar só
  runs contíguos com mesma textura.
- `RenderRectTex2*` (2 texturas, FVF 580): variante de shader `ui_quad2` (2 samplers,
  MODULATE) — usado por minimap e fundos.
- `RenderRectProgress2` (barra radial, fan de até 10 vértices, `RenderDevice.cpp:2544`):
  converter fan→triangles no push (índices 0,1,2 / 0,2,3 / ...).
- `RenderGeomControlBG` (glow pulsante, `:3425`): TRIANGLESTRIP idem → triangles.
- **RENDER_3DOBJ** (ícones 3D de item/personagem na UI, `:3316-3402`): manter passe
  separado — flush da UI, draw 3D com `glDepthFunc(GL_ALWAYS)` + state block 1, retomar UI.
  Hoje usa `ZFUNC=ALWAYS`+restaura — equivalente exato.
- Guild marks 16×12 (`:3166-3230`): texturas dinâmicas pequenas, mesmo batcher.

## 9.2 Fontes: TMFont2 (GDI) → stb_truetype

**Hoje** (`TMFont2.cpp:28-271`): cada `SetText` rasteriza a string via GDI `TextOut` num
DIB 32bpp (Tahoma 12px), converte luminância→alpha para textura A4R4G4B4 512×64 por
instância; render = 2 quads por linha (sombra +1px + face), filtro POINT, quebra DBCS
byte-a-byte em ~42 bytes/linha, slash-zero opcional, medida via `GetTextExtentPoint32`.

**GL 4.1 — duas fases**:

### Fase 1 (paridade, menor risco): textura por string, CPU raster

Manter a arquitetura "1 string = 1 textura" (cache LRU por (string, cor, tamanho)):

```cpp
struct GLFont {
    stbtt_fontinfo font;              // Tahoma.ttf (ou NanumGothic p/ coreano) carregado do disco
    std::unordered_map<StringKey, StringTexture> cache;   // LRU ~256 entradas
};
// SetText: se não está no cache → rasteriza glifos com stbtt_GetCodepointBitmapBox /
// stbtt_MakeCodepointBitmap num buffer RGBA8 (branco + alpha), glTexImage2D, mede com
// stbtt_GetCodepointHMetrics (alimenta m_szStringSize para alinhamento da UI).
```

- Preserva: sombra +1px, slash-zero, quebra de linha (a lógica byte-a-byte DBCS em
  `TMFont2.cpp:67-116` vira UTF-8 aware — atenção: o jogo roda em codepage coreana/MBCS;
  decodificar CP949→UTF-8 na entrada do SetText).
- Métricas vão divergir do GDI em subpixel. Layouts da UI usam estimativas
  `6*nLength/7*nLength` (`RenderDevice.cpp:3302-3309`) — validar centralização visualmente.
- A4R4G4B4 → RGBA8 (conversão trivial; a textura é branca com alpha — usar `GL_R8` +
  swizzle no shader, ou RGBA8 para simplicidade).

### Fase 2 (modernização): atlas de glifos + batcher de texto

Atlas dinâmico de glifos (estilo ImGui: `stbtt_PackFontRange` ou cache sob demanda),
quads por glifo empilhados no UIBatcher. Elimina textura-por-string. Só depois da fase 1
validada — texto é o elemento mais visível da UI.

**TMFont3** (números de dano): atlas de dígitos já existente (UI sets 137-141,
`TMFont3.cpp:29-52`) → quads no batcher com animação por `fProgress` (inalterada).

## 9.3 Cursor

- Manter apenas o modo **software**: `SDL_HideCursor()` no boot + `SCursor` renderizado
  pelo batcher como hoje (camada 29, sempre por cima). O modo hardware (`.cur` resources,
  `NewApp.cpp:224-227`) morre — os `.cur` podem virar texturas PNG se quisermos o mesmo desenho.
- Arrastar item (cursor com ícone anexado, `SControl.cpp:488-518`): inalterado — é UI.

## 9.4 IME (input de texto coreano) — nota

Fora do renderer, mas acoplado à UI: `imm32` → `SDL_StartTextInput`/`SDL_SetTextInputRect`
(posição do candidato sobre a caixa de chat). Fase 2; no MVP, input ASCII funciona.
