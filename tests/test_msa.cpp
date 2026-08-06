#include "test_framework.h"

#include "gl/GLMesh.h"

#include <cstring>
#include <vector>

using tmx::MsaData;

namespace {

// Builds a synthetic .msa blob in the exact disk layout the original reads
// (TMMesh.cpp:481-728):
//   [u32 fvf][u32 fileStride][u32 attCount][attCount x 20B attr ranges]
//   [attCount x 11B texture names][u32 ibBytes][u16 IB][u32 vbBytes][VB]
class MsaBuilder {
public:
    MsaBuilder& Header(uint32_t fvf, uint32_t stride, uint32_t attCount) {
        U32(fvf); U32(stride); U32(attCount);
        return *this;
    }
    MsaBuilder& Attr(uint32_t id, uint32_t faceStart, uint32_t faceCount, uint32_t vertStart) {
        U32(id); U32(faceStart); U32(faceCount); U32(vertStart); U32(0 /*vertexCount*/);
        return *this;
    }
    MsaBuilder& TexName(const char* raw11) {  // exactly 11 bytes written as-is
        for (int i = 0; i < 11; ++i)
            m_buf.push_back((uint8_t)raw11[i]);
        return *this;
    }
    MsaBuilder& IB(const std::vector<uint16_t>& idx) {
        U32((uint32_t)(idx.size() * 2));
        for (uint16_t v : idx) {
            m_buf.push_back((uint8_t)(v & 0xFF));
            m_buf.push_back((uint8_t)(v >> 8));
        }
        return *this;
    }
    MsaBuilder& VB(const std::vector<float>& floats, uint32_t stride) {
        U32((uint32_t)(floats.size() * 4));
        (void)stride;
        for (float f : floats) {
            uint32_t u;
            memcpy(&u, &f, 4);
            m_buf.push_back((uint8_t)(u & 0xFF));
            m_buf.push_back((uint8_t)((u >> 8) & 0xFF));
            m_buf.push_back((uint8_t)((u >> 16) & 0xFF));
            m_buf.push_back((uint8_t)((u >> 24) & 0xFF));
        }
        return *this;
    }

    const std::vector<uint8_t>& Buf() const { return m_buf; }

private:
    void U32(uint32_t v) {
        m_buf.push_back((uint8_t)(v & 0xFF));
        m_buf.push_back((uint8_t)((v >> 8) & 0xFF));
        m_buf.push_back((uint8_t)((v >> 16) & 0xFF));
        m_buf.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    std::vector<uint8_t> m_buf;
};

} // namespace

TEST(msa, parses_header_and_subsets) {
    auto blob = MsaBuilder()
        .Header(322, 24, 2)
        .Attr(0, 0, 1, 0)
        .Attr(1, 1, 1, 3)
        .TexName("wall_a.tga")   // 10 chars + NUL = 11
        .TexName("roof\0\0\0\0\0\0")
        .IB({ 0, 1, 2, 3, 4, 5 })
        .VB(std::vector<float>(6 * 6, 0.5f), 24)  // 6 verts x 24B
        .Buf();

    MsaData data;
    std::string err;
    EXPECT_TRUE(tmx::ParseMsa(blob.data(), blob.size(), data, &err));
    EXPECT_EQ(data.fvf, 322u);
    EXPECT_EQ(data.fileStride, 24u);
    EXPECT_EQ(data.memStride, 24u);          // fvf 322: sem expansão
    EXPECT_EQ(data.subsets.size(), 2u);
    EXPECT_EQ(data.subsets[1].faceStart, 1u);
    EXPECT_EQ(data.indices.size(), 6u);
    EXPECT_EQ(data.indices[4], 4);
    EXPECT_EQ(data.NumVerts(), 6u);
}

TEST(msa, texture_name_11_bytes_path_and_ext_stripped) {
    // "dir\sub\x.tga" style content crammed in 11 bytes, no NUL on disk
    auto blob = MsaBuilder()
        .Header(322, 24, 1)
        .Attr(0, 0, 1, 0)
        .TexName("ab\\cd_ef.tg")  // 11 bytes, no NUL, no '.' terminator issues
        .IB({ 0, 1, 2 })
        .VB(std::vector<float>(3 * 6, 0.0f), 24)
        .Buf();

    MsaData data;
    EXPECT_TRUE(tmx::ParseMsa(blob.data(), blob.size(), data, nullptr));
    EXPECT_TRUE(data.textureNames[0] == "cd_ef");  // path e extensão removidos
}

TEST(msa, fvf274_expands_to_530_stride_and_duplicates_uv) {
    // disk: fvf 274 (pos3 normal3 uv0, 32B) -> memory: fvf 530 (40B, uv1 = uv0)
    std::vector<float> verts;
    for (int i = 0; i < 3; ++i) {
        float v[8] = { (float)i, 1.0f, 2.0f, 0.0f, 1.0f, 0.0f, 0.25f + i, 0.75f };
        verts.insert(verts.end(), v, v + 8);
    }
    auto blob = MsaBuilder()
        .Header(274, 32, 1)
        .Attr(0, 0, 1, 0)
        .TexName("tex\0\0\0\0\0\0\0\0")
        .IB({ 0, 1, 2 })
        .VB(verts, 32)
        .Buf();

    MsaData data;
    EXPECT_TRUE(tmx::ParseMsa(blob.data(), blob.size(), data, nullptr));
    EXPECT_EQ(data.fvf, 530u);               // +256 (TEX1) como no original
    EXPECT_EQ(data.memStride, 40u);
    EXPECT_EQ(data.NumVerts(), 3u);

    const float* fv = reinterpret_cast<const float*>(data.vertices.data());
    // vertex 0: uv1 (floats 8,9) == uv0 (floats 6,7)
    EXPECT_NEAR(fv[8], 0.25f, 1e-6f);
    EXPECT_NEAR(fv[9], 0.75f, 1e-6f);
    // vertex 1 também
    EXPECT_NEAR(fv[10 + 8], 1.25f, 1e-6f);
    // quirk do original: último vértice fica com uv1 zerado (loop nVerts-1)
    EXPECT_NEAR(fv[20 + 8], 0.0f, 1e-6f);
}

TEST(msa, rejects_truncated) {
    auto blob = MsaBuilder()
        .Header(322, 24, 1)
        .Attr(0, 0, 1, 0)
        .TexName("x\0\0\0\0\0\0\0\0\0\0")
        .IB({ 0, 1, 2 })
        .VB(std::vector<float>(18, 0.0f), 24)
        .Buf();
    blob.resize(blob.size() - 5); // corta o VB no meio

    MsaData data;
    EXPECT_FALSE(tmx::ParseMsa(blob.data(), blob.size(), data, nullptr));
}
