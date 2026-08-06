#include "world/BoneAnimation.h"

#include "platform/Platform.h"

#include <cstdio>
#include <cstring>
#include <vector>

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

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

} // namespace

bool LoadBoneAni(const char* prefix, int aniFileIndex, BoneAniSet& out, std::string* err) {
    char path[160];
    snprintf(path, sizeof path, "%s.bon", prefix);
    std::vector<uint8_t> bon;
    if (!ReadAsset(path, bon)) {
        if (err) *err = std::string("boneani: missing ") + path;
        return false;
    }
    if (bon.size() < 8 || bon.size() % 8) {
        if (err) *err = "boneani: bad .bon size";
        return false;
    }

    const size_t pairs = bon.size() / 8;
    int maxId = 0;
    for (size_t i = 0; i < pairs; ++i) {
        const int id = (int)ReadU32(bon.data() + i * 8 + 4);
        if (id > maxId)
            maxId = id;
    }
    out.frames.resize((size_t)maxId + 1);
    for (size_t i = 0; i < pairs; ++i) {
        const uint32_t parent = ReadU32(bon.data() + i * 8);
        const int id = (int)ReadU32(bon.data() + i * 8 + 4);
        BoneAniSet::Frame& f = out.frames[id];
        f.id = id;
        f.parent = (parent == 0xFFFFFFFFu) ? -1 : (int)parent;
        if (id == 0)
            out.rootId = 0;
        if (f.parent >= 0 && (size_t)f.parent < out.frames.size())
            out.frames[f.parent].children.push_back(id);
    }

    snprintf(path, sizeof path, "%s%04d.ani", prefix, aniFileIndex);
    std::vector<uint8_t> ani;
    if (!ReadAsset(path, ani)) {
        if (err) *err = std::string("boneani: missing ") + path;
        return false;
    }
    if (ani.size() < 8) {
        if (err) *err = "boneani: bad .ani size";
        return false;
    }
    out.numTicks = (int)ReadU32(ani.data());       // "buffer"/numAniCut in the original
    out.numBones = (int)ReadU32(ani.data() + 4);   // "numAniFrame" in the original
    if (out.numTicks <= 0 || out.numBones <= 0 || out.numTicks > 4096 ||
        out.numBones > 64 ||
        ani.size() < 8 + (size_t)out.numTicks * out.numBones * 64) {
        if (err) *err = "boneani: truncated .ani";
        return false;
    }
    out.ticks.resize((size_t)out.numTicks * out.numBones);
    memcpy(out.ticks.data(), ani.data() + 8, out.ticks.size() * sizeof(D3DXMATRIX));

    out.matRot.resize(out.frames.size());
    out.combined.resize(out.frames.size());
    for (size_t i = 0; i < out.frames.size(); ++i)
        D3DXMatrixIdentity(&out.matRot[i]);
    return true;
}

void SampleBoneAni(BoneAniSet& set, float timeMs, const D3DXMATRIX& rootWorld) {
    if (set.numTicks > 0 && set.numBones > 0) {
        // TMSkinMesh::FrameMove with m_dwFPS=80: 4 sub-steps per tick.
        const uint32_t t = (uint32_t)(timeMs > 0.0f ? timeMs : 0.0f) / 80u;
        const uint32_t mod = t % (uint32_t)(4 * set.numTicks);
        const int tick = (int)(mod / 4u);
        const int sub = (int)(mod % 4u);

        for (int id = 0; id < (int)set.frames.size(); ++id) {
            if (id >= set.numBones) {
                D3DXMatrixIdentity(&set.matRot[id]);
                continue;
            }
            const D3DXMATRIX& cur = set.ticks[(size_t)tick * set.numBones + id];
            if (sub == 0) {
                set.matRot[id] = cur;
            } else {
                const D3DXMATRIX& nxt = set.ticks[(size_t)((tick + 1) % set.numTicks) * set.numBones + id];
                const float w = sub / 4.0f;
                const float* a = &cur._11;
                const float* b = &nxt._11;
                float* o = &set.matRot[id]._11;
                for (int k = 0; k < 16; ++k)
                    o[k] = a[k] + (b[k] - a[k]) * w;
            }
        }
    } else {
        for (size_t id = 0; id < set.frames.size(); ++id)
            D3DXMatrixIdentity(&set.matRot[id]);
    }

    // combined = matRot x parentCombined, root's matRot = object world transform
    // (CFrame::UpdateFrames; TMSkinMesh::Render overwrites the root's matRot with
    // the object world transform every frame, so the .ani row of bone 0 is unused).
    std::vector<int> stack;
    stack.push_back(set.rootId);
    set.combined[set.rootId] = rootWorld;

    while (!stack.empty()) {
        const int id = stack.back();
        stack.pop_back();
        for (int child : set.frames[id].children) {
            if (child < (int)set.matRot.size() && id < (int)set.combined.size()) {
                D3DXMatrixMultiply(&set.combined[child], &set.matRot[child], &set.combined[id]);
                stack.push_back(child);
            }
        }
    }
}

}
