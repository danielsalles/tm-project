#pragma once

#include <cstdint>
#include <string>

namespace tmx {

class SControlContainer;

// UI string table — g_UIString[500][64] (Basedef.cpp:1172, ReadUIString).
// Loaded from UI/UIString.txt ("index string" per line). Missing file = empty
// strings (the original tolerates this silently).
constexpr int UI_STRING_COUNT = 500;
constexpr int UI_STRING_LEN = 64;

class UILoader {
public:
    // Loads UI/UIString.txt into the string table.
    static bool LoadUIStrings(const char* textData, size_t size);

    // g_UIString lookup — bounds-checked, returns "" for out-of-range.
    static const char* UIString(int index);

    // Faithful port of TMScene::ReadRCBin (TMScene.cpp:315-815): reads a
    // [CONTROL_TYPE][BinXxx] stream and builds the control tree.
    static bool ReadRCBin(const uint8_t* data, size_t size,
                          SControlContainer& container, std::string* err);

private:
    static char s_strings[UI_STRING_COUNT][UI_STRING_LEN];
    static bool s_stringsLoaded;
};

}
