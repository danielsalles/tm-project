# Migração do Renderer D3D9 → OpenGL 4.1 Core

Documentação completa da migração do cliente TMProject (WYD) para OpenGL 4.1 core profile,
compatível com macOS, Linux e Windows. Baseada em auditoria linha-a-linha do código
(109 arquivos `.cpp`, ~237 headers/fontes analisados).

## Sumário executivo

O cliente **não** é fixed-function puro, como aparenta. A realidade:

| Camada | Técnica atual | Complexidade de porte |
|---|---|---|
| Meshes estáticos (cenário, itens) | FVF + SetTransform + texture stage combiners | Média |
| Personagens (skinned) | **8 vertex shaders vs.1.1 pré-compilados** (`Shader/skinmesh1-8.bin`), paleta de ossos em constantes c9+ | Média (shaders precisam ser **reescritos em GLSL** — o bytecode é insubstituível) |
| Terreno | `DrawPrimitiveUP` por tile (centenas de draws/frame), LOD por paridade | Média |
| Efeitos (~45 classes) | Partículas CPU, 1 draw call por partícula, `TRIANGLEFAN` | Baixa individualmente; volume alto |
| UI | Quads RHW pré-transformados + `ID3DXSprite`, draw por quad | Baixa |
| Fontes | GDI `TextOut` → DIB → textura A4R4G4B4 por string | Média (substituir por stb_truetype) |
| Texturas | Formato custom `.wys` (DDS truncado DXT1/DXT3) e TGA truncado, color key | Baixa |
| Pós-process (blur/bloom/glow) | **Stub morto** — JBlur nunca foi implementado | Zero (implementar do zero se quiser) |
| Device lost / D3DPOOL_MANAGED | Máquina Invalidate/Restore espalhada por tudo | **Deletar — não existe em GL** |

## As 5 decisões-chave (detalhadas em 04-convencoes.md)

1. **Manter convenção left-handed e z∈[0,1] do D3D.** Todo o gameplay (picking, culling
   por tile em `TMGround.cpp:3230`, flare em `TMSun.cpp:118`) assume isso. Em GL 4.1 não
   existe `glClipControl` (é 4.5), então: matrizes D3D-style idênticas + uma linha de
   correção de z no final de cada vertex shader (`gl_Position.z = 2*gl_Position.z - gl_Position.w`).
2. **Reimplementar a API de math do D3DX como shim header-only** (~40 funções, row-major,
   `v*M`). Com 295 usos de `D3DXVECTOR3` e 116 de `D3DXMATRIX`, um shim preserva todos os
   call-sites e elimina a classe inteira de bugs de transposição.
3. **Skinning em GPU via UBO** (`mat4 uBones[64]`), mantendo a cinemática em CPU
   (lerp/slerp de `m_matRot` + `UpdateFrames` recursivo) exatamente como hoje.
4. **Efeitos viram batchers instanciados**: 7 programas GLSL cobrem todos os ~45 efeitos.
   Neve sai de 200 draw calls para 1.
5. **Apagar a máquina de device-lost** (`InvalidateDeviceObjects`/`RestoreDeviceObjects`/
   `TestCooperativeLevel`/`Reset3DEnvironment`) — contexto GL não se perde.

## Mapa dos documentos

| Arquivo | Conteúdo |
|---|---|
| `01-auditoria.md` | Como o renderer funciona hoje, com referências arquivo:linha |
| `02-arquitetura-alvo.md` | Design do novo renderer (camadas, classes, donos de estado) |
| `03-setup-build.md` | CMake, SDL3, glad, glm, stb — bootstrap completo com código |
| `04-convencoes.md` | LH/RH, depth [0,1], winding, half-texel, row/col-major, shim D3DX |
| `05-renderdevice.md` | RenderDevice → GL: state blocks, blend table, FVF→VAO, fog/luzes |
| `06-meshes-skinning.md` | Loaders .msa/.msh/.bon/.ani, VAO layouts, UBO de ossos, GLSL skinning |
| `07-cena.md` | Terreno, céu, sol, mar, sombras blob, picking |
| `08-efeitos.md` | Batcher instanciado, tabela dos 45 efeitos, blend modes |
| `09-ui-fontes.md` | UI batcher, TMFont2 → stb_truetype, cursor |
| `10-texturas.md` | `.wys`/`.wyt`, DXT1/3, color key, política de mipmaps |
| `11-shaders.md` | Catálogo completo de shaders GLSL com código de referência |
| `12-modernizacoes.md` | Onde ganhar performance e qualidade visual |
| `13-roadmap.md` | Fases ordenadas, critérios de validação, riscos |
| `14-fase0-fundacao.md` | Plano completo da Fase 0: CMake, CI, shim D3DX, testes dourados |

## Stack recomendada

- **Contexto/janela/input**: SDL3 (também cobre áudio depois; GLFW como alternativa mínima)
- **Loader GL**: glad2 (core 4.1, sem compat)
- **Math**: shim próprio compatível com D3DX (ver decisão 2); GLM opcional para código novo
- **Imagens**: stb_image (TGA) + loader DXT próprio (~100 linhas) para `.wys`
- **Fontes**: stb_truetype
- **Build**: CMake + Ninja, C++17, presets para macOS/Linux/Windows

## Estimativa de escopo

~10-14 programas GLSL substituem: 8 VS de skinning + 4 blocos de estado + dezenas de
combinadores de texture stage. ~90% dos arquivos de gameplay (`TMHuman`, `TMObject`,
lógica de cena, rede, UI-modelo) **não mudam** — só as chamadas de render no final de
cada `Render()` são redirecionadas para o novo backend.
