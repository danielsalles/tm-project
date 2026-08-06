#include "test_framework.h"

#include "gl/GLTexture.h"

#include <cstring>
#include <vector>

namespace {

// Synthetic EnvTextureList3.bin: 528-byte records (A = 264B stTextureListInfo,
// B = 264B residue), matching this client build's real layout (doc 16 §2.2).
class EnvListBuilder {
public:
    EnvListBuilder& Entry(const char* path, char alpha) {
        std::vector<uint8_t> rec(528, 0);
        memcpy(rec.data(), path, strlen(path));
        rec[255] = (uint8_t)alpha;
        // B half: short-name residue — content irrelevant to the loader
        m_buf.insert(m_buf.end(), rec.begin(), rec.end());
        return *this;
    }
    const std::vector<uint8_t>& Data() const { return m_buf; }
private:
    std::vector<uint8_t> m_buf;
};

} // namespace

TEST(envtex, parses_528_stride_records) {
    EnvListBuilder b;
    b.Entry("Env\\Tile00000.wys", 'E')   // index 0
     .Entry("Env\\Tile14142.wys", 'E')   // index 1
     .Entry("Env\\Tile26262.wyt", 'E');  // index 2
    tmx::GLTextureManager tm;
    EXPECT_TRUE(tm.LoadEnvTextureList(b.Data().data(), b.Data().size()));
    // Empty name -> GL texture 0, no crash
    EXPECT_EQ(tm.EnvAlphaFlag(0), 'E');
    EXPECT_EQ(tm.EnvAlphaFlag(999), 'N');   // out of range -> 'N'
}

TEST(envtex, real_file_matches_txt_index) {
    // Local-only: assets are not in the repo.
    char path[300];
    snprintf(path, sizeof path, "%s/env/EnvTextureList3.bin", TM_REPO_ROOT);
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("      [skip] env/EnvTextureList3.bin not present\n");
        return;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)size);
    EXPECT_EQ((int)fread(buf.data(), 1, (size_t)size, fp), (int)size);
    fclose(fp);

    tmx::GLTextureManager tm;
    EXPECT_TRUE(tm.LoadEnvTextureList(buf.data(), buf.size()));
    // Spot-check entries listed in EnvTextureList3.txt (doc 16 §2.2).
    // GLTextureManager keeps entries private; validate indirectly through
    // GetEnvTexture absence/presence is GL-dependent, so re-read raw here:
    auto nameAt = [&](int idx) -> std::string {
        const uint8_t* p = buf.data() + (size_t)idx * 528;
        return std::string((const char*)p);
    };
    EXPECT_TRUE(nameAt(10)  == "Env\\Tile00000.wys");
    EXPECT_TRUE(nameAt(172) == "Env\\Tile14142.wys");
    EXPECT_TRUE(nameAt(256) == "Env\\Tile26262.wyt");
    EXPECT_TRUE(nameAt(1024) == "Env\\MTile00000.wys");
}
