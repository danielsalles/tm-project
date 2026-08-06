#include "world/SkillEffect.h"

namespace tmx {

void EffectContainer::FrameMove(uint32_t nowMs, const SkillCtx& ctx) {
    // A skill's FrameMove may itself Add() children (projectile impact spawns,
    // sub-effect bursts). push_back can reallocate m_items, so we must NOT hold
    // a reference/iterator across the callback. Reserve headroom so the first-n
    // slots stay stable (children append beyond n), and re-fetch each iteration.
    const size_t n = m_items.size();
    m_items.reserve(n + 32);
    std::vector<std::unique_ptr<SkillEffect>> alive;
    alive.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto& e = m_items[i];
        if (!e) continue;
        if (e->FrameMove(nowMs, ctx))
            alive.push_back(std::move(e));
    }
    // Children appended during the loop land in m_items at indices >= n.
    for (size_t i = n; i < m_items.size(); ++i)
        alive.push_back(std::move(m_items[i]));
    m_items = std::move(alive);
}

void EffectContainer::Render(const SkillCtx& ctx) {
    for (auto& e : m_items) {
        if (e && e->IsVisible(ctx))
            e->Render(ctx);
    }
}

} // namespace tmx
