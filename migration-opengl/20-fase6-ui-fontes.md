# 20 — Fase 6: UI e Fontes (plano completo)

**Objetivo**: toda a camada de interface do usuário que torna o jogo **jogável
end-to-end** — **UIBatcher** (batch de quads 2D com projeção ortográfica),
**GLFont** (stb_truetype substituindo GDI), **TMFont3** (números de dano
flutuante), **SControl tree** completa (SPanel/SButton/SText/SEditableText/
SListBox/SMessageBox/SCursor/SProgressBar/SScrollBar/SCheckBox/S3DObj),
**RenderGeomControl** (dispatch central de UI), **UI textures** (carregamento
de 512 texturas + 600 ControlTextureSets), **cursor software**, **guild marks**
e **minimap**. O ponto de estrangulamento é `RenderGeomControl` — uma única
função que desenha toda a UI; o ponto de acoplamento é `SControlContainer`
(gerenciador raiz da árvore de controles + pipeline de input).

> **Referência**: doc 09 (`09-ui-fontes.md`) definiu a arquitetura alvo
> (UIBatcher, stb_truetype fase 1/2, cursor software). Este doc 20 expande
> para implementation completa com todas as 16 sub-fases, incluindo o
> port do SControl tree e o dispatch RenderGeomControl.

**Pré-requisito**: Fase 5 merged (`SkillEffect`, `EffectContainer`,
`EffectRenderer`, `Billboard`, `SkinPipeline`, `CharacterAnimation`,
`GLRenderDevice` com state blocks 0/1, `GLTextureManager` com model/env/
effect textures).

**Estado final (DoD)**:
- [x] `UIBatcher`: batch de quads 2D, VBO dinâmico, 1 draw por run de textura,
       projeção ortográfica `ortho(0, W, H, 0, -1, 1)`. Render state block 0.
- [x] `GLFont` (stb_truetype fase 1): textura por string, cache LRU 256,
       rasterização cp949→UTF-8, sombra +1px, alinhamento 6/7*nLength.
- [x] `TMFont3` (dano flutuante): types 0-6, glyphs texturas 137-141, animação
       float-up/fade/scale, `RenderRectTexDamage`.
- [x] `RenderRect*` (10 funções): RenderRectC, RenderRectTex, RenderRectNoTex,
       RenderRectCoord, RenderRectTexDamage, RenderRectRot, RenderRectTex2C,
       RenderRectTex2M, RenderRectProgress2, RenderRect — todas via UIBatcher.
- [x] `RenderGeomControl` + `RenderGeomRectImage`: dispatch por eRenderType,
       ControlTextureSet→textura, sanct/legend overlays, guild marks.
- [x] UI texture pipeline: `LoadUITexture`, `GetUITexture` (512), `GetUITextureSet`
       (600), lazy loading. Parsing de TextureSetList.txt + TextureListNList.bin.
- [x] `SControl` tree: base + SPanel/SButton/SText/SEditableText/SListBox/
       SMessageBox/SCursor/SProgressBar/SScrollBar/SCheckBox/SGridControl.
- [x] `SControlContainer`: árvore, FrameMove2→GeomControl pipeline, input dispatch,
       modal stack, focus control.
- [x] Input: SDL→SControl dispatch (mouse, keyboard, char events).
- [x] `SCursor` software (SPanel layer 29).
- [x] `SetMatrixForUI`: ortho projection no GLRenderDevice.
- [x] `glScissor` clipping para SPanel (via GLStateCache).
- [~] Guild marks 16×12: pipeline de render pronto, download deferido (Fase 7).
- [x] `UIBatcher::Flush` preserva ordem das 30 camadas (runs contíguas).
- [x] 1 suíte nova (test_ui, 20 subtestes); **27/27 ctest verdes**.
- [x] Docs 13/README atualizados; retrospectiva (§16) no doc 20.

**Duração estimada**: 3 semanas (15 dias úteis) — UI é o maior volume de código
novo (UIBatcher+GLFont+16 controles+dispatch+textures+input).

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original | Linhas |
|---|---|---|
| `UIBatcher` (batch de quads 2D) | Novo (doc 09 §9.1) | ~200 |
| `GLFont` (stb_truetype fase 1) | `TMFont2.cpp` (390 linhas GDI→texture) | ~300 |
| `TMFont3` (dano flutuante) | `TMFont3.cpp` (399 linhas) | ~250 |
| `RenderRect*` (10 funções primitivas) | `RenderDevice.cpp:1891-2775` (884 linhas) | ~400 |
| `RenderGeomControl` + `RenderGeomRectImage` | `RenderDevice.cpp:2831-3485` (654 linhas) | ~350 |
| UI texture pipeline (512 tex + 600 sets) | `TextureManager.cpp:218-484` | ~300 |
| `SControl` base + 13 subclasses | `SControl.h` (667) + `SControl.cpp` (2994) | ~2000 |
| `SControlContainer` (input + tree + modal) | `SControlContainer.cpp` (385) | ~300 |
| `SetMatrixForUI` (ortho projection) | `RenderDevice.cpp:1854-1870` | ~20 |
| `glScissor` clipping | Novo (no GLStateCache) | ~30 |
| Cursor software | `SCursor` via SPanel | ~50 |
| Guild marks (16×12 dinâmicas) | `RenderDevice.cpp:3166-3230` | ~80 |
| `UIBinary.h` (deserialização de .bin) | `UIBinary.h` | ~150 |
| `IEventListener` interface | `EventTranslator.h` | ~10 |
| SDL input → SControl dispatch | `EventTranslator.cpp` | ~150 |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| **GLFont fase 2** (atlas de glifos + batcher de texto) | Risco visual alto; fase 1 validada primeiro | Fase 8 (modernização) |
| **IME coreano** completo (imm32→SDL) | Input ASCII suficiente para MVP | Fase 7 |
| **Áudio** (DirectSound→SDL_audio/miniaudio) | Fora do escopo de UI | Fase 7 |
| **Vídeos de intro** (libmpv/cortar) | Fora do escopo de UI | Fase 7 |
| **WinInet→libcurl** (guild mark download, patch) | Fora do escopo de UI | Fase 7 |
| **Config de vídeo** (gamma, MSAA, aniso) | Fora do escopo de UI | Fase 7 |
| **Screenshot** | Fora do escopo de UI | Fase 7 |
| **RENDER_3DOBJ** (ícones 3D na UI) | Caso raro (items lendários); 95% da UI é 2D | Fase 7 se necessário |
| **S3DObj** completo ( modelo 3D na UI ) | Requer render 3D em contexto 2D; complexo | Fase 7 |
| **SReelPanel** (slot machine/gamble) | Feature específica de gambling | Fase 7 |
| **SListBoxBoardItem/PartyItem/ServerItem** | Subclasses especializadas | Fase 7 |

---

## 2. UIBatcher — fundação 2D

### Arquitetura

```cpp
// src/gl/UIBatcher.h
struct UIQuad {
    float x, y, w, h;       // posição/tamanho em pixels (screen-space)
    float u0, v0, u1, v1;   // UVs na textura
    uint32_t color;          // ARGB (0xAARRGGBB)
    int texIndex;            // UI texture index (-1 = sem textura, 0 = branca)
    int layer;               // camada 0-29 (preservada, não reordenada)
};
class UIBatcher {
    static constexpr int MAX_QUADS = 16384;
    std::vector<UIQuad> queue;
    GLuint vao = 0, vbo = 0;
    GLuint shader = 0;
    GLint locProj, locTex;
    int currentLayer = -1;

    bool Init(std::string* err);
    void Begin();                                          // limpa queue, layer = -1
    void Push(const UIQuad& q);                            // adiciona à queue
    void Flush(GLTextureManager& tex, GLRenderDevice& dev); // batch por run
    void Shutdown();
};
```

### Vertex format (compatível com RDTLVERTEX do original)

```
struct UIVertex {
    float x, y, z;     // posição (z = FixZ(0) para UI)
    float rhw;          // 1.0 (perspective divide dummy)
    uint32_t color;     // ARGB
    float u, v;         // UV
};
// FVF = 0x184 (POSITION|XYZRHW|DIFFUSE|TEX1) = 36 bytes
```

### Projeção ortográfica

```cpp
// GLRenderDevice — SetMatrixForUI()
void SetMatrixForUI(int screenW, int screenH) {
    // glm::ortho(0, W, H, 0, -1, 1) — origem topo-esquerda (como D3D/Win32)
    // Armazenado no UBO ou como uniform separado para o shader ui_quad
    matUIProjection = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
}
```

- Original usa `perspective(fov=0.1, aspect*0.94, near=10, far=100)` + câmera em z=50.
  Ortho é visualmente equivalente (fov raso ≈ ortho) e mais correto para GL.
- Y-flip: origem no topo (y=0 no topo da tela) como o D3D/Win32 espera.
- Escala: 800×600 virtual → tela real via `m_fWidthRatio`/`m_fHeightRatio` (já no SControl constructor).

### Flush — preservação de ordem

```cpp
void UIBatcher::Flush(GLTextureManager& tex, GLRenderDevice& dev) {
    // 1. Sort estável por (texIndex) APENAS entre runs contíguas com mesma layer
    //    NÃO reordenar globalmente — camadas 0-29 mantêm ordem
    // 2. Para cada run de mesma textura: bind texture, draw VBO slice
    // 3. State: block 0 (depth OFF, blend ON, cull OFF, alphaTest OFF)
    dev.SetRenderStateBlock(0);
    // ... bind shader, upload ortho matrix, draw runs
}
```

- O original itera `m_pDrawControl[0..29]`, cada layer é uma `stGeomList` (linked list).
  O `RenderGeomControl` é chamado por item. Nosso batcher acumula tudo e faz flush
  no fim do frame, preservando a ordem de inserção (que é a ordem de layer).

---

## 3. GLFont — stb_truetype (fase 1)

### Arquitetura

```cpp
// src/gl/GLFont.h
struct StringKey {
    char str[64]; uint32_t color; float size;
    bool operator==(const StringKey&) const;
};
struct StringTexture {
    GLuint tex; int w, h; uint32_t lastUsed;
};
class GLFont {
    stbtt_fontinfo font;                           // Tahoma.ttf carregado
    float fontSize = 12.0f;
    std::unordered_map<StringKey, StringTexture> cache;  // LRU ~256
    int texW = 512, texH = 64;                     // dimensões da textura (como original)

    bool Init(const char* ttfPath, std::string* err);
    void SetText(const char* str, uint32_t color);  // rasteriza → cache
    void Render(UIBatcher& batch, float x, float y, int renderType);  // push quads
    void SetSize(float s);
    void FlushCache();                              // libera texturas LRU
    void Shutdown();
};
```

### Pipeline SetText (substitui GDI TextOut)

```
1. Check cache: key = (str, color, fontSize)
2. Se hit: retorna StringTexture existente
3. Se miss:
   a. Divide em max 3 linhas (42 chars cada, como original TMFont2.cpp:67-116)
   b. Para cada linha: stbtt_GetCodepointBitmapBox() para medir tamanho
   c. Aloca buffer RGBA8 (w × h × 4)
   d. Para cada caractere: stbtt_MakeCodepointBitmap() → alpha → pack RGBA
      (R=G=B=255, A=alpha do glifo)
   e. glTexImage2D(GL_RGBA8, buffer) → StringTexture
   f. Medida: stbtt_GetCodepointHMetrics() → alimenta m_szStringSize[]
   g. Insere no cache (evict LRU se >256)
```

### Pipeline Render

```
1. Para cada linha (1-3):
   a. Calcula x offset por alinhamento:
      - 0 = left (x fixo)
      - 1 = center (x + (w - stringWidth) / 2)
      - 2 = right (x + w - stringWidth)
      - 3 = no-margin (x direto)
      - 4 = battle (centro de tela)
   b. Se RENDER_SHADOW: Push quad (x+1, y+1, sombra preta 0xFF000000)
   c. Push quad (x, y, textura, cor do texto)
   d. Avança y += fontSize + 1
```

### Notas de implementação

- **Fonte**: carregar do disco (TTF). Se não encontrar Tahoma, usar sans-serif do
  sistema ou incluir NanumGothic (coreano). O path vem de `FontConfig_Path`.
- **CP949→UTF-8**: o jogo roda em codepage coreana. `SetText` recebe bytes CP949;
  converter para UTF-8 antes de passar ao stbtt. Usar `iconv` ou tabela estática.
- **A4R4G4B4 → RGBA8**: o original cria textura 4-bit (alpha+branco). Nós usamos
  RGBA8 direto (stbtt entrega alpha puro; packamos como R=G=B=255, A=alpha).
  Alternativa: `GL_R8` + swizzle (A→R) para economizar memória.
- **m_nFontTextureSize=512**, **m_nFontTextureSizeY=64**: dimensões fixas do atlas
  de texto. Manter para compatibilidade com o layout da UI.
- **m_nFontSize=12**: tamanho base da fonte. Escala via `m_fSize` no GeomControl.
- **Render state block 2**: `depthTest OFF, alphaTest ON (ref=8), blend ON,
  fog OFF, sampler=PointNoMip`. Já temos `PointNoMip` no `GLSamplers`.

---

## 4. TMFont3 — números de dano flutuante

### Arquitetura

```cpp
// src/gl/TMFont3.h
class TMFont3 : public TreeNode {
    GLFont m_Font2;              // para type==0 (texto normal)
    TMVector2 m_vecPosition;     // posição atual (anima)
    TMVector2 m_vecStartPosition;
    uint32_t m_dwCreateTime, m_dwLifeTime;
    float m_fVisualProgress;
    short m_sDir;                // 1=cima, 2=baixo, 3=esquerda, 4=direita
    int m_nType;                 // 0=texto, 1-6=glyphas
    int m_nTextureSetIndex;      // 137-141
    stNum m_stNum[11];           // dados por caractere
    char m_szString[64];

    int FrameMove(uint32_t dwServerTime);
    void Render(UIBatcher& batch, GLTextureManager& tex);
};
```

### Pipeline (port direto de TMFont3.cpp)

```
FrameMove():
  1. progress = (now - createTime) / lifeTime
  2. Se progress > 1.0: return 0 (deletar)
  3. Posição: y = startY - (speed × progress × 100 × heightRatio)
  4. Alpha: 0-30% fade-in, 30-80% visível, 80-100% fade-out
  5. Para type 5/6: scale = sin(π × progress) (pulse)

Render():
  1. Se type==0: m_Font2.Render() (stb_truetype)
  2. Se type>0:
     a. Para cada dígito em m_stNum[]:
        b. ControlTextureSet = GetUITextureSet(m_nTextureSetIndex)
        c. GetUITexture(coord.nTextureIndex) → textura do glifo
        d. Push UIQuad via RenderRectTexDamage()
```

### Tipos de glypha (texturas 137-141)

| Type | Set | Uso | Escala |
|------|-----|-----|--------|
| 0 | — | Texto normal (stb_truetype) | 1.0 |
| 1 | 137 | Dano | 0.6 |
| 2 | 138 | Dano | 1.0 |
| 3 | 139 | Dano | 1.0 |
| 4 | 140 | Cura | 1.0 |
| 5 | 141 | Dano grande | 1.1 (pulse sin) |
| 6 | 141 | Dano médio | 0.8 (pulse sin) |

---

## 5. RenderRect* — primitivas de desenho (10 funções)

Todas chamam `UIBatcher::Push()` com o quad apropriado.

### 5.1 RenderRectC (fonte)

```cpp
// RenderDevice.cpp — font text quad
void RenderRectC(float startX, float startY, float cx, float cy,
                 float destX, float destY, GLuint texture,
                 uint32_t color, float scaleX, float scaleY) {
    // UVs: startX/texW → (startX+cx)/texW, startY/texH → (startY+cy)/texH
    // Pos: destX, destY, cx*scaleX, cy*scaleY
    UIBatch.Push({destX, destY, cx*scaleX, cy*scaleY,
                  u0, v0, u1, v1, color, texIndex, currentLayer});
}
```

### 5.2 RenderRect (sprite simples)

```cpp
void RenderRect(float startX, float startY, float cx, float cy,
                float destX, float destY, GLuint texture,
                float scaleX, float scaleY) {
    // Mesmo que RenderRectC mas cor = 0xFFFFFFFF (branco)
}
```

### 5.3 RenderRectTex (textura com rotação/escala)

```cpp
void RenderRectTex(float startX, float startY, float cx, float cy,
                   float destX, float destY, float destCX, float destCY,
                   GLuint texture, uint32_t color, bool transparent,
                   float angle, float scale) {
    // UVs normalizados via descritor da textura
    // Ângulo e escala aplicados no push (offset do centro)
}
```

### 5.4 RenderRectNoTex (retângulo sólido)

```cpp
void RenderRectNoTex(float x, float y, float cx, float cy,
                     uint32_t color, bool transparent) {
    // Sem textura — usa white texture (1x1) com cor
}
```

### 5.5 RenderRectCoord (tile com UV custom)

```cpp
void RenderRectCoord(float destX, float destY, float cx, float cy,
                     GLuint texture, uint32_t color,
                     float fU, float fV) {
    // UVs de fU/fV até fU+cx/texW, fV+cy/texH
}
```

### 5.6 RenderRectTexDamage (dano flutuante)

```cpp
void RenderRectTexDamage(float startX, float startY, float cx, float cy,
                         float destX, float destY, float destCX, float destCY,
                         GLuint texture, uint32_t color, bool transparent,
                         float angle, float scale) {
    // Centralizado: destX -= destCX/2, destY -= destCY/2
}
```

### 5.7 RenderRectRot (sprite rotacionado)

```cpp
void RenderRectRot(float startX, float startY, float cx, float cy,
                   float destX, float destY, float cenX, float cenY,
                   float angle, GLuint texture, float scaleX, float scaleY) {
    // Rotação around (cenX, cenY) — usado por sanct/legend overlays
}
```

### 5.8 RenderRectTex2C (duas texturas)

```cpp
void RenderRectTex2C(float startX, float startY, float cx, float cy,
                     float startX2, float startY2, float cx2, float cy2,
                     float destX, float destY, float destCX, float destCY,
                     GLuint tex1, GLuint tex2, uint32_t color,
                     bool transparent, float angle, float scale) {
    // Dois samplers: stage0=MODULATE, stage1=MODULATE
    // Usado por minimap, overlays complexos
    // Requer shader ui_quad2 (2 samplers)
}
```

### 5.9 RenderRectTex2M (minimap)

Variante de `RenderRectTex2C` com vertex format diferente (`m_MiniMapVertex2`).
Mesmo shader, UVs diferentes.

### 5.10 RenderRectProgress2 (barra radial)

```cpp
void RenderRectProgress2(float x, float y, float cx, float cy,
                         float progress, uint32_t color) {
    // Fan de até 8 segmentos → triangles (0,1,2 / 0,2,3 / ...)
    // Progress 0-1 = preenchimento angular
}
```

---

## 6. RenderGeomControl + RenderGeomRectImage — dispatch central

### RenderGeomControl (linha 3249 do original)

```cpp
void RenderGeomControl(GeomControl* ipControl) {
    switch (ipControl->eRenderType) {
        case RENDER_TEXT:
        case RENDER_SHADOW:
            ipControl->pFont->Render(batch, ipControl->nPosX, ipControl->nPosY,
                                     ipControl->eRenderType);
            break;
        case RENDER_IMAGE:
        case RENDER_IMAGE_TILE:
        case RENDER_IMAGE_STRETCH:
        case RENDER_TEXT_FOCUS:
            RenderGeomRectImage(ipControl);
            if (ipControl->strString[0] && ipControl->pFont)
                ipControl->pFont->Render(batch, ...);  // texto centralizado
            break;
        case RENDER_3DOBJ:
            // Flush UI → render 3D com depthFunc=ALWAYS → retomar UI
            // (Fase 7 se necessário)
            break;
    }
}
```

### RenderGeomRectImage (linha 2831 do original)

```
1. pUISet = GetUITextureSet(nTextureSetIndex)
2. coord = pUISet->pTextureCoord[nTextureIndex]
3. pTexture = GetUITexture(coord.nTextureIndex, 2000)
4. Switch eRenderType:
   - RENDER_IMAGE: RenderRect(coord.startX, startY, w, h, destX, destY, tex)
   - RENDER_IMAGE_STRETCH: scale = (control.w/coord.w, control.h/coord.h)
     → RenderRectRot(..., scale, angle)
     → Se sSanc > 0: overlay sanct (tex 338)
     → Se sLegend > 0: overlay legend (tex 338)
   - RENDER_IMAGE_TILE: RenderRectCoord(destX, destY, w, h, tex, color, u, v)
5. Se nTextureSetIndex < -2: RenderRectTex(...) (caminho alternativo)
6. Se nMarkIndex >= 0: guild mark (§12)
7. Se nTextureSetIndex == -1: RenderRectNoTex(...) (cor sólida)
```

---

## 7. UI Texture Pipeline

### Estruturas (TextureManager.h)

```cpp
struct ControlTextureCoord {
    int nTextureIndex;   // índice em m_ppUITexture[]
    int nStartX, nStartY; // retângulo origem na textura
    int nWidth, nHeight;  // dimensões do retângulo
    int nDestX, nDestY;   // offset de destino
};
struct ControlTextureSet {
    int nCount;
    ControlTextureCoord* pTextureCoord;
};
```

### Pipeline de carregamento

```
InitUITextureSetList():
  1. Lê TextureSetList.txt (texto, 600 entries)
  2. Para cada set: parse nome → index,ItemCount → nCount
  3. Para cada coord: parse "texIndex,startX,startY,w,h,destX,destY"
  4. Armazena em m_UITextureSetList[600]

InitUITextureList():
  1. Lê TextureListNList.bin (binário, 512 entries)
  2. Cada entry: filename + alpha flag + timing
  3. Armazena em m_stUITextureList[512]

GetUITexture(index, showTime):
  1. Se m_ppUITexture[index] já carregado: retorna
  2. Senão: LoadUITexture(index) → carrega do disco (TGA/DDS)
  3. Retorna m_ppUITexture[index]
```

### Integração com GLTextureManager

```cpp
class GLTextureManager {
    // ... (já existe: model, env, effect textures)
    // NOVOS:
    ControlTextureSet m_UITextureSetList[600];
    GLuint m_ppUITexture[512];      // lazily loaded
    stTextureListInfo m_stUITextureList[512];
    bool InitUITextureSetList(const char* path, std::string* err);
    bool InitUITextureList(const char* path, std::string* err);
    ControlTextureSet* GetUITextureSet(int index);
    GLuint GetUITexture(int index, uint32_t showTime);
};
```

---

## 8. SControl tree — port do sistema de controles

### 8.1 SControl base

```cpp
// src/ui/SControl.h
class SControl {
public:
    CONTROL_TYPE m_eCtrlType;
    uint32_t m_dwControlID;
    float m_nPosX, m_nPosY, m_nWidth, m_nHeight;
    int m_bVisible, m_bEnable, m_bFocused, m_bOver;
    int m_bAlwaysOnTop, m_bModal, m_bDeleteThisObject;
    IEventListener* m_pEventListener;
    SControl* m_pParent;
    SControl* m_pDown;      // primeiro filho
    SControl* m_pNext;      // próximo irmão

    virtual ~SControl() = default;
    virtual void FrameMove2(stGeomList* pDrawList, IVector2 parentPos, int layer, int arg);
    virtual int OnMouseEvent(int evType, float x, float y, uint32_t param);
    virtual int OnKeyDownEvent(int key);
    virtual int OnCharEvent(char c);
    int PtInControl(float x, float y);
    SControl* FindControl(uint32_t id);
};
```

### 8.2 Subclasses (13 controles)

| Classe | Herda de | Campos-chave | Render |
|--------|----------|-------------|--------|
| SPanel | SControl | GeomControl m_GCPanel, m_bPickable, m_pDescPanel | GeomControl → batch |
| SButton | SPanel | m_bMouseOver/Pressed/Selected, m_cBlink, m_GrayType | 4 estados (common/over/press/sel) |
| SText | SControl | 4×TMFont2, 4×GeomControl, alignType, border | Font → batch |
| SEditableText | SText | cursor blink, maxLen, passwd, IME | Text + cursor |
| SCheckBox | SPanel | m_bChecked | 2 estados |
| SProgressBar | SPanel | current/max, style (0=vert, 1=horiz, 2=VH) | Progress calc → batch |
| SScrollBar | SControl | up/down buttons, thumb | Scroll logic |
| SListBox | SPanel+IEventListener | items[], visibleCount, scroll, editable | List + scroll |
| SListBoxItem | SControl | text, selected, font | Item render |
| SMessageBox | SPanel+IEventListener | OK/Cancel buttons, text | Modal dialog |
| SMessagePanel | SPanel | auto-hide timer | Toast notification |
| SCursor | SPanel | cursorType, drag-attach item | Layer 29 |
| S3DObj | SControl | GeomControl m_GCObj, objIndex | (Fase 7) |
| SGridControl | SControl | rows, cols, grid cells | Grid layout |

### 8.3 FrameMove2 pipeline

```
SControlContainer::FrameMove(dwServerTime):
  1. Walk depth-first da árvore m_pControlRoot
  2. Para cada controle visível:
     a. Acumula ivParentPos (offset relativo ao pai)
     b. Chama pControl->FrameMove2(m_pDrawControl, ivParentPos, layer, 0)
     c. Se tem filho (m_pDown): incrementa layer, acumula posição
     d. Ao retornar: desacumula posição
  3. Chama m_pCursor->FrameMove2(m_pDrawControl, ivParentPos, 29, 0)

SPanel::FrameMove2():
  1. Skip se sem textura e alpha=0
  2. m_GCPanel.nPosX = ivParentPos.x + m_nPosX
  3. Cull contra [0, 800*ratio] × [0, 600*ratio]
  4. AddRenderControlItem(pDrawList, &m_GCPanel, layer)

SText::FrameMove2():
  1. Configura render type (TEXT/SHADOW/TEXT_FOCUS)
  2. Calcula X por alinhamento
  3. Centraliza Y: ivParentPos.y + m_nPosY + (m_nHeight - 16*ratio)/2 + 2
  4. AddRenderControlItem para m_GCText
```

### 8.4 AddRenderControlItem

```cpp
int AddRenderControlItem(stGeomList* pDrawList, GeomControl* pGeom, int nLayer) {
    if (nLayer >= MAX_DRAW_CONTROL) return 0;
    pGeom->nLayer = nLayer;
    if (pDrawList[nLayer].pHeadGeom)
        pDrawList[nLayer].pTailGeom->m_pNextGeom = pGeom;
    else
        pDrawList[nLayer].pHeadGeom = pGeom;
    pDrawList[nLayer].pTailGeom = pGeom;
    return 1;
}
```

---

## 9. SControlContainer — gerenciador raiz

### Arquitetura

```cpp
// src/ui/SControlContainer.h
class SControlContainer : public IEventListener {
    SControl* m_pControlRoot;
    SCursor* m_pCursor;
    SControl* m_pFocusControl;
    SControl* m_pPickedControl;
    SControl* m_pModalControl[8];
    stGeomList m_pDrawControl[30];
    int m_bInvisibleUI;

    // Tree
    void FrameMove(uint32_t dwServerTime);
    SControl* FindControl(uint32_t id);

    // Input
    int OnMouseEvent(int evType, float x, float y, uint32_t param);
    int OnKeyDownEvent(int key);
    int OnKeyUpEvent(int key);
    int OnCharEvent(char c);

    // Events (IEventListener)
    int OnControlEvent(uint32_t controlID, uint32_t event) override;

    // Binary loading
    bool ReadRCBin(const char* filename, std::string* err);
};
```

### Pipeline de input

```
OnMouseEvent(evType, x, y, param):
  1. Se cursor tem item anexado: handle drag
  2. Se modal control existe: restringe a ele
  3. Walk depth-first da árvore
  4. Para cada controle visível:
     a. Chama pControl->OnMouseEvent(evType, x, y, param)
     b. Se retorna 1: evento consumido, para
  5. Auto-focus SEditableText no clique

OnKeyDownEvent(key):
  1. Se focus control existe: delega
  2. Senão: walk da árvore procurando shortcuts

OnCharEvent(c):
  1. Delega direto para m_pFocusControl
```

---

## 10. SetMatrixForUI — projeção ortográfica

```cpp
// GLRenderDevice — chamado antes do frame de UI
void SetMatrixForUI(int screenW, int screenH) {
    // Ortho: (0,0) topo-esquerda, (W,H) baixo-direita
    // Substitui a perspectiva rasa do original (fov=0.1, z=50)
    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
    // Upload para UBO ou uniform separado do shader ui_quad
}
```

### Comparação com original

| Aspecto | Original (D3D9) | GL 4.1 |
|---------|-----------------|--------|
| Projeção | `perspective(fov=0.1, aspect×0.94, 10, 100)` | `ortho(0, W, H, 0, -1, 1)` |
| Câmera | `(0, 0, 50)` → lookAt `(0, 0, 0)` | N/A (screen-space direto) |
| Distância | UI quads em z=0, câmera z=50 | N/A |
| Resultado | Perspectiva rasa ≈ ortho | Ortho puro |
| Y-axis | Cima = -Y (D3D) | Cima = 0 (ortho com flip) |

---

## 11. glScissor clipping

### Integração com GLStateCache

```cpp
// GLStateCache — novos campos
struct GLStateCache {
    // ... (existentes)
    bool scissorEnabled = false;
    int scissorX, scissorY, scissorW, scissorH;
};

void GLStateCache::SetScissor(int x, int y, int w, int h) {
    if (!scissorEnabled) { glEnable(GL_SCISSOR_TEST); scissorEnabled = true; }
    if (scissorX!=x || scissorY!=y || scissorW!=w || scissorH!=h) {
        glScissor(x, y, w, h);
        scissorX=x; scissorY=y; scissorW=w; scissorH=h;
    }
}
void GLStateCache::DisableScissor() {
    if (scissorEnabled) { glDisable(GL_SCISSOR_TEST); scissorEnabled = false; }
}
```

### Uso no SPanel

```cpp
void SPanel::FrameMove2(stGeomList* pDrawList, IVector2 parentPos, int layer, int arg) {
    if (m_bClip) {
        // Habilita scissor na área do painel (filhos são recortados)
        device.State().SetScissor(m_nPosX, m_nPosY, m_nWidth, m_nHeight);
    }
    // ... renderiza filhos ...
    if (m_bClip) {
        device.State().DisableScissor();
    }
}
```

---

## 12. Guild marks

### Pipeline

```
1. Download: WinInet → libcurl (Fase 7) baixa .bmp do servidor
2. Conversão: BMP → textura 16×12 pixels (RGBA8)
3. Render: usa RenderRectTex() ou RenderRectNoTex() via UIBatcher
4. Cache: texturas dinâmicas com invalidação por guild ID
```

### Nota

O download de guild marks depende de libcurl (Fase 7). Nesta fase, apenas o
**render pipeline** para guild marks estará pronto — se a textura estiver
disponível no disco, será renderizada. Download remoto fica para Fase 7.

---

## 13. Cursor software

```cpp
// SCursor::FrameMove2()
void SCursor::FrameMove2(stGeomList* pDrawList, IVector2 parentPos, int layer, int arg) {
    // Posição = mouse coords (SDL_GetMouseState)
    m_GCPanel.nPosX = mouseX;
    m_GCPanel.nPosY = mouseY;

    if (m_nCursorType == 2) {
        m_GCPanel.nPosX = -100;  // invisível
        return;
    }

    // Se tem item anexado (drag): renderiza ítem + cursor
    if (m_pAttachedItem) {
        AddRenderControlItem(pDrawList, &m_pAttachedItem->m_GCObj, layer);
    }
    AddRenderControlItem(pDrawList, &m_GCPanel, layer);
}
```

- `SDL_HideCursor()` no boot do aplicativo.
- Cursor sempre na camada 29 (última, sempre visível).
- Modo hardware (`.cur` resources) eliminado.

---

## 14. Shader ui_quad

### Vert (novo: `src/gl/shaders/ui_quad.vert`)

```glsl
#version 410
layout(location=0) in vec3 aPos;
layout(location=1) in float aRhw;
layout(location=2) in vec4 aColor;
layout(location=3) in vec2 aUV;
uniform mat4 uProj;
out vec4 vColor;
out vec2 vUV;
void main() {
    gl_Position = uProj * vec4(aPos, 1.0);
    vColor = aColor;
    vUV = aUV;
}
```

### Frag (novo: `src/gl/shaders/ui_quad.frag`)

```glsl
#version 410
in vec4 vColor;
in vec2 vUV;
uniform sampler2D uTex;
uniform bool uHasTex;
out vec4 fragColor;
void main() {
    if (uHasTex)
        fragColor = texture(uTex, vUV) * vColor;
    else
        fragColor = vColor;
}
```

### ui_quad2 (duas texturas, para minimap/overlays)

```glsl
// Frag:
uniform sampler2D uTex0;
uniform sampler2D uTex1;
void main() {
    vec4 c0 = texture(uTex0, vUV);
    vec4 c1 = texture(uTex1, vUV);
    fragColor = c0 * c1 * vColor;  // MODULATE
}
```

---

## 15. Ordem de execução (13 passos)

### Passo 1 — UIBatcher + orto + shader (fundação)
- `src/gl/UIBatcher.{h,cpp}`: Init, Push, Flush, Shutdown
- `src/gl/shaders/ui_quad.{vert,frag}`
- `GLRenderDevice::SetMatrixForUI()`
- Teste: push 3 quads coloridos → tela

### Passo 2 — RenderRect* (10 primitivas)
- Todas as 10 funções via UIBatcher
- `RenderRectC` (fonte), `RenderRectTex`, `RenderRectNoTex`, etc.
- Teste: cada primitiva isolada

### Passo 3 — GLFont (stb_truetype fase 1)
- `src/gl/GLFont.{h,cpp}`
- Cache LRU, rasterização cp949→UTF-8
- Teste: "Hello WYD" na tela, sombra, alinhamento

### Passo 4 — TMFont3 (dano flutuante)
- `src/gl/TMFont3.{h,cpp}`
- Types 0-6, animação, glyphs texturas 137-141
- Teste: número flutuante com fade

### Passo 5 — UI texture pipeline
- `GLTextureManager`: InitUITextureSetList, InitUITextureList, GetUITexture/Set
- Carregar texturas reais do disco
- Teste: carregar set 0, render primeiras 10 texturas

### Passo 6 — RenderGeomControl + RenderGeomRectImage
- Dispatch por eRenderType
- ControlTextureSet→textura, sanct/legend overlays
- Teste: render painel com textura real

### Passo 7 — SControl base + SPanel + SButton + SText
- Classe base + 3 subclasses mais usadas
- FrameMove2 pipeline, AddRenderControlItem
- Teste: 3 controles na tela

### Passo 8 — SControlContainer + tree + input
- Container, árvore, FrameMove, input dispatch
- IEventListener, FindControl, modal stack
- Teste: árvore de controles com interação

### Passo 9 — SEditableText + SListBox + SScrollBar
- Input de texto, lista com scroll, scroll bar
- Teste: caixa de chat funcional

### Passo 10 — SProgressBar + SCheckBox + SMessageBox
- Progress bar, checkbox, dialog modal
- Teste: barra de HP + checkbox + msgbox

### Passo 11 — SCursor + glScissor
- Cursor software, clipping de painéis
- Teste: cursor visível, painel com filhos recortados

### Passo 12 — Binary loading (UIBinary.h)
- Deserialização de .bin files
- TMScene::ReadRCBin → SControl tree completa
- Teste: carregar cena de UI real

### Passo 13 — Testes + regressão + docs
- 6+ suítes novas
- ≥30 ctest verdes
- Docs 13/README atualizados
- Retrospectiva §20

---

## 16. Retrospectiva (pós-execução)

### O que funcionou como esperado
- UIBatcher + ortho projection funcionou (via `fx_quad` screen-space, ver desvios)
- stb_truetype integrou sem problemas (substituiu GDI perfeitamente)
- SControl tree + input dispatch funcionou sem re-entrancy bugs
- UI texture pipeline (512+600) parsed correctly (UITextureListN.bin = 512×528 confirmado)
- **Integração end-to-end validada**: `FieldScene2.bin` carrega via `UILoader::ReadRCBin`
  e renderiza com texturas reais do jogo (painel de chat, skill bar, ícones)
- Input SDL→SControlContainer wired (mouse WM codes, TEXT_INPUT, backspace/enter/tab)
- 27 ctest verdes (test_ui com 24 subtestes, incl. 4 de UILoader com structs reais)

### Desvios do plano
- **Shader ui_quad dedicado DESCARTADO**: conflito `uProj` com o FrameData UBO do
  common.glsl + uniform otimizado para fora pelo linker. Solução: reusar o
  `fx_quad` (já validado na Fase 4) em modo screen-space — mesma primitiva do
  EffectRenderer, menos código, zero bugs novos.
- **Eventos de mouse usam WM codes** (512/513/514, não 0/1/2): EventTranslator
  passa WM_LBUTTONDOWN etc. direto. Corrigido em todo o SControl.
- **Container subtrai offsets dos pais** no dispatch (coords parent-relative):
  meu primeiro rascunho passava absolutas. Portado fiel (SControlContainer.cpp:64).
- **UIBinary.h reescrito**: primeiro rascunho era inventado; formato real tem
  structs fixas por tipo, `nID`/`nParentID`, strings via `nStringIndex`→g_UIString
  (UIString.txt — ausente neste build, strings vazias, original tolera).
- **Dois formatos de .bin no wild**: `FieldScene2.bin` (novo: panel 40B c/ nPickable)
  vs `LoginScene.bin` (legado: panel 36B, strings inline). O cliente Projects/ atual
  lê o novo — o legado falharia lá também. Parser cobre o novo.
- GLFont: hack `static` do TTF data virou membro `m_ttfData` (stbtt referencia os bytes).
- IVector2 adicionado em UITypes.h (não existia no código GL).
- SCursor::m_pAttachedItem tipado como SPanel* (acesso a m_GCPanel).

### Bugs difíceis encontrados
1. **Texturas UI pretas (o "retângulo preto")**: sampler mipmapped herdado da cena
   3D (`LinearMip`) + texturas UI single-level → textura INCOMPLETA → GL sampla
   (0,0,0,1). Fix: `glBindSampler(0, LinearNoMip())` no Flush + MAX_LEVEL=0 nas
   texturas criadas manualmente. **Diagnosticado pelo sintoma "quadrado preto".**
2. **RenderRect* descartavam a textura resolvida**: parâmetro `GLuint texture`
   ignorado, `texIndex=-1` sempre → tudo branco. Fix: `texHandle = texture` +
   UVs normalizadas por texW/texH (cache de dimensões no GLTextureManager).
3. **V-flip**: canto "LT" do quad cai na borda INFERIOR da tela (pixels y-down)
   mas samplava v0 (topo da textura). Fix: mapeamento vs[] trocado no Flush.

### Items deferidos para fases seguintes
- GLFont fase 2 (atlas de glifos + batcher de texto) → Fase 8
- IME coreano completo (imm32→SDL) → Fase 7
- RENDER_3DOBJ (ícones 3D na UI) → Fase 7 (loader já pula o record sem desalinhar)
- Download de guild marks (libcurl) → Fase 7
- SReelPanel, S3DObj, ListBox items especializados → Fase 7
- Posicionamento de painéis por código de cena (SetStickRight etc. em TMFieldScene)
  → vem com o port da cena, não da UI
