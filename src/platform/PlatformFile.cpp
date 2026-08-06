#include "platform/Platform.h"

#include <SDL3/SDL.h>

#include <sys/stat.h>

namespace tmx {

static std::string s_dataDir;

static std::string NormalizePath(const char* relPath) {
    std::string p = relPath;
    for (auto& c : p) {
        if (c == '\\')
            c = '/';
    }
    return p;
}

const std::string& DataDir() {
    if (s_dataDir.empty()) {
        const char* base = SDL_GetBasePath();
        if (base) {
            s_dataDir = base;
            SDL_free(const_cast<char*>(base));
        } else {
            s_dataDir = "./";
        }
    }
    return s_dataDir;
}

FILE* OpenAsset(const char* relPath, const char* mode) {
    std::string full = DataDir() + NormalizePath(relPath);
    FILE* f = fopen(full.c_str(), mode);
    if (!f)
        f = fopen(NormalizePath(relPath).c_str(), mode);
    return f;
}

bool FileExists(const char* relPath) {
    std::string full = DataDir() + NormalizePath(relPath);
    struct stat st;
    if (stat(full.c_str(), &st) == 0)
        return true;
    return stat(NormalizePath(relPath).c_str(), &st) == 0;
}

int64_t FileSize(const char* relPath) {
    std::string full = DataDir() + NormalizePath(relPath);
    struct stat st;
    if (stat(full.c_str(), &st) != 0 && stat(NormalizePath(relPath).c_str(), &st) != 0)
        return -1;
    return static_cast<int64_t>(st.st_size);
}

}
