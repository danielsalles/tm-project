#include "world/TreeRenderer.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

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

} // namespace

bool TreeRenderer::Init(const std::string& boneAniListTxt, GLTextureManager& textures,
                        std::string* err) {
    m_textures = &textures;

    if (!m_shader.Build(kCommonGlsl, kSkinVert, kSkinFrag, err))
        return false;
    m_locNumInfluence = m_shader.UniformLoc("uNumInfluence");
    m_locTex0         = m_shader.UniformLoc("uTex0");
    m_locAlphaRef     = m_shader.UniformLoc("uAlphaRef");
    m_locAlphaTest    = m_shader.UniformLoc("uAlphaTest");

    GLuint blockIdx = glGetUniformBlockIndex(m_shader.Program(), "BonePalette");
    if (blockIdx == GL_INVALID_INDEX) {
        *err = "skin shader: no BonePalette block";
        return false;
    }
    glUniformBlockBinding(m_shader.Program(), blockIdx, 1);

    glGenBuffers(1, &m_uboBones);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboBones);
    glBufferData(GL_UNIFORM_BUFFER, 40 * 64, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_uboBones);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // BoneAni4.txt: "<idx> <numAniTypes> <numParts> <prefix>"
    const char* p = boneAniListTxt.c_str();
    while (*p) {
        while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        int idx = 0, aniTypes = 0, parts = 0;
        bool any = false;
        while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); any = true; ++p; }
        if (!any) {
            while (*p && *p != '\n')
                ++p;
            continue;
        }
        while (*p == ' ' || *p == '\t') ++p;
        while (*p >= '0' && *p <= '9') { aniTypes = aniTypes * 10 + (*p - '0'); ++p; }
        while (*p == ' ' || *p == '\t') ++p;
        while (*p >= '0' && *p <= '9') { parts = parts * 10 + (*p - '0'); ++p; }
        while (*p == ' ' || *p == '\t') ++p;
        char prefix[128];
        int n = 0;
        while (*p && *p != '\r' && *p != '\n' && *p != ' ' && *p != '\t' && n < 127)
            prefix[n++] = *p++;
        prefix[n] = '\0';
        if (n > 0)
            m_info[idx] = { parts, prefix };
    }
    return true;
}

void TreeRenderer::Add(const Instance& inst) {
    m_instances.push_back(inst);
}

bool TreeRenderer::LoadSet(const Instance& inst, std::string* err) {
    const uint32_t key = (uint32_t)inst.boneAniIdx |
                         ((uint32_t)inst.meshLook << 8) | ((uint32_t)inst.skinLook << 16);
    auto it = m_sets.find(key);
    if (it != m_sets.end())
        return it->second.loaded;

    LoadedSet& set = m_sets[key];
    set.loaded = false;

    auto infoIt = m_info.find(inst.boneAniIdx);
    if (infoIt == m_info.end()) {
        if (err) *err = "tree: boneAni index not in BoneAni4.txt";
        return false;
    }
    const std::string& prefix = infoIt->second.prefix;
    const int numParts = infoIt->second.parts;

    // Bones shared per boneAniIdx regardless of look.
    if (!m_boneCache.count(inst.boneAniIdx)) {
        BoneAniSet bones;
        // Trees' only animation: ValidIndex 100 -> file suffix 0101.
        if (!LoadBoneAni(prefix.c_str(), 101, bones, err))
            return false;
        m_boneCache.emplace(inst.boneAniIdx, std::move(bones));
    }

    const int meshLook = inst.meshLook;
    const int skinLook = inst.skinLook;
    std::vector<GLSkinMesh> parts;
    std::vector<GLuint> textures;
    for (int part = 0; part < numParts; ++part) {
        // TMSkinMesh name rules (TMSkinMesh.cpp:167-193):
        //   mesh = <prefix><part+1><meshLook+1>.msh
        //   tex  = <prefix><part+1><skinLook + meshLook + 1>.wyt (resolved .wys too)
        char meshPath[160], texRel[160];
        snprintf(meshPath, sizeof meshPath, "%s%02d%02d.msh", prefix.c_str(),
                 part + 1, meshLook + 1);
        snprintf(texRel, sizeof texRel, "%s%02d%02d", prefix.c_str(),
                 part + 1, skinLook + meshLook + 1);

        std::vector<uint8_t> bytes;
        if (!ReadAsset(meshPath, bytes)) {
            if (err) *err = std::string("tree: missing ") + meshPath;
            return false;
        }
        MshData data;
        if (!ParseMsh(bytes.data(), bytes.size(), data, err))
            return false;

        GLSkinMesh mesh;
        if (!mesh.Upload(data)) {
            if (err) *err = "tree: upload failed";
            return false;
        }
        parts.push_back(mesh);

        // The original queries ".wyt" but the list stores ".wys" — it copies the
        // list extension over the query before comparing; try both.
        char withExt[200];
        snprintf(withExt, sizeof withExt, "%s.wys", texRel);
        int texIndex = m_textures->FindModelTexture(withExt);
        if (texIndex < 0) {
            snprintf(withExt, sizeof withExt, "%s.wyt", texRel);
            texIndex = m_textures->FindModelTexture(withExt);
        }
        textures.push_back(texIndex >= 0 ? m_textures->GetModelTexture(texIndex) : 0);
    }

    set.parts = std::move(parts);
    set.textures = std::move(textures);
    set.loaded = true;
    return true;
}

void TreeRenderer::Render(GLRenderDevice& device, float timeMs) {
    if (m_instances.empty())
        return;

    GLStateCache& st = device.State();
    // TMTree::Render: alpha test 0xAA, depth on+write, cull back, no blend.
    st.depthTest = true;
    st.depthWrite = true;
    st.depthFunc = GL_LEQUAL;
    st.blend = false;
    st.cull = true;
    st.cullFaceMode = GL_BACK;
    st.alphaTest = true;
    st.alphaRef = 170.0f;   // 0xAA
    st.sampler[0] = GLSamplers::LinearMip();

    m_shader.Bind();
    glUniform1i(m_locTex0, 0);
    glUniform1f(m_locAlphaRef, st.alphaRef);
    glUniform1i(m_locAlphaTest, 1);

    for (const Instance& inst : m_instances) {
        std::string err;
        if (!LoadSet(inst, &err)) {
            static int s_warned = 0;
            if (s_warned++ < 5)
                Log("tree load failed (%d): %s", inst.boneAniIdx, err.c_str());
            continue;
        }
        const uint32_t key = (uint32_t)inst.boneAniIdx |
                             ((uint32_t)inst.meshLook << 8) | ((uint32_t)inst.skinLook << 16);
        LoadedSet& set = m_sets[key];
        BoneAniSet& bones = m_boneCache[inst.boneAniIdx];

        // Object transform (TMSkinMesh::Render else-branch, boneAni 63-87):
        // root = YPR(angle-90, -90, 0) * Scale(1) * Translation(x, y, z)
        D3DXMATRIX rot, scale, trans, world;
        D3DXMatrixRotationYawPitchRoll(&rot, inst.angle - D3DXToRadian(90),
                                       -D3DXToRadian(90), 0.0f);
        D3DXMatrixScaling(&scale, 1.0f, 1.0f, 1.0f);
        D3DXMatrixTranslation(&trans, inst.x, inst.y, inst.z);
        D3DXMatrixMultiply(&world, &rot, &scale);
        D3DXMatrixMultiply(&world, &world, &trans);

        SampleBoneAni(bones, timeMs, world);

        for (size_t part = 0; part < set.parts.size(); ++part) {
            GLSkinMesh& m = set.parts[part];
            // palette[i] = bindInv[i] x combined[frameId[i]]  (D3D row order)
            D3DXMATRIX palette[40];
            for (uint32_t i = 0; i < m.numPalette; ++i) {
                const uint32_t fid = m.boneFrameId[i];
                const D3DXMATRIX& comb = fid < bones.combined.size()
                    ? bones.combined[fid] : world;
                D3DXMatrixMultiply(&palette[i], &m.boneBindInv[i], &comb);
            }
            glBindBuffer(GL_UNIFORM_BUFFER, m_uboBones);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, m.numPalette * 64, palette);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            glUniform1i(m_locNumInfluence, (int)m.numInfluence);
            st.texture[0] = set.textures[part];
            st.Apply();

            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
        }
    }
}

void TreeRenderer::Destroy() {
    m_shader.Destroy();
    if (m_uboBones) {
        glDeleteBuffers(1, &m_uboBones);
        m_uboBones = 0;
    }
    for (auto& [_, set] : m_sets) {
        for (auto& part : set.parts)
            part.Destroy();
    }
    m_sets.clear();
    m_instances.clear();
}

}
