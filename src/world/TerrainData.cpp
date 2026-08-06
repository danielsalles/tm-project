#include "world/TerrainData.h"

#include <cmath>
#include <cstring>

namespace tmx {

const float kTileCoordList[8][4][2] = {
    { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } },
    { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } },
    { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } },
    { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
    { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } },
    { { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f } },
};

const float kBackTileCoordList[32][4][2] = {
    { { 0.0f, 0.0f }, { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 0.5f, 0.5f } },
    { { 1.0f, 0.0f }, { 0.5f, 0.0f }, { 1.0f, 0.5f }, { 0.5f, 0.5f } },
    { { 1.0f, 1.0f }, { 1.0f, 0.5f }, { 0.5f, 1.0f }, { 0.5f, 0.5f } },
    { { 0.0f, 1.0f }, { 0.5f, 1.0f }, { 0.0f, 0.5f }, { 0.5f, 0.5f } },
    { { 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.5f }, { 0.5f, 0.5f } },
    { { 0.0f, 1.0f }, { 0.0f, 0.5f }, { 0.5f, 1.0f }, { 0.5f, 0.5f } },
    { { 1.0f, 1.0f }, { 0.5f, 1.0f }, { 1.0f, 0.5f }, { 0.5f, 0.5f } },
    { { 1.0f, 0.0f }, { 1.0f, 0.5f }, { 0.5f, 0.0f }, { 0.5f, 0.5f } },
    { { 0.0f, 0.5f }, { 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.5f, 1.0f } },
    { { 0.5f, 0.0f }, { 0.0f, 0.0f }, { 0.5f, 0.5f }, { 0.0f, 0.5f } },
    { { 1.0f, 0.5f }, { 1.0f, 0.0f }, { 0.5f, 0.5f }, { 0.5f, 0.0f } },
    { { 0.5f, 1.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f }, { 1.0f, 0.5f } },
    { { 0.5f, 0.0f }, { 1.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.5f } },
    { { 0.0f, 0.5f }, { 0.0f, 0.0f }, { 0.5f, 0.5f }, { 0.5f, 0.0f } },
    { { 0.5f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.5f }, { 1.0f, 0.5f } },
    { { 0.5f, 1.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f }, { 1.0f, 0.5f } },
    { { 1.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.5f, 1.0f } },
    { { 0.0f, 0.5f }, { 0.5f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.5f } },
    { { 0.5f, 0.5f }, { 1.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.5f, 0.0f }, { 0.0f, 0.5f }, { 1.0f, 0.5f }, { 0.0f, 0.0f } },
    { { 0.0f, 0.5f }, { 1.0f, 0.5f }, { 0.0f, 0.0f }, { 0.5f, 0.0f } },
    { { 0.5f, 1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.5f }, { 0.0f, 0.5f } },
    { { 1.0f, 0.5f }, { 0.5f, 0.0f }, { 0.5f, 1.0f }, { 0.0f, 0.0f } },
    { { 1.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.5f, 0.5f } },
    { { 0.5f, 0.0f }, { 0.5f, 0.5f }, { 0.0f, 0.5f }, { 0.0f, 1.0f } },
    { { 0.5f, 0.5f }, { 0.0f, 0.5f }, { 1.0f, 0.5f }, { 1.0f, 1.0f } },
    { { 0.5f, 0.5f }, { 1.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.5f, 1.0f }, { 0.5f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.0f, 0.5f }, { 0.5f, 1.0f }, { 0.0f, 0.0f }, { 0.5f, 0.0f } },
    { { 0.5f, 1.0f }, { 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f } },
    { { 0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f } },
    { { 0.5f, 0.5f }, { 0.5f, 0.0f }, { 0.0f, 0.5f }, { 0.0f, 0.0f } },
};

namespace {

float TileHeight(const TerrainData& t, int x, int y) {
    return (float)t.tiles[x + (y << 6)].height;
}

// Port of ComputeNormalVector (TMUtil.cpp:6-16): normalized cross of (v2-v1, v3-v1).
void ComputeNormal(const float* v1, const float* v2, const float* v3, float* out) {
    const float a[3] = { v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2] };
    const float b[3] = { v3[0] - v1[0], v3[1] - v1[1], v3[2] - v1[2] };
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
    const float len = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (len > 0.0f) {
        out[0] /= len;
        out[1] /= len;
        out[2] /= len;
    }
}

// Port of TMGround::GetNormalInGround (TMGround.cpp:3720-3747). Tile-space:
// neighbor offsets are +-1.0 and heights use the raw byte (no 0.1 scale).
void NormalInGround(const TerrainData& t, int x, int y, float* out) {
    out[0] = 0.0f;
    out[1] = 1.0f;
    out[2] = 0.0f;
    if (x <= 0 || x >= TerrainData::kTiles || y <= 0 || y >= TerrainData::kTiles)
        return;

    const float c = TileHeight(t, x, y);
    const float around[4][3] = {
        { -1.0f, TileHeight(t, x - 1, y),      0.0f },
        {  0.0f, TileHeight(t, x, y + 1),      1.0f },
        {  1.0f, TileHeight(t, x + 1, y),      0.0f },
        {  0.0f, TileHeight(t, x, y - 1),     -1.0f },
    };
    const float center[3] = { 0.0f, c, 0.0f };

    float sum[3] = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i) {
        float n[3];
        ComputeNormal(center, around[i], around[(i + 1) & 3], n);
        sum[0] += n[0];
        sum[1] += n[1];
        sum[2] += n[2];
    }
    out[0] = sum[0] * 0.25f;
    out[1] = sum[1] * 0.25f;
    out[2] = sum[2] * 0.25f;
}

} // namespace

bool ParseTrn(const uint8_t* data, size_t size, TerrainData& out, std::string* err) {
    if (!data || size < 3) {
        if (err) *err = "trn: buffer too small";
        return false;
    }

    size_t pos = 0;
    const uint32_t nameLen = data[pos++];
    if (nameLen > 128) {
        // The original clamps to 128 and keeps reading that many bytes
        // (TMGround.cpp:2477-2481) — same behavior, then NUL-terminates.
        if (size < pos + 128u) {
            if (err) *err = "trn: truncated name";
            return false;
        }
        memcpy(out.mapName, data + pos, 128);
        pos += 128;
        out.mapName[128] = '\0';
    } else {
        if (size < pos + nameLen) {
            if (err) *err = "trn: truncated name";
            return false;
        }
        memcpy(out.mapName, data + pos, nameLen);
        pos += nameLen;
        out.mapName[nameLen] = '\0';
    }

    if (size < pos + 2 + (size_t)TerrainData::kTiles * TerrainData::kTiles * 12) {
        if (err) *err = "trn: truncated tile data";
        return false;
    }

    out.posX = data[pos++];
    out.posY = data[pos++];

    for (int i = 0; i < TerrainData::kTiles * TerrainData::kTiles; ++i) {
        TerrainTile& t = out.tiles[i];
        t.height        = (int8_t)data[pos + 0];
        t.tileIndex     = data[pos + 1];
        t.tileCoord     = data[pos + 2];
        t.backTileIndex = data[pos + 3];
        t.backTileCoord = data[pos + 4];
        // u32 color LE; 7 bytes of struct padding on disk are skipped
        // (FileTileInfo is 12 bytes on disk: 5 bytes of fields + color + 3 pad).
        memcpy(&t.color, data + pos + 5, 4);
        pos += 12;
    }

    // Normals for the interior, then border clamps (TMGround.cpp:2519-2539).
    for (int y = 1; y < 63; ++y)
        for (int x = 1; x < 63; ++x)
            NormalInGround(out, x, y, out.normals[x + (y << 6)]);

    for (int n = 0; n < 64; ++n) {
        memcpy(out.normals[64 * n],       out.normals[64 * n + 1],  12);
        memcpy(out.normals[(n << 6) + 63], out.normals[(n << 6) + 62], 12);
    }
    for (int n = 0; n < 64; ++n) {
        memcpy(out.normals[n],         out.normals[n + 64],   12);
        memcpy(out.normals[n + 4032],  out.normals[n + 3968], 12);
    }

    // Row 0 inherits row 1 heights (TMGround.cpp:2560-2561).
    for (int j = 0; j < 64; ++j)
        out.tiles[j].height = out.tiles[j + 64].height;

    // Mask: 2x2 cells per tile, each corner averaged with the tile center
    // (TMGround.cpp:2563-2604).
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const float f1 = TileHeight(out, x, y);
            const float f3 = (y < 62) ? TileHeight(out, x, y + 1) : f1;
            float f2, f4;
            if (x < 62) {
                f2 = TileHeight(out, x + 1, y);
                f4 = (y >= 62) ? f2 : TileHeight(out, x + 1, y + 1);
            } else {
                f2 = f1;
                f4 = (y >= 62) ? f1 : TileHeight(out, x, y + 1);
            }
            const float center = (f1 + f2 + f3 + f4) * 0.25f;
            out.mask[2 * y][2 * x]         = (int8_t)((f1 + center) * 0.5f);
            out.mask[2 * y][2 * x + 1]     = (int8_t)((f2 + center) * 0.5f);
            out.mask[2 * y + 1][2 * x]     = (int8_t)((f3 + center) * 0.5f);
            out.mask[2 * y + 1][2 * x + 1] = (int8_t)((f4 + center) * 0.5f);
        }
    }

    // Edge blocks: single-ground viewer => no neighbors attached, so all four
    // borders are blocked (127), matching the original when every m_c*Enable
    // is 0 (TMGround.cpp:2606-2652). The combined-corner cases only fire when
    // a pair of enables is 1, so they never trigger here.
    for (int x = 0; x < 128; ++x) {
        for (int y = 0; y < 15; ++y)
            out.mask[y][x] = 127;               // up
        for (int y = 114; y < 128; ++y)
            out.mask[y][x] = 127;               // down
    }
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 15; ++x)
            out.mask[y][x] = 127;               // left
        for (int x = 114; x < 128; ++x)
            out.mask[y][x] = 127;               // right
    }

    return true;
}

}
