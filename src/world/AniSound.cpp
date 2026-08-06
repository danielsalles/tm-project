#include "world/AniSound.h"

#include <sstream>

namespace tmx {

bool ParseAniSound(const std::string& text, AniSoundData& out, std::string* err) {
    std::istringstream in(text);
    std::string line;
    int curType = -1;
    int curRow = 0;

    auto fail = [&](const char* msg) {
        if (err) *err = std::string("AniSound4: ") + msg;
        return false;
    };

    while (std::getline(in, line)) {
        // strip CR
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream ss(line);
        std::string first;
        if (!(ss >> first))
            continue; // blank

        if (first.front() == '[') {
            // header: "[Name] type"
            if (curType >= 0 && curRow != kAniMotions)
                return fail("section with wrong motion count");
            int type = -1;
            // header name may contain spaces? original uses "%s %d" -> no spaces.
            if (!(ss >> type))
                return fail("bad section header");
            if (type < 0 || type >= kAniTypes)
                return fail("type out of range");
            curType = type;
            curRow = 0;
            out.numTypes++;
            continue;
        }

        if (curType < 0)
            return fail("row before any section");
        if (curRow >= kAniMotions)
            return fail("too many rows in section");

        if (curType < kAniHumanTypes) {
            uint32_t a[kAniClasses], s[kAniClasses];
            uint32_t snd;
            for (int c = 0; c < kAniClasses; ++c) {
                if (!(ss >> a[c] >> s[c]))
                    return fail("bad human row");
            }
            if (!(ss >> snd))
                return fail("bad human row (sound)");
            for (int c = 0; c < kAniClasses; ++c) {
                out.human[c][curType].ani[curRow] = a[c];
                out.human[c][curType].speed[curRow] = s[c];
            }
            out.mob[curType].ani[curRow] = a[0];
            out.mob[curType].speed[curRow] = s[0];
        } else {
            uint32_t a, s, snd;
            if (!(ss >> a >> s >> snd))
                return fail("bad monster row");
            out.mob[curType].ani[curRow] = a;
            out.mob[curType].speed[curRow] = s;
        }
        ++curRow;
    }

    if (curType >= 0 && curRow != kAniMotions)
        return fail("last section with wrong motion count");
    if (out.numTypes == 0)
        return fail("no sections");
    return true;
}

}
