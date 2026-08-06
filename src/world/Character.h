#pragma once

#include <cstdint>
#include <string>

#include "math/TMMath.h"
#include "world/AniSound.h"
#include "world/CharacterMesh.h"
#include "world/Route.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;
class SkinPipeline;

// ECHAR_MOTION (Enums.h:35).
enum class CharMotion : int {
    None = -1,
    Stand01 = 0, Stand02, Walk, Run,
    Attack01, Attack02, Attack03, Attack04, Attack05, Attack06,
    Strike, Die, Dead, Seat, LevelUp, Punish, Cackle, Yawn,
    MerchantLounge, Relax, Never, ComeOn, Salute, HolyTouch,
    Seating, Stand03, Punishing, PunEnd,
};

struct CharDesc {
    int boneAniIndex = 0;         // BoneAni4.txt index (0=ch01 male classes...)
    int classIndex = 0;           // humanoids: 0..3 (TK/Foema/BM/Hunter)
    int weaponIndex = 0;          // humanoids: 0..15
    int16_t meshLook[8] = {};
    int16_t skinLook[8] = {};
    float scale = 1.0f;           // m_fScale
    float maxSpeed = 2.0f;        // m_fMaxSpeed (2 = walk, >2 = run)
};

// A character in the world (TMHuman movement/render core, no network/UI):
// position/heading on the terrain, route walking with height following,
// animation selection through the AniSound tables + animMap.
class Character {
public:
    // `mask`/`maskOrigin*` describe the walkable field in cell units
    // (1 cell = 1 world unit): cell = world - origin.
    // InitLogic is CPU-only (headless tests); InitMesh does the GL uploads.
    bool InitLogic(CharacterAnimationCache& cache, const AniSoundData& aniSound,
                   const CharDesc& d, const int8_t* maskCells, int maskW, int maskH,
                   float maskOriginX, float maskOriginZ, std::string* err);
    bool InitMesh(CharacterAnimationCache& cache, GLTextureManager& textures,
                  std::string* err);
    bool Init(CharacterAnimationCache& cache, GLTextureManager& textures,
              const AniSoundData& aniSound, const CharDesc& d,
              const int8_t* maskCells, int maskW, int maskH,
              float maskOriginX, float maskOriginZ, std::string* err) {
        return InitLogic(cache, aniSound, d, maskCells, maskW, maskH,
                         maskOriginX, maskOriginZ, err) &&
               InitMesh(cache, textures, err);
    }
    void Destroy();

    void SetPosition(float x, float z);
    void SetAngle(float yaw) { m_angle = m_wantAngle = yaw; }
    void SetMaxSpeed(float s) {
        m_desc.maxSpeed = s;
        m_unitTimeMs = s > 0.0f ? (uint32_t)(1000.0f / s) : 500;
    }
    float MaxSpeed() const { return m_desc.maxSpeed; }

    // Animation control (TMHuman::SetAnimation simplified). Returns false when
    // the motion has no cut mapped (original keeps the previous animation).
    bool SetMotion(CharMotion m, uint32_t nowMs);

    // Movement: computes a route to the world-space target and starts walking
    // (TMHuman::MoveTo + OnPacketMove path, without the server).
    bool MoveTo(float wx, float wz, uint32_t nowMs);
    void Stop(uint32_t nowMs);

    void FrameMove(uint32_t nowMs);
    void Render(SkinPipeline& pipe, GLRenderDevice& device, uint32_t nowMs);

    float X() const { return m_x; }
    float Z() const { return m_z; }
    float Height() const { return m_height; }
    float Angle() const { return m_angle; }
    bool Moving() const { return m_routeLen > 0; }
    CharMotion Motion() const { return m_motion; }
    const CharDesc& Desc() const { return m_desc; }
    CharacterMesh& Mesh() { return m_mesh; }

private:
    int MaskAt(float wx, float wz) const;      // GroundGetMask equivalent
    void UpdateWorldMatrix();

    CharacterMesh m_mesh;
    const AniSoundData* m_aniSound = nullptr;
    CharDesc m_desc;

    RouteMask m_mask{};
    float m_originX = 0, m_originZ = 0;   // mask origin in world units

    float m_x = 0, m_z = 0;
    float m_height = 0;
    float m_angle = 0, m_wantAngle = 0;

    // Route walking (TMHuman::FrameMove core).
    float m_routeX[24] = {};              // world-space cell centers
    float m_routeZ[24] = {};
    int m_routeLen = 0;
    int m_routeIndex = 0;
    uint32_t m_segmentStartMs = 0;
    uint32_t m_unitTimeMs = 500;          // 1000 / m_fMaxSpeed

    CharMotion m_motion = CharMotion::Stand01;
    bool m_running = false;

    D3DXMATRIX m_world;
};

}
