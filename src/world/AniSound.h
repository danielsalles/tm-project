#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace tmx {

// AniSound4.txt -> motion tables (ObjectManager::InitAniSoundTable,
// ObjectManager.cpp:859-907). Sounds are parsed but ignored (no audio yet).
//
// Types 0/1 (ch01/ch02 humanoids) carry 4 class columns (TK/Foema/BM/Hunter);
// types >= 2 (monsters) carry a single ani/speed column. Each type has 56 motion
// rows: 0..27 normal (ECHAR_MOTION order), 28..55 mounted (M* rows).
constexpr int kAniClasses = 4;
constexpr int kAniHumanTypes = 2;
constexpr int kAniTypes = 60;
constexpr int kAniMotions = 56;

struct MobAniTable {
    std::array<uint32_t, kAniMotions> speed{};
    std::array<uint32_t, kAniMotions> ani{};
};

struct AniSoundData {
    // Humanoid types 0/1, per class.
    MobAniTable human[kAniClasses][kAniHumanTypes];
    // All 60 types; for 0/1 this is the class-0 copy (mirrors g_MobAniTable).
    MobAniTable mob[kAniTypes];
    // Type index per section as read (file order == type id in practice).
    int numTypes = 0;
};

bool ParseAniSound(const std::string& text, AniSoundData& out, std::string* err);

}
