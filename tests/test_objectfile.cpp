#include "test_framework.h"

#include "scene/ObjectFile.h"

#include <cstring>
#include <vector>

namespace {

void PutU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v & 0xFF));
    b.push_back((uint8_t)((v >> 8) & 0xFF));
    b.push_back((uint8_t)((v >> 16) & 0xFF));
    b.push_back((uint8_t)((v >> 24) & 0xFF));
}

void PutF32(std::vector<uint8_t>& b, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    PutU32(b, u);
}

// 28-byte record: type, posX, posY, height, angle, texSet, mask
void PutRecord(std::vector<uint8_t>& b, uint32_t type, float x, float y,
               float h, float angle, int32_t texSet, int32_t mask) {
    PutU32(b, type);
    PutF32(b, x);
    PutF32(b, y);
    PutF32(b, h);
    PutF32(b, angle);
    PutU32(b, (uint32_t)texSet);
    PutU32(b, (uint32_t)mask);
}

} // namespace

TEST(objectfile, parses_28_byte_records) {
    std::vector<uint8_t> blob;
    PutRecord(blob, 100, 10.5f, 20.5f, 3.25f, 1.5f, 0, 0);
    PutRecord(blob, 200, -4.0f, 8.0f, 0.0f, 0.0f, 2, 1);

    tmx::ObjectFile file;
    EXPECT_TRUE(file.Load(blob.data(), blob.size(), nullptr));
    EXPECT_EQ(file.records.size(), 2u);
    EXPECT_EQ(file.records[0].dwObjType, 100u);
    EXPECT_NEAR(file.records[0].posX, 10.5f, 1e-6f);
    EXPECT_NEAR(file.records[0].fAngle, 1.5f, 1e-6f);
    EXPECT_EQ(file.records[1].nTextureSetIndex, 2);
}

TEST(objectfile, classifies_special_types) {
    using tmx::ObjectKind;
    EXPECT_TRUE(tmx::ClassifyObjectType(2) == ObjectKind::Sea);
    EXPECT_TRUE(tmx::ClassifyObjectType(343) == ObjectKind::Butterfly);
    EXPECT_TRUE(tmx::ClassifyObjectType(315) == ObjectKind::Leaf);
    EXPECT_TRUE(tmx::ClassifyObjectType(335) == ObjectKind::Tree);
    EXPECT_TRUE(tmx::ClassifyObjectType(360) == ObjectKind::Tree);
    EXPECT_TRUE(tmx::ClassifyObjectType(488) == ObjectKind::Ship);
    EXPECT_TRUE(tmx::ClassifyObjectType(251) == ObjectKind::House);
    EXPECT_TRUE(tmx::ClassifyObjectType(1855) == ObjectKind::House);
    EXPECT_TRUE(tmx::ClassifyObjectType(502) == ObjectKind::TorchEffect);
    EXPECT_TRUE(tmx::ClassifyObjectType(100) == ObjectKind::GenericStatic);
    EXPECT_TRUE(tmx::ClassifyObjectType(1000) == ObjectKind::GenericStatic);
}

TEST(objectfile, counts_skipped_per_kind) {
    std::vector<uint8_t> blob;
    PutRecord(blob, 100, 0, 0, 0, 0, 0, 0);   // static
    PutRecord(blob, 2, 0, 0, 0, 0, 0, 0);     // sea
    PutRecord(blob, 343, 0, 0, 0, 0, 0, 0);   // butterfly
    PutRecord(blob, 251, 0, 0, 0, 0, 0, 0);   // house

    tmx::ObjectFile file;
    EXPECT_TRUE(file.Load(blob.data(), blob.size(), nullptr));
    EXPECT_EQ(file.skipped[(int)tmx::ObjectKind::GenericStatic], 0);
    EXPECT_EQ(file.skipped[(int)tmx::ObjectKind::Sea], 1);
    EXPECT_EQ(file.skipped[(int)tmx::ObjectKind::Butterfly], 1);
    EXPECT_EQ(file.skipped[(int)tmx::ObjectKind::House], 1);
}
