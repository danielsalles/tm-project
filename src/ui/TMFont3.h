#pragma once

#include <cstdint>

#include "ui/UITypes.h"
#include "math/TMMath.h"

namespace tmx {

class UIBatcher;
class GLTextureManager;
class GLFont;

// Floating damage/combat text — wraps GLFont for type 0, uses pre-baked
// glyph textures (137-141) for types 1-6. Port of TMFont3.cpp.
class TMFont3 {
public:
    bool Init(GLFont* font);
    void Shutdown();

    // Set text content and animation parameters.
    void SetText(const char* str, uint32_t color, int type = 0,
                 int textureSetIndex = 0, float size = 1.0f);

    // Animation tick. Returns 0 when lifetime expired (caller should delete).
    int FrameMove(uint32_t dwServerTime);

    // Push quads into the batcher.
    void Render(UIBatcher& batch, GLTextureManager& tex, int layer);

    // Position
    void SetPosition(float x, float y) { m_startX = x; m_startY = y; m_x = x; m_y = y; }
    void SetDirection(short dir) { m_sDir = dir; }
    void SetLifeTime(uint32_t ms) { m_dwLifeTime = ms; }

    bool IsExpired() const { return m_expired; }

private:
    GLFont* m_font = nullptr;

    char m_string[64] = {};
    uint32_t m_color = 0xFFFFFFFF;
    int m_nType = 0;
    int m_nTextureSetIndex = 0;
    float m_scale = 1.0f;

    float m_startX = 0, m_startY = 0;
    float m_x = 0, m_y = 0;
    short m_sDir = 1;   // 1=up, 2=down, 3=left, 4=right

    uint32_t m_dwCreateTime = 0;
    uint32_t m_dwLifeTime = 2000;  // 2 seconds default
    bool m_expired = false;

    // Per-character glyph data for types 1-6
    struct GlyphData {
        float x, y;
        float w, h;
        int texIndex;
    };
    GlyphData m_glyphs[11] = {};
    int m_glyphCount = 0;
};

}
