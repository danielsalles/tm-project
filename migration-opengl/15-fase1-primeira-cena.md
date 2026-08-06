# 15 — Fase 1: Primeira cena real (plano completo)

**Objetivo**: primeiro render GL de verdade — `GLRenderDevice` esquelético (Clear/Swap,
StateCache, UBO de frame), loaders de assets do jogo (`.wys` texturas, `.msa` meshes
estáticas), shader `mesh_lit` (MODULATE), e a **cena de seleção de servidor**
(objetos estáticos de `env\Field2723.dat`) renderizada com câmera fixa, visualmente
comparável ao cliente D3D original.

**Critério de saída (definition of done)**:
- [ ] `GLRenderDevice`: Init/Shutdown, BeginFrame/Clear/EndFrame(Swap), StateCache com
      dirty-check, presets de state block 0 e 1, UBO `FrameData` atualizado 1×/frame
- [ ] `mesh_lit.vert/.frag` compilando nos 3 OS; `FIX_Z` aplicado em todo vertex shader
- [ ] Loader `.wys`: DXT1/DXT3 via S3TC, mip chain do arquivo + `GL_TEXTURE_MAX_LEVEL`;
      teste de parse com blob sintético no CI
- [ ] Loader `.msa`: subsets por `D3DXATTRIBUTERANGE`, expansão de stride (armadilha -8B),
      nomes de textura 11B sem NUL, cores BGRA→RGBA; teste com blob sintético no CI
- [ ] Cena select-server: objetos estáticos de `env\Field2723.dat` na tela, câmera fixa,
      screenshot lado-a-lado com o cliente D3D (mesma câmera)
- [ ] GL smoke test no CI: contexto + compile dos shaders + 1 draw sem `GL_ERROR`
      (macOS direto; Linux via `xvfb-run` + Mesa llvmpipe; Windows build-only — ver §9)
- [ ] CI verde nos 3 OS; `Projects/` continua intocado (oráculo)

**Duração estimada**: 1 semana (5-7 dias úteis).

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte do plano |
|---|---|
| `GLRenderDevice` esquelético: Clear/Swap, `GLStateCache`, presets block 0/1 | 05 §5.1-5.2 |
| `common.glsl` (FIX_Z, layout std140, helpers) + UBO `FrameData` | 04 §4.1, 05 §5.5 |
| Shader `mesh_lit` — **só o combiner #1 (MODULATE tex×diffuse)** | 05 §5.4 |
| Loader `.wys` (DXT1/DXT3) + política de mip "subir o que existe" | 10 §10.1-10.3 |
| Loader `.msa` + VAOs estáticos (`VAO_N2`, `VAO_L` conforme FVF do arquivo) | 06 §6.1-6.2 |
| Parser de `env\*.dat` (`ObjectFileItem`, 28B/registro) + objetos estáticos comuns | este doc §7 |
| Câmera fixa com matrizes do shim (já golden-testadas na fase 0) | 04 §4.1 |
| Luz: ambient + 1 luz direcional (a do estado default do block 1) | 05 §5.5 simplificado |

### NÃO entra (e por quê)

| Item | Fase |
|---|---|
| Terreno (`Field2723.trn`), céu, sol, mar, neve | 2 — a cena fica "flutuando" sem chão nesta fase, é esperado |
| Skinned meshes (`.msh/.bon/.ani`), personagens | 3 |
| Combiners 2-10 (ADDSIGNED do sky, DOTPRODUCT3, multitex...) | 2-4, por demanda real |
| Point lights, alpha test refinado, fog | 2 |
| Cache eviction do `GLMeshManager` (1200ms) | 2 — fase 1 carrega uma cena, memória não pressiona |
| Texture array de flipbooks | 4 (08 §8.2) |
| Half-texel offset (UI) | 5 — sem UI nesta fase |
| Port de `TMScene`/`ObjectManager`/`TMCamera` | 2+ — câmera é hardcoded, cena é um renderer de lista |

**Regra de ouro da fase** (igual à fase 0): paridade, não melhoria. Nada de PBR, instancing,
"ja que estamos aqui". O objetivo é uma imagem **indistinguível** da do cliente original
na mesma câmera — divergência visual é bug, não oportunidade.

---

## 2. Estrutura de diretórios (adicionada nesta fase)

```
src/
├── gl/
│   ├── GLRenderDevice.h/.cpp     (fachada: Init/BeginFrame/EndFrame/SetBlock/DrawMesh)
│   ├── GLStateCache.h/.cpp       (dirty-check; presets block 0/1)
│   ├── GLShader.h/.cpp           (compile/link cache; CombinerKey — só MODULATE por ora)
│   ├── GLTexture.h/.cpp          (LoadTextureWYS; sampler objects)
│   ├── GLMesh.h/.cpp             (LoadMsa; GLMesh + subsets; VAOs por FVF)
│   └── shaders/
│       ├── common.glsl           (FIX_Z, FrameData std140, helpers — concatenado nos shaders)
│       ├── mesh_lit.vert
│       └── mesh_lit.frag
├── scene/
│   ├── ObjectFile.h/.cpp         (parser env/*.dat — ObjectFileItem 28B, LE, campo-a-campo)
│   └── SelectServerView.h/.cpp   (lista de objetos + câmera fixa; o "demo" da fase)
└── platform/main.cpp             (loop da fase 1: carrega cena, renderiza, screenshot com F12)

tests/
├── test_wys.cpp                  (blob DXT sintético: header reconstruído, mips contados)
├── test_msa.cpp                  (blob .msa sintético: subsets, stride expandido, BGRA→RGBA)
└── test_glsmoke.cpp              (contexto + compile shaders + draw + glGetError==0)

tools/
└── (vazio — embed de shader é um script cmake, ver §4.3)
```

`Projects/TMProject/` segue **intocado**: os parsers novos são validados contra a leitura
do código original (`TMMesh.cpp:481-728`, `TextureManager.cpp:299-310`,
`TMObjectContainer.cpp:54-100`), que continua compilando no Windows como referência.

---

## 3. `GLRenderDevice` esquelético

### 3.1 Superfície de API (mínima, sem ambição)

```cpp
class GLRenderDevice {
public:
    bool Init(SDL_Window* window);          // contexto já criado pelo main (fase 0)
    void Shutdown();

    void BeginFrame();                      // upload do UBO FrameData (view/proj/time/luz)
    void Clear(uint32_t rgba, float depth); // glClearColor/Depth + clear
    void EndFrame();                        // SDL_GL_SwapWindow

    void SetRenderStateBlock(int n);        // 0=UI quad, 1=cena 3D (só estes dois)
    void SetWorldMatrix(const D3DXMATRIX& m); // uniform uWorld por draw (não vai no UBO)

    void DrawMesh(const GLMesh& mesh, int textureBase); // itera subsets, bind textura+VAO, draws

    GLStateCache& State();                  // acesso fino quando o preset não basta
};
```

Deliberadamente **não** replica os ~50 métodos do `RenderDevice` D3D. Cada consumidor
novo (fases 2+) adiciona o método que precisar, quando precisar. Interface cedo demais
vira interface errada — mesma regra dos stubs da fase 0.

### 3.2 `GLStateCache` — subconjunto da fase 1

Da struct completa (05 §5.1), implementar agora:

```cpp
struct GLStateCache {
    bool depthTest, depthWrite, blend, cull;
    GLenum depthFunc, cullFaceMode;
    GLenum blendSrc, blendDst;
    float alphaRef; bool alphaTest;         // uniform uAlphaRef/uAlphaTest no FS
    GLuint texture[2];                      // units 0-1 (stage1 desligado nesta fase)
    GLuint sampler[2];
    void Apply();                           // diffs na ordem: depth → blend → cull → textura
};
```

Fog, point lights, stages multitex e `textureTransformFlags` entram quando o primeiro
consumidor real aparecer (fase 2). Campos existem na struct mas `Apply()` ignora —
documentado no código.

### 3.3 Presets (05 §5.2, fiel ao original)

| Block | Estado GL | Programa |
|---|---|---|
| 0 — UI quad | depth test on / write off, blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA, cull off | (sem uso na fase 1 — definido, não exercido) |
| 1 — Cena 3D | depth LEQUAL + write, blend off, alpha test **ref 0xDD fixo**, cull BACK/CCW | `mesh_lit` |

`ALPHAREF=0xDD` sempre (path não-NVIDIA do original — o correto/majoritário, 05 §5.2).
O FS faz `if (uAlphaTest && (c.a * 255.0) < uAlphaRef) discard;`.

---

## 4. Shaders

### 4.1 `common.glsl` — incluído (por concatenação) em todo shader

```glsl
#define FIX_Z(p) do { vec4 _q = (p); _q.z = _q.z * 2.0 - _q.w; gl_Position = _q; } while(0)

layout(std140, binding = 0) uniform FrameData {
    mat4  uView;          // transposta no upload (04 §4.3 — helper SetMat4Uniform)
    mat4  uProj;
    vec4  uAmbient;
    vec4  uLightDir;      // xyz=dir, w=enabled
    vec4  uLightColor;
    float uTime;
    // fog/point lights: campos reservados, ignorados nesta fase
};
```

### 4.2 `mesh_lit` (combiner #1 — MODULATE)

```glsl
// mesh_lit.vert
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;    // BGRA→RGBA já convertido no loader (05 §5.3)
layout(location=3) in vec2 aUV;
uniform mat4 uWorld;
out vec2 vUV; out vec4 vColor;
void main() {
    vec3 n = normalize(mat3(uWorld) * aNormal);
    vColor = (uAmbient + uLightColor * max(dot(n, -uLightDir.xyz), 0.0) * uLightDir.w) * aColor;
    vUV = aUV;
    FIX_Z(uProj * uView * uWorld * vec4(aPos, 1.0));
}

// mesh_lit.frag — COMBINE_MODULATE apenas
#version 410 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
uniform sampler2D uTex0;
uniform float uAlphaRef; uniform bool uAlphaTest;
void main() {
    vec4 c = texture(uTex0, vUV) * vColor;
    if (uAlphaTest && c.a * 255.0 < uAlphaRef) discard;
    fragColor = c;
}
```

Modelo de luz = subconjunto do fixed-function D3D (05 §5.5): ambient + 1 direcional,
sem specular (SPECULARENABLE=0 no jogo), vertex color sempre multiplica.
Iluminação é calculada no VS como o original (per-vertex, gouraud) — **não** "melhorar"
para per-pixel: mudaria o visual.

### 4.3 Embed dos shaders

Shaders vivem em `src/gl/shaders/*.glsl` (arquivos de verdade, com highlight de editor).
Um script cmake (`cmake/EmbedShaders.cmake`) gera `shaders_embedded.h` com string
literals por arquivo no configure time. Sem `#embed` (C++23), sem carregar de disco em
runtime (funciona igual instalado/portátil). Regenerado quando o `.glsl` muda
(`configure_depends` / glob com CONFIGURE_DEPENDS).

---

## 5. Loader `.wys` (texturas)

Formato (10 §10.1): `[1 byte descartado][DDS sem magic][fourCC corrompido no offset 84:
'2'→DXT1, senão→DXT3]`.

```cpp
GLuint LoadTextureWYS(const uint8_t* file, size_t size) {
    // 1. pular 1 byte; header DDS começa aí (sem "DDS ")
    // 2. ler DDS_HEADER campo-a-campo (LE): height, width, mipMapCount
    // 3. fourCC = byte[84]: '2' → DXT1, senão → DXT3 (fidelidade: é o que o original faz)
    // 4. para cada nível presente: glCompressedTexImage2D(GL_COMPRESSED_RGBA_S3TC_DXT*_EXT)
    // 5. glTexParameteri(GL_TEXTURE_MAX_LEVEL, nMips-1)  — NÃO glGenerateMipmap (inválido em compressed)
    // 6. sem S3TC no contexto → fallback: descompressão CPU DXT1/DXT3 (~120 linhas) → RGBA8
}
```

Flags de alpha vêm de `Mesh\MeshTextureList.bin` (`cAlpha`: 'C'=cutout/alpha test,
'A'/'a'=blend, 'N'=opaca — 10 §10.1). Nesta fase: 'C' → `alphaTest=true, alphaRef=0xDD`
no draw; 'A' → blend SRC_ALPHA/INVSRC_ALPHA; 'N' → opaco. Tabela carregada no init
(parser próprio de 264B/registro, campo-a-campo).

**S3TC no macOS**: confirmado presente no boot_report da fase 0 (Apple M3, `4.1 Metal`).
O fallback CPU existe mas nunca deve disparar nos alvos atuais — se disparar, logar
warning alto.

## 6. Loader `.msa` (meshes estáticas)

Layout em disco (06 §6.1, parser original `TMMesh.cpp:481-728`):

```
[u32 FVF][u32 stride][u32 nSubsets][D3DXATTRIBUTERANGE × nSubsets (16B cada)]
[nSubsets × nome de textura: 11 bytes SEM NUL][u32 nIdx][u16 IB blob][u32 nVerts][VB blob]
(ordem exata a confirmar contra TMMesh.cpp:481-728 na implementação — o original manda)
```

Armadilhas (06 §6.1-6.2), todas com teste:

1. **Stride -8**: quando `FVF != 322`, os vértices em disco têm stride 8B **menor** que em
   memória — expandir no load (passadas espaçadas, duplicar uv0→uv1) como o original.
2. **Nomes 11B sem NUL**: copiar para buffer de 12 e terminar manualmente.
3. **BGRA→RGBA**: converter `D3DCOLOR` no load (opção (a) de 05 §5.3) — shaders limpos.
4. **Leitura campo-a-campo, nunca `fread` de struct** (packing/padding, 06 §6.1).
5. **Índices u16** preservados (`GL_UNSIGNED_SHORT`); subsets viram
   `{indexStart, indexCount, textureIndex}` (baseVertex=0 — VB já expandido).

```cpp
struct GLMesh {
    GLuint vao, vbo, ebo;                    // VAO conforme FVF do arquivo (05 §5.3)
    struct Subset { uint32_t indexStart, indexCount; int textureIndex; } subsets[32];
    int subsetCount;
};
```

VAOs da fase 1: `VAO_L` (FVF 322: pos/color/uv) e `VAO_N2` (FVF 530: pos/normal/uv0/uv1).
Outro FVF encontrado em arquivo real → log + tratar como N2 se possível, senão falhar
alto (não adivinhar layout). `GL_STATIC_DRAW`, sem `glBufferStorage` (é 4.4).

`GLMeshManager`: lazy load por índice com cache em `unordered_map` — **sem** evicção por
timestamp nesta fase (fase 2, quando houver troca de mapa).

## 7. Cena select-server: parser `env/*.dat` + renderer

A cena original (`TMSelectServerScene.cpp:270-330`) carrega terreno (fase 2), objetos
(estes sim, fase 1), sol/céu/neve (fase 2). O que renderizamos: **os objetos estáticos**.

### 7.1 Formato `env\Field2723.dat` (`TMObjectContainer.cpp:54-100`)

Registro `ObjectFileItem` (28 bytes, `Structures.h:416-426`), lido campo-a-campo LE:

```cpp
struct ObjectFileItem {   // NÃO fread a struct — ler campo a campo
    uint32_t dwObjType;
    float posX, posY;     // TMVector2
    float fHeight;
    float fAngle;
    int32_t nTextureSetIndex;
    int32_t nMaskIndex;
    float fScaleH, fScaleV;
};
```

Offset do terreno (`m_fOffsetX/Y = (offsetIndex << 6) * 2`) se aplica — sem terreno na
fase 1, usar os offsets de `Field2723` hardcoded no demo (vêm do `.trn` na fase 2).

### 7.2 Quais tipos renderizar

- `dwObjType` mapeando para mesh estática comum (o grosso da cena: casas, muros, props):
  instanciar `SceneObject { mesh, world = T·Ry(fAngle)·S(fScaleH,fScaleV), textureSet }`.
- Tipos especiais conhecidos (`2`=mar, `343`=borboleta, portões animados...): **logar e
  pular**, com contador por tipo no fim do load. Lista exata de tipos sai do primeiro
  run real contra o `.dat` — o log dirá o que falta (e alimenta o backlog da fase 2).
- `TMHouse/TMGate` (objetos com animação/estado): fase 2. Aqui, renderizar frame base se
  trivial, senão pular logando.

### 7.3 `SelectServerView` (o demo executável)

`main.cpp` da fase 1 vira:

```
init SDL+GL (fase 0) → GLRenderDevice::Init
→ carrega MeshTextureList.bin, Field2723.dat, meshes/texturas referenciadas (lazy)
→ loop: BeginFrame(UBO: view/proj fixas) → Clear(céu: cor sólida placeholder)
        → SetRenderStateBlock(1) → DrawMesh × N objetos (ordenados por textura, sem sort global)
        → EndFrame
→ F12: screenshot (glReadPixels + stbi_write_bmp, flip Y — 10 §10.6)
→ ESC: sair
```

**Câmera fixa**: posição/look-at copiadas dos valores da cena original no cliente D3D
(capturar 1× com breakpoint/log no original; congelar no demo como constantes
documentadas). `PerspectiveFovLH` com near/far do jogo. Matrizes vêm do shim — já
validadas bit-a-bit contra D3DX na fase 0, então divergência de câmera é impossível
por construção.

### 7.4 Verificação visual (o "golden" desta fase é uma imagem)

1. Rodar cliente D3D original no Windows, mesma câmera → `CaptureScreen` (`D3DDevice.cpp:1051`).
2. Rodar o port → F12 → BMP.
3. Comparar lado a lado (script `tools/` opcional gera diff image; olho humano decide).
4. **Aceitável**: AA/dithering, ausência de terreno/céu. **Inaceitável**: objeto faltando,
   textura trocada, iluminação/culling invertido, escala/rotação errada.

Screenshot de referência do port vira artefato de CI (upload-artifact), para revisão
humana em PR — comparação automática por hash fica para a fase 2 (determinismo entre
GPUs ainda não foi medido).

---

## 8. Testes automatizados novos

| Teste | Tipo | Como |
|---|---|---|
| `test_wys` | CPU, CI | blob `.wys` sintético montado no teste (header DDS mínimo DXT1 4x4, 1 mip): parse → width/height/formato/mips corretos; fourCC '2'→DXT1 e 'X'→DXT3 |
| `test_msa` | CPU, CI | blob sintético FVF≠322 com 2 subsets: stride expandido +8B, uv1==uv0, nomes 11B terminados, BGRA→RGBA (caso 0xAARRGGBB conhecido), indexStart/Count corretos |
| `test_objectfile` | CPU, CI | blob `.dat` sintético com 3 registros: campos lidos corretos; tipo especial (2) é pulado+contado |
| `test_glsmoke` | **GPU, CI** | cria contexto (janela oculta), compila `mesh_lit`, 1 draw de triângulo em FBO RGBA8+depth, `glGetError()==0`, `glReadPixels` ≠ tudo zero |
| `tmmath`/`platform`/`cpsock` | CPU, CI | seguem rodando (regressão da fase 0) |

Assets reais (Mesh/, env/) **não estão no repo** — por isso os testes de parser usam
blobs sintéticos construídos byte a byte conforme o formato. O teste com asset real é o
smoke visual local (§7.4).

### CI: GL headless por OS

| OS | GL no CI | Decisão |
|---|---|---|
| macos-14 | WindowServer existe, contexto 4.1 sobe headless | `test_glsmoke` roda no ctest |
| ubuntu-24.04 | sem display; Mesa llvmpipe suporta GL 4.5 | `xvfb-run ctest`; deps += `xvfb mesa-utils libgl1-mesa-dri` |
| windows-2022 | runner só tem GL 1.1 (GDI) | **build-only**, smoke pulado (skip com log); validação GL no Windows é manual/local |

Documentar no workflow: `# GL runtime test skipped on Windows runners (no GL>1.1)`.

---

## 9. Cronograma (5-7 dias úteis)

| Dia | Entrega |
|---|---|
| 1 | `GLShader` + embed cmake + `common.glsl`; `mesh_lit` compilando; triângulo com FIX_Z na tela (hardcoded, sem loader) |
| 2 | `GLStateCache` + presets 0/1 + UBO `FrameData`; `GLRenderDevice` (Begin/Clear/End/DrawMesh); triângulo via fachada |
| 3 | `test_glsmoke` verde local; CI: xvfb no Linux, skip documentado no Windows |
| 4 | Loader `.msa` + `GLMesh` + VAOs; `test_msa` verde; primeira mesh real na tela (sem textura, cor flat) |
| 5 | Loader `.wys` + `MeshTextureList.bin` + samplers; `test_wys` verde; mesh texturizada com MODULATE |
| 6 | Parser `env/*.dat` + `SelectServerView` + câmera fixa; **cena na tela**; log de tipos pulados |
| 7 | Screenshot lado-a-lado vs. cliente D3D; corrigir divergências visuais; DoD revisado; PR |

Buffer realista: o passo 7 historicamente come 1-2 dias extras (sempre há um cull
invertido ou uma textura trocada). Se estourar, cortar do cronograma o diff-script de
screenshot, **nunca** a comparação visual em si.

## 10. Riscos da fase

| Risco | Prob. | Mitigação |
|---|---|---|
| S3TC indisponível em algum alvo | Baixa (confirmado no M3; Mesa tem) | fallback CPU DXT→RGBA8 no loader; warning alto no log |
| Armadilha do stride -8B no `.msa` | Média | teste sintético dedicado; port literal da lógica de `TMMesh.cpp:644-688` |
| BGRA/RGBA invertido (tudo com cor trocada) | Média | caso de teste com cor conhecida; conversão única no loader |
| Culling/winding invertido | Média | defaults do GL já reproduzem o jogo (04 §4.2) — mas é o bug clássico; screenshot denuncia |
| Layout exato do `.msa` divergir do documentado | Média | **o código original manda**: confirmar ordem dos campos em `TMMesh.cpp:481-728` antes de escrever o parser; blob sintético espelha o original, não o doc |
| Windows CI sem GL | Alta (certeza) | aceito: build-only + skip documentado; validação manual |
| Scope creep ("já vou fazendo o terreno") | Alta | DoD explícito; terreno é fase 2 com plano próprio (07 §7.1) |
| Câmera divergente da original | Baixa | matrizes do shim são golden-testadas; câmera copiada do original |

## 11. Saída esperada

**Uma screenshot real**: a cena de seleção de servidor do WYD — casas, muros e props
texturizados, iluminados, com culling correto — renderizada em OpenGL 4.1 core no
macOS/Linux/Windows, ao lado da screenshot idêntica do cliente D3D de 2003.

É a primeira prova end-to-end de que a estratégia inteira (convenções LH+z01, FIX_Z,
shim, loaders fiéis) produz **imagem correta**, não só teste verde. As fases 2-5 são
variações de volume sobre essa base.
