#include "gl/GLTexture.h"

#include "platform/Platform.h"

#include <cstring>
#include <cctype>

namespace tmx {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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
    constexpr size_t kEntry = 264;
    constexpr size_t kMaxEntries = 2048;

    size_t count = size / kEntry;
    if (count > kMaxEntries)
        count = kMaxEntries;

    m_entries.resize(count);
    m_textures.assign(count, 0);

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = data + i * kEntry;
        memcpy(m_entries[i].fileName, p, 255);
        m_entries[i].fileName[254] = '\0';
        m_entries[i].cAlpha = (char)p[255];
        m_entries[i].dwLastUsedTime = ReadU32(p + 256);
        m_entries[i].dwShowTime = ReadU32(p + 260);
        if (m_entries[i].cAlpha == 0)
            m_entries[i].cAlpha = 'N';
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
    if (m_textures[index])
        return m_textures[index];

    FILE* f = OpenAsset(m_entries[index].fileName, "rb");
    if (!f) {
        Log("texture missing: %s", m_entries[index].fileName);
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

    m_textures[index] = LoadTextureWYS(bytes.data(), bytes.size());
    return m_textures[index];
}

void GLTextureManager::DestroyAll() {
    for (GLuint t : m_textures) {
        if (t)
            glDeleteTextures(1, &t);
    }
    m_textures.clear();
    m_entries.clear();
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
