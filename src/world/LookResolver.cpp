#include "world/LookResolver.h"

#include <cstdio>
#include <cstring>

namespace tmx {

namespace {

// TMSkinMesh::God2Exception (TMSkinMesh.cpp:1947+): prefix letter test.
// szAniName[5] is the first prefix letter ("mesh\<prefix>"); C precedence makes
// each && group bind tighter than the ||s.
bool God2Exception(const char* p, int i) {
    const char c5 = p[5], c6 = p[6], c8 = p[8];
    return c5 == 'g' || c5 == 'o'
        || (c5 == 'd' && c6 == 'r' && c8 == '2' && i == 1)
        || (c5 == 'd' && c6 == 'r' && c8 == '1')
        || (c5 == 'b' && c6 == 'd' && i == 1)
        || (c5 == 'b' && c6 == 'e')
        || (c5 == 'b' && c6 == 'o')
        || (c5 == 'b' && c6 == 'm')
        || (c5 == 'h' && c6 == 'y')
        || (c5 == 's' && c6 == 'p')
        || (c5 == 'c' && c6 == 'r')
        || (c5 == 'w' && c6 == 'b')
        || (c5 == 'w' && c6 == 'f')
        || (c5 == 'c' && c6 == 'b')
        || (c5 == 'm' && c6 == 'i')
        || (c5 == 'm' && c6 == 'o')
        || (c5 == 't' && c6 == 'w')
        || (c5 == 't' && c6 == 'r')
        || (c5 == 'h' && c6 == 's' && i == 1)
        || (c5 == 'e' && c6 == 't')
        || (c5 == 'b' && c6 == 'n')
        || (c5 == 'r' && c6 == 'c')
        || (c5 == 'f' && c6 == 'n')
        || (c5 == 'b' && c6 == 'l')
        || (c5 == 't' && c6 == 'g');
}

// TMSkinMesh::MantleException (TMSkinMesh.cpp:1881-1945).
bool MantleException(const char* tex) {
    static const char* const kList[] = {
        "mesh\\mt0101170.wyt", "mesh\\mt0101171.wyt", "mesh\\mt0101172.wyt",
        "mesh\\mt0101173.wyt", "mesh\\mt0101174.wyt", "mesh\\mt0101175.wyt",
        "mesh\\mt0101176.wyt", "mesh\\mt0101177.wyt", "mesh\\mt0101178.wyt",
        "mesh\\mt0101179.wyt", "mesh\\mt0101180.wyt", "mesh\\mt0101181.wyt",
        "mesh\\mt0101182.wyt", "mesh\\mt0101183.wyt", "mesh\\mt0101184.wyt",
        "mesh\\mt0101185.wyt", "mesh\\mt0101186.wyt", "mesh\\mt0101187.wyt",
        "mesh\\mt0101188.wyt", "mesh\\mt0101189.wyt", "mesh\\mt0101190.wyt",
        "mesh\\mt0101191.wyt", "mesh\\mt0101192.wyt", "mesh\\mt0101193.wyt",
        "mesh\\mt0101195.wyt", "mesh\\mt0101196.wyt", "mesh\\mt0101197.wyt",
        "mesh\\mt0101198.wyt", "mesh\\mt0101199.wyt", "mesh\\mt0101200.wyt",
    };
    for (const char* s : kList) {
        if (!strcmp(tex, s))
            return true;
    }
    return false;
}

bool StartsWith(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

} // namespace

void ResolveLookParts(const LookInput& in, LookPart out[8]) {
    const bool fixedVariant = in.boneAniIndex == 45 || in.boneAniIndex == 46 ||
                              in.boneAniIndex == 53 || in.boneAniIndex == 54;
    const bool god2Index53 = in.boneAniIndex == 53;

    for (int i = 0; i < 8; ++i) {
        LookPart& part = out[i];
        const int meshLook = in.meshLook[i];
        const int skinLook = in.skinLook[i];

        part.visible = (in.meshLook[0] < 90) || i == 0 || meshLook != 0;
        if (!part.visible) {
            part.mesh[0] = part.tex[0] = '\0';
            continue;
        }

        const int variant = meshLook + 20 * in.bExpand + 1;
        snprintf(part.mesh, sizeof part.mesh, "%s%02d%02d.msh", in.prefix, i + 1,
                 fixedVariant ? 1 : variant);

        if (God2Exception(in.prefix, i)) {
            // Single texture (part 01) for every part.
            const int v = (skinLook & 0xFFF) + variant;
            snprintf(part.tex, sizeof part.tex, "%s%02d%02d.wyt", in.prefix,
                     god2Index53 ? (i + 1) : 1, god2Index53 ? 1 : v);
        } else {
            const int v = (skinLook & 0xFFF) + variant;
            snprintf(part.tex, sizeof part.tex, "%s%02d%02d.wyt", in.prefix, i + 1,
                     fixedVariant ? 1 : v);
        }

        // --- exception table (verbatim order) ---
        if (!strcmp(part.mesh, "mesh\\ch010218.msh") &&
            !strcmp(part.tex, "mesh\\ch010219.wyt")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\ch010214.wyt");
        } else if (MantleException(part.tex)) {
            snprintf(part.mesh, sizeof part.mesh, "mesh\\mt010131.msh");
        } else if (!strcmp(part.tex, "mesh\\mt010124.wyt")) {
            snprintf(part.mesh, sizeof part.mesh, "mesh\\mt010124.msh");
        } else if (!strcmp(part.tex, "mesh\\mt010132.wyt") ||
                   !strcmp(part.tex, "mesh\\mt010133.wyt") ||
                   !strcmp(part.tex, "mesh\\mt010134.wyt") ||
                   !strcmp(part.tex, "mesh\\mt010135.wyt") ||
                   !strcmp(part.tex, "mesh\\mt010136.wyt") ||
                   !strcmp(part.tex, "mesh\\mt010137.wyt")) {
            snprintf(part.mesh, sizeof part.mesh, "mesh\\mt010131.msh");
        }

        // ch02?13? textures with variant digit 1/4/5 redirect to ch01?30.
        if (part.tex[5] == 'c' && part.tex[6] == 'h' && part.tex[8] == '2' &&
            part.tex[11] == '1' && part.tex[12] == '3') {
            if (part.tex[10] == '1')
                snprintf(part.tex, sizeof part.tex, "mesh\\ch010130.wyt");
            if (part.tex[10] == '4')
                snprintf(part.tex, sizeof part.tex, "mesh\\ch010430.wyt");
            if (part.tex[10] == '5')
                snprintf(part.tex, sizeof part.tex, "mesh\\ch010530.wyt");
        }

        if (!strcmp(part.tex, "mesh\\ch020315.wyt")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\ch020314.wyt");
        } else if (!strcmp(part.tex, "mesh\\bm010102.wyt")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\mi010105.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr13")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr130101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr14")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr130101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr15")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr130101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr16")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr130101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr17")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr130101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr190101")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr180101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr190102")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr180102.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr200101")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr180101.wyt");
        } else if (StartsWith(part.tex, "mesh\\tr200102")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\tr180102.wyt");
        } else if (StartsWith(part.tex, "mesh\\ch010237")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\ch010137.wyt");
        } else if (StartsWith(part.tex, "mesh\\ch010238")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\ch010138.wyt");
        } else if (StartsWith(part.tex, "mesh\\ch020217")) {
            snprintf(part.tex, sizeof part.tex, "mesh\\ch020117.wyt");
        }
    }
}

}
