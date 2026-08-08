#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <vector>

namespace tmx {

// Parsed .wys header — CPU-side so tests run without a GL context (doc 15 §5).
// Disk format: [1 byte junk][DDS without "DDS " magic][corrupted fourCC @84: '2'->DXT1 else DXT3]
// (TextureManager.cpp:299-310).
struct WysInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 0;   // levels present in the file
    bool     dxt1 = true;     // false = DXT3
    size_t   dataOffset = 0;  // offset into the file where level-0 blocks start
};

bool ParseWysHeader(const uint8_t* data, size_t size, WysInfo& out, std::string* err);

// Uploads a .wys as a compressed texture. Uploads the mip levels present in the
// file and sets GL_TEXTURE_MAX_LEVEL (glGenerateMipmap is invalid on compressed
// formats — 10 §10.2). Returns 0 on failure.
GLuint LoadTextureWYS(const uint8_t* fileBytes, size_t size);

// Uploads a .wyt ("WT10" + TGA without footer) via stb_image, with mipmaps.
// Returns 0 on failure.
GLuint LoadTextureWYT(const uint8_t* fileBytes, size_t size);

// Entry of Mesh\MeshTextureList.bin — 264-byte records, raw struct fread in the
// original (TextureManager.h:39-45, TextureManager.cpp:715-731).
struct TextureListEntry {
    char     fileName[255];   // full path, e.g. "mesh\Foo.wys"
    char     cAlpha;          // 'C'=cutout(alpha test), 'A'/'a'=alpha blend, 'N'=opaque
    uint32_t dwLastUsedTime;
    uint32_t dwShowTime;
};

class GLTextureManager {
public:
    // Loads Mesh\MeshTextureList.bin (2048 entries, 264 bytes each, field-by-field LE).
    bool LoadModelTextureList(const uint8_t* data, size_t size);

    // Loads Env\EnvTextureList3.bin. This client build's real layout is 2048
    // entries of 528 bytes (two 264B records per entry; the first is active) —
    // validated against EnvTextureList3.txt. Indices match the txt's first column.
    bool LoadEnvTextureList(const uint8_t* data, size_t size);

    // Loads Effect\EffectTextureList.bin — same 528-byte A/B record layout.
    bool LoadEffectTextureList(const uint8_t* data, size_t size);

    // Case-insensitive lookup of "mesh\<name>.wys". -1 when missing (mirrors
    // TextureManager::GetModelTextureIndex semantics).
    int FindModelTexture(const char* meshRelativeWysPath) const;

    char AlphaFlag(int index) const {
        return (index >= 0 && index < (int)m_entries.size()) ? m_entries[index].cAlpha : 'N';
    }
    char EnvAlphaFlag(int index) const {
        return (index >= 0 && index < (int)m_envEntries.size()) ? m_envEntries[index].cAlpha : 'N';
    }

    // GL texture for a list entry, loaded lazily from the data dir.
    // Returns 0 when the file is missing/unreadable.
    GLuint GetModelTexture(int index);
    GLuint GetEnvTexture(int index);
    GLuint GetEffectTexture(int index);

    // Raw list file name (for debugging / weather-driven overrides).
    const char* ModelTextureFileName(int index) const {
        return (index >= 0 && index < (int)m_entries.size()) ? m_entries[index].fileName : "";
    }

    void DestroyAll();

    // --- UI textures (Phase 6) ---
    struct ControlTextureCoord {
        int nTextureIndex;
        int nStartX, nStartY;
        int nWidth, nHeight;
        int nDestX, nDestY;
    };
    struct ControlTextureSet {
        int nCount = 0;
        std::vector<ControlTextureCoord> coords;
    };

    bool LoadUITextureList(const uint8_t* data, size_t size);
    bool LoadUITextureSetList(const char* textData, size_t textSize);
    ControlTextureSet* GetUITextureSet(int index);
    GLuint GetUITexture(int index, uint32_t showTime);
    // Pixel dimensions of a loaded UI texture (0,0 until loaded).
    void GetUITextureSize(int index, int* w, int* h);

    // --- Guild marks (Phase 7, doc 21 §6) ---
    // 64 slots of 16x12 24-bit BMPs downloaded over HTTP (original:
    // TextureManager::LoadGuildTexture + stGuildMark, TextureManager.cpp:1398).
    static constexpr int GUILD_MARK_COUNT = 64;
    static constexpr int GUILD_MARK_W = 16;
    static constexpr int GUILD_MARK_H = 12;

    // Validates the downloaded buffer (TMFieldScene::Guildmark_IsCorrectBMP):
    // 'BM', total size 630 or 632, 16x12, 24bpp. Pure — unit tested.
    static bool GuildmarkIsCorrectBMP(const uint8_t* data, size_t size);

    // Decodes + uploads the BMP into a slot. GL context required. Existing
    // content is replaced (original refuses overwrite; the download path
    // always targets a free/stale slot — Replace is the useful semantic here).
    bool LoadGuildTexture(int index, const uint8_t* data, size_t size);

    // GL texture of a slot (0 = empty).
    GLuint GetGuildMarkTexture(int index) const;
    void   ClearGuildMark(int index);

private:
    // Shared loader for the 528-byte-record lists (A half = stTextureListInfo).
    bool LoadList528(const uint8_t* data, size_t size, size_t maxEntries,
                     std::vector<TextureListEntry>& entries, std::vector<GLuint>& textures);

    std::vector<TextureListEntry> m_entries;
    std::vector<GLuint>           m_textures; // parallel to m_entries, 0 = not loaded
    std::vector<TextureListEntry> m_envEntries;
    std::vector<GLuint>           m_envTextures;
    std::vector<TextureListEntry> m_fxEntries;
    std::vector<GLuint>           m_fxTextures;

    // UI texture system
    static constexpr int UI_TEX_COUNT = 512;
    static constexpr int UI_SET_COUNT = 600;
    std::vector<TextureListEntry> m_uiEntries;
    std::vector<GLuint>           m_uiTextures;
    std::vector<int>              m_uiTexW;
    std::vector<int>              m_uiTexH;
    ControlTextureSet             m_uiSets[UI_SET_COUNT];
    bool                          m_uiSetsLoaded = false;

    // Guild marks (64 x 16x12 BMP, HTTP-downloaded)
    GLuint m_guildMarks[GUILD_MARK_COUNT] = {};
};

// Sampler objects for the phase-1 state blocks (10 §10.4).
namespace GLSamplers {
    GLuint LinearMip();    // block 1 (3D scene): LINEAR_MIPMAP_LINEAR / LINEAR, wrap
    GLuint LinearNoMip();  // block 3 (panels)
    GLuint PointNoMip();   // block 2 (fonts)
    void   Init();
    void   Destroy();
    // Anisotropic filtering on the world sampler (phase 7 config). 1 = off.
    void   SetAnisotropy(int level);
}

}
