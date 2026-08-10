#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tmx {

// .guimat (GUI material) — plaintext UTF-16LE with sections:
//   FontList     { index,nameIdx,height,spacing,bold,italic }
//   TextureList  { index flags path }
//   TextureAlias { name <texIndex> x1 y1 x2 y2 }   (two sections exist)
// Referenced by .pane files via `Resource ui/default.guimat`.
class GuiMat {
public:
    struct Texture {
        int index = 0;
        uint32_t flags = 0;
        std::string path;
    };
    struct Alias {
        int texIndex = 0;
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        int Width() const { return x2 - x1; }
        int Height() const { return y2 - y1; }
    };

    // Input: UTF-8 text (already converted from UTF-16LE).
    bool Parse(const std::string& text);

    const Texture* FindTexture(int index) const;
    const Alias* FindAlias(const std::string& name) const;
    // FontList: index → pixel height (font 2 = 12px in default.guimat).
    int FontHeight(int index) const;

    size_t TextureCount() const { return m_textures.size(); }
    size_t AliasCount() const { return m_aliases.size(); }

private:
    std::vector<Texture> m_textures;
    std::unordered_map<int, size_t> m_texByIndex;
    std::unordered_map<std::string, Alias> m_aliases;
    std::unordered_map<int, int> m_fontHeights;
};

} // namespace tmx
