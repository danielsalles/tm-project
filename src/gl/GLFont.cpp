#include "gl/GLFont.h"

#include "gl/UIBatcher.h"
#include "platform/Platform.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cstring>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <vector>

namespace tmx {

// CP949 → UTF-8 conversion table for common Korean syllables.
// For the MVP, we handle ASCII directly and use a simple pass-through for
// high bytes (they'll render as '?' if the font doesn't have them, which is
// acceptable for the initial port — full IME support is Phase 7).
static int cp949_to_utf8(unsigned char byte, char* out) {
    if (byte < 0x80) {
        out[0] = (char)byte;
        return 1;
    }
    // Double-byte: emit as '?' for now (full table deferred to Phase 7)
    out[0] = '?';
    return 1;
}

bool GLFont::Init(const char* ttfPath, float fontSize, std::string* err) {
    m_fontSize = fontSize;

    FILE* f = OpenAsset(ttfPath, "rb");
    if (!f) {
        if (err) *err = std::string("font not found: ") + ttfPath;
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    m_ttfData.resize((size_t)sz);
    if (fread(m_ttfData.data(), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        if (err) *err = "font read failed";
        return false;
    }
    fclose(f);

    // stbtt_fontinfo keeps pointers INTO m_ttfData — the member must outlive it.
    m_fontData = new stbtt_fontinfo;
    if (!stbtt_InitFont((stbtt_fontinfo*)m_fontData, m_ttfData.data(),
                        stbtt_GetFontOffsetForIndex(m_ttfData.data(), 0))) {
        delete (stbtt_fontinfo*)m_fontData;
        m_fontData = nullptr;
        m_ttfData.clear();
        if (err) *err = "stbtt_InitFont failed";
        return false;
    }
    return true;
}

void GLFont::Shutdown() {
    if (m_fontData) {
        delete (stbtt_fontinfo*)m_fontData;
        m_fontData = nullptr;
    }
    for (auto& [key, st] : m_cache) {
        if (st.tex) glDeleteTextures(1, &st.tex);
    }
    m_cache.clear();
}

int GLFont::SplitLines(const char* str, char outLines[3][256], int* outWidths) const {
    int lineCount = 0;
    int lineLen = 0;
    const char* p = str;

    for (int line = 0; line < 3; ++line) {
        lineLen = 0;
        while (*p && *p != '\n' && lineLen < 42) {
            outLines[line][lineLen++] = *p++;
        }
        outLines[line][lineLen] = '\0';
        if (outWidths) outWidths[line] = GetLineWidth(outLines[line], lineLen);
        lineCount = line + 1;
        if (!*p || *p == '\n') break;
        if (*p == '\n') ++p;
    }
    return lineCount;
}

int GLFont::GetLineWidth(const char* str, int len) const {
    if (!m_fontData || !str || len <= 0) return 0;
    auto* font = (stbtt_fontinfo*)m_fontData;
    float scale = stbtt_ScaleForPixelHeight(font, m_fontSize);
    int width = 0;
    for (int i = 0; i < len; ++i) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(font, str[i], &advance, &lsb);
        width += (int)(advance * scale);
    }
    return width;
}

int GLFont::GetStringWidth(const char* str) const {
    if (!str) return 0;
    int maxWidth = 0;
    int curWidth = 0;
    for (const char* p = str; *p; ++p) {
        if (*p == '\n') {
            maxWidth = std::max(maxWidth, curWidth);
            curWidth = 0;
        } else {
            curWidth += GetLineWidth(p, 1);
        }
    }
    return std::max(maxWidth, curWidth);
}

bool GLFont::Rasterize(const char* str, uint32_t color, StringTexture& out) {
    if (!m_fontData || !str || !str[0]) return false;
    auto* font = (stbtt_fontinfo*)m_fontData;
    float scale = stbtt_ScaleForPixelHeight(font, m_fontSize);

    // Measure total size
    int maxWidth = 0;
    int totalHeight = 0;
    int lineCount = 0;
    int lineY = 0;

    char lines[3][256];
    int widths[3];
    if (m_noWrap) {
        // Pane UI: single line, no 42-char chunking (not TMFont2 semantics).
        strncpy(lines[0], str, 255);
        lines[0][255] = '\0';
        lineCount = 1;
        widths[0] = GetLineWidth(lines[0], (int)strlen(lines[0]));
    } else {
        lineCount = SplitLines(str, lines, widths);
    }

    for (int i = 0; i < lineCount; ++i) {
        maxWidth = std::max(maxWidth, widths[i]);
        totalHeight = lineY + (int)(m_fontSize + 1);
        lineY = totalHeight;
    }

    if (maxWidth <= 0 || totalHeight <= 0) return false;

    // Clamp to atlas size
    if (maxWidth > m_atlasW) maxWidth = m_atlasW;
    if (totalHeight > m_atlasH) totalHeight = m_atlasH;

    // Rasterize into RGBA8 buffer
    std::vector<unsigned char> pixels(maxWidth * totalHeight * 4, 0);

    lineY = 0;
    for (int line = 0; line < lineCount; ++line) {
        int x = 0;
        for (const char* p = lines[line]; *p; ++p) {
            int advance, lsb, x0, y0, x1, y1;
            stbtt_GetCodepointHMetrics(font, *p, &advance, &lsb);
            stbtt_GetCodepointBitmapBox(font, *p, scale, scale, &x0, &y0, &x1, &y1);

            int glyphW = x1 - x0;
            int glyphH = y1 - y0;
            int glyphY = lineY + (int)(m_fontSize) + y0;

            // Advance BEFORE the empty-glyph early-out: space has no bitmap
            // but still consumes width (words were collapsing together).
            const int adv = (int)(advance * scale);
            if (glyphW <= 0 || glyphH <= 0) {
                x += adv;
                continue;
            }

            // Render glyph bitmap
            std::vector<unsigned char> glyphBitmap(glyphW * glyphH);
            stbtt_MakeCodepointBitmap(font, glyphBitmap.data(),
                                       glyphW, glyphH, glyphW,
                                       scale, scale, *p);

            // Pack into RGBA8: white RGB + alpha from glyph
            for (int gy = 0; gy < glyphH; ++gy) {
                for (int gx = 0; gx < glyphW; ++gx) {
                    int px = x + gx + x0;
                    int py = glyphY + gy;
                    if (px < 0 || px >= maxWidth || py < 0 || py >= totalHeight)
                        continue;
                    unsigned char a = glyphBitmap[gy * glyphW + gx];
                    int idx = (py * maxWidth + px) * 4;
                    pixels[idx + 0] = 255;  // R
                    pixels[idx + 1] = 255;  // G
                    pixels[idx + 2] = 255;  // B
                    pixels[idx + 3] = a;    // A
                }
            }

            x += adv;
        }
        lineY += (int)(m_fontSize + 1);
    }

    // Apply color: multiply RGB by the color's R/G/B channels
    uint8_t cr = (color >> 16) & 0xFF;
    uint8_t cg = (color >> 8) & 0xFF;
    uint8_t cb = color & 0xFF;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = (unsigned char)((pixels[i + 0] * cr) >> 8);
        pixels[i + 1] = (unsigned char)((pixels[i + 1] * cg) >> 8);
        pixels[i + 2] = (unsigned char)((pixels[i + 2] * cb) >> 8);
    }

    // Upload to GL texture
    if (out.tex) glDeleteTextures(1, &out.tex);
    glGenTextures(1, &out.tex);
    glBindTexture(GL_TEXTURE_2D, out.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, maxWidth, totalHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);  // single level: complete
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    out.w = maxWidth;
    out.h = totalHeight;
    return true;
}

void GLFont::SetText(const char* str, uint32_t color) {
    if (!str || !str[0]) return;

    m_frameCounter++;

    // Check cache
    StringKey key;
    memset(&key, 0, sizeof(key));
    strncpy(key.str, str, 63);
    key.str[63] = '\0';
    key.color = color;
    key.size = m_fontSize;

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.lastUsed = m_frameCounter;
        m_lastTex = it->second.tex;
        m_lastW = it->second.w;
        m_lastH = it->second.h;
        return;
    }

    // Evict LRU if cache full
    if ((int)m_cache.size() >= m_cacheMax) {
        auto oldest = m_cache.begin();
        for (auto it2 = m_cache.begin(); it2 != m_cache.end(); ++it2) {
            if (it2->second.lastUsed < oldest->second.lastUsed)
                oldest = it2;
        }
        if (oldest->second.tex) glDeleteTextures(1, &oldest->second.tex);
        m_cache.erase(oldest);
    }

    // Rasterize and cache
    StringTexture st;
    if (Rasterize(str, color, st)) {
        st.lastUsed = m_frameCounter;
        m_cache[key] = st;
        m_lastTex = st.tex;
        m_lastW = st.w;
        m_lastH = st.h;
    }
}

void GLFont::Render(UIBatcher& batch, float x, float y, int renderType,
                     int layer, float scale) {
    if (!m_lastTex || m_lastW <= 0 || m_lastH <= 0) return;

    // Pixel-align: fractional positions sample the glyph texture between
    // texels and blur the text (the original GDI path renders at int coords).
    x = floorf(x);
    y = floorf(y);

    // Shadow pass (+1, +1 offset, black with alpha)
    if (renderType == 1) {  // RENDER_SHADOW
        UIQuad q;
        q.x = x + 1; q.y = y + 1;
        q.w = m_lastW * scale; q.h = m_lastH * scale;
        q.u0 = 0; q.v0 = 0; q.u1 = 1; q.v1 = 1;
        q.color = 0xFF000000;  // black shadow
        q.texIndex = -1;
        q.texHandle = m_lastTex;  // direct GL handle
        q.layer = layer;
        batch.Push(q);
    }

    // Main text
    UIQuad q;
    q.x = x; q.y = y;
    q.w = m_lastW * scale; q.h = m_lastH * scale;
    q.u0 = 0; q.v0 = 0; q.u1 = 1; q.v1 = 1;
    q.color = 0xFFFFFFFF;  // white (texture carries the color)
    q.texIndex = -1;
    q.texHandle = m_lastTex;  // direct GL handle
    q.layer = layer;
    batch.Push(q);
}

}
