// test_serverlist.cpp — Phase 8c: serverlist.bin / sn.bin decoders.
#include "test_framework.h"
#include "net/ServerListBin.h"

#include <cstring>
#include <string>
#include <vector>

using namespace tmx;

namespace {

std::vector<uint8_t> ReadFile(const char* rel) {
    FILE* f = fopen((std::string(TM_REPO_ROOT) + "/" + rel).c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> out((size_t)sz);
    if (fread(out.data(), 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return {}; }
    fclose(f);
    return out;
}

} // namespace

TEST(serverlist, decode_official_bin) {
    auto bytes = ReadFile("serverlist.bin");
    EXPECT_TRUE(bytes.size() >= 7044);
    if (bytes.size() < 7044) return;

    std::vector<ServerGroup> groups;
    EXPECT_TRUE(DecodeServerListBin(bytes.data(), bytes.size(), groups));
    EXPECT_TRUE(groups.size() == 10);

    // Official WYD Global (current serverlist.bin): group 1 has the raidhut
    // status URL + 6 channels.
    EXPECT_TRUE(groups[1].statusUrl == "http://server-status.raidhut.com/wyd-s01.php");
    EXPECT_TRUE(groups[1].channelIPs.size() == 6);
    if (groups[1].channelIPs.size() == 6) {
        EXPECT_TRUE(groups[1].channelIPs[0] == "92.38.151.230");
        EXPECT_TRUE(groups[1].channelIPs[1] == "92.38.151.231");
        EXPECT_TRUE(groups[1].channelIPs[2] == "92.38.151.232");
        EXPECT_TRUE(groups[1].channelIPs[5] == "92.38.151.235");
    }
    // Other groups are empty in the official file.
    EXPECT_TRUE(groups[0].channelIPs.empty());
    EXPECT_TRUE(groups[9].channelIPs.empty());
}

TEST(serverlist, decode_sn_bin) {
    auto bytes = ReadFile("sn.bin");
    EXPECT_TRUE(bytes.size() >= 143);
    if (bytes.size() < 143) return;

    std::vector<std::string> names;
    std::vector<int> counts;
    EXPECT_TRUE(DecodeServerNamesBin(bytes.data(), bytes.size(), names, counts));
    EXPECT_TRUE(names.size() == 11);
    EXPECT_TRUE(counts.size() == 11);
    // names[1] = "Global" (the group with servers in serverlist.bin)
    EXPECT_TRUE(names[1] == "Global");
}

TEST(serverlist, decode_rejects_short_input) {
    uint8_t junk[64] = {};
    std::vector<ServerGroup> groups;
    EXPECT_TRUE(!DecodeServerListBin(junk, sizeof junk, groups));
}

#include "net/ServerRules.h"

TEST(serverlist, rules_gauge_fill) {
    using namespace tmx::ServerRules;
    EXPECT_TRUE(GaugeFillFromCount(100) == 200);   // <300 doubled (WYD 769.2.c:212949)
    EXPECT_TRUE(GaugeFillFromCount(299) == 598);
    EXPECT_TRUE(GaugeFillFromCount(300) == 300);
    EXPECT_TRUE(GaugeFillFromCount(897) == 897);
}

TEST(serverlist, rules_crown_local) {
    using namespace tmx::ServerRules;
    // 2026-08-09 = Sunday; epoch day 20674 → (20674-3)=20671=7*2953, week odd.
    // Crown = Sunday && week%2 != (ch-1)%2 → odd channels (1,3,5).
    const int64_t day = 20674;
    EXPECT_TRUE(CrownChannelLocal(1, day));
    EXPECT_TRUE(!CrownChannelLocal(2, day));
    EXPECT_TRUE(CrownChannelLocal(3, day));
    EXPECT_TRUE(!CrownChannelLocal(4, day));
    EXPECT_TRUE(CrownChannelLocal(5, day));
    EXPECT_TRUE(!CrownChannelLocal(6, day));
    // Next day (Monday) → no crowns at all.
    EXPECT_TRUE(!CrownChannelLocal(1, day + 1));
    EXPECT_TRUE(!CrownChannelLocal(3, day + 1));
    // Next Sunday (week 2954, even) → even channels.
    EXPECT_TRUE(!CrownChannelLocal(1, day + 7));
    EXPECT_TRUE(CrownChannelLocal(2, day + 7));
}

TEST(serverlist, rules_crown_server_driven) {
    using namespace tmx::ServerRules;
    // day=8: ((8-1)/7+1)%2 = (1+1)%2 = 0 → even channels (ch%2==0)
    EXPECT_TRUE(!CrownChannelServerRule(1, 8));
    EXPECT_TRUE(CrownChannelServerRule(2, 8));
    // day=15: ((15-1)/7+1)%2 = (2+1)%2 = 1 → odd channels
    EXPECT_TRUE(CrownChannelServerRule(1, 15));
    EXPECT_TRUE(!CrownChannelServerRule(2, 15));
}

TEST(serverlist, rules_full_thresholds) {
    using namespace tmx::ServerRules;
    // Label FULL only when count > 900; block at >= 900 (edge case faithful).
    EXPECT_TRUE(kFullLabelThreshold == 900);
    EXPECT_TRUE(kFullBlockThreshold == 900);
    EXPECT_TRUE(897 <= kFullLabelThreshold);   // Global-3 @897: no FULL label
    EXPECT_TRUE(900 >= kFullBlockThreshold);   // but blocks selection
}
