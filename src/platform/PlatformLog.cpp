#include "platform/Platform.h"

#include <SDL3/SDL.h>

#include <cstdarg>

namespace tmx {

static FILE* s_logFile = nullptr;

void LogInit(const char* filePath) {
    if (s_logFile)
        fclose(s_logFile);
    s_logFile = fopen(filePath, "a");
}

void LogShutdown() {
    if (s_logFile) {
        fclose(s_logFile);
        s_logFile = nullptr;
    }
}

void Log(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SDL_Log("%s", buf);
    if (s_logFile) {
        fputs(buf, s_logFile);
        fputc('\n', s_logFile);
        fflush(s_logFile);
    }
}

}
