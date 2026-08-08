#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace tmx {

uint32_t GetTicks();
uint64_t GetPerformanceCounter();
uint64_t GetPerformanceFrequency();
void GetLocalTime(int& year, int& month, int& day, int& hour, int& min, int& sec);

void Log(const char* fmt, ...);
void LogInit(const char* filePath);
void LogShutdown();

FILE* OpenAsset(const char* relPath, const char* mode);
bool FileExists(const char* relPath);
int64_t FileSize(const char* relPath);
const std::string& DataDir();
// Overrides the asset base dir (tests point it at the repo root).
void SetDataDir(const char* path);
// Full filesystem path of an existing asset ("" when not found). For
// libraries that open files by path themselves (e.g. miniaudio).
std::string ResolveAssetPath(const char* relPath);

}
