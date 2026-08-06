#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace tmx {

class GLRenderDevice;
class EffectRenderer;
class GLTextureManager;
struct GLMesh;
struct TerrainData;
class SkinPipeline;
class CharacterAnimationCache;
class SkillMeshRenderer;
class GroundDecalRenderer;
class EffectContainer;

// Per-frame context handed to every SkillEffect. FieldView fills it before the
// container ticks/renders. Skill effects draw meshes via device.DrawMesh, emit
// billboard quads via fx.Emit, sample terrain height via terrain, and load
// common meshes via getMesh (FieldView::GetMesh by MeshList index).
struct SkillCtx {
    GLRenderDevice*        device   = nullptr;
    EffectRenderer*        fx       = nullptr;
    GLTextureManager*      textures = nullptr;
    SkinPipeline*          skin     = nullptr;   // skinned skill meshes (may be null)
    CharacterAnimationCache* charCache = nullptr;
    const TerrainData*     terrain  = nullptr;
    SkillMeshRenderer*     skillMesh = nullptr;   // TMEffectMesh path (phase 5)
    GroundDecalRenderer*   decal     = nullptr;   // TMShade path (phase 5)
    EffectContainer*       host      = nullptr;   // for spawning child effects (impacts)

    // Camera frame (mirrors FieldView::FxFrameInfo) for billboarding/cull.
    float camYawH = 0, camPitchV = 0;
    int   screenW = 0, screenH = 0;
    float focusX = 0, focusH = 0, focusZ = 0;
    float viewRight[3] = { 1, 0, 0 };
    float viewUp[3]    = { 0, 1, 0 };

    // Common-mesh lookup by MeshList index (FieldView::GetMesh); may return null.
    std::function<GLMesh*(int)> getMesh;
};

// Base of the combat/skill VFX tree (doc 19 §2). Simulation on CPU; FrameMove
// returns false once the effect has expired and should be removed from the
// container. Render draws translucent parts (meshes/decals with zwrite off) and
// emits billboard quads into ctx.fx; called every visible frame, after the
// opaque scene, before EffectRenderer::Flush.
class SkillEffect {
public:
    virtual ~SkillEffect() = default;
    // Returns false when the effect is finished and should be deleted.
    virtual bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) = 0;
    virtual void Render(const SkillCtx& ctx) { (void)ctx; }
    // Frustum/radius cull hook; default always visible.
    virtual bool IsVisible(const SkillCtx& ctx) const { (void)ctx; return true; }

    uint32_t StartTime() const { return m_startMs; }
    void     SetStart(uint32_t nowMs) { m_startMs = nowMs; }
protected:
    uint32_t m_startMs = 0;
};

// Holds live skill/projectile/effect objects. FieldView owns one; Add() takes
// ownership. FrameMove ticks + sweeps dead effects; Render draws survivors.
class EffectContainer {
public:
    void Add(std::unique_ptr<SkillEffect> e) {
        if (e) m_items.push_back(std::move(e));
    }
    size_t Count() const { return m_items.size(); }
    void Clear() { m_items.clear(); }

    // Tick every effect and remove those whose FrameMove returned false. nowMs
    // is the local game clock (same one fed to the billboard sim).
    void FrameMove(uint32_t nowMs, const SkillCtx& ctx);

    // Render survivors (translucent pass). Cull- Invisible effects are skipped.
    void Render(const SkillCtx& ctx);

private:
    std::vector<std::unique_ptr<SkillEffect>> m_items;
};

// Sets start time on an effect right after construction (helper used by the
// skill factories so lifetimes are anchored to the game clock).
template <class T, class... Args>
std::unique_ptr<T> MakeSkillEffect(uint32_t nowMs, Args&&... args) {
    auto p = std::make_unique<T>(std::forward<Args>(args)...);
    p->SetStart(nowMs);
    return p;
}

} // namespace tmx
