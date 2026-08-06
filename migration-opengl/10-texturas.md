# 10 — Texturas e Assets

## 10.1 Os formatos custom

Nenhum arquivo é TGA/DDS padrão — há ofuscação proposital:

### `.wys` (DDS truncado) — `TextureManager.cpp:299-310`

```
Disco:  [1 byte descartado][DDS sem magic][fourCC corrompido no offset 84: '2'→DXT1, senão→DXT3]
Loader: pular 1 byte; prefixar "DDS "; corrigir fourCC; parsear DDS header → mip chain DXT
```

### TGA truncado (demais arquivos) — `TextureManager.cpp:269-292`

```
Disco:  [4 bytes descartados][TGA sem header completo e sem footer]
Loader: pular 4 bytes; o header TGA (18B) está intacto a partir daí? NÃO — os 4 primeiros
        bytes do header (idLength, colorMapType, imageType, colorMapSpec[0]) foram removidos.
        Reconstruir: idLength=0, colorMapType=0, imageType=2 (uncompressed true-color) ou 10 (RLE),
        depois alimentar o buffer reconstituído ao stb_image (stbi_load_from_memory lê TGA
        sem precisar do footer "TRUEVISION-XFILE").
```

### Listas de texturas (`.bin`)

`stTextureListInfo[256]` por categoria (`TextureManager.h:39-45`): nome[255] + cAlpha(1) +
flags(4) + reserved(4) — lidas de `UITextureListN.bin`, `EffectTextureList.bin`,
`MeshTextureList.bin`, `EnvTextureList3.bin` (`TMPaths.h`). `cAlpha` escolhia o formato D3D;
em GL tudo vira RGBA8/DXT, mas `cAlpha` ainda dirige: **'C' = alpha test cutout**,
**'A'/'a' = alpha blend**, **'N' = opaca** — viram flags no material, não formato.

## 10.2 Loader GL

```cpp
GLuint LoadTextureWYS(const uint8_t* fileBytes, size_t size) {
    // reconstruir header DDS (10-texturas §1) → parse DDS_PIXELFORMAT
    // DXT1 → glCompressedTexImage2D(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, ...)
    // DXT3 → GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    // mip chain: níveis presentes no arquivo; gerar restantes se política pedir
    //   (glGenerateMipmap NÃO funciona em formatos compressed → se o arquivo tem a chain
    //    completa, ok; senão descomprimir nível 0 p/ RGBA8, gerar mips CPU/GPU e re-comprimir?
    //    NÃO: simplesmente subir os níveis existentes e setar GL_TEXTURE_MAX_LEVEL)
    // Fallback sem EXT_texture_compression_s3tc (raro): descompressão CPU (~60 linhas/block DXT)
}

GLuint LoadTextureTGA(const uint8_t* fileBytes, size_t size, bool colorKey) {
    // reconstruir 4 bytes de header → stbi_load_from_memory → RGBA8
    // colorKey (0xFF000000): na CPU pós-decode: if (px == 0x000000) px.a = 0
    //   — replica o comportamento do D3DX (TextureManager.cpp:265,542,757,1022)
}
```

S3TC no macOS/Apple Silicon: `GL_EXT_texture_compression_s3tc` está disponível nos drivers
GL da Apple (Rosetta e nativo) e em todo Mesa. Confirmar no boot com checagem de extensão
e fallback de descompressão CPU (stb_dxt-like).

## 10.3 Política de mipmaps (paridade com `m_nMipMap`)

| Categoria | Níveis | Filtro de geração | GL |
|---|---|---|---|
| UI | 1 | point | `glTexImage2D` nível 0, `GL_TEXTURE_MAX_LEVEL=0` |
| Effect | 4 | box | subir chain ou gerar; MIN = LINEAR_MIPMAP_LINEAR |
| Model | 4 (progressivo por VRAM — simplificar: sempre 4) | box | idem |
| Env | 4 se m_nMipMap≥20 | box | idem |
| Guild mark | todos | — | `glGenerateMipmap` (RGBA8, 16×12 — trivial) |

VRAM deixou de ser restrição: **simplificar a degradação progressiva**
(`TextureManager.cpp:835-894`) para a política máxima. Opção de config mantém o slider
funcional, mas sem o path 16bpp (`m_dwBitCount==16`, Voodoo/Savage — deletar).

## 10.4 Samplers

Default do jogo é WRAP (o mar depende: `TMSea.cpp:211-214` UVs > 1):

```cpp
glSamplerParameteri(s, GL_TEXTURE_WRAP_S/T, GL_REPEAT);   // default — explicitar mesmo assim
// filtros por state block: block1 = LINEAR/LINEAR/LINEAR; block2 (fonte) = NEAREST; block3 = LINEAR
// 4 sampler objects criados no init: SAMP_LINEAR_MIP, SAMP_LINEAR_NOMIP, SAMP_POINT, SAMP_POINT_NOMIP
```

GL 4.1 tem sampler objects (`glBindSampler`) — desacopla sampler de textura, igual ao
modelo D3D9 (sampler state independente). Usar.

## 10.5 Texturas dinâmicas

| Caso | Hoje | GL |
|---|---|---|
| Fonte por string | LockRect A4R4G4B4 (`TMFont2.cpp:177-269`) | RGBA8 + `glTexSubImage2D` (09 §9.2) |
| Atlas UI `GenerateTexture` | LockRect copy (`TextureManager.cpp:1186-1380`) | composição CPU em buffer → upload (mesma lógica, sem downsample 2× — VRAM não falta mais) |
| Guild mark (rede, BMP 632B) | InMemoryEx 16×12 (`:1406-1421`) | stb_image do buffer → `glTexImage2D` |
| RT bloom/blur (morto) | `InitRenderTargetTexture` (`:1446-1560`) | **FBOs novos** quando implementar pós (12) |
| Minimap `.wyt` | TGA truncado | mesmo loader TGA |

## 10.6 Screenshot

`D3DDevice::CaptureScreen` (`D3DDevice.cpp:1051-1077`) → `glReadPixels` do backbuffer +
`stbi_write_bmp` (flip Y: GL lê de baixo para cima — `stbi_flip_vertically_on_write`).

## 10.7 Flipbook de efeitos → texture array

Pré-processo offline (script Python) ou no load: sequências `Effect\fire00.wys..fire08.wys`
(quadros consecutivos por índice) empacotadas em `GL_TEXTURE_2D_ARRAY`. Requer mapear as
sequências a partir de `EffectTextureList.bin` + convenção de nomes. Benefício grande
(08 §8.2). Fase 1.5 — fase 1 pode subir texturas 2D individuais com bind por batch.
