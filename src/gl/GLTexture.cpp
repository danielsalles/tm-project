#include "gl/GLTexture.h"

#include "platform/Platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <cctype>

namespace tmx {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Shared parser for the 264-byte-record texture lists (model/env/effect/ui all use
// stTextureListInfo). `maxEntries` mirrors the original's fixed-size array.
bool ParseTextureList(const uint8_t* data, size_t size, size_t maxEntries,
                      std::vector<TextureListEntry>& out) {
    constexpr size_t kEntry = 264;
    size_t count = size / kEntry;
    if (count > maxEntries)
        count = maxEntries;

    out.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = data + i * kEntry;
        memcpy(out[i].fileName, p, 255);
        out[i].fileName[254] = '\0';
        out[i].cAlpha = (char)p[255];
        out[i].dwLastUsedTime = ReadU32(p + 256);
        out[i].dwShowTime = ReadU32(p + 260);
        if (out[i].cAlpha == 0)
            out[i].cAlpha = 'N';
    }
    return count > 0;
}

// .wyt = "WT10" + TGA without the 18-byte footer (validated on mesh/cbt054.wyt:
// uncompressed BGR(A), type 2). stb_image parses the TGA directly (footer not
// needed) and handles the bottom-left origin flip.
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

bool GLTextureManager::LoadModelTextureList(const uint8_t* data, size_t size) {
    const bool ok = ParseTextureList(data, size, 2048, m_entries);
    m_textures.assign(m_entries.size(), 0);
    return ok;
}

bool GLTextureManager::LoadEnvTextureList(const uint8_t* data, size_t size) {
    // REAL layout of this client build's EnvTextureList3.bin: 2048 entries of
    // 528 bytes = two consecutive 264B stTextureListInfo records (A = active,
    // B = short-name residue). Validated against EnvTextureList3.txt: entry i
    // lives at i*528. The original's fread of 512*264 only covers indices
    // 0..127 interleaved with B halves — an upstream-format mismatch in the
    // leaked source; the shipped client evidently uses the 528 stride.
    constexpr size_t kEntry = 528;
    size_t count = size / kEntry;
    if (count > 2048)
        count = 2048;

    m_envEntries.resize(count);
    m_envTextures.assign(count, 0);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = data + i * kEntry;   // A half
        memcpy(m_envEntries[i].fileName, p, 255);
        m_envEntries[i].fileName[254] = '\0';
        m_envEntries[i].cAlpha = (char)p[255];
        m_envEntries[i].dwLastUsedTime = ReadU32(p + 256);
        m_envEntries[i].dwShowTime = ReadU32(p + 260);
        if (m_envEntries[i].cAlpha == 0 || m_envEntries[i].cAlpha == (char)0xCD)
            m_envEntries[i].cAlpha = 'N';
    }
    return count > 0;
}

int GLTextureManager::FindModelTexture(const char* meshRelativeWysPath) const {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].fileName[0] == '\0')
            continue;
        const char* a = m_entries[i].fileName;
        const char* b = meshRelativeWysPath;
        size_t j = 0;
        while (a[j] && b[j]) {
            char ca = a[j] == '\\' ? '/' : (char)tolower((unsigned char)a[j]);
            char cb = b[j] == '\\' ? '/' : (char)tolower((unsigned char)b[j]);
            if (ca != cb)
                break;
            ++j;
        }
        if (a[j] == b[j]) // both ended together
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

void GLTextureManager::DestroyAll() {
    for (GLuint t : m_textures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    for (GLuint t : m_envTextures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    m_textures.clear();
    m_entries.clear();
    m_envTextures.clear();
    m_envEntries.clear();
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
