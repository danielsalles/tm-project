#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tmx {

// serverlist.bin (current WYD Global client): 4-byte header + 10 groups ×
// 11 entries × 64 bytes. Entry 0 of each group = status URL, entries 1..10
// = channel IPs (port 8281). Decryption: subtract key[63-j] from data[j]
// per byte in each 64-byte block (key from the client's .rdata).
struct ServerGroup {
    std::string statusUrl;
    std::vector<std::string> channelIPs;
};

bool DecodeServerListBin(const uint8_t* data, size_t size,
                         std::vector<ServerGroup>& outGroups);

// sn.bin: 11 × 9-byte group names + 11 × i32 channel counts (143 bytes).
bool DecodeServerNamesBin(const uint8_t* data, size_t size,
                          std::vector<std::string>& outNames,
                          std::vector<int>& outCounts);

} // namespace tmx
