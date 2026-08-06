#pragma once

#include <glad/gl.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "gl/GLShader.h"
#include "world/BoneAnimation.h"
#include "world/SkinMesh.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

// Animated trees (TMTree port): TMSkinMesh-driven objects with bone animations.
// Routing (TMTree::InitLook): dwObjType 331-342 -> boneAni 63+(t-331)/2,
// 351-378 -> 71+(t-351)/2; look overrides below. Meshes/textures named
// <prefix><part+1><look+1> (tr010101.msh / tr010101.wys...).
class TreeRenderer {
public:
    bool Init(const std::string& boneAniListTxt, GLTextureManager& textures,
              std::string* err);
    void Destroy();

    struct Instance {
        int   boneAniIdx = 0;   // BoneAni4.txt index (63-87)
        int   meshLook = 0;     // Mesh0 look override
        int   skinLook = 0;     // Skin0 look override
        float x, y, z;
        float angle;
    };
    void Add(const Instance& inst);

    void Render(GLRenderDevice& device, float timeMs);

    int TreeCount() const { return (int)m_instances.size(); }

private:
    struct LoadedSet {
        std::vector<GLSkinMesh> parts;   // per numParts
        std::vector<GLuint>     textures;
        bool loaded = false;
    };

    // Cache key includes the look overrides: mesh/skin looks change the file suffix.
    bool LoadSet(const Instance& inst, std::string* err);

    GLShader m_shader;
    GLint m_locNumInfluence = -1, m_locTex0 = -1, m_locAlphaRef = -1, m_locAlphaTest = -1;
    GLuint m_uboBones = 0;
    GLTextureManager* m_textures = nullptr;

    struct BoneAniInfo { int parts; std::string prefix; };
    std::unordered_map<int, BoneAniInfo> m_info;     // boneAniIdx -> prefix/parts
    std::unordered_map<uint32_t, LoadedSet> m_sets;  // key = idx | meshLook<<8 | skinLook<<16
    std::unordered_map<uint32_t, BoneAniSet> m_boneCache; // .bon/.ani per boneAniIdx
    std::vector<Instance> m_instances;
};

}
