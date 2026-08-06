#include "platform/Platform.h"

#include <SDL3/SDL.h>

#include <ctime>

namespace tmx {

uint32_t GetTicks() {
    return static_cast<uint32_t>(SDL_GetTicks());
}

uint64_t GetPerformanceCounter() {
    return SDL_GetPerformanceCounter();
}

uint64_t GetPerformanceFrequency() {
    return SDL_GetPerformanceFrequency();
}

void GetLocalTime(int& year, int& month, int& day, int& hour, int& min, int& sec) {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    year = tmv.tm_year + 1900;
    month = tmv.tm_mon + 1;
    day = tmv.tm_mday;
    hour = tmv.tm_hour;
    min = tmv.tm_min;
    sec = tmv.tm_sec;
}

}
