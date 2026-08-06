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

void TerrainGetColor(const TerrainData& t, float worldX, float worldZ, float* out4) {
    out4[0] = out4[1] = out4[2] = out4[3] = 1.0f;
    const float lx = worldX - t.OffsetX();
    const float lz = worldZ - t.OffsetY();
    const int nX = (int)(lx / 2.0f);
    const int nY = (int)(lz / 2.0f);

    uint32_t dwColor[4] = { 0, 0, 0, 0 };
    const bool inMain  = (nX >= 0 && nX < 63 && nY >= 0 && nY < 63);
    const bool rightEd = (nX == 63 && nY >= 0 && nY < 63);
    if (inMain) {
        dwColor[0] = t.tiles[nX + (nY << 6)].color;
        dwColor[1] = t.tiles[nX + (nY << 6) + 1].color;
        dwColor[2] = t.tiles[nX + ((nY + 1) << 6)].color;
        dwColor[3] = t.tiles[nX + ((nY + 1) << 6) + 1].color;
    } else if (rightEd) {
        dwColor[0] = t.tiles[(nY << 6) + 63].color;
        dwColor[1] = t.tiles[(nY << 6) + 63].color;
        dwColor[2] = t.tiles[((nY + 1) << 6) + 63].color;
        dwColor[3] = t.tiles[((nY + 1) << 6) + 63].color;
    } else if (nX < 0 || nX > 63 || nY < 0 || nY > 63) {
        return;   // fully outside: white (original's default)
    }
    // else: bottom edge — the original's third branch repeats "nX == 63" (dead
    // code), so nY==63 & nX<63 computes with zeros (black). Preserved.

    float color[4][3];
    for (int i = 0; i < 4; ++i) {
        color[i][0] = ((dwColor[i] & 0xFF0000) >> 16) / 256.0f;   // original divides by 256
        color[i][1] = ((dwColor[i] & 0x00FF00) >> 8) / 256.0f;
        color[i][2] = (dwColor[i] & 0x0000FF) / 256.0f;
    }

    const float fDX = (float)(nX * 2.0f) - lx;
    const float fDY = (float)(nY * 2.0f) - lz;
    for (int c = 0; c < 3; ++c) {
        out4[c] = ((fDX + fDY) * color[3][c] + ((4.0f - fDX) - fDY) * color[0][c] +
                   ((fDX + 2.0f) - fDY) * color[1][c] + ((2.0f - fDX) + fDY) * color[2][c]) / 12.0f;
    }
    out4[3] = 1.0f;
}

void TerrainSetColor(TerrainData& t, float worldX, float worldZ, uint32_t dwColor) {
    const int nX = (int)(worldX - t.OffsetX()) / 2;
    const int nY = (int)(worldZ - t.OffsetY()) / 2;
    if (nX >= 0 && nX <= 63 && nY >= 0 && nY <= 63)
        t.tiles[nX + (nY << 6)].color = dwColor;
}

namespace {

// Möller–Trumbore, no backface culling (D3DXIntersectTri semantics):
// returns true + barycentric u,v + distance t along the (normalized) ray.
bool IntersectTri(const float* v0, const float* v1, const float* v2,
                  const float* orig, const float* dir, float* u, float* v, float* t) {
    const float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    const float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
    const float p[3] = { dir[1] * e2[2] - dir[2] * e2[1],
                         dir[2] * e2[0] - dir[0] * e2[2],
                         dir[0] * e2[1] - dir[1] * e2[0] };
    const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::fabs(det) < 1e-9f)
        return false;
    const float inv = 1.0f / det;
    const float tv[3] = { orig[0] - v0[0], orig[1] - v0[1], orig[2] - v0[2] };
    const float uu = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
    if (uu < 0.0f || uu > 1.0f)
        return false;
    const float q[3] = { tv[1] * e1[2] - tv[2] * e1[1],
                         tv[2] * e1[0] - tv[0] * e1[2],
                         tv[0] * e1[1] - tv[1] * e1[0] };
    const float vv = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
    if (vv < 0.0f || uu + vv > 1.0f)
        return false;
    const float tt = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    if (tt < 0.0f)
        return false;
    *u = uu;
    *v = vv;
    *t = tt;
    return true;
}

} // namespace

float TerrainGetHeight(const TerrainData& t, float worldX, float worldZ) {
    const int nX = (int)((worldX - t.OffsetX()) / 2.0f);
    const int nY = (int)((worldZ - t.OffsetY()) / 2.0f);
    if (nX < 0 || nY < 0 || nX > 64 || nY > 64)
        return -10000.0f;

    const float offX = t.OffsetX(), offY = t.OffsetY();
    float v0[3], v2[3], v6[3], v8[3];
    if (nX < 63 && nY < 63) {
        const auto H = [&](int x, int y) { return t.tiles[x + (y << 6)].height * 0.1f; };
        v0[0] = nX * 2.0f + offX;       v0[1] = H(nX, nY);         v0[2] = nY * 2.0f + offY;
        v2[0] = (nX + 1) * 2.0f + offX; v2[1] = H(nX + 1, nY);     v2[2] = nY * 2.0f + offY;
        v6[0] = nX * 2.0f + offX;       v6[1] = H(nX, nY + 1);     v6[2] = (nY + 1) * 2.0f + offY;
        v8[0] = (nX + 1) * 2.0f + offX; v8[1] = H(nX + 1, nY + 1); v8[2] = (nY + 1) * 2.0f + offY;
    } else if (nX == 63) {
        const auto H = [&](int y) { return t.tiles[63 + (y << 6)].height * 0.1f; };
        v0[0] = 126.0f + offX; v0[1] = H(nY);     v0[2] = nY * 2.0f + offY;
        v2[0] = 128.0f + offX; v2[1] = H(nY);     v2[2] = nY * 2.0f + offY;
        v6[0] = 126.0f + offX; v6[1] = H(nY + 1); v6[2] = (nY + 1) * 2.0f + offY;
        v8[0] = 128.0f + offX; v8[1] = H(nY + 1); v8[2] = (nY + 1) * 2.0f + offY;
    } else {   // nY == 63
        const auto H = [&](int x) { return t.tiles[x + 4032].height * 0.1f; };
        v0[0] = nX * 2.0f + offX;       v0[1] = H(nX);     v0[2] = 126.0f + offY;
        v2[0] = (nX + 1) * 2.0f + offX; v2[1] = H(nX + 1); v2[2] = 126.0f + offY;
        v6[0] = nX * 2.0f + offX;       v6[1] = H(nX);     v6[2] = 128.0f + offY;
        v8[0] = (nX + 1) * 2.0f + offX; v8[1] = H(nX + 1); v8[2] = 128.0f + offY;
    }

    const float orig[3] = { worldX, 100.0f, worldZ };
    const float dir[3] = { 0.0f, -1.0f, 0.0f };
    float u, v, dist;
    if (IntersectTri(v0, v2, v6, orig, dir, &u, &v, &dist))
        return 100.0f - dist;
    if (IntersectTri(v8, v6, v2, orig, dir, &u, &v, &dist))
        return 100.0f - dist;
    return -10000.0f;
}

bool TerrainPick(const TerrainData& t, float focusX, float focusZ,
                 const float* rayOrig3, const float* rayDir3, float* outPos3) {
    // Port of TMGround::GetPickPos: scans mask quads (128x128, heights from
    // the mask with the original's 127->400 blocked mapping) around (focusX,focusZ),
    // ray-tri per cell; returns false when nothing is hit.
    const float offX = t.OffsetX(), offY = t.OffsetY();
    const int nCamPosX = (int)(focusX - offX);
    const int nCamPosY = (int)(focusZ - offY);
    const int nClipIndex = 25;
    const int half = nClipIndex / 2;

    float dir[3] = { rayDir3[0], rayDir3[1], rayDir3[2] };
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1e-9f)
        return false;
    dir[0] /= len;
    dir[1] /= len;
    dir[2] /= len;

    for (int nY = nCamPosY - half; nY < nClipIndex + nCamPosY; ++nY) {
        if (nY < 0 || nY > 127)
            continue;
        for (int nX = nCamPosX - half; nX < nClipIndex + nCamPosX; ++nX) {
            if (nX < 0 || nX > 127)
                continue;
            int h = t.mask[nY][nX];
            if (h > 127)
                h = 0;
            if (h == 127)
                h = 400;
            const float y = h * 0.1f;
            const float v0[3] = { nX + offX,         y, nY + offY };
            const float v1[3] = { nX + offX,         y, nY + 1.0f + offY };
            const float v2[3] = { nX + 1.0f + offX,  y, nY + offY };
            const float v3[3] = { nX + 1.0f + offX,  y, nY + 1.0f + offY };
            float u, v, dist;
            if (IntersectTri(v0, v1, v2, rayOrig3, dir, &u, &v, &dist)) {
                outPos3[0] = v0[0] + v;
                outPos3[1] = y;
                outPos3[2] = v0[2] + u;
                return true;
            }
            if (IntersectTri(v3, v2, v1, rayOrig3, dir, &u, &v, &dist)) {
                outPos3[0] = v3[0] - v;
                outPos3[1] = y;
                outPos3[2] = v3[2] - u;
                return true;
            }
        }
    }
    return false;
}

}
