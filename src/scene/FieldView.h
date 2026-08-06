#pragma once

#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "scene/ObjectFile.h"
#include "world/TerrainData.h"
#include "world/TerrainRenderer.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tmx {

class GLRenderDevice;

// Phase-2 validation scene (doc 16 §3): one ground (env\<map>.trn) + its static
// objects (env\<map>.dat). Drives any map via --map FieldXXYY; Field2723 is the
// select-server scene, Field0101 has trees/houses, Field1616 has sea.
class FieldView {
public:
    // Loads env\<mapName>.trn + env\<mapName>.dat + the mesh/texture lists.
    // Missing pieces degrade gracefully (terrain-only or objects-only).
    bool Load(const char* mapName, GLTextureManager& textures,
              const std::string& meshListTxt);

    // GL objects (terrain shader/buffers) — call with a live context.
    bool InitGL(std::string* err);

    void Render(GLRenderDevice& device);
    void Destroy();

    const TerrainData& Terrain() const { return m_terrain; }
    bool HasTerrain() const { return m_hasTerrain; }
    int  ObjectCount() const { return (int)m_objects.size(); }

    // World bounds across terrain+objects (for the initial camera).
    void Bounds(float* minXYZ, float* maxXYZ) const;

    // Lamp tint etc. (D8): rewrite a tile's color and refresh the GPU copy.
    TerrainData& TerrainMutable() { return m_terrain; }
    void RefreshTerrainColors() { m_terrainRenderer.RefreshColors(m_terrain); }

private:
    struct Object {
        int   meshIndex;
        float x, y, z;
        float angle;
    };

    GLMesh* GetMesh(int index, GLTextureManager& textures);

    TerrainData     m_terrain;
    TerrainRenderer m_terrainRenderer;
    bool            m_hasTerrain = false;

    std::vector<Object> m_objects;
    std::unordered_map<int, std::string> m_meshFiles;
    std::unordered_map<int, GLMesh>      m_meshes;
    GLTextureManager* m_textures = nullptr;

    float m_bmin[3] = { 0, 0, 0 };
    float m_bmax[3] = { 0, 0, 0 };
};

}
