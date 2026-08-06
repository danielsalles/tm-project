#include "scene/FieldView.h"

#include "gl/GLRenderDevice.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace tmx {

namespace {

// MeshList.txt lines: "<index> <path>". Paths use backslashes.
void ParseMeshList(const std::string& txt,
                   std::unordered_map<int, std::string>& out) {
    const char* p = txt.c_str();
    while (*p) {
        while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;

        int index = 0;
        bool any = false;
        while (*p >= '0' && *p <= '9') {
            index = index * 10 + (*p - '0');
            any = true;
            ++p;
        }
        if (!any) {
            while (*p && *p != '\n')
                ++p;
            continue;
        }
        while (*p == ' ' || *p == '\t')
            ++p;

        char path[256];
        int n = 0;
        while (*p && *p != '\r' && *p != '\n' && n < 255)
            path[n++] = *p++;
        path[n] = '\0';
        while (n > 0 && (path[n - 1] == ' ' || path[n - 1] == '\t'))
            path[--n] = '\0';

        if (n > 0)
            out[index] = path;
    }
}

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

GLuint EnvTextureFn(int index, void* ctx) {
    return ((GLTextureManager*)ctx)->GetEnvTexture(index);
}

} // namespace

bool FieldView::Load(const char* mapName, GLTextureManager& textures,
                     const std::string& meshListTxt, const std::string& boneAniListTxt) {
    m_textures = &textures;
    m_boneAniListTxt = boneAniListTxt;
    bool any = false;

    char trnPath[64], datPath[64];
    snprintf(trnPath, sizeof trnPath, "env\\%s.trn", mapName);
    snprintf(datPath, sizeof datPath, "env\\%s.dat", mapName);

    std::vector<uint8_t> trnBuf;
    if (ReadAsset(trnPath, trnBuf)) {
        std::string err;
        m_hasTerrain = ParseTrn(trnBuf.data(), trnBuf.size(), m_terrain, &err);
        if (!m_hasTerrain)
            Log("FieldView: trn parse falhou (%s): %s", trnPath, err.c_str());
        else
            any = true;
    } else {
        Log("FieldView: %s nao encontrado", trnPath);
    }

    ParseMeshList(meshListTxt, m_meshFiles);

    std::vector<uint8_t> datBuf;
    if (ReadAsset(datPath, datBuf)) {
        ObjectFile file;
        std::string err;
        if (file.Load(datBuf.data(), datBuf.size(), &err)) {
            static const char* kKindName[] = {
                "GenericStatic", "Sea", "Float", "Butterfly", "Fish",
                "Leaf", "Tree", "Ship", "House", "TorchEffect",
            };
            for (int k = 1; k < 10; ++k) {
                if (k == (int)ObjectKind::Sea || k == (int)ObjectKind::Tree)
                    continue;   // collected into m_seaDescs / m_treeInsts
                if (file.skipped[k])
                    Log("FieldView: pulando %d objetos do tipo %s (D6-D8/fase 4)",
                        file.skipped[k], kKindName[k]);
            }

            // Object records are ground-local; the original places them at
            // groundOffset + local (TMObjectContainer.cpp:57-62, 313).
            const float offX = m_terrain.OffsetX();
            const float offY = m_terrain.OffsetY();
            for (const auto& r : file.records) {
                const ObjectKind kind = ClassifyObjectType(r.dwObjType);
                if (kind == ObjectKind::Sea) {
                    m_seaDescs.push_back({ r.nMaskIndex / 2, r.nTextureSetIndex / 2,
                                           offX + r.posX, r.fHeight, offY + r.posY });
                    continue;
                }
                if (kind == ObjectKind::Tree) {
                    // TMTree::InitLook mapping + look overrides
                    const uint32_t t = r.dwObjType;
                    TreeRenderer::Instance inst;
                    if (t >= 331 && t <= 342)
                        inst.boneAniIdx = (int)(t - 331) / 2 + 63;
                    else
                        inst.boneAniIdx = (int)(t - 351) / 2 + 71;
                    switch (t) {
                    case 342: inst.meshLook = 1; break;
                    case 362: inst.meshLook = 1; break;
                    case 354: case 361: case 377: case 375: case 373:
                        inst.skinLook = 1; break;
                    default: break;
                    }
                    inst.x = offX + r.posX;
                    inst.y = r.fHeight;
                    inst.z = offY + r.posY;
                    inst.angle = r.fAngle;
                    m_treeInsts.push_back(inst);
                    continue;
                }
                if (kind == ObjectKind::TorchEffect && r.dwObjType >= 501 && r.dwObjType <= 503) {
                    m_lampRecords.push_back(r);   // terrain tint applied after the loop
                    continue;
                }
                if (kind != ObjectKind::GenericStatic &&
                    kind != ObjectKind::House && kind != ObjectKind::Ship &&
                    kind != ObjectKind::Float)
                    continue;
                Object o;
                o.meshIndex = (int)r.dwObjType;
                o.x = offX + r.posX;
                o.y = r.fHeight;
                o.z = offY + r.posY;
                o.angle = r.fAngle;
                m_objects.push_back(o);
            }
            any = any || !m_objects.empty();
        } else {
            Log("FieldView: dat parse falhou (%s): %s", datPath, err.c_str());
        }
    }

    // Bounds: terrain extent + objects.
    if (m_hasTerrain) {
        m_bmin[0] = m_terrain.OffsetX();
        m_bmax[0] = m_terrain.OffsetX() + 63.0f * TerrainData::kWorldScale;
        m_bmin[2] = m_terrain.OffsetY();
        m_bmax[2] = m_terrain.OffsetY() + 63.0f * TerrainData::kWorldScale;
        m_bmin[1] = 127.0f;
        m_bmax[1] = -128.0f;
        for (int i = 0; i < 4096; ++i) {
            const float h = m_terrain.tiles[i].height * TerrainData::kHeightScale;
            if (h < m_bmin[1]) m_bmin[1] = h;
            if (h > m_bmax[1]) m_bmax[1] = h;
        }
    }
    for (const auto& o : m_objects) {
        if (o.x < m_bmin[0]) m_bmin[0] = o.x;
        if (o.x > m_bmax[0]) m_bmax[0] = o.x;
        if (o.y < m_bmin[1]) m_bmin[1] = o.y;
        if (o.y > m_bmax[1]) m_bmax[1] = o.y;
        if (o.z < m_bmin[2]) m_bmin[2] = o.z;
        if (o.z > m_bmax[2]) m_bmax[2] = o.z;
    }

    Log("FieldView: %s -> terreno=%s, %zu objetos estaticos",
        mapName, m_hasTerrain ? "ok" : "ausente", m_objects.size());

    // Lamp ground tint (TMObjectContainer.cpp dwObjType 501-503): the averaged
    // lamp/ground color is baked into the tile corner color at load time.
    // (The glow billboards themselves are phase 4 — only the terrain tint here.)
    if (m_hasTerrain) {
        static const uint32_t kLampCol[3][2] = {
            { 0x00FFAA00, 0x33331100 },
            { 0x00FFAA00, 0x33331100 },
            { 0x00AA00FF, 0x00110033 },
        };
        for (const auto& r : m_lampRecords) {
            const int n = (int)(r.dwObjType - 501);
            if (n < 0 || n > 2)
                continue;
            const float wx = m_terrain.OffsetX() + r.posX;
            const float wz = m_terrain.OffsetY() + r.posY;
            float g[4];
            TerrainGetColor(m_terrain, wx, wz, g);
            const uint32_t c = kLampCol[n][0];
            const uint32_t dwGA = ((uint32_t)(g[3] * 256.0f)) & 0xFF;   // original's *256 wrap
            const uint32_t dwGR = ((uint32_t)(g[0] * 256.0f)) & 0xFF;
            const uint32_t dwGG = ((uint32_t)(g[1] * 256.0f)) & 0xFF;
            const uint32_t dwGB = ((uint32_t)(g[2] * 256.0f)) & 0xFF;
            const uint32_t dwCA = (c & 0xFF000000) >> 24;
            const uint32_t dwCR = (c & 0x00FF0000) >> 16;
            const uint32_t dwCG = (c & 0x0000FF00) >> 8;
            const uint32_t dwCB = (c & 0x000000FF);
            const uint32_t dwCol =
                (((dwCB + dwGB) >> 1)) | (((dwCG + dwGG) >> 1) << 8) |
                (((dwCR + dwGR) >> 1) << 16) | (((dwCA + dwGA) >> 1) << 24);
            TerrainSetColor(m_terrain, wx, wz, dwCol);
        }
    }
    return any;
}

bool FieldView::InitGL(std::string* err) {
    if (!m_terrainRenderer.Init(err))
        return false;
    if (m_hasTerrain && !m_terrainRenderer.Build(m_terrain, err))
        return false;
    if (!m_seaShader.Build(kCommonGlsl, kSeaVert, kSeaFrag, err))
        return false;
    m_locSeaWorld = m_seaShader.UniformLoc("uWorld");
    m_locSeaTex0  = m_seaShader.UniformLoc("uTex0");
    m_locSeaTex1  = m_seaShader.UniformLoc("uTex1");
    m_seas.resize(m_seaDescs.size());
    for (size_t i = 0; i < m_seaDescs.size(); ++i) {
        if (!m_seas[i].Init(m_seaDescs[i].gridX, m_seaDescs[i].gridY,
                            m_seaDescs[i].x, m_seaDescs[i].h, m_seaDescs[i].z, err))
            return false;
    }
    if (!m_trees.Init(m_boneAniListTxt, *m_textures, err))
        return false;
    for (const auto& inst : m_treeInsts)
        m_trees.Add(inst);
    return true;
}

GLMesh* FieldView::GetMesh(int index, GLTextureManager& textures) {
    auto it = m_meshes.find(index);
    if (it != m_meshes.end())
        return &it->second;

    auto fit = m_meshFiles.find(index);
    if (fit == m_meshFiles.end())
        return nullptr;

    std::vector<uint8_t> bytes;
    if (!ReadAsset(fit->second.c_str(), bytes)) {
        Log("mesh missing: %s", fit->second.c_str());
        return nullptr;
    }

    MsaData data;
    std::string err;
    if (!ParseMsa(bytes.data(), bytes.size(), data, &err)) {
        Log("msa parse failed %s: %s", fit->second.c_str(), err.c_str());
        return nullptr;
    }

    GLMesh& mesh = m_meshes[index];
    if (!mesh.Upload(data)) {
        Log("msa upload failed (FVF %u): %s", data.fvf, fit->second.c_str());
        m_meshes.erase(index);
        return nullptr;
    }

    for (int i = 0; i < mesh.subsetCount; ++i) {
        char rel[300];
        snprintf(rel, sizeof rel, "mesh\\%s.wys", mesh.textureNames[i].c_str());
        int texIndex = textures.FindModelTexture(rel);
        mesh.subsets[i].textureIndex = (int)textures.GetModelTexture(texIndex);
        mesh.subsets[i].alphaFlag = textures.AlphaFlag(texIndex);
    }
    return &mesh;
}

void FieldView::Render(GLRenderDevice& device) {
    if (m_hasTerrain)
        m_terrainRenderer.Render(device, EnvTextureFn, m_textures);

    device.SetRenderStateBlock(1);
    for (const auto& o : m_objects) {
        GLMesh* mesh = GetMesh(o.meshIndex, *m_textures);
        if (!mesh)
            continue;

        // TMMesh::Render transform (TMMesh.cpp:159-178)
        D3DXMATRIX rot, scale, trans, world;
        D3DXMatrixRotationYawPitchRoll(&rot, o.angle, -D3DXToRadian(90), 0.0f);
        D3DXMatrixScaling(&scale, 1.0f, 1.0f, 1.0f);
        D3DXMatrixTranslation(&trans, o.x, o.y, o.z);
        D3DXMatrixMultiply(&world, &rot, &scale);
        D3DXMatrixMultiply(&world, &world, &trans);

        device.SetWorldMatrix(world);
        device.DrawMesh(*mesh);
    }

    // Seas last: additive-ish shimmer over the opaque scene (TMSea blend).
    for (auto& sea : m_seas)
        sea.Render(device, *m_textures, m_seaShader,
                   m_locSeaWorld, m_locSeaTex0, m_locSeaTex1);

    // Animated trees (skinned) — alpha-tested opaque, anywhere in the opaque pass.
    m_trees.Render(device, m_lastTimeMs);
}

void FieldView::FrameMove(float timeSec) {
    for (auto& sea : m_seas)
        sea.FrameMove(timeSec);
    m_lastTimeMs = timeSec * 1000.0f;
}

void FieldView::Destroy() {
    m_terrainRenderer.Destroy();
    m_seaShader.Destroy();
    m_trees.Destroy();
    for (auto& sea : m_seas)
        sea.Destroy();
    m_seas.clear();
    for (auto& [_, mesh] : m_meshes)
        mesh.Destroy();
    m_meshes.clear();
}

void FieldView::Bounds(float* minXYZ, float* maxXYZ) const {
    memcpy(minXYZ, m_bmin, sizeof m_bmin);
    memcpy(maxXYZ, m_bmax, sizeof m_bmax);
}

}
