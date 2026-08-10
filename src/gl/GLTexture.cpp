#include "gl/GLTexture.h"

#include "platform/Platform.h"

#include <SDL3/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <cctype>

namespace tmx {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// .wyt = "WT10" + TGA without the 18-byte footer (validated on mesh/cbt054.wyt:
// uncompressed BGR(A), type 2). stb_image parses the TGA directly (footer not
// needed) and handles the bottom-left origin flip.
// NOTE: public (declared in GLTexture.h) — used by the pane system (phase 8d).
} // namespace (temporarily, for LoadTextureWYT's external linkage)

GLuint LoadTextureWYT(const uint8_t* fileBytes, size_t size) {
    if (size < 5 || memcmp(fileBytes, "WT10", 4) != 0) {
        Log("LoadTextureWYT: bad magic");
        return 0;
    }
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(fileBytes + 4, (int)(size - 4), &w, &h, &comp, 4);
    if (!pixels) {
        Log("LoadTextureWYT: stb decode failed (%s)", stbi_failure_reason());
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    return tex;
}

namespace {

GLuint LoadListTexture(const char* fileName, GLuint& slot) {
    if (slot)
        return slot;
    FILE* f = OpenAsset(fileName, "rb");
    if (!f) {
        Log("texture missing: %s", fileName);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes((size_t)sz);
    if (fread(bytes.data(), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 0;
    }
    fclose(f);
    // Dispatch by extension: .wys = mangled DDS (DXT), .wyt = WT10 + TGA.
    const size_t len = strlen(fileName);
    if (len >= 4 && tolower((unsigned char)fileName[len - 1]) == 't')
        slot = LoadTextureWYT(bytes.data(), bytes.size());
    else
        slot = LoadTextureWYS(bytes.data(), bytes.size());
    return slot;
}

// DDS_HEADER field offsets, relative to the start of the magic ("DDS ").
// The .wys skips 1 junk byte, then behaves like a full DDS for our purposes
// (TextureManager.cpp:301-307 patches magic + fourCC in place at these offsets).
constexpr size_t kOffHeight  = 12;
constexpr size_t kOffWidth   = 16;
constexpr size_t kOffMips    = 28;
constexpr size_t kOffFourCC  = 84;
constexpr size_t kOffData    = 128;

uint32_t DxtBlockSize(bool dxt1) { return dxt1 ? 8 : 16; }

uint32_t DxtLevelBytes(uint32_t w, uint32_t h, bool dxt1) {
    uint32_t bw = (w + 3) / 4;
    uint32_t bh = (h + 3) / 4;
    return bw * bh * DxtBlockSize(dxt1);
}

} // namespace

bool ParseWysHeader(const uint8_t* data, size_t size, WysInfo& out, std::string* err) {
    auto fail = [&](const char* msg) {
        if (err) *err = msg;
        return false;
    };

    if (size < 1 + kOffData)
        return fail("file too small for DDS header");

    const uint8_t* dds = data + 1; // skip the junk byte
    out.height    = ReadU32(dds + kOffHeight);
    out.width     = ReadU32(dds + kOffWidth);
    out.mipLevels = ReadU32(dds + kOffMips);
    out.dxt1      = (dds[kOffFourCC] == '2');
    out.dataOffset = 1 + kOffData;

    if (out.width == 0 || out.height == 0 || out.width > 8192 || out.height > 8192)
        return fail("implausible dimensions");
    if (out.mipLevels == 0)
        out.mipLevels = 1;

    // Validate the blob actually holds every advertised level.
    size_t need = out.dataOffset;
    uint32_t w = out.width, h = out.height;
    for (uint32_t i = 0; i < out.mipLevels; ++i) {
        need += DxtLevelBytes(w, h, out.dxt1);
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
    }
    if (need > size)
        return fail("truncated mip chain");

    return true;
}

GLuint LoadTextureWYS(const uint8_t* fileBytes, size_t size) {
    WysInfo info;
    if (!ParseWysHeader(fileBytes, size, info, nullptr)) {
        Log("LoadTextureWYS: parse failed");
        return 0;
    }

    GLenum format = info.dxt1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                              : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    size_t pos = info.dataOffset;
    uint32_t w = info.width, h = info.height;
    for (uint32_t i = 0; i < info.mipLevels; ++i) {
        uint32_t bytes = DxtLevelBytes(w, h, info.dxt1);
        glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)i, format, (GLsizei)w, (GLsizei)h,
                               0, (GLsizei)bytes, fileBytes + pos);
        pos += bytes;
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)(info.mipLevels - 1));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool GLTextureManager::LoadList528(const uint8_t* data, size_t size, size_t maxEntries,
                                   std::vector<TextureListEntry>& entries,
                                   std::vector<GLuint>& textures) {
    constexpr size_t kEntry = 528;
    size_t count = size / kEntry;
    if (count > maxEntries)
        count = maxEntries;

    entries.resize(count);
    textures.assign(count, 0);
    for (size_t i = 0; i < count; ++i) {
        // Current client layout (v769.2, stTextureListInfo with szFilePart):
        //   [0:255]   szFileName   ("mesh\bird0101.wys")
        //   [255:510] szFilePart   ("mesh\bird0101" — name without extension)
        //   [510]     cAlpha       ('N'/'A'/'C')
        //   [512:528] dwLastUsedTime, dwShowTime, dwLastUsedTimeOld, dwShowTimeOld
        const uint8_t* p = data + i * kEntry;
        memcpy(entries[i].fileName, p, 255);
        entries[i].fileName[254] = '\0';
        entries[i].cAlpha = (char)p[510];
        entries[i].dwLastUsedTime = ReadU32(p + 512);
        entries[i].dwShowTime = ReadU32(p + 516);
        if (entries[i].cAlpha == 0 || entries[i].cAlpha == (char)0xCD)
            entries[i].cAlpha = 'N';
    }
    return count > 0;
}

bool GLTextureManager::LoadModelTextureList(const uint8_t* data, size_t size) {
    // 528-byte A/B record layout (validated on MeshTextureList.bin).
    return LoadList528(data, size, 4096, m_entries, m_textures);
}

bool GLTextureManager::LoadEnvTextureList(const uint8_t* data, size_t size) {
    // Validated against EnvTextureList3.txt: entry i at i*528 (doc 16 §2.2).
    return LoadList528(data, size, 2048, m_envEntries, m_envTextures);
}

bool GLTextureManager::LoadEffectTextureList(const uint8_t* data, size_t size) {
    // Effect\EffectTextureList.bin — same layout (600 entries in this build).
    return LoadList528(data, size, 1024, m_fxEntries, m_fxTextures);
}

int GLTextureManager::FindModelTexture(const char* meshRelativeWysPath) const {
    // Extension-insensitive match (like the original's GetModelTextureIndex, which
    // copies the list extension onto the query): .msa files name textures with
    // stale extensions (".tga"!), and TMSkinMesh queries ".wyt" where the list
    // has ".wys". Compare dir + basename, ignore the extension.
    auto stemEquals = [](const char* a, const char* b) {
        size_t i = 0;
        while (a[i] && b[i] && a[i] != '.' && b[i] != '.') {
            char ca = a[i] == '\\' ? '/' : (char)tolower((unsigned char)a[i]);
            char cb = b[i] == '\\' ? '/' : (char)tolower((unsigned char)b[i]);
            if (ca != cb)
                return false;
            ++i;
        }
        const bool aEnd = !a[i] || a[i] == '.';
        const bool bEnd = !b[i] || b[i] == '.';
        return aEnd && bEnd;
    };
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].fileName[0] == '\0')
            continue;
        if (stemEquals(m_entries[i].fileName, meshRelativeWysPath))
            return (int)i;
    }
    return -1;
}

GLuint GLTextureManager::GetModelTexture(int index) {
    if (index < 0 || index >= (int)m_entries.size())
        return 0;
    return LoadListTexture(m_entries[index].fileName, m_textures[index]);
}

GLuint GLTextureManager::GetEnvTexture(int index) {
    if (index < 0 || index >= (int)m_envEntries.size())
        return 0;
    if (m_envEntries[index].fileName[0] == '\0')
        return 0;
    return LoadListTexture(m_envEntries[index].fileName, m_envTextures[index]);
}

GLuint GLTextureManager::GetEffectTexture(int index) {
    if (index < 0 || index >= (int)m_fxEntries.size())
        return 0;
    if (m_fxEntries[index].fileName[0] == '\0')
        return 0;
    return LoadListTexture(m_fxEntries[index].fileName, m_fxTextures[index]);
}

void GLTextureManager::DestroyAll() {
    for (GLuint t : m_textures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    for (GLuint t : m_envTextures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    for (GLuint t : m_fxTextures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    for (GLuint t : m_uiTextures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    for (GLuint& t : m_guildMarks) {
        if (t)
            glDeleteTextures(1, &t);
        t = 0;
    }
    m_textures.clear();
    m_entries.clear();
    m_envTextures.clear();
    m_envEntries.clear();
    m_fxTextures.clear();
    m_fxEntries.clear();
    m_uiTextures.clear();
    m_uiEntries.clear();
    m_uiSetsLoaded = false;
}

bool GLTextureManager::LoadUITextureList(const uint8_t* data, size_t size) {
    bool ok = LoadList528(data, size, UI_TEX_COUNT, m_uiEntries, m_uiTextures);
    m_uiTexW.assign(m_uiEntries.size(), 0);
    m_uiTexH.assign(m_uiEntries.size(), 0);
    return ok;
}

bool GLTextureManager::LoadUITextureSetList(const char* textData, size_t textSize) {
    // Text format: SetName\r\nSetIndex: N\r\nItemCount: M\r\n
    //   texIndex,startX,startY,width,height,destX,destY\r\n...
    // Parse line by line.
    if (!textData || textSize == 0)
        return false;

    std::string content(textData, textSize);
    size_t pos = 0;
    int currentSet = -1;

    auto nextLine = [&]() -> std::string {
        size_t end = content.find('\n', pos);
        std::string line;
        if (end == std::string::npos) {
            line = content.substr(pos);
            pos = content.size();
        } else {
            line = content.substr(pos, end - pos);
            pos = end + 1;
        }
        // Strip \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        return line;
    };

    while (pos < content.size()) {
        std::string line = nextLine();
        if (line.empty())
            continue;

        // Try to parse "SetIndex: N"
        if (line.compare(0, 10, "SetIndex: ") == 0) {
            currentSet = atoi(line.c_str() + 10);
            if (currentSet >= 0 && currentSet < UI_SET_COUNT)
                m_uiSets[currentSet].coords.clear();
            continue;
        }

        // Try to parse "ItemCount: M"
        if (line.compare(0, 11, "ItemCount: ") == 0) {
            if (currentSet >= 0 && currentSet < UI_SET_COUNT)
                m_uiSets[currentSet].nCount = atoi(line.c_str() + 11);
            continue;
        }

        // Try to parse coord line: "texIndex,startX,startY,width,height,destX,destY"
        if (currentSet >= 0 && currentSet < UI_SET_COUNT && isdigit((unsigned char)line[0])) {
            ControlTextureCoord coord = {};
            if (sscanf(line.c_str(), "%d,%d,%d,%d,%d,%d,%d",
                       &coord.nTextureIndex, &coord.nStartX, &coord.nStartY,
                       &coord.nWidth, &coord.nHeight, &coord.nDestX, &coord.nDestY) == 7) {
                m_uiSets[currentSet].coords.push_back(coord);
            }
        }
    }
    m_uiSetsLoaded = true;
    return true;
}

GLTextureManager::ControlTextureSet* GLTextureManager::GetUITextureSet(int index) {
    if (index < 0 || index >= UI_SET_COUNT || !m_uiSetsLoaded)
        return nullptr;
    return &m_uiSets[index];
}

GLuint GLTextureManager::GetUITexture(int index, uint32_t showTime) {
    if (index < 0 || index >= (int)m_uiEntries.size())
        return 0;
    if (m_uiEntries[index].fileName[0] == '\0')
        return 0;
    // Lazy-load on first access (same pattern as model textures)
    (void)showTime;
    GLuint tex = LoadListTexture(m_uiEntries[index].fileName, m_uiTextures[index]);
    if (tex && index < (int)m_uiTexW.size() && m_uiTexW[index] == 0) {
        // Cache the pixel size once — UV normalization needs it
        // (RenderDevice queries D3D texture desc; we query GL level 0).
        GLint w = 0, h = 0;
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_uiTexW[index] = w;
        m_uiTexH[index] = h;
    }
    return tex;
}

void GLTextureManager::GetUITextureSize(int index, int* w, int* h) {
    if (w) *w = (index >= 0 && index < (int)m_uiTexW.size()) ? m_uiTexW[index] : 0;
    if (h) *h = (index >= 0 && index < (int)m_uiTexH.size()) ? m_uiTexH[index] : 0;
}

// --- Guild marks (Phase 7) ---

bool GLTextureManager::GuildmarkIsCorrectBMP(const uint8_t* data, size_t size) {
    // TMFieldScene.cpp:22678-22697 — 'BM', file size 630|632, 16x12, 24bpp.
    if (!data || size < 54)
        return false;
    const uint16_t bfType = (uint16_t)(data[0] | (data[1] << 8));
    if (bfType != 19778)
        return false;
    const uint32_t bfSize = (uint32_t)(data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24));
    if (bfSize != 630 && bfSize != 632)
        return false;
    // BITMAPINFOHEADER at offset 14: width(4) height(4) planes(2) bitCount(2)
    const int32_t biW = (int32_t)(data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24));
    const int32_t biH = (int32_t)(data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24));
    const uint16_t biBits = (uint16_t)(data[28] | (data[29] << 8));
    return biW == GUILD_MARK_W && biH == GUILD_MARK_H && biBits == 24;
}

bool GLTextureManager::LoadGuildTexture(int index, const uint8_t* data, size_t size) {
    if (index < 0 || index >= GUILD_MARK_COUNT)
        return false;
    if (!GuildmarkIsCorrectBMP(data, size))
        return false;

    int w = 0, h = 0, comp = 0;
    uint8_t* rgba = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    if (!rgba)
        return false;

    if (m_guildMarks[index]) {
        glDeleteTextures(1, &m_guildMarks[index]);
        m_guildMarks[index] = 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(rgba);

    m_guildMarks[index] = tex;
    return true;
}

GLuint GLTextureManager::GetGuildMarkTexture(int index) const {
    return (index >= 0 && index < GUILD_MARK_COUNT) ? m_guildMarks[index] : 0;
}

void GLTextureManager::ClearGuildMark(int index) {
    if (index >= 0 && index < GUILD_MARK_COUNT && m_guildMarks[index]) {
        glDeleteTextures(1, &m_guildMarks[index]);
        m_guildMarks[index] = 0;
    }
}

namespace GLSamplers {

static GLuint s_linearMip = 0;
static GLuint s_linearNoMip = 0;
static GLuint s_pointNoMip = 0;

void Init() {
    if (s_linearMip)
        return;

    glGenSamplers(1, &s_linearMip);
    glSamplerParameteri(s_linearMip, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(s_linearMip, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(s_linearMip, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(s_linearMip, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenSamplers(1, &s_linearNoMip);
    glSamplerParameteri(s_linearNoMip, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(s_linearNoMip, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(s_linearNoMip, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(s_linearNoMip, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenSamplers(1, &s_pointNoMip);
    glSamplerParameteri(s_pointNoMip, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(s_pointNoMip, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s_pointNoMip, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(s_pointNoMip, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SetAnisotropy(int level) {
    // GL_EXT_texture_filter_anisotropic on the world sampler only (UI/fonts
    // stay bilinear). level 1 = off; clamped to the implementation maximum.
    if (!s_linearMip)
        return;
    GLfloat maxAniso = 1.0f;
    if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    GLfloat v = (GLfloat)level;
    if (v > maxAniso) v = maxAniso;
    if (v < 1.0f) v = 1.0f;
    glSamplerParameterf(s_linearMip, GL_TEXTURE_MAX_ANISOTROPY_EXT, v);
}

void Destroy() {
    if (s_linearMip)   glDeleteSamplers(1, &s_linearMip);
    if (s_linearNoMip) glDeleteSamplers(1, &s_linearNoMip);
    if (s_pointNoMip)  glDeleteSamplers(1, &s_pointNoMip);
    s_linearMip = s_linearNoMip = s_pointNoMip = 0;
}

GLuint LinearMip()   { return s_linearMip; }
GLuint LinearNoMip() { return s_linearNoMip; }
GLuint PointNoMip()  { return s_pointNoMip; }

} // namespace GLSamplers

}
