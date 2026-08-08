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

## Fase 3 — Personagens (2 semanas) ✅ (plano/retrospectiva: `17-fase3-personagens.md`)

13. Multi-`.ani` por tipo + cuts + `m_sAnimationArray` (ch01/ch02 com slerp) — MeshManager.cpp:95-270.
14. Looks (8 partes, armas skinned) + exceções; MAX_BONES 64 (ch01 tem 47 ossos).
15. Character runtime: rota (BASE_GetRoute), altura de terreno, câmera follow, click-to-move.
16. NPCs/monstros de demo (or01/wb01) + presets das 4 classes.

Notas do estudo (doc 17 §7): **não existe blob shadow** neste client (sh01 é legado;
`m_pShadow` é billboard de stealth) — item 11 da Fase 2 cancelado; armas de humanos
são partes skinned (não há attach de .msa em bone); outline de mouse-over e montaria/
mantua → Fase 4/5.

**Saída**: personagem jogável (click-to-move), NPCs, monstros animados.

## Fase 4 — Efeitos (2-3 semanas) ✅ (plano/retrospectiva: `18-fase4-efeitos.md`)

17. EffectRenderer (batch de quads, flipbook por índice consecutivo, blends
    EF_BRIGHT/EF_DEFAULT) + sim de billboard bit-fiel (fades/motions/scaleVel).
18. Efeitos do viewer sem combate: glows de lâmpada 501-505, sol+lens flare
    (TMSun), chuva/neve (TMRain/TMSnow), folhas/borboletas/peixes (skinmesh
    61/69/24/70 via cache da Fase 3), highlight de mouse-over + pick box.
19. SWSwing/TrailRenderer, TMShade/decals, sanc glow, skills → Fase 5 (sem
    gatilho de combate nesta fase).

Notas do estudo (doc 18 §11): mouse-over é **emissive verde** no material (não
existe círculo de seleção neste build); flipbook = índices consecutivos na
EffectTextureList; fade 2 usa ponteiro como fase (substituir por índice estável).

**Saída**: mundo vivo (fogo, sol, clima, critters) + fundação de efeitos p/ skills.

## Fase 5 — Combate & VFX de skill (3 semanas) (plano: `19-fase5-combate.md`)

20. Fundação: `SkillEffect` base + `EffectContainer` no FieldView (tick/render/
    auto-delete, cull frustum) — porta o modelo `TMEffect`/`TreeNode`.
21. `SkillMeshRenderer` (`TMEffectMesh`): mesh comum como VFX, tipos 0-5
    (scale/angle por progress), texture override, EF_BRIGHT/DEFAULT/ALPHA.
22. `GroundDecalRenderer` (`TMShade`): grid (N+1)² conformado ao heightmap, UV
    rotacionado, fade; **novo shader `fx_decal`**.
23. `SwingTrailRenderer` (`TMEffectSWSwing`) — **o mais difícil do repo**:
    ribbon 32v TRIANGLESTRIP, ring buffer 48 matrizes de pose do osso-mão,
    slerp 5-frames + Vec3Lerp, tex 221, child billboards (fire/magic/enchant);
    `SetSwingMatrix` precompute + `g_dwHandIndex`/`sSwingScale` tables; **novo
    shader `fx_ribbon`**.
24. Projéteis (`TMArrow` 13 tipos, `TMCannon`, `TMFlail`): travel + arco + trail
    + impacto (mesh+decal+particle).
25. Skills one-shot (20 `TMSkill*`: Bash/MeteorStorm/Fire/Heal/MagicShield…)
    + buffs persistentes — VFX fiéis, gatilho via CLI (`--skill`/`--shield`).
26. Efeitos base (`BillBoard2/3/4`, `Spark/Spark2`, `MeshRotate`, `Particle`,
    `Charge`, `Dust`, `Firework`, `Gold`, `LevelUp`, `Start`, `Drop`).
27. Glow sanc/legend (pulse emissive) + `RenderEffect` ×15 classes de monstro
    (billboards ambiente ancorados em osso).
28. Montarias (`TMBike`) + mantuas (`SetVecMantua`): ride pose + mount skinmesh.
29. Fix fs01 fish (deformado GPU-side, Fase 4 §11).

Notas do estudo (doc 19): skills são **composições** sobre o `EffectRenderer`
da Fase 4 (Billboard) + meshes da Fase 2/3; `m_matEffectMat`/`Combine` do SWSwing
são identidade nunca atribuídos (simplificação); MagicShield/Rescue são buffs
persistentes dono=character.

**Saída**: mundo vivo com combate VFX (skills, trails, projéteis, decals,
glows de monstro, montarias). Sem netcode/dano real.

## Fase 6 — UI e fontes (3 semanas) ✅ (plano/retrospectiva: `20-fase6-ui-fontes.md`)

30. UIBatcher (batch de quads 2D, projeção ortográfica, VBO dinâmico, 1 draw por
    run de textura) + `SetMatrixForUI` (ortho) + shader `ui_quad` — doc 09 §9.1.
31. RenderRect* (10 primitivas: RenderRectC/Tex/NoTex/Coord/TexDamage/Rot/
    Tex2C/Tex2M/Progress2) via UIBatcher.
32. GLFont (stb_truetype fase 1: textura por string, cache LRU 256, cp949→UTF-8,
    sombra +1px, alinhamento 6/7*nLength) — doc 09 §9.2.
33. TMFont3 (dano flutuante: types 0-6, glyphs texturas 137-141, animação
    float-up/fade/scale, RenderRectTexDamage).
34. UI texture pipeline (512 texturas + 600 ControlTextureSets, lazy loading,
    LRU eviction).
35. RenderGeomControl + RenderGeomRectImage (dispatch por eRenderType,
    ControlTextureSet→textura, sanct/legend overlays, guild marks).
36. SControl tree (base + 13 subclasses: SPanel/SButton/SText/SEditableText/
    SListBox/SMessageBox/SCursor/SProgressBar/SScrollBar/SCheckBox/S3DObj/
    SGridControl/SMessagePanel) + FrameMove2 pipeline.
37. SControlContainer (árvore, input dispatch, modal stack, focus, binary .bin
    loading via UIBinary.h).
38. Input SDL→SControl (mouse, keyboard, char events), cursor software
    (SDL_HideCursor, layer 29), glScissor clipping.
39. Testes (6+ suítes, ≥30 ctest verdes), docs 13/README, retrospectiva §20.

**Saída**: jogo completo jogável end-to-end (HUD, inventário, chat, menus).

## Fase 7 — Paridade de plataforma (1-2 semanas) ✅ (plano/retrospectiva: `21-fase7-plataforma.md`)

34. Input completo (DirectInput→SDL): gestos de câmera fiéis (middle-drag/Alt+RMB
    0.002/0.0049, wheel com fClose, inversão, quarter-view), IME básico
    (TEXT_EDITING + input area), clipboard.
35. Áudio (DirectSound/DirectShow→miniaudio): soundlist.txt + SFX WAV polifônico
    + BGM MP3 (15 faixas), mapeamento de volume DS→linear. Vídeos de intro:
    cortados na fonte (TMVideoWnd é stub no original).
36. WinInet→HTTP mínimo sobre socket (BASE_GetHttpRequest, guild mark BMP em
    thread); sem libcurl (3 GETs simples, sem HTTPS).
37. Screenshot (PrintScreen→Capture%04d.bmp), config de vídeo (Config.bin,
    gamma=ganho linear via FBO+blit, MSAA, aniso, resolução).

Absorve leftovers pequenos: TMArrow 13-tipos data-table (Fase 5), wiring de
sons de skill/clima/UI (Fase 5/6), guild mark download + IME (Fase 6).

**Saída**: release candidato cross-platform.

## Fase 8+ — Modernizações (contínuo, 12-modernizacoes.md)

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
