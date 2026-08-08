#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tmx {

class UIBatcher;

// stb_truetype-based font renderer (Phase 6, §3). Replaces the GDI TextOut →
// DIB → A4R4G4B4 texture pipeline of TMFont2.
// Architecture: one string = one texture (cache LRU by (string, color, fontSize)).
struct StringKey {
    char str[64];
    uint32_t color;
    float size;
    bool operator==(const StringKey& o) const {
        return color == o.color && size == o.size && strcmp(str, o.str) == 0;
    }
};
struct StringKeyHash {
    size_t operator()(const StringKey& k) const {
        size_t h = std::hash<std::string>()(k.str);
        h ^= std::hash<uint32_t>()(k.color) << 1;
        h ^= std::hash<float>()(k.size) << 2;
        return h;
    }
};
struct StringTexture {
    GLuint tex = 0;
    int w = 0, h = 0;
    uint32_t lastUsed = 0;
};

class GLFont {
public:
    bool Init(const char* ttfPath, float fontSize, std::string* err);
    void Shutdown();

    // Rasterize string → texture (cached). Color is ARGB.
    void SetText(const char* str, uint32_t color);

    // Push quads into the UIBatcher for rendering.
    // renderType: 0=RENDER_TEXT, 1=RENDER_SHADOW
    void Render(UIBatcher& batch, float x, float y, int renderType,
                int layer, float scale = 1.0f);

    // Measure string width in pixels.
    int GetStringWidth(const char* str) const;

    // Measure a single line width.
    int GetLineWidth(const char* str, int len) const;

    void SetSize(float s) { m_fontSize = s; }
    float GetSize() const { return m_fontSize; }

    // Split text into up to 3 lines (42 chars each, like original TMFont2).
    int SplitLines(const char* str, char outLines[3][44], int* outWidths) const;

    // Access to the last rendered texture (for RenderGeomControl text overlay).
    GLuint GetLastTexture() const { return m_lastTex; }
    int GetLastWidth() const { return m_lastW; }
    int GetLastHeight() const { return m_lastH; }

private:
    bool Rasterize(const char* str, uint32_t color, StringTexture& out);

    void* m_fontData = nullptr;          // stbtt_fontinfo internal
    std::vector<unsigned char> m_ttfData; // TTF bytes — must outlive m_fontData
    float m_fontSize = 12.0f;
    int m_atlasW = 512, m_atlasH = 64;  // texture dimensions (match original)

    std::unordered_map<StringKey, StringTexture, StringKeyHash> m_cache;
    int m_cacheMax = 256;
    uint32_t m_frameCounter = 0;

    // Last rendered state
    GLuint m_lastTex = 0;
    int m_lastW = 0, m_lastH = 0;
    int m_lineCount = 0;
    float m_lineX[3] = {};
    float m_lineY[3] = {};
    float m_lineW[3] = {};
};

}
