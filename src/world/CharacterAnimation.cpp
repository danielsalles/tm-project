#include "world/CharacterAnimation.h"

#include "platform/Platform.h"

#include <cstdio>
#include <cstring>

namespace tmx {

namespace {

bool ReadAsset(const char* relPath, std::vector<uint8_t>& out) {
    FILE* f = OpenAsset(relPath, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)sz);
    bool ok = fread(out.data(), 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    return ok;
}

bool AssetExists(const char* relPath) {
    FILE* f = OpenAsset(relPath, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

} // namespace

bool LoadValidIndex(const char* path, std::vector<int32_t>& out, std::string* err) {
    std::vector<uint8_t> bytes;
    if (!ReadAsset(path, bytes)) {
        if (err) *err = std::string("validindex: missing ") + path;
        return false;
    }
    if (bytes.size() < 100 * 186 * 4) {
        if (err) *err = "validindex: truncated";
        return false;
    }
    out.resize(100 * 186);
    memcpy(out.data(), bytes.data(), out.size() * 4);
    return true;
}

bool LoadCharacterAnimation(int boneAniIndex, const char* prefix, int numAniTypeCount,
                            const int32_t* validRow, bool humanoid,
                            CharacterAnimation& out, std::string* err) {
    // Skeleton: reuse the .bon half of LoadBoneAni by loading a dummy-free parse.
    // LoadBoneAni requires an .ani too, so do the .bon part inline (same rules).
    {
        char path[160];
        snprintf(path, sizeof path, "%s.bon", prefix);
        std::vector<uint8_t> bon;
        if (!ReadAsset(path, bon)) {
            if (err) *err = std::string("charani: missing ") + path;
            return false;
        }
        if (bon.size() < 8 || bon.size() % 8) {
            if (err) *err = "charani: bad .bon size";
            return false;
        }
        const size_t pairs = bon.size() / 8;
        int maxId = 0;
        for (size_t i = 0; i < pairs; ++i) {
            const int id = (int)ReadU32(bon.data() + i * 8 + 4);
            if (id > maxId)
                maxId = id;
        }
        out.base.frames.resize((size_t)maxId + 1);
        out.base.rootId = 0;
        for (size_t i = 0; i < pairs; ++i) {
            const uint32_t parent = ReadU32(bon.data() + i * 8);
            const int id = (int)ReadU32(bon.data() + i * 8 + 4);
            BoneAniSet::Frame& f = out.base.frames[id];
            f.id = id;
            f.parent = (parent == 0xFFFFFFFFu) ? -1 : (int)parent;
            if (f.parent >= 0 && (size_t)f.parent < out.base.frames.size())
                out.base.frames[f.parent].children.push_back(id);
        }
    }

    memset(out.animMap, 0xFF, sizeof out.animMap);  // -1 = no cut
    out.dwModMinus2 = (boneAniIndex == 49);

    // Pass 1: count ticks / discover which files exist.
    uint32_t totalTicks = 0;
    std::vector<uint8_t> exists((size_t)numAniTypeCount, 0);
    std::vector<uint32_t> fileTicks((size_t)numAniTypeCount, 0);
    uint32_t numBones = 0;
    for (int nFI = 0; nFI < numAniTypeCount; ++nFI) {
        char path[160];
        snprintf(path, sizeof path, "%s%04d.ani", prefix, validRow[nFI] + 1);
        std::vector<uint8_t> ani;
        if (!ReadAsset(path, ani) || ani.size() < 8)
            continue;
        const uint32_t ticks = ReadU32(ani.data());
        const uint32_t bones = ReadU32(ani.data() + 4);
        if (ticks == 0 || bones == 0 || bones > 100 ||
            ani.size() < 8 + (size_t)ticks * bones * 64)
            continue;
        if (numBones == 0)
            numBones = bones;
        exists[(size_t)nFI] = 1;
        fileTicks[(size_t)nFI] = ticks;
        totalTicks += ticks;
    }
    if (numBones == 0 || totalTicks == 0) {
        if (err) *err = std::string("charani: no .ani cuts for ") + prefix;
        return false;
    }
    out.numBones = numBones;
    out.mats.resize((size_t)totalTicks * numBones);
    if (humanoid)
        out.quats.resize((size_t)totalTicks * numBones);

    // Pass 2: load cuts, build animMap with the original fallbacks
    // (MeshManager.cpp:158-335; only the index 0/1 humanoid rules are ported —
    // monsters address cuts directly through g_MobAniTable).
    uint32_t dwFileIndex = 0;
    size_t tickOffset = 0;
    for (int nFI = 0; nFI < numAniTypeCount; ++nFI) {
        if (!exists[(size_t)nFI])
            continue;

        char path[160];
        snprintf(path, sizeof path, "%s%04d.ani", prefix, validRow[nFI] + 1);
        std::vector<uint8_t> ani;
        if (!ReadAsset(path, ani))  // raced deletion: keep indexes consistent
            continue;

        const uint32_t ticks = fileTicks[(size_t)nFI];
        out.numAniCut.push_back(ticks);
        out.cutTickOffset.push_back((uint32_t)tickOffset);

        D3DXMATRIX* dst = out.mats.data() + tickOffset * numBones;
        memcpy(dst, ani.data() + 8, (size_t)ticks * numBones * sizeof(D3DXMATRIX));

        if (humanoid) {
            D3DXQUATERNION* qdst = out.quats.data() + tickOffset * numBones;
            for (size_t j = 0; j < (size_t)ticks * numBones; ++j)
                D3DXQuaternionRotationMatrix(&qdst[j], &dst[j]);
        }

        if (humanoid) {
            const int nArrayIndex = validRow[nFI] + 1;
            const int nWeapon = nArrayIndex / 100 - 1;
            const int nAnimation = nArrayIndex % 100 - 1;
            if (nWeapon >= 0 && nWeapon < kMaxWeaponSlots &&
                nAnimation >= 0 && nAnimation < kCharMotions) {
                out.animMap[nWeapon][nAnimation] = (int16_t)dwFileIndex;

                // TK-BM: attacks 4..8 fill forward to 9; punish 25..28 fill to 29.
                if (boneAniIndex == 0 || boneAniIndex == 1) {
                    if (nAnimation >= 4 && nAnimation < 9) {
                        for (int r = nAnimation + 1; r < 10; ++r)
                            out.animMap[nWeapon][r] = (int16_t)dwFileIndex;
                    }
                    if (nAnimation >= 25 && nAnimation < 29) {
                        for (int r = nAnimation + 1; r < 30; ++r)
                            out.animMap[nWeapon][r] = (int16_t)dwFileIndex;
                    }
                }
                if (boneAniIndex == 0) {
                    // Weapons >= 12 inherit die/dead/levelup from weapon 11.
                    if (nWeapon >= 12) {
                        out.animMap[nWeapon][11] = out.animMap[11][11];
                        out.animMap[nWeapon][12] = out.animMap[11][12];
                        out.animMap[nWeapon][14] = out.animMap[11][14];
                    }
                    // File 138: copy weapon-0 row over every other slot.
                    if (nArrayIndex == 138) {
                        for (int t = 1; t < kMaxWeaponSlots; ++t)
                            for (int m = 0; m < kCharMotions; ++m)
                                out.animMap[t][m] = out.animMap[0][m];
                    }
                } else { // boneAniIndex == 1
                    // Weapon 2 attacks 0..3 fall back to weapon 1.
                    if (nWeapon == 2 && nAnimation == 4) {
                        for (int r = 0; r < 4; ++r)
                            out.animMap[2][r] = out.animMap[1][r];
                    }
                    if (nArrayIndex == 137) {
                        for (int t = 1; t < kMaxWeaponSlots; ++t)
                            for (int m = 0; m < kCharMotions; ++m)
                                out.animMap[t][m] = out.animMap[0][m];
                    }
                }
            }
        }

        tickOffset += ticks;
        if ((int)++dwFileIndex >= numAniTypeCount)
            break;
    }
    out.numCuts = (int)dwFileIndex;

    out.base.numTicks = 0;   // unused for characters (cuts drive sampling)
    out.base.numBones = (int)numBones;
    out.base.matRot.resize(out.base.frames.size());
    out.base.combined.resize(out.base.frames.size());
    for (size_t i = 0; i < out.base.frames.size(); ++i)
        D3DXMatrixIdentity(&out.base.matRot[i]);
    return true;
}

bool CharSetAnimation(const CharacterAnimation& anim, CharPlayback& pb, int cut,
                      uint32_t fps, uint32_t nowMs) {
    if (cut < 0 || cut >= anim.numCuts)
        return false;
    if (pb.cut == cut)
        return false;

    // Freeze the current pose for the crossfade (TMSkinMesh::SetAnimation):
    // m_dwTickLast = numAniFrame * (m_dwOffset + m_nAniBaseIndex).
    uint32_t dwOffset = pb.fps ? (nowMs - pb.switchTimeMs) / pb.fps : 0;
    uint32_t dwMod = anim.numAniCut[(size_t)pb.cut];
    if (anim.dwModMinus2 && dwMod >= 2)
        dwMod -= 2;
    if (dwMod > 0) {
        dwOffset %= 4 * dwMod;
        const uint32_t tickInCut = dwOffset / 4;
        pb.tickLastFlat = anim.cutTickOffset[(size_t)pb.cut] + tickInCut;
    } else {
        pb.tickLastFlat = anim.cutTickOffset[(size_t)pb.cut];
    }

    pb.lastCut = pb.cut;
    pb.cut = cut;
    pb.fps = fps ? fps : 30;
    pb.switchTimeMs = nowMs;
    pb.blending = true;
    return true;
}

void SampleCharacter(const CharacterAnimation& anim, CharacterPose& pose,
                     CharPlayback& pb, uint32_t nowMs, const D3DXMATRIX& rootWorld) {
    const BoneAniSet& base = anim.base;
    const size_t numFrames = base.frames.size();
    if (pose.matRot.size() < numFrames)
        return;

    uint32_t dwMod = anim.numAniCut.empty() ? 0 : anim.numAniCut[(size_t)pb.cut];
    if (anim.dwModMinus2 && dwMod >= 2)
        dwMod -= 2;

    if (dwMod == 0 || anim.numBones == 0) {
        for (size_t id = 0; id < numFrames; ++id)
            D3DXMatrixIdentity(&pose.matRot[id]);
    } else {
        const uint32_t fps = pb.fps ? pb.fps : 30;
        uint32_t dwOffset = (nowMs - pb.switchTimeMs) / fps;
        dwOffset %= 4 * dwMod;
        const uint32_t tickInCut = dwOffset / 4;
        const uint32_t tickFlat = anim.cutTickOffset[(size_t)pb.cut] + tickInCut;
        const uint32_t addr = anim.numBones * tickFlat;
        const uint32_t numBone = anim.numBones;
        const uint32_t EndEdge = 4 * dwMod - 3;
        const bool crossfade = pb.blending && dwOffset < 10;

        for (uint32_t j = 0; j < numBone && j < numFrames; ++j) {
            const D3DXMATRIX& cur = anim.mats[addr + j];

            if (!crossfade) {
                const uint32_t mod = dwOffset % 4;
                if (mod == 0) {
                    pose.matRot[j] = cur;
                } else {
                    // Next tick; wraps to the cut's first tick at the end edge.
                    const uint32_t nextFlat = (dwOffset >= EndEdge)
                        ? anim.cutTickOffset[(size_t)pb.cut]
                        : tickFlat + 1;
                    const D3DXMATRIX& nxt = anim.mats[anim.numBones * nextFlat + j];
                    D3DXMATRIX& o = pose.matRot[j];
                    const float* a = &cur._11;
                    const float* b = &nxt._11;
                    float* op = &o._11;
                    // Original fixed weights: mod1 = (3a+b)/4, mod2 = (a+b)/2,
                    // mod3 = (a+3b)/4.
                    const float wb = mod / 4.0f;
                    for (int k = 0; k < 16; ++k)
                        op[k] = a[k] + (b[k] - a[k]) * wb;
                }
            } else {
                // 10-step crossfade from the frozen previous pose.
                const D3DXMATRIX& before = anim.mats[anim.numBones * pb.tickLastFlat + j];
                const float inv = (float)(10 - dwOffset);  // 10..1
                if (!anim.quats.empty()) {
                    D3DXQUATERNION q;
                    D3DXQuaternionSlerp(&q, &anim.quats[addr + j],
                                        &anim.quats[anim.numBones * pb.tickLastFlat + j],
                                        inv / 10.0f);
                    D3DXMATRIX qm;
                    D3DXMatrixRotationQuaternion(&qm, &q);
                    // Translation lerped (rows _41.._43).
                    float* now = &qm._41;
                    const float* o = &cur._41;
                    const float* b = &before._41;
                    for (int l = 0; l < 3; ++l)
                        now[l] = ((float)dwOffset * o[l] + inv * b[l]) / 10.0f;
                    pose.matRot[j] = qm;
                } else {
                    D3DXMATRIX& o = pose.matRot[j];
                    const float* a = &cur._11;
                    const float* b = &before._11;
                    float* op = &o._11;
                    for (int k = 0; k < 16; ++k)
                        op[k] = ((float)dwOffset * a[k] + inv * b[k]) / 10.0f;
                }
            }
        }
        for (size_t j = numBone; j < numFrames; ++j)
            D3DXMatrixIdentity(&pose.matRot[j]);

        if (crossfade && dwOffset >= 10)
            pb.blending = false;
    }

    // combined = matRot x parentCombined; root's matRot = object world transform.
    std::vector<int> stack;
    stack.push_back(base.rootId);
    pose.combined[base.rootId] = rootWorld;
    while (!stack.empty()) {
        const int id = stack.back();
        stack.pop_back();
        for (int child : base.frames[id].children) {
            D3DXMatrixMultiply(&pose.combined[child], &pose.matRot[child], &pose.combined[id]);
            stack.push_back(child);
        }
    }
}

}
