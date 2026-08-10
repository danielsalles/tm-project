#include "pane/GuiMat.h"

#include <cstdio>
#include <sstream>

namespace tmx {

bool GuiMat::Parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    enum Section { None, Fonts, Textures, Aliases } section = None;
    int depth = 0;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // strip leading whitespace
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos)
            continue;
        line = line.substr(b);
        if (line.rfind("//", 0) == 0)
            continue;

        if (line == "{") { ++depth; continue; }
        if (line == "}") { --depth; if (depth <= 0) { section = None; depth = 0; } continue; }

        if (depth == 0) {
            if (line.rfind("FontList", 0) == 0) section = Fonts;
            else if (line.rfind("TextureList", 0) == 0) section = Textures;
            else if (line.rfind("TextureAlias", 0) == 0) section = Aliases;
            continue;
        }

        switch (section) {
        case Fonts: {
            // index,nameIdx,height,spacing,bold,italic
            int idx = 0, nameIdx = 0, height = 0;
            if (sscanf(line.c_str(), "%d,%d,%d", &idx, &nameIdx, &height) == 3)
                m_fontHeights[idx] = height;
            break;
        }
        case Textures: {
            // index flags path
            std::istringstream ls(line);
            Texture t;
            if (ls >> t.index >> t.flags >> t.path) {
                m_texByIndex[t.index] = m_textures.size();
                m_textures.push_back(std::move(t));
            }
            break;
        }
        case Aliases: {
            // name <texIndex> x1 y1 x2 y2
            std::string name;
            char lt = 0, gt = 0;
            Alias a;
            char nameBuf[128] = {};
            if (sscanf(line.c_str(), "%127s %c%d%c %d %d %d %d",
                       nameBuf, &lt, &a.texIndex, &gt, &a.x1, &a.y1, &a.x2, &a.y2) == 8
                && lt == '<' && gt == '>') {
                m_aliases[nameBuf] = a;
            }
            break;
        }
        default:
            break;
        }
    }
    return !m_textures.empty() || !m_aliases.empty();
}

const GuiMat::Texture* GuiMat::FindTexture(int index) const {
    auto it = m_texByIndex.find(index);
    if (it == m_texByIndex.end())
        return nullptr;
    return &m_textures[it->second];
}

const GuiMat::Alias* GuiMat::FindAlias(const std::string& name) const {
    auto it = m_aliases.find(name);
    return it != m_aliases.end() ? &it->second : nullptr;
}

int GuiMat::FontHeight(int index) const {
    auto it = m_fontHeights.find(index);
    return it != m_fontHeights.end() ? it->second : 12;
}

} // namespace tmx
