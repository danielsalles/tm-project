# 19 — Fase 5: Combate & VFX de skill (plano completo)

**Objetivo**: todo o sistema de efeitos de combate que a Fase 4 deferiu —
**skills** (20 arquivos `TMSkill*`), **trail de arma** (`TMEffectSWSwing`, o mais
difícil do repo: ribbon 32v + ring buffer de 48 matrizes + slerp 5-frames),
**projéteis** (`TMArrow`/`TMCannon`/`TMFlail`), **decals de terreno** (`TMShade`),
**meshes de skill** (`TMEffectMesh`/`TMEffectSkinMesh`/`TMEffectMeshRotate`),
**efeitos base** (`TMEffectBillBoard2/3/4`, `TMEffectSpark/Spark2`, `Charge`,
`Dust`, `Firework`, `Gold`, `LevelUp`, `Particle`, `Start`, `Fire`, `Drop`),
**glow de sanc/legend + ambiente por classe de monstro** (`TMHuman::RenderEffect`
+ 15 `RenderEffect_*`), **montarias/mantuas** (`TMBike`/ride pose) e o
**fix do peixe fs01** (deformado GPU-side, Fase 4 §11). Tudo reusa a fundação da
Fase 4 (`EffectRenderer`, `Billboard`) + skin pipeline da Fase 3; sem netcode —
gatilhos via flags CLI (`--skill`, `--arrow`, `--swing`, `--mount`).

> **Nota de escopo**: o roadmap original (doc 13) reservava a Fase 5 para UI &
> fontes. A retrospectiva da Fase 4 deferiu combate para cá; UI passa para a
> **Fase 6**. Justificativa: combate é continuação temática da Fase 4 (todos os
> VFX de skill são composições sobre o `EffectRenderer`), menor risco
> arquitetural, e fecha o ciclo "mundo vivo com combate" antes do pivot de UI.

**Pré-requisito**: Fase 4 merged (`EffectRenderer`, `Billboard`, `SkinPipeline`,
`CharacterMesh`, `CharacterAnimationCache`, `TerrainData` heightmap, pick).

**Estado final (DoD)**:
- [ ] `SkillEffect` base + `EffectContainer` no FieldView: tick (`FrameMove` por
      tempo local) + render + auto-delete por lifetime, cull por câmera
      (`IsVisible` portado). **Ponto único de entrada** p/ todas as skills.
- [ ] `SkillMeshRenderer` (`TMEffectMesh`): desenha `TMMesh` comum como VFX,
      blend EF_BRIGHT/DEFAULT/ALPHA, tipos 0-5 (scale/angle por progress),
      texture override, cor tint. Reusa pipeline de mesh estática da Fase 2.
- [ ] `TMEffectSkinMesh` via `CharacterMesh`: skill meshes skinned (viajam
      start→target, LOOK_INFO). Reusa 100% da Fase 3.
- [ ] `GroundDecalRenderer` (`TMShade`): grid (N+1)² conformado ao heightmap,
      UV rotacionado por `m_fAngle`, fade por lifetime, blend
      EF_BRIGHT/DEFAULT. **Novo shader** `fx_decal`.
- [ ] `SwingTrailRenderer` (`TMEffectSWSwing`): ribbon 32v TRIANGLESTRIP,
      ring buffer 48 matrizes de pose do osso-mão, **slerp 5-frames** +
      Vec3Lerp, tex 221 EF_BRIGHT; child billboards (fire/magic/enchant/gold)
      + modo `cSForce` (meshes 10/19/20). Tabela `sSwingScale[22]`. **Novo
      shader** `fx_ribbon`.
- [ ] Projéteis (`TMArrow` 13 tipos 10000-10003/104/105/151-153, `TMCannon`,
      `TMFlail`): travel start→target (lerp + arco opcional), trail por frame,
      impacto spawn (mesh+decal+particle+billboards). Composite.
- [ ] Skills one-shot (Bash/DoubleSwing/SlowSlash/FreezeBlade, Fire/Poison/Snow,
      MeteorStorm 10 níveis, MagicArrow/IceSpear, Heal/Cure/HolyTouch,
      ThunderBolt/Judgement, Haste/SpeedUp/SpChange, TownPortal, Flash,
      Explosion2, LusterFurnish, HeavenDust, MagicShield) — VFX fiéis.
- [ ] Buffs persistentes (`MagicShield`/`Rescue`/`Cancelation`/`EleStream`):
      toggle no character via flag.
- [ ] Efeitos base: `BillBoard2` (anel expandente), `BillBoard3` (beam),
      `BillBoard4`, `Spark`/`Spark2` (linhas), `MeshRotate`, `Particle`,
      `Charge`, `Dust`, `Firework`, `Gold`, `LevelUp`, `Start`, `Fire`,
      `Drop` (item drop girando).
- [ ] Glow sanc/legend + `RenderEffect` por classe (15 monstros: Skull, Golem,
      BoneDragon, EmeraldDragon, Minotauros, DarkElf, DarkNightZombieTroll,
      Hydra, DungeonBear, Pig/Wolf, Khepra, LegendBeriel/Keeper, Rudolph) —
      billboards ancorados em osso, spawn por intervalo.
- [ ] Montarias (`TMBike`): ride pose (`ECMOTION_SEATING`), `m_pMount`
      (TMSkinMesh tipo hs01/40), `m_stMountLook`, cloak `SetVecMantua`.
- [ ] **Fix fs01**: peixe renderiza correto (isolar defeito GPU-side da Fase 4).
- [ ] Testes: +8 suítes (skillmesh, decal, swing, projectile, skills-golden,
      spawneffect, mounts, fisheffect); **30+/30+ verdes**.
- [ ] `Projects/` intocado; docs 13/README atualizados.

**Duração estimada**: 3 semanas (15 dias úteis) — SWSwing + decals + 20 skills
é volume maior que as fases anteriores.

---

## 1. Escopo — o que entra e o que NÃO entra

### Entra

| Item | Fonte original | Linhas |
|---|---|---|
| `SkillEffect` base + `EffectContainer` | `TMEffect.{h,cpp}`, `TMFieldScene` container loop | ~63 |
| `SkillMeshRenderer` (`TMEffectMesh`) | `TMEffectMesh.cpp` | 461 |
| `TMEffectSkinMesh` (skill meshes skinned) | `TMEffectSkinMesh.cpp` | 646 |
| `GroundDecalRenderer` (`TMShade`) | `TMShade.cpp` | 361 |
| `SwingTrailRenderer` (`TMEffectSWSwing`) | `TMEffectSWSwing.cpp` + `SetSwingMatrix` (TMSkinMesh:779) + `InitEffect` (CMesh:937) | 850+ |
| Projéteis | `TMArrow.cpp`, `TMCannon.cpp`, `TMFlail.cpp` | 811+135+ |
| Skills one-shot (20) | `TMSkillBash`…`TMSkillTownPortal` | ~3500 |
| Buffs persistentes | `TMSkillMagicShield` (407) + dono em TMHuman | 407 |
| Efeitos base | `TMEffectBillBoard2/3/4`, `Spark/Spark2`, `MeshRotate`, `Particle`, `Charge`, `Dust`, `Firework`, `Gold`, `LevelUp`, `Start`, `Fire`, `Drop` | ~2500 |
| Glow sanc/legend + `RenderEffect` ×15 | `TMHuman::RenderEffect*` (8198+), `CMesh::SetMaterial` legend | ~1500 |
| Montarias/mantuas | `TMBike.cpp`, `TMHuman::UpdateMount` (14743), `SetVecMantua` | ~400 |
| Fix fs01 | `TMFish.cpp` × nosso `Critter.cpp` | — |

### NÃO entra (defer explícito)

| Item | Motivo | Vai para |
|---|---|---|
| **UI & fontes** (UIBatcher, TMFont2/3, SControl, minimap, guild marks) | Pivot arquitetural maior (renderer 2D + raster fontes) | **Fase 6** |
| Dano/rede/alvo real (HP, hit detection servidor, packets) | Sem netcode | Fase 8 (rede) |
| `TMEffectSkinMesh` com mirror/2º mesh exótico (`m_bMirror`) | Caso raro de skill; 95% do uso é 1 mesh | Fase 7 se surgir |
| Sombra dinâmica / blob shadow real | Não existe neste build (Fase 3 §7 confirmou) | — |
| Áudio das skills (`GetSoundAndPlay`) | Sistema de som | Fase 6 |
| `TMEffectFirework` komplejo de evento / `TMShip` | Casos de cena específica | Fase 7 |

---

## 2. Fundação: `SkillEffect` + `EffectContainer`

O original tem `TMEffect : TreeNode` com `FrameMove(dwServerTime)` + `Render()` +
`IsVisible()`, e o `m_pEffectContainer` do FieldScene percorre a árvore chamando
os três por frame, removendo quem se auto-deleta (`g_pObjectManager->DeleteObject`).
Portamos esse modelo de forma enxuta:

```cpp
// src/world/SkillEffect.h
struct SkillEffect {
    virtual ~SkillEffect() = default;
    virtual int  FrameMove(uint32_t nowMs) = 0;   // retorna 0 = deletar
    virtual void Render(EffectRenderer& fx,
                        SkinPipeline& skin,
                        GLRenderDevice& dev) = 0;
    virtual bool IsVisible(const CameraFrustum&) const { return true; }
    D3DXMATRIX world; TMVector3 pos; uint32_t startTime, lifeTime;
};
class EffectContainer {
    std::vector<std::unique_ptr<SkillEffect>> items;
public:
    void Add(std::unique_ptr<SkillEffect> e);
    void FrameMove(uint32_t nowMs);   // chama FrameMove, remove os que retornam 0
    void Render(...);                 // opaque (mesh/skin) primeiro, depois alpha
};
```

- **Tempo**: `nowMs` = nosso relógio local (já usado por Billboard/Critter).
- **Ordem de render**: opacos (SkillMesh/SkinMesh com zwrite) → decals (TMShade,
  zwrite off) → alpha (Billboards/Swing/Spark, EF_BRIGHT por último).
- **Cull**: `IsVisible` porta o teste do `TMEffect::IsVisible` (frustum AABB pelo
  `m_fRadius` + posição) — nosso FieldView já tem frustum da câmera.
- **Auto-delete**: skill retorna 0 de `FrameMove` quando `now-startTime > lifeTime`
  (padrão do original: `g_pObjectManager->DeleteObject(this)`).

## 3. `SkillMeshRenderer` (`TMEffectMesh`)

Desenha um **mesh comum** (`MeshManager::GetCommonMesh`) como VFX. `src/gl/SkillMeshRenderer.{h,cpp}`:

```cpp
struct SkillMeshDesc {
    int meshIndex;            // índice na MeshManager (comum)
    uint32_t color;           // BGRA tint (override dos vértices)
    float angle, angle2, angle3;  // rotação Y/X/Z
    float scaleH = 1, scaleV = 1;
    int   type = 0;           // 0..5 animação por progress
    int   textureIndex = -1;  // override da tex 0
    uint32_t lifeTime = 0, cycleTime = 1000;
    int   alphaType = EF_BRIGHT;  // EF_BRIGHT/DEFAULT/ALPHA
};
```

- Render: `g_pMeshManager->GetCommonMesh(meshIndex)` → desenha via pipeline de mesh
  estática da Fase 2, mas com **material/emissive zerado** (só tex × tint), blend
  por `alphaType` (EF_BRIGHT = SRCALPHA/ONE). Tint aplicado multiplicando a cor
  do vértice (lock do VB no original; nosso: uniform `uTint` no shader).
- Tipos (FrameMove): `progress = (now % cycleTime) / cycleTime`.
  - **0**: estático (só angle).
  - **1**: texture override fixa.
  - **2**: `scaleV = scaleH × progress × 3`; `angle = progress × π` (expansão).
  - **3**: `angle = progress × π` (rotação pura).
  - **4**: ring de impacto — `progress<0.2` cresce `(p×5+0.5)`, depois
    `sin((p-0.2)×π/2)+1.5` × scale; texture override.
  - **5**: texture override fixa (variante).
- Lifetime>0 → deleta quando `now-startTime > lifeTime`.
- `m_fRadius` vem do mesh (cull correto).
- Exemplos de uso: mesh 531 (anel de impacto de flecha), 28 (trail de flecha),
  meshes 10/19/20 do `cSForce` do SWSwing, 800/871-877 (corpos de flecha).

**Atenção**: o original faz `pMesh->Render(x,y,z, angle,angle2,angle3, 0,0)` que
aplica world = Scale×Rot×Trans internamente. Nosso TMMesh portado tem o mesmo
helper — reusar.

## 4. `TMEffectSkinMesh` (skill meshes skinned)

Skill que renderiza um **mesh skinned animado** (ex: fera deslizante, elemental).
**Reusa 100% o `CharacterMesh` da Fase 3** — é um `CharacterMesh` com 1 parte,
LOOK_INFO específica, que viaja start→target.

```cpp
struct SkillSkinMesh : SkillEffect {
    int skinMeshType;        // ex 20 (mantua), beasts de skill
    LOOK_INFO look; SANC_INFO sanc;
    TMVector3 start, target; int level; int motionType;
    float angle, scale = 1;
    // render: CharacterMesh(skinMeshType, look, sanc) + SetPosition(curPos)
    //         + SetMotion(motionType) ; skin.Render(...)
};
```

- `lifeTime = 200 × distância` (clamp 1..5000), `progress = (now-start)/lifeTime`.
- `curPos = lerp(start, target, progress)`; deleta ao chegar.
- Dtor do original (`TMEffectSkinMesh::~`) spawna `TMSkillFire` no target em
  alguns tipos — portar o caso comum (spawn de splash).
- `m_pSkinMesh->m_vScale = (scale,scale,scale)`, FPS 80, `m_bBaseMat=0`.
- Caso `skinMeshType==20 && look.Mesh0==7`: `SetVecMantua(3, 20)` (mantua voadora).
- **Defer**: `m_bMirror` e 2º mesh (`m_pSkinMesh2`) — só 2 skills usam; Fase 7.

## 5. `GroundDecalRenderer` (`TMShade`)

Decal conformado ao terreno. `src/gl/GroundDecalRenderer.{h,cpp}` + shader
`fx_decal.{vert,frag}`:

- Grid `(N+1)²` (N = `nGridNum`, tipicamente 1-7), índices 6×N² (2 tri/célula).
- **Conformação**: para cada vértice (x,z) = `(x+nX)×2, (y+nY)×2` (grid de 2
  unidades), amostra **heightmap do terreno** (`TerrainData::HeightAt(wx,wz)`) +
  offset 0.05; quirk: se `maskHeight - tileHeight ≥ 0.3` usa maskHeight (água?
  portar o teste do original `TMShade::SetPosition` linhas 75-130).
- **UV**: `fU = (pos.x - vtx.x)/(N×2)`, `fV = -(pos.z - vtx.z)/(N×2)`;
  rotação por `m_fAngle`: `tu = (-cos·U - sin·V) - 0.5`, `tv = (sin·U - cos·V) - 0.5`.
- **Fade**: `lifeTime>0` → `fAlpha = |sin(progress·180°)|` (FI=1) ou
  `|cos(progress·90°)|` (FI=0); multiplica ARGB. `nFade` flag inverte pré/post.
  Quirk: últimos 3 vértices do grid≥3 vão p/ alpha 0 (borda suave).
- **Blend**: EF_BRIGHT = SRCALPHA/ONE, ALPHATEST off, zwrite off, fog off;
  EF_DEFAULT = SRCALPHA/INVSRCALPHA + fog vertex mode.
- Render: `DrawIndexedPrimitiveUP` (nosso: VBO dinâmico + IBO estático por grid).
- Textura: effect texture index (ex 7 = lightmap, 118 = impacto, 55 = sanc).
- **Nossa altura**: já temos `TerrainData::HeightAt`; o maskData do original
  (blend água/borda) — portar se visível (teste rápido: sem mask primeiro).

## 6. `SwingTrailRenderer` (`TMEffectSWSwing`) — o mais difícil

Trail de arma: ribbon que acompanha o osso da mão. `src/gl/SwingTrail.{h,cpp}`
+ shader `fx_ribbon.{vert,frag}`. **Ciclo de vida**:

1. **Criação** (`CMesh::InitEffect`, CMesh.cpp:937): quando uma parte skinned
   cujo `m_dwID == g_dwHandIndex[boneAniIndex][0/1]` é anexada, cria o
   `TMEffectSWSwing`, percorre a **cadeia de ossos pais** (`FindFrame(parentID)`)
   gravando em `m_dwIndices[48]` até chegar na raiz → `m_dwNumIndex`. Registra
   como `m_pSwingEffect[0/1]` do `TMSkinMesh`.
2. **Precompute** (`TMSkinMesh::SetSwingMatrix`, TMSkinMesh.cpp:779): a cada
   `SetMotion`, para cada tick `i` da animação atual (≤48), computa
   `m_matRot[i] = matHand × ∏(matParent[m_dwIndices[j]])` multiplicando a cadeia
   pelas matrizes de animação `pmatStart[handIndex + numBones×i]`. Ou seja:
   **pré-assa a pose completa do braço+arma para cada frame da animação**.
3. **Por frame** (`FrameMove`): `m_matEffect = m_matCombined` do osso-mão vivo
   (setado em `CFrame.cpp:101/156`). O `FrameMove` então:
   - `nIndex = (now-start)/(100×30/FPS)` (rescale por FPS da anim).
   - Pega **5 frames consecutivos** `m_matRot[nIndex-3..nIndex+1]`, extrai
     quaternion + translação, faz **slerp** + **Vec3Lerp** interpolando `dwStart`
     em passos de 8ms para gerar 16 pares de vértices (32 total).
   - `m_matEffectMat`/`m_matEffectCombine` são **identidade** no repo (grepped:
     nunca atribuídos) → simplifica: `matTrans = matRot interpolado × Identity`,
     translação sobrescrita por `m_matEffect._41/_42/_43` (posição da mão viva).
   - UV: `tu = 1 - i/30`, `tv` = 1.0 (topo) / 0.20 (base); diffuse = rampa
     `t×180` (gray ramp p/ fade da ponta).
   - Spawna **child billboards** conforme flags (intervalos): `m_nHandEffect`
     (fogo azul/mana), `m_cFireEffect`, `m_cAssert` (fogo laranja), `m_cMagicWeapon`
     (magia), `m_cArmorClass`, `m_bEnchant` (azul persistente `m_pEnchant`),
     `m_cGoldPiece` (ouro).
   - `m_cMixEffect` (80-144): spawna `TMEffectSpark2` colorido (mix de encantamento).
4. **Render**: TRIANGLESTRIP 30 primitivas, tex 221, EF_BRIGHT (SRCALPHA/ONE),
  zwrite off, cull NONE. Se `cSForce` (skill especial 1-5): em vez do ribbon,
  desenha 3 meshes (10/19/20) rotacionados + escalados por `m_fEffectLength`.

**Tabela** `sSwingScale[22]` (weapon type → scale, só 33=1.3, resto 1.0).
**`m_dwSWTextureIndex`** default 221 (pode mudar p/ enchant gold etc).
**Trigger** (TMHuman.cpp:2716+): `m_dwStartTime = now` ao atacar (motion attack);
`m_fEffectLength = m_fSowrdLength[h] × fEffectLen` (comprimento da arma × fator
por nível de skill). `m_fSowrdLength` = `pMesh->m_fMaxZ` do mesh da arma.

**Nossa implementação**:
- `g_dwHandIndex[boneAniIdx][2]` — tabela de índice do osso-mão por tipo; portar.
- `SetSwingMatrix` reusa `CharacterAnimationCache` (já temos `matAnimation`).
- Ribbon shader: pos/color/uv, world por vértice (CPU entrega mundo), `tex×color`,
  `FixZ`. Igual ao `fx_quad` mas TRIANGLESTRIP em vez de fan — pode até ser o
  mesmo shader com topologia diferente.
- **Demo sem combate**: flag `--swing` dispara um swing de demo (SetMotion
  attack + `m_dwStartTime=now`) no character focado, num loop, p/ validar.

## 7. Projéteis (`TMArrow`/`TMCannon`/`TMFlail`)

Composite: viaja start→target, spawna trail/impacto. `src/world/Projectile.{h,cpp}`:

```cpp
struct Projectile : SkillEffect {
    int type;            // 10000-10003, 104/105, 151/152/153 (arrow); cannon; flail
    int level, color, destID; char avatar;
    TMVector3 start, target, cur; int meshIndex;
    float angle, rotAngle;
    SkillMesh trail;     // TMEffectMesh opcional (28)
    Beam* beam;          // BillBoard3 opcional
};
```

- `lifeTime = fator × distância` (fator por tipo: 50/70/100/150). `progress =
  (now-start)/lifeTime`. `cur = lerp(start, target, progress)`.
- Arco: tipo 152 level 2 → `cur.y += sin(progress·180°·4)·0.1`. Tipo 153 →
  `angle = (now%300/300)·180°·2` (rotação).
- Trail por frame: spawn `TMEffectBillBoard(0/11, 1000, …)` EF_BRIGHT stickGround
  em `cur.y-0.5` (cor por tipo/level).
- Mesh corpo: `GetCommonMesh(meshIndex)` desenhado orientado por `angle`/`rotAngle`.
- **Impacto** (`ReleaseEffect`, ao `progress≥1`):
  - `TMEffectMesh(531, color, angle, 4)` — anel de impacto (tipo 4 expandente).
  - `TMShade(7, 118, 1.0)` — decal de impacto no chão (cor por `m_nColor` 5-8).
  - `TMEffectParticle` (se critical) — 5 faíscas + 2 billboards 230 (slash arcs)
    com `m_nParticleType 14/15`, `lookCam=0`, `vecRotAxis=dir`.
  - `TMEffectBillBoard2(8, …)` — anel expandente (alguns tipos).
  - Som (defer Fase 6).
- **Tabela de mesh por tipo/level** (TMArrow.cpp ctor): 151→800, 152→871-877,
  153→873-909/37/767/2814/2921, 104→879, etc. Portar verbatim.
- `TMCannon` (135 linhas): canhão — mesh rotativo, sim. `TMFlail`: mangual
  (relacionado a SWSwing duplo).

## 8. Skills (20 `TMSkill*`)

Cada skill é uma **receita VFX** que instancia os blocos acima + billboards.
`src/world/Skills.{h,cpp}` (um arquivo, funções-fábrica):

| Skill | Composição (do fonte) |
|---|---|
| **Bash** | spawn `Explosion2` + a cada 250ms `SpeedUp` rand |
| **DoubleSwing** | 2 `Explosion2` + slashes; `SWSwing.cSForce` |
| **SlowSlash** | slash lento + `Particle` |
| **FreezeBlade** | gelo: `BillBoard` azuis + `Mesh` + `Shade` |
| **Fire** | `BillBoard` fogo (tex 11) + `Shade` laranja + dano residual |
| **Poison** | `BillBoard` verde (tex 56) + `Shade` |
| **Snow** | `BillBoard` flocos + `Shade` azul |
| **MeteorStorm** (10 níveis) | travel + `BillBoard`/`BillBoard2`/`Shade`/`Particle` por nível; o maior (751 linhas) |
| **MagicArrow** | `Arrow` mágica + splash |
| **IceSpear** | lança de gelo (mesh + trail) |
| **Heal/Cure/HolyTouch** | `BillBoard` verde/dourado ascendente + `Mesh` anel |
| **ThunderBolt** | raio (`BillBoard3` beam + `Spark`) |
| **Judgement** | julgamento — `Mesh` + `Shade` grande |
| **Haste/SpeedUp** | aura nos pés (`Shade` + `BillBoard`) |
| **SpChange** | mudança de SP — partículas |
| **TownPortal** | portal giratório (`MeshRotate` + `BillBoard`) |
| **Flash** | flash branco expansivo |
| **Explosion2** | explosão genérica (`BillBoard2` + `Particle`) — usado por Bash/DoubleSwing |
| **LusterFurnish** | brilho ascendente |
| **HeavenDust** | poeira celestial |
| **MagicShield** (buff) | escudo ao redor (persistente, dono=character) |

- **Gatilho CLI**: `--skill <nome>,x,z[,level,targetX,targetZ]`. Ex:
  `--skill meteor,192,192,6` / `--skill bash,192,192,1` / `--skill meteorstorm,0,0,9`
  (level 9 = start==target, chuva no ponto).
- **Buffs**: `--shield on/off` no character focado (toggle `MagicShield`).
- Cada skill vira um `struct : SkillEffect` com `FrameMove`/`Render` que chama
  os blocos. **Sem lógica de dano/alvo** — só VFX (o real HP vem de packet).

## 9. Efeitos base (blocos compostos)

| Classe | O que faz | Port |
|---|---|---|
| `TMEffectBillBoard2` (239) | **Anel expandente**: ring de billboards radiais; scaleVel + count | novo `BillboardRing` no EffectRenderer |
| `TMEffectBillBoard3` (185) | **Beam**: linha entre 2 pontos com tex scroll | novo `BeamRenderer` (2 tris) |
| `TMEffectBillBoard4` (221) | variante screen-space / ícone | extensão do EffectRenderer |
| `TMEffectSpark` (159) / `Spark2` (104) | **Linhas/faíscas** (line list com scroll) | novo `SparkRenderer` (LINES) |
| `TMEffectMeshRotate` (346) | mesh girando contínuo (portais/escudos) | SkillMesh + rotação per-frame |
| `TMEffectParticle` (132) | batch de N `BillBoard` com offsets/direções | recipe sobre Billboard |
| `TMEffectCharge` (207) | carga acumulativa (pre-cast) | recipe |
| `TMEffectDust` (300) | poeira ao mover/impactar | recipe |
| `TMEffectFirework` | fogos (evento) | recipe (defer parcial) |
| `TMEffectGold` | flash dourado (drop) | recipe |
| `TMEffectLevelUp` (131) | anel level-up | recipe |
| `TMEffectStart` (205) | efeito de início de cast | recipe |
| `TMFireEffect` (75) | fogo residual pequeno | recipe |
| `TMDrop` (137) | item drop: mesh girando + billboard brilho | SkillMesh + Billboard |

**Estratégia**: `BillboardRing`, `BeamRenderer`, `SparkRenderer` são 3 novos
caminhos no `EffectRenderer` (além do quad). O resto são recipes (sem GL novo).

## 10. Glow sanc/legend + `RenderEffect` por classe

**Sanc/legend** (`CMesh::SetMaterial`, já parcial): itens `sLegendType 116-125`
com `sMultiType>0` boostam emissive p/ ≥0.55 e alpha 0.89 — **pulsado por
`sin((now%10000)/10000)`** (vConst.z). Nosso skin.frag já tem `uEmissiveAdd`;
adicionar o pulso (uniform `uTime` + sin no frag) e a regra legend no material.

**`RenderEffect` ×15** (`TMHuman.cpp:8198+`): dispatcher por `m_nClass` que
spawna billboards ambiente ancorados em `m_vecTempPos[osso]` a cada intervalo:
- Skull/Golem (eye fire), BoneDragon/EmeraldDragon ( brasas), Minotauros
  (pó), DarkElf (sombra), Hydra, DungeonBear, Pig/Wolf, **Khepra** (areia
  vermelha descendo), LegendBeriel/Keeper (boss flag `m_bShowBoss`),
  RudolphCostume (brilho natalino).
- Cada um = ~30-80 linhas de `new TMEffectBillBoard(...)` com cor/pos/vel
  específicas. Reuso total do Billboard da Fase 4.
- `m_vecTempPos[]` = posições de ossos (olhos/mão) — já expostas pelo
  `CharacterMesh` (precisamos exportar bone-world positions; hoje só usamos
  para SWSwing `m_matEffect`). Adicionar `Character::BoneWorld(idx)`.

## 11. Montarias (`TMBike`) + mantuas

- `m_pMount` = `TMSkinMesh(m_stMountLook, m_stMountSanc, m_nMountSkinMeshType)`
  (outro `CharacterMesh` na nossa arquitetura). Tipos: hs01 (cavalo), 40 (mantua
  voadora do air-move), beasts de ride.
- Character entra em `ECMOTION_SEATING`; mount renderiza **antes** do rider
  (z-order), ambos na mesma posição.
- `SetVecMantua(tipo, skinIdx)` (TMSkinMesh:829): ajusta `m_matMantua`
  (yaw/pitch do cloak) por tipo — tabela `fMantuaUp` por skinIdx (25/28/20/39/
  29/31/30/38/40). Portar a tabela.
- `m_fMountScale` × `m_fScale` (class 40 só mountScale).
- **Demo**: `--mount <type>` no character focado (ride pose + mount mesh).

## 12. Fix fs01 (peixe deformado)

Fase 4 §11: sim CPU validada (posição/pose sãs), defeito GPU-side. Hipóteses a
isolar (bisect com env-flags, como no bug do EffectRenderer):
1. **Escala não aplicada**: TMFish seta `m_vScale.xyz = m_fScale` (1.0-2.0) no
   `TMSkinMesh`; nosso `Critter`/`CharacterMesh` pode estar ignorando scale ou
   aplicando dobro (world × scale × world).
2. **Stride/FVF do tipo 70**: fs01 pode ter FVF/stride diferente (palette
   maior?) não coberto pelo `GLSkinMesh::Upload` — clamp `numPalette>40→40`
   stale (resumo aponta isso).
3. **`Render(0.0, 1.0, 0.0)`**: TMFish passa escala parcial (Y=0, X=1, Z=0) —
   achatar? Nosso CharacterMesh não tem essa assinatura.
4. **Bone count do fs01** > MAX_BONES ou palette clamp.

Plano: rodar fs01 isolado com RenderDoc, comparar world matrix / palette vs
ch01 (que funciona). Provavelmente #1 ou #2.

## 13. CLI (demo triggers)

```
--skill <nome>,x,z[,level[,tx,tz]]   # one-shot skill (meteor/bash/fire/...)
--arrow sx,sz,tx,tz[,type[,level]]    # projétil (default type 152)
--swing [loop]                        # trail de arma no char focado
--mount <skinMeshType>                # ride (40=mantua, hs01 type)
--shield on|off                       # buff MagicShield no char focado
--class N                             # spawna monstro classe N (RenderEffect)
```

## 14. Testes

| Teste | Conteúdo |
|---|---|
| `test_skillmesh` | Tipos 0-5: scale/angle por progress batem; texture override; lifetime delete |
| `test_decal` | Grid (N+1)²; UV rotacionado por angle; conformação heightmap (mock); fade sin/cos; borda alpha-0 |
| `test_swing` | Ring buffer 48 matrizes; slerp 5-frames em indices conhecidos; sSwingScale table; child-spawn throttles (fire/enchant); cSForce mesh scale por modo |
| `test_projectile` | Travel lerp; arco tipo 152/153; meshIndex por tipo/level; impacto spawn (mesh+decal); lifetime=dist×fator |
| `test_skills_golden` | Receita de 6 skills representativas (Bash/Heal/MeteorStorm L6/Fire/MagicShield/Explosion2): sub-efeitos esperados por progress |
| `test_spawneffect` | EffectContainer: Add/FrameMove/Render/delete; cull frustum; ordem opaque→decal→alpha |
| `test_mounts` | ride pose; mount mesh tipo; SetVecMantua tabela fMantuaUp por skinIdx |
| `test_fisheffect` | fs01: scale aplicado; pose sã; (regressão do fix) |

Sem GL obrigatória p/ sim (SkillMesh/Decal/Swing são CPU; render só valida estado).

## 15. Ordem de execução (TODO)

1. `SkillEffect` base + `EffectContainer` no FieldView + CLI `--skill` stub
2. `SkillMeshRenderer` (`TMEffectMesh`) + tipos 0-5 + `test_skillmesh`
3. `GroundDecalRenderer` (`TMShade`) + `fx_decal` + `test_decal`
4. `SwingTrailRenderer` (`TMEffectSWSwing`): ring buffer + slerp + ribbon shader
   + `SetSwingMatrix` hook + `--swing` demo + `test_swing` — **bloco mais longo**
5. Projéteis (`TMArrow` tipos principais) + `test_projectile`
6. Skills one-shot (6 representativas + recipes) + `test_skills_golden`
7. Efeitos base (`BillboardRing`/`Beam`/`Spark` + recipes Dust/Particle/etc.)
8. Buffs persistentes (`MagicShield`/Rescue) + `--shield`
9. `RenderEffect` ×15 + sanc/legend pulse + `--class N`
10. Montarias/mantuas + `--mount`
11. **Fix fs01** (bisect) + `test_fisheffect` regressão
12. Docs (13/README/este §16), CI, PR

**Critério de aceite final**: flecha voando com trail + anel/decal de impacto;
meteor storm caindo (L6) com glows+decals; char atacando mostra trail de espada
(SWSwing); orc com glow ambiente da classe; char montado; peixe fs01 reto;
30+ suítes verdes.

## 16. Arquivos novos / alterados (previsto)

**Novos**
- `src/gl/SkillMeshRenderer.{h,cpp}`, `src/gl/GroundDecalRenderer.{h,cpp}`,
  `src/gl/SwingTrail.{h,cpp}`, `src/gl/shaders/fx_decal.{vert,frag}`,
  `src/gl/shaders/fx_ribbon.{vert,frag}`
- `src/world/SkillEffect.{h,cpp}` (base + `EffectContainer`),
  `src/world/SkillSkinMesh.{h,cpp}` (`TMEffectSkinMesh`),
  `src/world/Projectile.{h,cpp}` (Arrow/Cannon/Flail),
  `src/world/Skills.{h,cpp}` (20 recipes), `src/world/SkillExtras.{h,cpp}`
  (Ring/Beam/Spark/Particle/Charge/Dust/LevelUp/Start/Drop/Fire),
  `src/world/MonsterFx.{h,cpp}` (`RenderEffect` ×15),
  `src/world/Mount.{h,cpp}` (`TMBike` + ride pose),
  `src/world/HandBoneTable.h` (`g_dwHandIndex`, `sSwingScale`)
- `tests/test_skillmesh.cpp`, `test_decal.cpp`, `test_swing.cpp`,
  `test_projectile.cpp`, `test_skills_golden.cpp`, `test_spawneffect.cpp`,
  `test_mounts.cpp`, `test_fisheffect.cpp`

**Alterados**
- `src/scene/FieldView.{h,cpp}` — `EffectContainer`, render passes (opaque→
  decal→alpha), spawn de skills/arrows por CLI, frustum cull
- `src/world/Character.{h,cpp}` — `BoneWorld(idx)`, ride pose, mount mesh,
  `SetShield`, swing trigger, `RenderEffect` hook
- `src/world/CharacterMesh.{h,cpp}` — expose bone-world positions; fix fs01 scale
- `src/gl/EffectRenderer.{h,cpp}` — caminhos Ring/Beam/Spark (além do quad)
- `src/gl/SkinPipeline.{h,cpp}` / `skin.frag` — `uTime` p/ pulse sanc/legend
- `src/platform/main.cpp` — flags `--skill/--arrow/--swing/--mount/--shield/--class`
- `tests/CMakeLists.txt`, `migration-opengl/13-roadmap.md`, `README.md`
