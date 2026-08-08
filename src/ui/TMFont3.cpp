#include "ui/TMFont3.h"

#include "gl/UIBatcher.h"
#include "gl/GLTexture.h"
#include "gl/GLFont.h"

#include <cstring>
#include <cmath>

namespace tmx {

bool TMFont3::Init(GLFont* font) {
    m_font = font;
    return true;
}

void TMFont3::Shutdown() {
    m_font = nullptr;
}

void TMFont3::SetText(const char* str, uint32_t color, int type,
                       int textureSetIndex, float size) {
    if (str) {
        strncpy(m_string, str, 63);
        m_string[63] = '\0';
    }
    m_color = color;
    m_nType = type;
    m_nTextureSetIndex = textureSetIndex;
    m_scale = size;
    m_expired = false;
    m_glyphCount = 0;

    // For types 1-6, parse digits and build glyph data from texture sets
    if (m_nType > 0 && str) {
        const char* p = str;
        int idx = 0;
        while (*p && idx < 11) {
            char c = *p++;
            int texIdx = -1;
            float gw = 16, gh = 16;  // default glyph size

            if (c >= '0' && c <= '9') texIdx = c - '0';
            else if (c == '+') texIdx = 10;
            else if (c == '-') texIdx = 11;
            else if (c == ':') texIdx = 12;
            else if (c == 'm') texIdx = 13;
            else if (c == '?') texIdx = 14;
            else if (c == 'E') texIdx = 15;
            else if (c == 'C') texIdx = 16;
            else continue;

            m_glyphs[idx].texIndex = texIdx;
            m_glyphs[idx].w = gw;
            m_glyphs[idx].h = gh;
            idx++;
        }
        m_glyphCount = idx;

        // Center glyphs around start position
        float totalW = m_glyphCount * 16 * m_scale;
        float cx = -totalW * 0.5f;
        for (int i = 0; i < m_glyphCount; ++i) {
            m_glyphs[i].x = cx + i * 16 * m_scale;
            m_glyphs[i].y = 0;
        }
    }
}

int TMFont3::FrameMove(uint32_t dwServerTime) {
    if (m_expired) return 0;

    if (m_dwCreateTime == 0) {
        m_dwCreateTime = dwServerTime;
    }

    uint32_t elapsed = dwServerTime - m_dwCreateTime;
    if (elapsed >= m_dwLifeTime) {
        m_expired = true;
        return 0;
    }

    float progress = (float)elapsed / (float)m_dwLifeTime;

    // Float upward (direction 1 = up)
    float speed = 100.0f;
    if (m_sDir == 1) {
        m_y = m_startY - speed * progress * m_scale;
    } else if (m_sDir == 2) {
        m_y = m_startY + speed * progress * m_scale;
    } else if (m_sDir == 3) {
        m_x = m_startX - speed * progress * m_scale;
    } else if (m_sDir == 4) {
        m_x = m_startX + speed * progress * m_scale;
    }

    return 1;
}

void TMFont3::Render(UIBatcher& batch, GLTextureManager& tex, int layer) {
    if (m_expired || m_string[0] == '\0') return;

    float progress = 0;
    if (m_dwLifeTime > 0) {
        progress = (float)(/* current time - createTime */ 0) / (float)m_dwLifeTime;
    }

    // Alpha: 0-30% fade in, 30-80% visible, 80-100% fade out
    float alpha = 1.0f;
    if (progress < 0.3f)
        alpha = progress / 0.3f;
    else if (progress > 0.8f)
        alpha = (1.0f - progress) / 0.2f;

    uint32_t alphaByte = (uint32_t)(alpha * 255.0f);
    uint32_t color = (alphaByte << 24) | (m_color & 0x00FFFFFF);

    if (m_nType == 0 && m_font) {
        // Text mode: use GLFont
        m_font->SetText(m_string, color);
        m_font->Render(batch, m_x, m_y, 1, layer, m_scale);
    } else if (m_nType > 0) {
        // Glyph mode: render each digit from the texture set
        // The actual texture binding happens via the ControlTextureSet system
        // For now, emit quads that the caller can resolve
        for (int i = 0; i < m_glyphCount; ++i) {
            UIQuad q;
            q.x = m_x + m_glyphs[i].x;
            q.y = m_y + m_glyphs[i].y;
            q.w = m_glyphs[i].w * m_scale;
            q.h = m_glyphs[i].h * m_scale;
            q.u0 = 0; q.v0 = 0; q.u1 = 1; q.v1 = 1;
            q.color = color;
            q.texIndex = -1;  // resolved via ControlTextureSet by caller
            q.layer = layer;
            batch.Push(q);
        }
    }
}

}
