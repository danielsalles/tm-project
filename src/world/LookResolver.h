#pragma once

#include <cstdint>

namespace tmx {

// Look -> per-part file names (TMSkinMesh::RestoreDeviceObjects,
// TMSkinMesh.cpp:160-330). Pure function; the exceptions table is ported
// verbatim (mantles, ch02->ch01 texture redirects, tr1x aliases, etc.).
//
// Base rules (bExpand = 0 in our scope):
//   mesh = <prefix>%02d(part+1)%02d(meshLook + 1).msh
//   tex  = <prefix>%02d(part+1)%02d((skinLook & 0xFFF) + meshLook + 1).wyt
// BoneAni4 indices 45/46/53/54 force variant 01. God2Exception prefixes use
// part 01 in the texture name for all parts.
struct LookInput {
    int boneAniIndex = 0;
    char prefix[32] = {};          // BoneAni4 szAniName, e.g. "mesh\\ch01"
    int16_t meshLook[8] = {};      // LOOK_INFO Mesh0..7
    int16_t skinLook[8] = {};      // LOOK_INFO Skin0..7
    int bExpand = 0;               // god2 expand flag (0 = base game)
};

struct LookPart {
    char mesh[80];                 // relative path with extension
    char tex[80];                  // relative path with extension (.wyt)
    bool visible = false;          // original rule: (look[0] < 90 || i==0 || meshLook[i])
};

void ResolveLookParts(const LookInput& in, LookPart out[8]);

}
