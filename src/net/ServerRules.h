#pragma once

#include <cstdint>

namespace tmx {

// Server-select display rules (WYD 769.2, decompiled client).
namespace ServerRules {

// Gauge fill value from a user count (WYD 769.2.c:212949-212950):
// counts below 300 are doubled before feeding the 0..500 gauge.
inline int GaugeFillFromCount(int count) {
    return count < 300 ? count * 2 : count;
}

// Crown ("castle channel") — local PC-date rule (sub_516A50,
// WYD 769.2.c:211691-211703; BASE_GetWeekNumber, Basedef.cpp:423-430):
// unixDays = time(NULL)/86400. Crown shows only on Sundays (siege day),
// alternating channels by week parity: week%2 != (channel-1)%2.
inline bool CrownChannelLocal(int channelIndex1Based, int64_t unixDays) {
    const int64_t d = unixDays - 3;              // week starts Sunday
    const int64_t week = d / 7;
    return (d % 7 == 0) && ((week % 2) != ((channelIndex1Based - 1) % 2));
}

// Crown — server-driven rule (WYD 769.2.c:212868): used when the status
// feed's 11th field (weekFlag) == 1; `day` is the feed's 12th field.
inline bool CrownChannelServerRule(int channelIndex1Based, int day) {
    return (channelIndex1Based % 2) == (((day - 1) / 7 + 1) % 2);
}

// FULL label: count > 900 pads the channel name to 14 columns and appends
// "FULL" (WYD 769.2.c:212902-212916). Selection is blocked at count >= 900
// (0x384, WYD 769.2.c:340566) — note the strict/non-strict split: 900
// blocks without showing FULL.
constexpr int kFullLabelThreshold = 900;   // label when count > this
constexpr int kFullBlockThreshold = 900;   // block when count >= this

// Gauge colors (WYD 769.2.c:213076-213079): normal channels red, the day's
// recommended channel (wDay % numChannels) green.
constexpr uint32_t kGaugeColorNormal = 0xFFFF0000;
constexpr uint32_t kGaugeColorRecommended = 0xFF00FF99;

} // namespace ServerRules
} // namespace tmx
