#pragma once

#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "gl/EffectRenderer.h"
#include "gl/SkinPipeline.h"
#include "gl/SkillMeshRenderer.h"
#include "gl/GroundDecalRenderer.h"
#include "scene/ObjectFile.h"
#include "world/AniSound.h"
#include "world/Billboard.h"
#include "world/Character.h"
#include "world/CharacterMesh.h"
#include "world/TerrainData.h"
#include "world/TerrainRenderer.h"
#include "world/SeaSurface.h"
#include "world/TreeRenderer.h"
#include "world/WeatherFx.h"
#include "world/Critter.h"
#include "world/SkillEffect.h"

#include <memory>
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
    // boneAniListTxt = Mesh\BoneAni4.txt contents (for animated trees).
    // Missing pieces degrade gracefully (terrain-only or objects-only).
    bool Load(const char* mapName, GLTextureManager& textures,
              const std::string& meshListTxt, const std::string& boneAniListTxt);

    // GL objects (terrain shader/buffers) — call with a live context.
    bool InitGL(std::string* err);
    void Render(GLRenderDevice& device);
    void Destroy();

    // Advances sea waves etc. (call once per frame, seconds).
    void FrameMove(float timeSec);

    const TerrainData& Terrain() const { return m_terrain; }
    bool HasTerrain() const { return m_hasTerrain; }
    int  ObjectCount() const { return (int)m_objects.size(); }
    int  SeaCount() const { return (int)m_seas.size(); }

    // World bounds across terrain+objects (for the initial camera).
    void Bounds(float* minXYZ, float* maxXYZ) const;

    // Lamp tint etc. (D8): rewrite a tile's color and refresh the GPU copy.
    TerrainData& TerrainMutable() { return m_terrain; }
    void RefreshTerrainColors() { m_terrainRenderer.RefreshColors(m_terrain); }

    // Phase 3: characters. Call InitCharacters after Load (aniSoundTxt =
    // AniSound4.txt contents); Spawn adds a character on the terrain mask.
    bool InitCharacters(const std::string& boneAniListTxt, const std::string& aniSoundTxt,
                        GLTextureManager& textures, std::string* err);
    Character* Spawn(const CharDesc& d, float x, float z, std::string* err);
    void RemoveCharacter(Character* c);
    CharacterAnimationCache& CharCache() { return m_charCache; }
    const AniSoundData& AniSound() const { return m_aniSound; }
    bool HasCharacters() const { return m_charReady; }
    size_t CharacterCount() const { return m_chars.size(); }
    Character* GetCharacter(size_t i) { return i < m_chars.size() ? m_chars[i].get() : nullptr; }

    // Phase 4: effects. Lamp glows are built at Load; FrameMove steps the
    // billboard sim (call SetFxFrame once per frame with camera/focus info).
    struct FxFrameInfo {
        float yawH = 0, pitchV = 0;         // game-convention camera angles
        int screenW = 0, screenH = 0;
        float focusX = 0, focusH = 0, focusZ = 0;  // char pos/height (or camera fallback)
        float right[3] = { 1, 0, 0 };       // view matrix row axes (billboarding)
        float up[3] = { 0, 1, 0 };
    };
    void SetFxFrame(const FxFrameInfo& f) { m_fxFrame = f; }
    // Weather precipitation: 2 = rain, 3 = snow (anything else = off).
    void SetWeatherFx(int mode);
    size_t EffectCount() const { return m_lampFx.size(); }

    // Screen-space/world effect injection (sun flare, weather). Emitted quads
    // are drawn at the end of Render with the other effects.
    void EmitScreenFx(const FxQuad& q) { m_fx.Emit(q); }

    // Phase 5: combat/skill effect container (skills, projectiles, decals,
    // weapon trails). FieldView ticks it in FrameMove and renders it in the
    // translucent pass right before m_fx.Flush. Add() takes ownership.
    void AddSkillEffect(std::unique_ptr<SkillEffect> e) { m_skills.Add(std::move(e)); }
    EffectContainer& Skills() { return m_skills; }

private:
    struct Object {
        int   meshIndex;
        float x, y, z;
        float angle;
    };
    struct SeaDesc {
        int gridX, gridY;
        float x, h, z;
    };

    GLMesh* GetMesh(int index, GLTextureManager& textures);

    TerrainData     m_terrain;
    TerrainRenderer m_terrainRenderer;
    bool            m_hasTerrain = false;

    std::vector<SeaSurface> m_seas;
    std::vector<SeaDesc>    m_seaDescs;
    GLShader    m_seaShader;
    GLint       m_locSeaWorld = -1, m_locSeaTex0 = -1, m_locSeaTex1 = -1;

    TreeRenderer m_trees;
    std::vector<TreeRenderer::Instance> m_treeInsts;
    std::vector<ObjectFileRecord> m_lampRecords;
    std::string  m_boneAniListTxt;

    std::vector<Object> m_objects;
    std::unordered_map<int, std::string> m_meshFiles;
    std::unordered_map<int, GLMesh>      m_meshes;
    GLTextureManager* m_textures = nullptr;

    // Phase 3 characters.
    CharacterAnimationCache m_charCache;
    AniSoundData            m_aniSound;
    SkinPipeline            m_charPipe;
    std::vector<std::unique_ptr<Character>> m_chars;
    bool m_charReady = false;

    // Phase 4 effects (lamp glows 501-505 + weather + future emitters).
    EffectRenderer       m_fx;
    std::vector<Billboard> m_lampFx;
    FxFrameInfo m_fxFrame;
    WeatherSystem m_rain, m_snow1, m_snow2;
    int m_weatherFx = -1;

    // Phase 4 critters (leaves/butterflies/fish from the .dat).
    std::vector<Critter> m_critters;
    std::vector<std::unique_ptr<CharacterMesh>> m_critterMeshes;
    bool m_crittersBuilt = false;

    // Phase 5 combat/skill effects.
    EffectContainer m_skills;
    SkillCtx        m_skillCtx;
    SkillMeshRenderer   m_skillMeshR;
    GroundDecalRenderer m_decalR;

    float m_lastTimeMs = 0.0f;
    float m_bmin[3] = { 0, 0, 0 };
    float m_bmax[3] = { 0, 0, 0 };
};

}
