#pragma once

#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "scene/ObjectFile.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tmx {

class GLRenderDevice;

// Phase-1 demo view: renders the static objects of env\Field2723.dat (the
// select-server scene) with a fixed camera (doc 15 §7). Terrain/sky/sea are
// phase 2 — objects float over a placeholder background on purpose.
class SelectServerView {
public:
    // meshListTxt: contents of Mesh\MeshList.txt ("index path" per line).
    // objectDat:   contents of env\Field2723.dat.
    bool Load(const std::string& meshListTxt, const uint8_t* objectDat, size_t datSize,
              GLTextureManager& textures);

    void Render(GLRenderDevice& device);

    int ObjectCount() const { return (int)m_objects.size(); }
    int MeshesLoaded() const { return (int)m_meshes.size(); }

    // Scene bounds of the loaded objects (for the fixed camera).
    void Bounds(float* minXYZ, float* maxXYZ) const;

private:
    struct Object {
        int   meshIndex;
        float x, y, z;      // world position (fHeight is Y)
        float angle;
    };

    GLMesh* GetMesh(int index, GLTextureManager& textures);

    std::vector<Object> m_objects;
    std::unordered_map<int, std::string> m_meshFiles;      // index -> "mesh\mNNNN.msa"
    std::unordered_map<int, GLMesh>      m_meshes;         // lazy cache (no eviction in phase 1)
    GLTextureManager* m_textures = nullptr;

    float m_bmin[3] = { 0, 0, 0 };
    float m_bmax[3] = { 0, 0, 0 };
};

}
