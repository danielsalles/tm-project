#include "platform/GameConfig.h"

#include "platform/Platform.h"

#include <cstring>

namespace tmx {

GameConfig::GameConfig() {
    // Defaults from the original else-branch (NewApp.cpp:149-166).
    version = kVersion;
    slot[0] = 7;    // 1024x768
    slot[1] = 2;    // smooth
    slot[2] = 0;    // sound off
    slot[3] = 0;    // music off
    slot[4] = -1;
    slot[5] = 57;   // bright
    slot[6] = 2;    // cursor
    slot[7] = 1;    // play demo
    slot[8] = 1;    // windowed
    slot[9] = 1;    // UI version
    slot[10] = 0;   // camera rot
    slot[11] = 0;   // DXT
    slot[12] = 0;   // key type
    slot[13] = 1;   // quarter view
}

bool GameConfig::Load(const char* path) {
    FILE* fp = OpenAsset(path, "rb");
    if (!fp)
        return false;
    uint8_t raw[30];
    const size_t got = fread(raw, 1, sizeof raw, fp);
    fclose(fp);
    if (got < sizeof raw)
        return false;

    auto rd16 = [&](int i) -> int16_t {
        return (int16_t)((uint16_t)raw[i * 2] | ((uint16_t)raw[i * 2 + 1] << 8));
    };
    version = rd16(0);
    for (int i = 0; i < kSlotCount; ++i)
        slot[i] = rd16(1 + i);
    return true;
}

bool GameConfig::Save(const char* path) const {
    // Writes to the data dir root (next to the executable assets).
    std::string full = DataDir() + path;
    FILE* fp = fopen(full.c_str(), "wb");
    if (!fp)
        fp = fopen(path, "wb");
    if (!fp)
        return false;
    uint8_t raw[30];
    auto wr16 = [&](int i, int16_t v) {
        raw[i * 2] = (uint8_t)((uint16_t)v & 0xFF);
        raw[i * 2 + 1] = (uint8_t)(((uint16_t)v >> 8) & 0xFF);
    };
    wr16(0, version);
    for (int i = 0; i < kSlotCount; ++i)
        wr16(1 + i, slot[i]);
    const bool ok = fwrite(raw, 1, sizeof raw, fp) == sizeof raw;
    fclose(fp);
    return ok;
}

void GameConfig::ResolutionWH(int index, int& outW, int& outH) {
    // NewApp.cpp:98-131 (5/6/7 duplicate 0/1/2 — legacy 16-bit slots).
    static const int kW[kResCount] = { 640, 800, 1024, 1280, 1600, 640, 800, 1024, 1280, 1600, 3200 };
    static const int kH[kResCount] = { 480, 600,  768, 1024, 1200, 480, 600,  768, 1024, 1200, 2400 };
    if (index < 0 || index >= kResCount)
        index = 2;
    outW = kW[index];
    outH = kH[index];
}

}
