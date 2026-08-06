#include "test_framework.h"

#include "world/AniSound.h"
#include "world/Character.h"
#include "platform/Platform.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using tmx::AniSoundData;
using tmx::Character;
using tmx::CharacterAnimationCache;
using tmx::CharDesc;
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

std::string ReadTextAsset(const char* rel, bool& ok) {
    FILE* f = tmx::OpenAsset(rel, "rb");
    if (!f) { ok = false; return {}; }
    std::ostringstream ss;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        ss.write(buf, (std::streamsize)n);
    fclose(f);
    ok = true;
    return ss.str();
}

}

TEST(charpick, ray_hits_cylinder) {
    tmx::SetDataDir(TM_REPO_ROOT);
    bool okA = false, okB = false;
    const std::string aniTxt = ReadText(TM_REPO_ROOT "/AniSound4.txt", okA);
    const std::string boneTxt = ReadTextAsset("Mesh/BoneAni4.txt", okB);
    if (!okA || !okB) {
        printf("      (assets ausentes — pulando)\n");
        return;
    }
    AniSoundData ani;
    CharacterAnimationCache cache;
    std::string err;
    EXPECT_TRUE(ParseAniSound(aniTxt, ani, &err));
    EXPECT_TRUE(cache.Init(boneTxt, &err));

    std::vector<int8_t> mask(128 * 128, 10);
    Character c;
    CharDesc d;
    if (!c.InitLogic(cache, ani, d, mask.data(), 128, 128, 0.0f, 0.0f, &err)) {
        printf("      (sem ch01 — pulando)\n");
        return;
    }
    c.SetPosition(50.5f, 50.5f);   // height = 1.0 (mask 10 * 0.1)

    // Ray straight down -X at the char center: origin (60, 1, 50.5) dir (-1,0,0).
    const float ro[3] = { 60.0f, 1.0f, 50.5f };
    const float rd[3] = { -1.0f, 0.0f, 0.0f };
    const float t = c.PickTest(ro, rd);
    EXPECT_TRUE(t > 9.0f && t < 10.0f);   // radius 0.4 for type 0

    // Miss: 1 unit to the side.
    const float ro2[3] = { 60.0f, 1.0f, 52.0f };
    EXPECT_TRUE(c.PickTest(ro2, rd) < 0.0f);

    // Miss: above the head (type 0 height = 2.0, base 1.0 -> top 3.0).
    const float ro3[3] = { 60.0f, 5.0f, 50.5f };
    EXPECT_TRUE(c.PickTest(ro3, rd) < 0.0f);
}
