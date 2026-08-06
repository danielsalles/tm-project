# 13 — Roadmap

Critério de saída de cada fase: **validação visual contra o cliente D3D original**
(screenshots lado a lado + capturas RenderDoc dos dois).

## Fase 0 — Fundação (1-2 semanas)

1. CMake + SDL3 + glad + janela vazia rodando nos 3 OS (03-setup-build.md).
2. CI matrix (macOS/Ubuntu/Windows) verde.
3. `TMMath.h` shim D3DX completo + testes unitários contra valores dourados do D3D real
   (multiplicações, LookAtLH, PerspectiveFovLH, IntersectTri, QuaternionSlerp).
4. Stub de plataforma: file IO, `timeGetTime`→SDL, sockets (CPSock quase drop-in).

**Saída**: jogo compila e linka nos 3 OS sem chamar nada de render (Render() stubs).

## Fase 1 — Triângulo na tela (1 semana)

5. `GLRenderDevice` esquelético: Clear/Swap, StateCache, `common.glsl`, UBO de frame.
6. Loader `.wys` + `.msa` + VAOs estáticos; `mesh_lit` MODULATE.
7. Renderizar **uma cena de login/Seleção de servidor** com meshes estáticos.

**Saída**: primeira cena real na tela, câmera fixa.

## Fase 2 — Mundo 3D (2-3 semanas) ✅ (plano/retrospectiva: `16-fase2-mundo-3d.md`)

8. Terreno (VBO+IBO por frame, combiner, fog) — 07 §7.1.
9. Céu + sol + mar (shaders dedicados, crossfade de clima) — 07 §7.2-7.4.
10. Objetos estáticos + alpha test + 2 luzes + point lights — 05 §5.5.
11. Blob shadows — 07 §7.5 (versão CPU). **→ movido para a Fase 3** (sombras são
    desenhadas pelo dono — TMHuman/TMObject — não pelo mundo).
12. Picking de terreno/humanos funcionando (deve sair de graça com o shim).

Notas da execução (doc 16 §7): árvores/casas usam `TMSkinMesh` (pipeline de skinning
entra na Fase 2, não na 3 — Fase 3 fica quase de graça no render); "Leaf" (311-322)
são partículas de folha caindo → Fase 4.

**Saída**: andar pelo campo com câmera, dia/noite, clima. Sem personagens ainda.

## Fase 3 — Personagens (2 semanas)

13. Loaders `.msh/.bon/.ani` + árvore CFrame (cinemática inalterada) — 06.
14. `skinned.vert` (4 variantes) + UBO de ossos + armas na mão — 06 §6.3.
15. Outline de mouse-over (2º passe) — `CMesh.cpp:665-754`.
16. TMHuman completo: montaria, capa, crossfade de animação.

**Saída**: personagem jogável, NPCs, monstros animados.

## Fase 4 — Efeitos (2-3 semanas)

17. QuadBatchRenderer + blend modes + flipbook (bind por textura na 1ª versão) — 08.
18. Portar efeitos por categoria (tabela 08 §8.3), dos triviais aos difíceis.
19. TrailRenderer (SWSwing) por último.
20. Clima (chuva/neve instanciados), TMShade, TMFont3 (dano).

**Saída**: combate completo com todas as skills.

## Fase 5 — UI e fontes (2 semanas)

21. UIBatcher + conversão dos `RenderRect*` + remoção dos half-texel offsets — 09.
22. TMFont2 → stb_truetype (fase 1: textura por string + cache) — 09 §9.2.
23. RENDER_3DOBJ (ícones 3D), grids, cursor software.
24. Guild marks, minimap.

**Saída**: jogo completo jogável end-to-end.

## Fase 6 — Paridade de plataforma (1-2 semanas)

25. Input completo (DirectInput→SDL), IME básico.
26. Áudio (DirectSound→SDL_audio/miniaudio), vídeos de intro (cortar ou libmpv).
27. WinInet→libcurl (guild mark download, patch).
28. Screenshot, config de vídeo (gamma→uniform, MSAA, aniso).

**Saída**: release candidato cross-platform.

## Fase 7+ — Modernizações (contínuo, 12-modernizacoes.md)

Instancing de skinned, atlas de flipbook, bloom real, reflexão de água, sombras,
streaming thread.

## Riscos ranqueados

| # | Risco | Probabilidade | Mitigação |
|---|---|---|---|
| 1 | **Convenção de coordenadas errada em algum subsistema** (z-test, picking, UI) | Alta se apressar a fase 0 | Testes dourados do shim; `FIX_Z` único; validação por cena desde a fase 1 |
| 2 | **Combinador de texture stage perdido** → bioma/item com visual errado | Média | Log de `CombinerKey` desconhecida em runtime + fallback magenta; grep sistemático dos 187 sites de `D3DTSS_COLOROP` |
| 3 | **Skinning visualmente divergente** (interpolação, paleta, transposição) | Média | A/B screenshot por animação; unit test da paleta (bind×world) contra dump do cliente D3D |
| 4 | **Fontes com métricas divergentes** quebrando layout da UI | Média | Fase 1 conservadora (textura por string); tunar métricas contra screenshots |
| 5 | **Ordem de render alterada pelo batching** → transparências erradas | Média | Batches só dentro de runs; nunca sort global (08 §8.2) |
| 6 | **Blend DESTALPHA do flare** sem alpha no backbuffer | Baixa | `SDL_GL_ALPHA_SIZE=8` + clear alpha 1.0 (03) |
| 7 | **DXT no macOS/ARM** | Baixa | checagem de extensão + fallback CPU |
| 8 | Scope creep de modernização durante a fase 1 | Alta | disciplina: fase 1 = paridade, ponto. 12-modernizacoes.md é fase 2 |

## Ordem de ataque recomendada dentro do código

```
math/TMMath.h          → gl/GLShaderLibrary, GLStateCache
→ gl/GLTextureManager  → gl/GLMeshManager (estáticos)
→ gl/GLRenderDevice (fachada) + troca mecânica dos ~60 DrawPrimitiveUP diretos
→ TMGround → TMSky/TMSun/TMSea → TMObject/TMHouse/TMGate
→ CMesh/TMSkinMesh/CFrame → TMHuman
→ TMEffect* (45 arquivos, diffs cirúrgicos via DrawQuad/DrawStrip da fachada)
→ UI (SControl via RenderGeomControl — ponto único de estrangulamento!) → TMFont2
```

Nota feliz: a UI inteira passa por `RenderGeomControl` (`RenderDevice.cpp:3249`) e os
efeitos por meia dúzia de primitivas — os pontos de estrangulamento são poucos e bem
definidos. A maioria dos 109 `.cpp` nem sabe que a API mudou.
