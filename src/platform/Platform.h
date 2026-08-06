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

}
