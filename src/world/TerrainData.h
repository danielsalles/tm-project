#pragma once

#include <cstdint>
#include <string>

namespace tmx {

// CPU-side mirror of a .trn ground file, exactly as TMGround::LoadTileMap reads it
// (TMGround.cpp:2467-2683). Kept GL-free so tests can validate parsing and the
// derived data (normals, mask) without a context.
//
// Disk layout:
//   u8 nameLen, char name[nameLen], u8 posX, u8 posY, FileTileInfo tiles[4096]
// FileTileInfo (Structures.h): { i8 cHeight; u8 byTileIndex; u8 byTileCoord;
//                                u8 byBackTileIndex; u8 byBackTileCoord; u32 dwColor }
struct TerrainTile {
    int8_t   height;         // world height = height * 0.1f
    uint8_t  tileIndex;      // env texture index (front)
    uint8_t  tileCoord;      // rotation/variant -> TileCoordList
    uint8_t  backTileIndex;  // env texture index = backTileIndex + 256 (blend layer)
    uint8_t  backTileCoord;  // -> BackTileCoordList
    uint32_t color;          // baked per-vertex light (A = blend factor front/back)
};

struct TerrainData {
    static constexpr int kTiles  = 64;    // 64x64 tile corners
    static constexpr int kMask   = 128;   // 2x2 mask cells per tile
    static constexpr float kWorldScale = 2.0f;   // tile -> world XZ
    static constexpr float kHeightScale = 0.1f;  // height byte -> world Y

    char         mapName[129] = {};
    int          posX = 0, posY = 0;            // ground index in the world
    TerrainTile  tiles[kTiles * kTiles] = {};
    float        normals[kTiles * kTiles][3] = {}; // per-corner, tile-space
    int8_t       mask[kMask][kMask] = {};          // collision/pick heights, 127 = blocked

    float OffsetX() const { return (float)(posX << 6) * kWorldScale; }
    float OffsetY() const { return (float)(posY << 6) * kWorldScale; }
};

// UV rotation/variant tables, copied verbatim from TMGround.cpp:13-58.
// TileCoordList[byTileCoord][corner 0..3][u,v] — corners in tristrip order:
// (x,y) (x,y+1) (x+1,y) (x+1,y+1).
extern const float kTileCoordList[8][4][2];
extern const float kBackTileCoordList[32][4][2];

// Parses a .trn blob and derives normals + mask in the same order as the
// original: normals (1..62, then border clamps) -> row0 := row1 heights ->
// mask 2x2 averaging -> edge blocks (all four sides blocked: single-ground
// view has no attached neighbors; the original's per-map SetAttatchEnable
// table only matters when streaming neighbors — phase 3+).
bool ParseTrn(const uint8_t* data, size_t size, TerrainData& out, std::string* err);

// Baked-light accessors (ports of TMGround::GetColor/SetColor, including the
// original's /256 and interpolation quirks — TMGround.cpp:3637-3718).
// worldX/worldZ are absolute world coordinates; out is rgba float[4].
void TerrainGetColor(const TerrainData& t, float worldX, float worldZ, float* out4);
void TerrainSetColor(TerrainData& t, float worldX, float worldZ, uint32_t dwColor);

// Height query (TMGround::GetHeight): vertical ray from y=100 against the two
// triangles of the containing tile; -10000 outside the ground.
float TerrainGetHeight(const TerrainData& t, float worldX, float worldZ);

// Mouse picking (TMGround::GetPickPos): scans mask quads (128x128, heights from
// the mask with the original's 127->400 blocked mapping) around (focusX,focusZ),
// ray-tri per cell; returns false when nothing is hit.
bool TerrainPick(const TerrainData& t, float focusX, float focusZ,
                 const float* rayOrig3, const float* rayDir3, float* outPos3);

}
