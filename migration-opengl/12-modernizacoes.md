# 12 — Modernizações (fase 2+, após paridade)

Ordenadas por custo/benefício. Nenhuma deve entrar na fase 1.

## 12.1 Performance — os grandes ganhos

| # | Mudança | Ganho estimado | Esforço |
|---|---|---|---|
| 1 | **Terreno: IBO por frame** (07 §7.1) | centenas → 2 draws/ground | M (feito na fase 1 por necessidade) |
| 2 | **Efeitos instanciados** (08 §8.2) | neve 200→1, chuva 50→1, skills N→1-3 | M (fase 1) |
| 3 | **UI batcher** (09 §9.1) | ~300-500 draws → ~10-20 | M (fase 1) |
| 4 | **Skinned: 1 UBO por frame + instancing de partes iguais** (mesma mesh+textura, ex.: NPCs idênticos em cidade) | ~40% dos draws de personagem | G — paletas em texture buffer (TBO existe em 4.1) |
| 5 | **Flipbook texture array** (10 §10.7) | binds de textura de efeito → ~0 | M |
| 6 | **Fonte: atlas de glifos** (09 §9.2 fase 2) | elimina textura-por-string, cache frio | M |
| 7 | **Mar: ondas no VS** (07 §7.4) | elimina lock de VB por frame | P (fase 1) |
| 8 | **TMShade no VS com heightmap texture** (07 §7.5) | elimina conformação CPU | M |
| 9 | **Frustum culling real para objetos estáticos** (hoje só culling por tile/paridade) | ~20-30% menos draws em campo aberto | M — **cuidado**: validar contra comportamento de picking/walk |
| 10 | **Thread de streaming** (grounds/meshes) com contexto GL compartilhado | elimina hitch de troca de zona | M |

Resultado esperado: de ~2.000-5.000 draws/frame (típico D3D9 2003) para **~150-300** —
folga enorme em qualquer GPU alvo.

## 12.2 Qualidade visual (resgatando o que ficou morto)

| # | Feature | Base | Notas |
|---|---|---|---|
| 1 | **Bloom/glow real** | design do `JBlur.h` (ping-pong ¼/½, accum) + RT textures mortas do `TextureManager.cpp:1446` | FBO chain: scene → bright pass → blur H/V (¼) → composite. 3 shaders novos |
| 2 | **Motion blur** | `JBlur.h` MBVERTEX | opcional; accum buffer com feedback |
| 3 | **Reflexão de água** (`g_nReflection` existe e é zerado por caps, `RenderDevice.cpp:539-543`) | planar reflection: render-to-texture espelhado + clip plane oblique | G — o jogo já tem o gancho de config |
| 4 | **Sombras reais** | `DeclEquipShadow` (pos-only) + `m_nShadowTextureSize` existem | CSM simples ou shadow map por luz direcional; blob shadows ficam como fallback |
| 5 | **Anisotrópico** | `GL_EXT_texture_filter_anisotropic` | 1 linha por sampler; terreno melhora muito em ângulo raso |
| 6 | **MSAA 4x** | atributo de contexto | já previsto no bootstrap; `MULTISAMPLEANTIALIAS` era desligado nos blocks 2D por causa do pipeline D3D — em GL não conflita |
| 7 | **sRGB correto** | framebuffer SRGB + texturas SRGB8 | muda o visual global — decisão de produto, fazer com toggle |
| 8 | **Gamma/brightness** | uniform no composite final | substitui `SetGammaRamp` (que em windowed nem funcionava direito) |

## 12.3 Qualidade de código (o que a migração já paga)

- **Apagar ~500 linhas de device-lost** + ~60 overrides `Invalidate/RestoreDeviceObjects`.
- **Apagar quirks de GPU de 2003**: `m_iVGAID/m_bVoodoo/m_bSavage`, ALPHAREF por vendor,
  paths 16bpp, `GetAvailableTextureMem`, truncamento de UV para NVIDIA (`RenderDevice.cpp:2099-2108`).
- **Apagar código morto**: 4 VS + 6 PS de efeito, 15 vertex declarations sem shader,
  `LPD3DXFONT`, JBlur stubs, glow/bloom declarados.
- **Eliminar VB-locks por frame** (UV scroll, cor de efeito) → uniforms: também remove a
  classe de bugs de estado compartilhado na mesh cache global.
- **Estado por escopo**: RAII `ScopedState` no renderer novo — mata o padrão
  "seta e reza para restaurar" (dezenas de sites com restore manual hoje).

## 12.4 O que NÃO modernizar

- **Modelo de iluminação**: trocar FFP-Lambert por PBR destrói o visual original. Manter
  a fidelidade; no máximo specular opcional (o jogo roda com SPECULAR off).
- **Ordem de render / sort de transparentes**: qualquer reordenação quebra composição.
  O batching sempre respeita a sequência.
- **Cinemática de animação**: lerp de matriz 3:1/1:1/1:3 e crossfade de 10 ticks são parte
  do "feel" do jogo. Não trocar por slerp global.
- **LOD de paridade do terreno**: acoplado a picking/movimento (07 §7.1).
