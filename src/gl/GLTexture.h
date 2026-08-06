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

    // Case-insensitive lookup of "mesh\<name>.wys". -1 when missing (mirrors
    // TextureManager::GetModelTextureIndex semantics).
    int FindModelTexture(const char* meshRelativeWysPath) const;

    char AlphaFlag(int index) const {
        return (index >= 0 && index < (int)m_entries.size()) ? m_entries[index].cAlpha : 'N';
    }

    // GL texture for a list entry, loaded lazily from the data dir.
    // Returns 0 when the file is missing/unreadable.
    GLuint GetModelTexture(int index);

    void DestroyAll();

private:
    std::vector<TextureListEntry> m_entries;
    std::vector<GLuint>           m_textures; // parallel to m_entries, 0 = not loaded
};

// Sampler objects for the phase-1 state blocks (10 §10.4).
namespace GLSamplers {
    GLuint LinearMip();    // block 1 (3D scene): LINEAR_MIPMAP_LINEAR / LINEAR, wrap
    GLuint LinearNoMip();  // block 3 (panels)
    GLuint PointNoMip();   // block 2 (fonts)
    void   Init();
    void   Destroy();
}

}
