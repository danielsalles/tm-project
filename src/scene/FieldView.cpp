#include "scene/FieldView.h"

#include "gl/GLRenderDevice.h"
#include "platform/Platform.h"
#include "world/LampFx.h"

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
                if (kind == ObjectKind::TorchEffect) {
                    m_lampRecords.push_back(r);   // tint 501-503 + glow billboards (fase 4)
                    continue;
                }
                if (kind == ObjectKind::Butterfly || kind == ObjectKind::Fish ||
                    kind == ObjectKind::Leaf) {
                    CritterSpawn(r.dwObjType, offX + r.posX, r.fHeight, offY + r.posY,
                                 m_critters);
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

    // Lamp glow billboards (TMObjectContainer.cpp:405-530). Our .dat records are
    // 28B and carry no fScaleH/fScaleV (the original leaks them from the next
    // record); use the billboard default scale 0.5 for both axes.
    for (const auto& r : m_lampRecords) {
        BuildLampGlow(r.dwObjType,
                      m_terrain.OffsetX() + r.posX, r.fHeight,
                      m_terrain.OffsetY() + r.posY, r.fAngle, 0.5f, m_lampFx);
    }
    return any;
}

bool FieldView::InitGL(std::string* err) {
    if (!m_terrainRenderer.Init(err))
        return false;
    if (!m_fx.Init(err))
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

    // Characters (phase 3): same skin pipeline state as trees.
    if (m_charReady && !m_chars.empty()) {
        m_charPipe.Begin(device, 170.0f);
        const uint32_t nowMs = (uint32_t)m_lastTimeMs;
        for (auto& c : m_chars)
            c->Render(m_charPipe, device, nowMs);
    }

    // Critters (phase 4): skinned ambient objects; leaves use the fade path.
    if (m_charReady && m_crittersBuilt) {
        m_charPipe.Begin(device, 170.0f);
        const uint32_t nowMs = (uint32_t)m_lastTimeMs;
        for (size_t i = 0; i < m_critters.size(); ++i) {
            if (i >= m_critterMeshes.size() || !m_critterMeshes[i])
                continue;
            const Critter& c = m_critters[i];
            D3DXMATRIX rot, scale, trans, world;
            D3DXMatrixRotationYawPitchRoll(&rot, c.curAngle - D3DXToRadian(90),
                                           -D3DXToRadian(90), 0.0f);
            D3DXMatrixScaling(&scale, c.scale, c.scale, c.scale);
            D3DXMatrixTranslation(&trans, c.curX, c.curY, c.curZ);
            D3DXMatrixMultiply(&world, &rot, &scale);
            D3DXMatrixMultiply(&world, &world, &trans);
            m_charPipe.SetFadeBlend(c.kind == 0);   // leaves fade by distance
            m_critterMeshes[i]->Render(m_charPipe, device, world, nowMs);
            m_charPipe.SetFadeBlend(false);
        }
    }

    // Effects last: no depth writes, camera-facing (TMEffectBillBoard states).
    for (auto& b : m_lampFx) {
        if (b.dead)
            continue;
        FxQuad q;
        q.world = b.world;
        q.u0 = 0.02f; q.v0 = 0.02f; q.u1 = 0.98f; q.v1 = 0.98f;
        q.bgra = b.curBgra;
        q.textureIndex = BillboardTexture(b);
        q.blendMode = 1;   // every lamp billboard is EF_BRIGHT in the original
        m_fx.Emit(q);
    }
    if (m_weatherFx == 2)
        WeatherEmit(m_rain, m_fxFrame.focusX, m_fxFrame.focusZ,
                    m_fxFrame.right[0], m_fxFrame.right[1], m_fxFrame.right[2],
                    m_fxFrame.up[0], m_fxFrame.up[1], m_fxFrame.up[2],
                    m_fxFrame.focusH, m_fx);
    else if (m_weatherFx == 3) {
        WeatherEmit(m_snow1, m_fxFrame.focusX, m_fxFrame.focusZ,
                    m_fxFrame.right[0], m_fxFrame.right[1], m_fxFrame.right[2],
                    m_fxFrame.up[0], m_fxFrame.up[1], m_fxFrame.up[2],
                    m_fxFrame.focusH, m_fx);
        WeatherEmit(m_snow2, m_fxFrame.focusX, m_fxFrame.focusZ,
                    m_fxFrame.right[0], m_fxFrame.right[1], m_fxFrame.right[2],
                    m_fxFrame.up[0], m_fxFrame.up[1], m_fxFrame.up[2],
                    m_fxFrame.focusH, m_fx);
    }
    m_fx.Flush(device, *m_textures, m_fxFrame.screenW, m_fxFrame.screenH);
}

void FieldView::FrameMove(float timeSec) {
    for (auto& sea : m_seas)
        sea.FrameMove(timeSec);
    m_lastTimeMs = timeSec * 1000.0f;
    for (auto& c : m_chars)
        c->FrameMove((uint32_t)m_lastTimeMs);
    const uint32_t nowMs = (uint32_t)m_lastTimeMs;
    for (size_t i = 0; i < m_lampFx.size(); ++i)
        BillboardFrameMove(m_lampFx[i], nowMs, m_fxFrame.yawH, m_fxFrame.pitchV, (uint32_t)i);
    for (size_t i = 0; i < m_critters.size(); ++i) {
        CritterFrameMove(m_critters[i], nowMs, m_fxFrame.focusX, m_fxFrame.focusZ);
        if (i < m_critterMeshes.size() && m_critterMeshes[i])
            m_critterMeshes[i]->alphaMul = m_critters[i].alpha;
    }
    if (m_weatherFx == 2)
        WeatherFrameMove(m_rain, nowMs, m_fxFrame.focusX, m_fxFrame.focusH, m_fxFrame.focusZ);
    else if (m_weatherFx == 3) {
        WeatherFrameMove(m_snow1, nowMs, m_fxFrame.focusX, m_fxFrame.focusH, m_fxFrame.focusZ);
        WeatherFrameMove(m_snow2, nowMs, m_fxFrame.focusX, m_fxFrame.focusH, m_fxFrame.focusZ);
    }
}

void FieldView::SetWeatherFx(int mode) {
    if (mode == m_weatherFx)
        return;
    m_weatherFx = mode;
    if (mode == 2 && m_rain.drops.empty())
        WeatherInit(m_rain, 0, 1.0f);
    if (mode == 3) {
        if (m_snow1.drops.empty())
            WeatherInit(m_snow1, 1, 1.0f);
        if (m_snow2.drops.empty())
            WeatherInit(m_snow2, 1, 2.0f);
    }
}

bool FieldView::InitCharacters(const std::string& boneAniListTxt,
                               const std::string& aniSoundTxt,
                               GLTextureManager& textures, std::string* err) {
    if (!ParseAniSound(aniSoundTxt, m_aniSound, err))
        return false;
    if (!m_charCache.Init(boneAniListTxt, err))
        return false;
    if (!m_charPipe.Init(err))
        return false;
    m_charReady = true;

    // Critters: one CharacterMesh each (single-part skinned types 61/69/24/70).
    for (auto& c : m_critters) {
        auto mesh = std::make_unique<CharacterMesh>();
        int16_t meshLook[8] = {};
        int16_t skinLook[8] = {};
        meshLook[0] = c.meshLook0;
        skinLook[0] = c.skinLook0;
        std::string cerr;
        if (mesh->Init(m_charCache, textures, c.skinMeshType, meshLook, skinLook, &cerr)) {
            mesh->pb.fps = c.fps;
            m_critterMeshes.push_back(std::move(mesh));
        } else {
            Log("critter type %d: %s", c.skinMeshType, cerr.c_str());
            m_critterMeshes.push_back(nullptr);
        }
    }
    m_crittersBuilt = true;
    return true;
}

Character* FieldView::Spawn(const CharDesc& d, float x, float z, std::string* err) {
    if (!m_charReady || !m_hasTerrain) {
        if (err) *err = "spawn: characters or terrain not ready";
        return nullptr;
    }
    auto c = std::make_unique<Character>();
    if (!c->Init(m_charCache, *m_textures, m_aniSound, d,
                 &m_terrain.mask[0][0], TerrainData::kMask, TerrainData::kMask,
                 m_terrain.OffsetX(), m_terrain.OffsetY(), err))
        return nullptr;
    c->SetPosition(x, z);
    c->SetMotion(CharMotion::Stand01, (uint32_t)m_lastTimeMs);
    Log("spawn type=%d @(%.1f,%.1f): %d parts", d.boneAniIndex, x, z,
        c->Mesh().PartCount());
    Character* ptr = c.get();
    m_chars.push_back(std::move(c));
    return ptr;
}

void FieldView::RemoveCharacter(Character* c) {
    for (size_t i = 0; i < m_chars.size(); ++i) {
        if (m_chars[i].get() == c) {
            m_chars[i]->Destroy();
            m_chars.erase(m_chars.begin() + i);
            return;
        }
    }
}

void FieldView::Destroy() {
    for (auto& c : m_chars)
        c->Destroy();
    m_chars.clear();
    m_charPipe.Destroy();
    m_fx.Destroy();
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
