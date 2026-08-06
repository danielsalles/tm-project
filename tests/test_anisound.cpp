#include "test_framework.h"

#include "world/AniSound.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using tmx::AniSoundData;
using tmx::ParseAniSound;

namespace {

std::string ReadText(const char* path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;
    return ss.str();
}

}

TEST(anisound, synthetic_minimal) {
    // One humanoid type (4 class columns) + one monster type, 56 rows each.
    std::string text = "[TestHuman] 0\n";
    for (int i = 0; i < 56; ++i) {
        char row[128];
        snprintf(row, sizeof row, "MOTION%d\t%d 2%d  %d 2%d\t%d 2%d  %d 2%d\t0\n",
                 i, i, i % 10, i + 1, i % 10, i + 2, i % 10, i + 3, i % 10);
        text += row;
    }
    text += "[TestMob] 2\n";
    for (int i = 0; i < 56; ++i) {
        char row[64];
        snprintf(row, sizeof row, "MOTION%d\t%d 3%d  0\n", i, i, i % 10);
        text += row;
    }

    AniSoundData d;
    std::string err;
    EXPECT_TRUE(ParseAniSound(text, d, &err));
    EXPECT_EQ(d.numTypes, 2);
    // humanoid: class columns distinct; mob copy takes class 0
    EXPECT_EQ(d.human[0][0].ani[5], 5u);
    EXPECT_EQ(d.human[3][0].ani[5], 8u);
    EXPECT_EQ(d.human[2][0].speed[7], 27u);
    EXPECT_EQ(d.mob[0].ani[5], 5u);
    // monster: single column
    EXPECT_EQ(d.mob[2].ani[10], 10u);
    EXPECT_EQ(d.mob[2].speed[10], 30u);
}

TEST(anisound, rejects_short_section) {
    std::string text = "[Broken] 0\nSTAND01 0 20 0 20 0 20 0 20 0\n";
    AniSoundData d;
    std::string err;
    EXPECT_TRUE(!ParseAniSound(text, d, &err));
    EXPECT_TRUE(!err.empty());
}

TEST(anisound, real_file_parses_when_present) {
    bool ok = false;
    const std::string text = ReadText(TM_REPO_ROOT "/AniSound4.txt", ok);
    if (!ok) {
        printf("      (AniSound4.txt ausente — pulando parse real)\n");
        return;
    }

    AniSoundData d;
    std::string err;
    EXPECT_TRUE(ParseAniSound(text, d, &err));
    EXPECT_EQ(d.numTypes, 51);  // types 13-19, 52, 58 absent; last is EUC-KR named [..] 59

    // [Knight] 0: STAND01 = ani 0 / speed 20 in all 4 class columns.
    for (int c = 0; c < 4; ++c) {
        EXPECT_EQ(d.human[c][0].ani[0], 0u);
        EXPECT_EQ(d.human[c][0].speed[0], 20u);
    }
    // [Knight] WALK row = 2/13.
    EXPECT_EQ(d.human[0][0].ani[2], 2u);
    EXPECT_EQ(d.human[0][0].speed[2], 13u);

    // [orc] 2: STAND01 0/20, WALK 1/20 (single column).
    EXPECT_EQ(d.mob[2].ani[0], 0u);
    EXPECT_EQ(d.mob[2].speed[0], 20u);
    EXPECT_EQ(d.mob[2].ani[2], 1u);

    // [wolfbear] 3: STAND01 0/26.
    EXPECT_EQ(d.mob[3].speed[0], 26u);

    printf("      60 tipos, Knight WALK ani=%u speed=%u, orc RUN ani=%u\n",
           d.human[0][0].ani[2], d.human[0][0].speed[2], d.mob[2].ani[3]);
}
