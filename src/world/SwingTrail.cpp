#include "world/SwingTrail.h"
#include "gl/EffectRenderer.h"

#include <algorithm>
#include <cmath>

namespace tmx {

namespace { const float kPi = 3.1415927f; }

bool SwingTrail::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    (void)ctx;
    // Record a fresh segment while the attack window is open.
    if (m_active && nowMs - m_startMs < m_dur) {
        D3DXVECTOR3 base, tip;
        if (m_fn && m_fn(base, tip)) {
            // Drop near-duplicate segments (weapon barely moved) to avoid a
                // clumpy ribbon on slow frames.
            if (m_hist.empty()) {
                m_hist.push_back({ base, tip, nowMs });
            } else {
                D3DXVECTOR3 db = base - m_hist.back().base;
                D3DXVECTOR3 dt = tip - m_hist.back().tip;
                if (D3DXVec3Length(&db) > 0.001f || D3DXVec3Length(&dt) > 0.001f)
                    m_hist.push_back({ base, tip, nowMs });
            }
        }
    } else if (m_active) {
        m_active = false;   // recording done; let the tail fade out
    }

    // Age out old segments.
    const uint32_t cutoff = (m_trailMs > nowMs) ? 0 : (nowMs - m_trailMs);
    m_hist.erase(std::remove_if(m_hist.begin(), m_hist.end(),
                                [cutoff](const Seg& s) { return s.t < cutoff; }),
                 m_hist.end());

    // Expire once the trail has fully faded after the recording window.
    if (!m_active && m_hist.empty() && nowMs - m_startMs > m_dur)
        return false;
    return true;
}

void SwingTrail::Render(const SkillCtx& ctx) {
    if (!ctx.fx || m_hist.size() < 2)
        return;
    // Build a TRIANGLESTRIP: alternating base/tip from oldest to newest. UVs
    // run along the trail length (tu = 1 - i/N); the gray alpha ramps from the
    // tail (faint) to the head (bright), matching the original's t*180 curve.
    const int N = (int)m_hist.size();
    std::vector<EffectRenderer::WorldVertex> verts(N * 2);
    for (int i = 0; i < N; ++i) {
        const float along = (float)i / (float)(N - 1);   // 0 oldest .. 1 newest
        const float gray = std::min(along * 180.0f, 255.0f) / 255.0f;
        const float tu = 1.0f - along;
        // base vertex (tv = 1.0)
        verts[i * 2].x = m_hist[i].base.x;
        verts[i * 2].y = m_hist[i].base.y;
        verts[i * 2].z = m_hist[i].base.z;
        verts[i * 2].r = verts[i * 2].g = verts[i * 2].b = gray;
        verts[i * 2].a = gray;
        verts[i * 2].u = tu;
        verts[i * 2].v = 1.0f;
        // tip vertex (tv = 0.2)
        verts[i * 2 + 1].x = m_hist[i].tip.x;
        verts[i * 2 + 1].y = m_hist[i].tip.y;
        verts[i * 2 + 1].z = m_hist[i].tip.z;
        verts[i * 2 + 1].r = verts[i * 2 + 1].g = verts[i * 2 + 1].b = gray;
        verts[i * 2 + 1].a = gray;
        verts[i * 2 + 1].u = tu;
        verts[i * 2 + 1].v = 0.20f;
    }
    ctx.fx->DrawWorldStrip(verts.data(), (int)verts.size(),
                           *ctx.textures, m_texIndex, 1 /* EF_BRIGHT */);
}

} // namespace tmx
