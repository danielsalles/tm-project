#pragma once

// GameConfig — Config.bin read/write (SaveUpdatAndConfig, NewApp.h:13-16).
//
// Layout on disk: 30 bytes = int16 Version + int16 Config[14], little-endian.
// The original fread()s the struct directly; we parse explicitly to stay
// endian-safe. Unknown/unused slots are preserved verbatim on rewrite.
//
// Slot map (NewApp.cpp:131-220):
//   [0]  resolution index (table of 11 below)
//   [1]  skinmesh smooth (LOD 0-2)
//   [2]  sound slider 0-100
//   [3]  music slider 0-100
//   [4]  (unused by the client, -1)
//   [5]  brightness 0-100 (gamma ramp gain = value * 0.02)
//   [6]  cursor type
//   [7]  play demo
//   [8]  windowed (0 = fullscreen)
//   [9]  UI version
//   [10] camera rotation inverted
//   [11] DXT textures enabled
//   [12] key type
//   [13] camera view (1 = quarter view)

#include <cstdint>

namespace tmx {

struct GameConfig {
    static constexpr int kVersion = 7000; // original default Version
    static constexpr int kSlotCount = 14;

    int16_t version = kVersion;
    int16_t slot[kSlotCount];

    GameConfig();

    bool Load(const char* path);        // missing file -> defaults, returns false
    bool Save(const char* path) const;

    // Typed accessors
    int  ResIndex() const { return slot[0]; }
    int  Sound() const { return slot[2]; }
    int  Music() const { return slot[3]; }
    int  Bright() const { return slot[5]; }
    bool Windowed() const { return slot[8] != 0; }
    bool CameraRotInverted() const { return slot[10] > 0; }
    bool QuarterView() const { return slot[13] != 0; }

    void SetResIndex(int v) { slot[0] = (int16_t)v; }
    void SetSound(int v) { slot[2] = (int16_t)v; }
    void SetMusic(int v) { slot[3] = (int16_t)v; }
    void SetBright(int v) { slot[5] = (int16_t)v; }

    // Brightness gain for the final blit: the D3D gamma ramp was
    // ramp[i] = bright*0.02*i (RenderDevice.cpp:288-310) — a pure linear gain.
    float BrightGain() const { return slot[5] * 0.02f; }

    // Resolution table (NewApp.cpp:98-131). Index out of range -> 1024x768.
    static void ResolutionWH(int index, int& outW, int& outH);
    static constexpr int kResCount = 11;
};

}
