#include "test_framework.h"

#include "gl/GLTexture.h"

#include <cstring>
#include <vector>

namespace {

void PutU32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off + 0] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFF);
    b[off + 2] = (uint8_t)((v >> 16) & 0xFF);
    b[off + 3] = (uint8_t)((v >> 24) & 0xFF);
}

// Synthetic .wys: [1 junk byte][DDS header without magic][DXT blocks].
// Header field offsets are relative to the magic position (doc 15 §5).
std::vector<uint8_t> BuildWys(uint32_t w, uint32_t h, uint32_t mips, char fourccByte) {
    std::vector<uint8_t> b(1 + 128, 0);
    b[0] = 0xAA; // junk byte
    PutU32(b, 1 + 12, h);
    PutU32(b, 1 + 16, w);
    PutU32(b, 1 + 28, mips);
    b[1 + 84] = (uint8_t)fourccByte;

    uint32_t blockBytes = (fourccByte == '2') ? 8 : 16;
    while (mips--) {
        uint32_t levelBytes = ((w + 3) / 4) * ((h + 3) / 4) * blockBytes;
        size_t old = b.size();
        b.resize(old + levelBytes, 0x11);
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
    }
    return b;
}

} // namespace

TEST(wys, parses_dxt1_header) {
    auto blob = BuildWys(64, 32, 3, '2');
    tmx::WysInfo info;
    EXPECT_TRUE(tmx::ParseWysHeader(blob.data(), blob.size(), info, nullptr));
    EXPECT_EQ(info.width, 64u);
    EXPECT_EQ(info.height, 32u);
    EXPECT_EQ(info.mipLevels, 3u);
    EXPECT_TRUE(info.dxt1);
    EXPECT_EQ(info.dataOffset, 129u);
}

TEST(wys, fourcc_byte_2_is_dxt1_anything_else_dxt3) {
    auto b1 = BuildWys(4, 4, 1, '2');
    auto b2 = BuildWys(4, 4, 1, '3');
    auto b3 = BuildWys(4, 4, 1, 'X');
    tmx::WysInfo i1, i2, i3;
    tmx::ParseWysHeader(b1.data(), b1.size(), i1, nullptr);
    tmx::ParseWysHeader(b2.data(), b2.size(), i2, nullptr);
    tmx::ParseWysHeader(b3.data(), b3.size(), i3, nullptr);
    EXPECT_TRUE(i1.dxt1);
    EXPECT_FALSE(i2.dxt1);
    EXPECT_FALSE(i3.dxt1);
}

TEST(wys, zero_mips_becomes_one) {
    auto blob = BuildWys(8, 8, 0, '2');
    // fix: BuildWys com mips=0 não gera nível nenhum — reconstruir com 1 nível de dados
    blob = BuildWys(8, 8, 1, '2');
    PutU32(blob, 1 + 28, 0); // mas anuncia 0 mips no header

    tmx::WysInfo info;
    EXPECT_TRUE(tmx::ParseWysHeader(blob.data(), blob.size(), info, nullptr));
    EXPECT_EQ(info.mipLevels, 1u);
}

TEST(wys, rejects_truncated_mip_chain) {
    auto blob = BuildWys(64, 64, 4, '3');
    blob.resize(blob.size() - 100); // remove parte do último mip
    tmx::WysInfo info;
    EXPECT_FALSE(tmx::ParseWysHeader(blob.data(), blob.size(), info, nullptr));
}

TEST(wys, texture_list_lookup_case_insensitive) {
    // 528-byte records (A = 264B stTextureListInfo, B = residue) — this client
    // build's real layout (doc 16 §2.2)
    std::vector<uint8_t> list(528 * 3, 0);
    memcpy(list.data() + 0, "mesh\\Wall_One.wys", 17);
    list[255] = 'C';
    memcpy(list.data() + 528, "MESH\\ROOF.WYS", 13);
    list[528 + 255] = 'A';
    memcpy(list.data() + 1056, "mesh\\floor.wys", 14);
    list[1056 + 255] = 'N';

    tmx::GLTextureManager mgr;
    EXPECT_TRUE(mgr.LoadModelTextureList(list.data(), list.size()));

    int wall = mgr.FindModelTexture("mesh\\wall_one.wys");
    int roof = mgr.FindModelTexture("mesh\\roof.wys");
    int missing = mgr.FindModelTexture("mesh\\nope.wys");
    EXPECT_EQ(wall, 0);
    EXPECT_EQ(roof, 1);
    EXPECT_EQ(missing, -1);
    EXPECT_EQ(mgr.AlphaFlag(wall), 'C');
    EXPECT_EQ(mgr.AlphaFlag(roof), 'A');
    EXPECT_EQ(mgr.AlphaFlag(missing), 'N');
}
