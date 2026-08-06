#include "test_framework.h"

#include "world/TerrainData.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

// Flat/ramped synthetic terrain via the trn builder.
std::vector<uint8_t> BuildTrn(int8_t (*heightFn)(int x, int y)) {
    std::vector<uint8_t> b;
    b.push_back(4);
    b.insert(b.end(), { 'F', 'l', 'a', 't' });
    b.push_back(0);
    b.push_back(0);   // posX, posY -> offset (0,0)
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            b.push_back((uint8_t)heightFn(x, y));
            b.push_back(0);
            b.push_back(0);
            b.push_back(0);
            b.push_back(0);
            for (int k = 0; k < 4; ++k) b.push_back(0xFF);   // white color
            for (int k = 0; k < 3; ++k) b.push_back(0);
        }
    }
    return b;
}

tmx::TerrainData MakeTerrain(int8_t (*fn)(int, int)) {
    auto blob = BuildTrn(fn);
    tmx::TerrainData t;
    tmx::ParseTrn(blob.data(), blob.size(), t, nullptr);
    return t;
}

int8_t Flat10(int, int) { return 10; }
int8_t RampX(int x, int) { return (int8_t)x; }

} // namespace

TEST(picking, height_flat) {
    auto t = MakeTerrain(Flat10);
    EXPECT_NEAR(tmx::TerrainGetHeight(t, 32.0f, 32.0f), 1.0f, 1e-4f);
    EXPECT_NEAR(tmx::TerrainGetHeight(t, 0.5f, 0.5f), 1.0f, 1e-4f);
    EXPECT_NEAR(tmx::TerrainGetHeight(t, 125.9f, 125.9f), 1.0f, 1e-4f);
}

TEST(picking, height_outside_is_sentinel) {
    auto t = MakeTerrain(Flat10);
    EXPECT_TRUE(tmx::TerrainGetHeight(t, -1.0f, 32.0f) == -10000.0f);
    EXPECT_TRUE(tmx::TerrainGetHeight(t, 32.0f, 129.0f) == -10000.0f);
}

TEST(picking, height_ramp_matches_plane) {
    auto t = MakeTerrain(RampX);
    // At x in tile 10 (world x in [20,22)), heights 10*0.1..11*0.1 on a plane
    // h = x*0.1 (tile space). World x = 21.0 -> halfway between 1.0 and 1.1.
    const float h = tmx::TerrainGetHeight(t, 21.0f, 10.0f);
    EXPECT_NEAR(h, 1.05f, 0.051f);   // plane approx within half a step
}

TEST(picking, pick_straight_down) {
    auto t = MakeTerrain(Flat10);
    const float orig[3] = { 32.0f, 50.0f, 32.0f };
    const float dir[3] = { 0.0f, -1.0f, 0.0f };
    float pos[3];
    EXPECT_TRUE(tmx::TerrainPick(t, 32.0f, 32.0f, orig, dir, pos));
    EXPECT_NEAR(pos[1], 1.0f, 0.06f);   // mask averages to the tile height
    EXPECT_NEAR(pos[0], 32.0f, 0.51f);
    EXPECT_NEAR(pos[2], 32.0f, 0.51f);
}

TEST(picking, pick_miss_returns_false) {
    auto t = MakeTerrain(Flat10);
    const float orig[3] = { 500.0f, 50.0f, 500.0f };   // far outside the scan window
    const float dir[3] = { 0.0f, -1.0f, 0.0f };
    float pos[3];
    EXPECT_FALSE(tmx::TerrainPick(t, 32.0f, 32.0f, orig, dir, pos));
}

TEST(picking, real_field2723_heights) {
    char path[300];
    snprintf(path, sizeof path, "%s/env/Field2723.trn", TM_REPO_ROOT);
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

    tmx::TerrainData t;
    EXPECT_TRUE(tmx::ParseTrn(buf.data(), buf.size(), t, nullptr));

    // The center of the select-server scene: height must be in the file's range
    // and match the tile's own byte (flat area) — sample tile-corner positions.
    const float cx = t.OffsetX() + 63.0f;
    const float cz = t.OffsetY() + 63.0f;
    const float h = tmx::TerrainGetHeight(t, cx, cz);
    EXPECT_TRUE(h > -12.6f && h < 3.8f);

    // Mask pick agrees with the height query near flat ground.
    const float orig[3] = { cx, 50.0f, cz };
    const float dir[3] = { 0.0f, -1.0f, 0.0f };
    float pos[3];
    EXPECT_TRUE(tmx::TerrainPick(t, cx, cz, orig, dir, pos));
    EXPECT_TRUE(std::fabs(pos[1] - h) < 0.15f);
}
