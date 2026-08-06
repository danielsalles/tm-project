#include "test_framework.h"

#include "world/TerrainData.h"

#include <cstring>
#include <vector>

using tmx::TerrainData;
using tmx::TerrainTile;

namespace {

// Builds a synthetic .trn blob: u8 nameLen, name, u8 posX, u8 posY,
// 4096 x 12B FileTileInfo (i8 height, u8 tile, u8 coord, u8 backTile,
// u8 backCoord, u32 color, 3 pad).
class TrnBuilder {
public:
    TrnBuilder& Header(const char* name, uint8_t posX, uint8_t posY) {
        const size_t len = strlen(name);
        m_buf.push_back((uint8_t)len);
        for (size_t i = 0; i < len; ++i)
            m_buf.push_back((uint8_t)name[i]);
        m_buf.push_back(posX);
        m_buf.push_back(posY);
        return *this;
    }
    TrnBuilder& Tile(int8_t h, uint8_t idx, uint8_t coord, uint8_t back, uint8_t backCoord,
                     uint32_t color) {
        m_buf.push_back((uint8_t)h);
        m_buf.push_back(idx);
        m_buf.push_back(coord);
        m_buf.push_back(back);
        m_buf.push_back(backCoord);
        for (int i = 0; i < 4; ++i)
            m_buf.push_back((uint8_t)(color >> (8 * i)));
        for (int i = 0; i < 3; ++i)
            m_buf.push_back(0);
        return *this;
    }
    TrnBuilder& Tiles(int8_t h) {  // 4096 identical flat tiles
        for (int i = 0; i < 4096; ++i)
            Tile(h, 10, 0, 200, 0, 0xFFFFFFFF);
        return *this;
    }
    const std::vector<uint8_t>& Data() const { return m_buf; }
private:
    std::vector<uint8_t> m_buf;
};

} // namespace

TEST(trn, parses_header_and_tiles) {
    auto b = TrnBuilder().Header("Field2723", 27, 23).Tiles(5);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    EXPECT_TRUE(strcmp(t.mapName, "Field2723") == 0);
    EXPECT_EQ(t.posX, 27);
    EXPECT_EQ(t.posY, 23);
    EXPECT_EQ((int)t.tiles[0].height, 5);
    EXPECT_EQ((int)t.tiles[4095].tileIndex, 10);
    EXPECT_EQ(t.tiles[65].color, 0xFFFFFFFFu);
    // World offset: (pos<<6)*2
    EXPECT_TRUE(t.OffsetX() == (float)(27 << 6) * 2.0f);
    EXPECT_TRUE(t.OffsetY() == (float)(23 << 6) * 2.0f);
}

TEST(trn, rejects_truncated) {
    auto b = TrnBuilder().Header("X", 1, 1).Tiles(0);
    TerrainData t;
    std::string err;
    EXPECT_TRUE(!tmx::ParseTrn(b.Data().data(), 10, t, &err));
    EXPECT_TRUE(!tmx::ParseTrn(b.Data().data(), b.Data().size() - 5, t, &err));
}

TEST(trn, flat_ground_normal_is_up) {
    auto b = TrnBuilder().Header("F", 0, 0).Tiles(10);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    const float* n = t.normals[32 + (32 << 6)];
    EXPECT_TRUE(std::fabs(n[0]) < 1e-6f && std::fabs(n[1] - 1.0f) < 1e-6f &&
                std::fabs(n[2]) < 1e-6f);
}

TEST(trn, slope_normal_matches_hand_computed) {
    // cHeight = x (clamped to int8 range): hand-computed normal at any interior
    // point = normalize(-1, 1, 0) (see plan doc 16 §2.1 derivation).
    TrnBuilder b;
    b.Header("S", 0, 0);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            b.Tile((int8_t)x, 0, 0, 0, 0, 0);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    const float* n = t.normals[32 + (32 << 6)];
    const float inv = 0.70710678f;
    EXPECT_TRUE(std::fabs(n[0] + inv) < 1e-4f);
    EXPECT_TRUE(std::fabs(n[1] - inv) < 1e-4f);
    EXPECT_TRUE(std::fabs(n[2]) < 1e-4f);
}

TEST(trn, border_normals_clamp_to_neighbor) {
    auto b = TrnBuilder().Header("B", 0, 0).Tiles(3);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    // col0 = col1, row0 = row1, col63 = col62, row63 = row62
    EXPECT_TRUE(!memcmp(t.normals[0], t.normals[1], 12));
    EXPECT_TRUE(!memcmp(t.normals[0], t.normals[64], 12));
    EXPECT_TRUE(!memcmp(t.normals[63], t.normals[62], 12));
    EXPECT_TRUE(!memcmp(t.normals[4032 + 32], t.normals[3968 + 32], 12));
}

TEST(trn, row0_heights_inherit_row1) {
    TrnBuilder b;
    b.Header("R", 0, 0);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            b.Tile((int8_t)(y), 0, 0, 0, 0, 0);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    for (int x = 0; x < 64; ++x)
        EXPECT_EQ((int)t.tiles[x].height, 1);  // row0 = row1
    EXPECT_EQ((int)t.tiles[64 + 7].height, 1);
    EXPECT_EQ((int)t.tiles[2 * 64 + 7].height, 2);
}

TEST(trn, mask_averaging_and_blocked_edges) {
    auto b = TrnBuilder().Header("M", 0, 0).Tiles(8);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    // Flat 8: center = 8, each cell = 8.
    EXPECT_EQ((int)t.mask[32][32], 8);
    // All four borders blocked (single-ground, no neighbors).
    EXPECT_EQ((int)tmx::TerrainData{}.mask[0][0], 0);  // sanity: zero-init
    EXPECT_EQ((int)t.mask[0][64], 127);
    EXPECT_EQ((int)t.mask[14][64], 127);
    EXPECT_EQ((int)t.mask[15][64], 8);
    EXPECT_EQ((int)t.mask[114][64], 127);
    EXPECT_EQ((int)t.mask[113][64], 8);
    EXPECT_EQ((int)t.mask[64][0], 127);
    EXPECT_EQ((int)t.mask[64][127], 127);
}

TEST(trn, mask_center_average_sloped) {
    // Tile (16,16) heights: f1=0 (x,y), f2=4 (x+1,y), f3=0 (x,y+1), f4=4.
    TrnBuilder b;
    b.Header("C", 0, 0);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            b.Tile((int8_t)((x == 17 && (y == 16 || y == 17)) ? 4 : 0), 0, 0, 0, 0, 0);
    TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(b.Data().data(), b.Data().size(), t, nullptr));
    // center = (0+4+0+4)/4 = 2; cell(2y,2x)   = (f1+c)/2 = 1
    //                          cell(2y,2x+1) = (f2+c)/2 = 3
    EXPECT_EQ((int)t.mask[32][32], 1);
    EXPECT_EQ((int)t.mask[32][33], 3);
}

TEST(trn, real_field2723_parses_when_present) {
    // Local-only (assets are not in the repo): validates against the real client file.
    const char* path = TM_REPO_ROOT "/env/Field2723.trn";
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("      [skip] env/Field2723.trn not present\n");
        return;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)size);
    EXPECT_EQ((int)fread(buf.data(), 1, (size_t)size, fp), (int)size);
    fclose(fp);

    TerrainData t;
    std::string err;
    EXPECT_TRUE(tmx::ParseTrn(buf.data(), buf.size(), t, &err));
    printf("      map='%s' pos=(%d,%d)\n", t.mapName, t.posX, t.posY);
    EXPECT_EQ(t.posX, 27);
    EXPECT_EQ(t.posY, 23);

    int minH = 127, maxH = -128;
    for (int i = 0; i < 4096; ++i) {
        minH = t.tiles[i].height < minH ? t.tiles[i].height : minH;
        maxH = t.tiles[i].height > maxH ? t.tiles[i].height : maxH;
    }
    printf("      height range [%d, %d] (world y [%.1f, %.1f])\n",
           minH, maxH, minH * 0.1f, maxH * 0.1f);
    // Sanity: normals roughly unit length in the interior.
    const float* n = t.normals[32 + (32 << 6)];
    const float len = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
    EXPECT_TRUE(len > 0.5f && len <= 1.0001f);
}
