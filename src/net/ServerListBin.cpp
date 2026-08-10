#include "net/ServerListBin.h"

#include <cstring>

namespace tmx {

namespace {

// 63-byte key (+1 null) — same table used by the current client
// (.rdata 0x0088eb1c; also documented in the wydElectron project).
const uint8_t kKey[64] = {
    0xc1, 0xb6, 0xc0, 0xcc, 0xc0, 0xd3, 0xc6, 0xd1, 0xc6, 0xae, 0xbe, 0xcf, 0xc8, 0xa3, 0xc8, 0xad,
    0xc0, 0xdb, 0xbe, 0xf7, 0xc0, 0xbb, 0xc0, 0xa7, 0xc7, 0xd1, 0xbd, 0xba, 0xc5, 0xa9, 0xb8, 0xb3,
    0xc6, 0xae, 0xc0, 0xd4, 0xb4, 0xcf, 0xb4, 0xd9, 0xb8, 0xb8, 0xc7, 0xd1, 0xb1, 0xdb, 0xb7, 0xce,
    0xbe, 0xcf, 0xc8, 0xad, 0xc8, 0xad, 0xc7, 0xd2, 0xc1, 0xd9, 0xa4, 0xbb, 0xa4, 0xbb, 0x00, 0x00,
};

constexpr size_t kHeader = 4;
constexpr size_t kRecord = 64;
constexpr size_t kChannelsPerGroup = 11;
constexpr size_t kMaxGroups = 10;

std::string ReadRecordString(const uint8_t* p) {
    size_t end = 0;
    while (end < kRecord && p[end] != 0)
        ++end;
    return std::string((const char*)p, end);
}

} // namespace

bool DecodeServerListBin(const uint8_t* data, size_t size,
                         std::vector<ServerGroup>& outGroups) {
    if (!data || size < kHeader + kRecord * kChannelsPerGroup * kMaxGroups)
        return false;

    std::vector<uint8_t> buf(data + kHeader,
                             data + kHeader + kRecord * kChannelsPerGroup * kMaxGroups);
    for (size_t off = 0; off < buf.size(); off += kRecord) {
        for (size_t j = 0; j < kRecord && off + j < buf.size(); ++j)
            buf[off + j] = (uint8_t)(buf[off + j] - kKey[63 - j]);
    }

    outGroups.clear();
    for (size_t g = 0; g < kMaxGroups; ++g) {
        const uint8_t* base = buf.data() + g * kChannelsPerGroup * kRecord;
        ServerGroup grp;
        grp.statusUrl = ReadRecordString(base);
        for (size_t ch = 1; ch < kChannelsPerGroup; ++ch) {
            std::string ip = ReadRecordString(base + ch * kRecord);
            if (!ip.empty())
                grp.channelIPs.push_back(std::move(ip));
        }
        outGroups.push_back(std::move(grp));
    }
    return true;
}

bool DecodeServerNamesBin(const uint8_t* data, size_t size,
                          std::vector<std::string>& outNames,
                          std::vector<int>& outCounts) {
    if (!data || size < 11 * 9 + 11 * 4)
        return false;
    outNames.clear();
    outCounts.clear();
    for (size_t i = 0; i < 11; ++i) {
        char name[10] = {};
        memcpy(name, data + i * 9, 9);
        outNames.emplace_back(name);
    }
    for (size_t i = 0; i < 11; ++i) {
        const uint8_t* p = data + 11 * 9 + i * 4;
        outCounts.push_back((int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                  ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)));
    }
    return true;
}

} // namespace tmx
