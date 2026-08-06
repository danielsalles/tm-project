#include "world/CharacterMesh.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLTexture.h"
#include "gl/SkinPipeline.h"
#include "platform/Platform.h"
#include "world/LookResolver.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace tmx {

namespace {

bool ReadAsset(const char* relPath, std::vector<uint8_t>& out) {
    FILE* f = OpenAsset(relPath, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)sz);
    bool ok = fread(out.data(), 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    return ok;
}

} // namespace

bool CharacterAnimationCache::Init(const std::string& boneAniListTxt, std::string* err) {
    // BoneAni4.txt: "<idx> <numAniTypes> <numParts> <prefix>"
    const char* p = boneAniListTxt.c_str();
    while (*p) {
        while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        int idx = 0, aniTypes = 0, parts = 0;
        bool any = false;
        while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); any = true; ++p; }
        if (!any) {
            while (*p && *p != '\n')
                ++p;
            continue;
        }
        while (*p == ' ' || *p == '\t') ++p;
        while (*p >= '0' && *p <= '9') { aniTypes = aniTypes * 10 + (*p - '0'); ++p; }
        while (*p == ' ' || *p == '\t') ++p;
        while (*p >= '0' && *p <= '9') { parts = parts * 10 + (*p - '0'); ++p; }
        while (*p == ' ' || *p == '\t') ++p;
        char prefix[128];
        int n = 0;
        while (*p && *p != '\r' && *p != '\n' && *p != ' ' && *p != '\t' && n < 127)
            prefix[n++] = *p++;
        prefix[n] = '\0';
        if (n > 0)
            m_info[idx] = { aniTypes, parts, prefix };
    }
    if (m_info.empty()) {
        if (err) *err = "charcache: empty BoneAni4 list";
        return false;
    }
    return true;
}

const CharacterAnimation* CharacterAnimationCache::Get(int boneAniIndex, std::string* err) {
    auto it = m_cache.find(boneAniIndex);
    if (it != m_cache.end())
        return it->second.get();

    auto info = m_info.find(boneAniIndex);
    if (info == m_info.end()) {
        if (err) *err = "charcache: boneAni index not in BoneAni4.txt";
        return nullptr;
    }
    if (!m_validLoaded) {
        if (!LoadValidIndex("Mesh/ValidIndex.bin", m_valid, err))
            return nullptr;
        m_validLoaded = true;
    }

    auto anim = std::make_unique<CharacterAnimation>();
    const bool humanoid = boneAniIndex == 0 || boneAniIndex == 1;
    if (!LoadCharacterAnimation(boneAniIndex, info->second.prefix.c_str(),
                                info->second.aniTypes,
                                m_valid.data() + (size_t)boneAniIndex * 186,
                                humanoid, *anim, err))
        return nullptr;

    const CharacterAnimation* ptr = anim.get();
    m_cache.emplace(boneAniIndex, std::move(anim));
    return ptr;
}

int CharacterAnimationCache::NumParts(int boneAniIndex) const {
    auto it = m_info.find(boneAniIndex);
    return it == m_info.end() ? 0 : it->second.parts;
}

const char* CharacterAnimationCache::Prefix(int boneAniIndex) const {
    auto it = m_info.find(boneAniIndex);
    return it == m_info.end() ? "" : it->second.prefix.c_str();
}

bool CharacterMesh::LoadLogic(CharacterAnimationCache& cache, int boneAniIndex,
                              std::string* err) {
    m_anim = cache.Get(boneAniIndex, err);
    if (!m_anim)
        return false;
    m_boneAniIndex = boneAniIndex;
    m_pose.Resize(m_anim->base.frames.size());
    pb.fps = 20;
    return true;
}

bool CharacterMesh::LoadParts(CharacterAnimationCache& cache, GLTextureManager& textures,
                              const int16_t meshLook[8],
                              const int16_t skinLook[8], std::string* err) {
    const int boneAniIndex = m_boneAniIndex;
    if (!m_anim) {
        if (err) *err = "char: LoadLogic first";
        return false;
    }

    LookInput in;
    in.boneAniIndex = boneAniIndex;
    strncpy(in.prefix, cache.Prefix(boneAniIndex), sizeof in.prefix - 1);
    for (int i = 0; i < 8; ++i) {
        in.meshLook[i] = meshLook[i];
        in.skinLook[i] = skinLook[i];
    }

    LookPart parts[8];
    ResolveLookParts(in, parts);

    const int numParts = cache.NumParts(boneAniIndex);
    for (int i = 0; i < numParts; ++i) {
        if (!parts[i].visible)
            continue;

        std::vector<uint8_t> bytes;
        if (!ReadAsset(parts[i].mesh, bytes)) {
            // Original has no fallback; we skip the part with a warning.
            Log("char: missing part mesh %s (skipped)", parts[i].mesh);
            continue;
        }
        MshData data;
        if (!ParseMsh(bytes.data(), bytes.size(), data, err))
            return false;

        GLSkinMesh mesh;
        if (!mesh.Upload(data)) {
            if (err) *err = "char: upload failed";
            return false;
        }

        // Texture: query .wyt first, fall back to .wys (list stores either).
        int texIndex = textures.FindModelTexture(parts[i].tex);
        if (texIndex < 0) {
            char alt[96];
            strncpy(alt, parts[i].tex, sizeof alt - 5);
            alt[sizeof alt - 5] = '\0';
            char* dot = strrchr(alt, '.');
            if (dot)
                strcpy(dot, ".wys");
            texIndex = textures.FindModelTexture(alt);
        }

        m_parts.push_back(mesh);
        m_textures.push_back(texIndex >= 0 ? textures.GetModelTexture(texIndex) : 0);
    }

    if (m_parts.empty()) {
        if (err) *err = "char: no visible parts";
        return false;
    }
    return true;
}

void CharacterMesh::Render(SkinPipeline& pipe, GLRenderDevice& device,
                           const D3DXMATRIX& world, uint32_t nowMs) {
    if (!m_anim || m_parts.empty())
        return;
    SampleCharacter(*m_anim, m_pose, pb, nowMs, world);
    for (size_t i = 0; i < m_parts.size(); ++i)
        pipe.DrawPart(device, m_parts[i], m_textures[i], m_pose.combined, world,
                      alphaMul, emissiveAdd);
}

void CharacterMesh::Destroy() {
    for (auto& p : m_parts)
        p.Destroy();
    m_parts.clear();
    m_textures.clear();
    m_anim = nullptr;
}

}
