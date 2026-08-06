#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "world/SkillEffect.h"
#include "math/TMMath.h"

namespace tmx {

// Weapon trail (TMEffectSWSwing), position-history approach (doc 19 §6
// deviation). Each frame during an attack the Character feeds the current
// weapon segment (base = hand bone origin, tip = hand + effectLength along the
// bone's local +Z); the trail keeps a short ring buffer and renders a 2N-vertex
// TRIANGLESTRIP ribbon (tex 221, EF_BRIGHT, gray alpha ramp along the length),
// matching the original's 32-vertex structure. Sampling is the live pose per
// frame rather than the original's precomputed 48-matrix 5-frame slerp — the
// visual result is a faithful sweep at 60fps, documented as a deviation.
class SwingTrail : public SkillEffect {
public:
    // Returns false when no segment is available this frame (trail idles).
    using SegmentFn = std::function<bool(D3DXVECTOR3& base, D3DXVECTOR3& tip)>;

    // Begin recording. dur = active recording window (ms); after it elapses the
    // trail fades out over trailMs then expires.
    void Start(uint32_t nowMs, uint32_t dur, SegmentFn fn) {
        m_fn = std::move(fn);
        m_startMs = nowMs;
        m_dur = dur;
        m_active = true;
        m_hist.clear();
    }

    bool FrameMove(uint32_t nowMs, const SkillCtx& ctx) override;
    void Render(const SkillCtx& ctx) override;

    void SetTexture(int tex) { m_texIndex = tex; }
    void SetTrailMs(uint32_t ms) { m_trailMs = ms; }

private:
    struct Seg { D3DXVECTOR3 base, tip; uint32_t t; };
    std::vector<Seg> m_hist;
    SegmentFn m_fn;
    uint32_t m_startMs = 0, m_dur = 0;
    uint32_t m_trailMs = 300;   // fade-out window after recording stops
    int      m_texIndex = 221;  // m_dwSWTextureIndex default
    bool     m_active = false;
};

} // namespace tmx
