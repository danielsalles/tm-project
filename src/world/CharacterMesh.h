#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "world/CharacterAnimation.h"
#include "world/SkinMesh.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;
class SkinPipeline;

// Shared, immutable per-type character animation data (skeleton + cuts +
// animMap + quats), keyed by BoneAni4.txt index. Loads BoneAni4.txt +
// ValidIndex.bin lazily.
class CharacterAnimationCache {
public:
    bool Init(const std::string& boneAniListTxt, std::string* err);

    // Returns nullptr (and fills err) when the type can't be loaded.
    const CharacterAnimation* Get(int boneAniIndex, std::string* err);
    int NumParts(int boneAniIndex) const;
    const char* Prefix(int boneAniIndex) const;  // "" when unknown

private:
    struct Info { int aniTypes = 0; int parts = 0; std::string prefix; };
    std::unordered_map<int, Info> m_info;
    std::vector<int32_t> m_valid;  // [100][186]
    bool m_validLoaded = false;
    std::unordered_map<int, std::unique_ptr<CharacterAnimation>> m_cache;
};

// One visible character instance: up to 8 skinned parts resolved from a
// LOOK_INFO, one pose + playback, drawn through the shared SkinPipeline.
class CharacterMesh {
public:
    bool Init(CharacterAnimationCache& cache, GLTextureManager& textures,
              int boneAniIndex, const int16_t meshLook[8], const int16_t skinLook[8],
              std::string* err) {
        return LoadLogic(cache, boneAniIndex, err) &&
               LoadParts(cache, textures, meshLook, skinLook, err);
    }

    // Split init: LoadLogic is CPU-only (tests run headless); LoadParts does
    // the GL uploads.
    bool LoadLogic(CharacterAnimationCache& cache, int boneAniIndex, std::string* err);
    bool LoadParts(CharacterAnimationCache& cache, GLTextureManager& textures,
                   const int16_t meshLook[8], const int16_t skinLook[8], std::string* err);
    int BoneAniIndex() const { return m_boneAniIndex; }
    void Destroy();

    // Wraps CharSetAnimation (returns false when the cut doesn't exist).
    bool SetCut(int cut, uint32_t fps, uint32_t nowMs) {
        return m_anim ? CharSetAnimation(*m_anim, pb, cut, fps, nowMs) : false;
    }
    int CutCount() const { return m_anim ? m_anim->numCuts : 0; }
    const CharacterAnimation* Anim() const { return m_anim; }
    int PartCount() const { return (int)m_parts.size(); }
    unsigned int DebugTexture0() const { return m_textures.empty() ? 0u : m_textures[0]; }

    // Samples the pose at nowMs and draws every part.
    void Render(SkinPipeline& pipe, GLRenderDevice& device, const D3DXMATRIX& world,
                uint32_t nowMs);

    CharPlayback pb;
    float alphaMul = 1.0f;   // leaf distance fade (TMLeaf)
    float emissiveAdd[3] = { 0.0f, 0.0f, 0.0f };  // mouse-over highlight

private:
    const CharacterAnimation* m_anim = nullptr;  // cache-owned
    int m_boneAniIndex = 0;
    CharacterPose m_pose;
    std::vector<GLSkinMesh> m_parts;
    std::vector<GLuint> m_textures;
};

}
