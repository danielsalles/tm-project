#include "scene/SelectServerView.h"

#include "gl/GLRenderDevice.h"
#include "platform/Platform.h"

#include <cstdio>
#include <cstring>

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
        if (!any) { // not a data line: skip to next line
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
        // trim trailing spaces
        while (n > 0 && (path[n - 1] == ' ' || path[n - 1] == '\t'))
            path[--n] = '\0';

        if (n > 0)
            out[index] = path;
    }
}

} // namespace

bool SelectServerView::Load(const std::string& meshListTxt,
                            const uint8_t* objectDat, size_t datSize,
                            GLTextureManager& textures) {
    m_textures = &textures;

    ParseMeshList(meshListTxt, m_meshFiles);
    if (m_meshFiles.empty()) {
        Log("SelectServerView: MeshList.txt vazio");
        return false;
    }

    ObjectFile file;
    std::string err;
    if (!file.Load(objectDat, datSize, &err)) {
        Log("SelectServerView: dat parse falhou: %s", err.c_str());
        return false;
    }

    static const char* kKindName[] = {
        "GenericStatic", "Sea", "Float", "Butterfly", "Fish",
        "Leaf", "Tree", "Ship", "House", "TorchEffect",
    };
    for (int k = 1; k < 10; ++k) {
        if (file.skipped[k])
            Log("SelectServerView: pulando %d objetos do tipo %s (fase 2+)",
                file.skipped[k], kKindName[k]);
    }

    m_objects.clear();
    bool first = true;
    for (const auto& r : file.records) {
        if (ClassifyObjectType(r.dwObjType) != ObjectKind::GenericStatic)
            continue;

        Object o;
        o.meshIndex = (int)r.dwObjType;
        o.x = r.posX;
        o.y = r.fHeight;
        o.z = r.posY;
        o.angle = r.fAngle;
        m_objects.push_back(o);

        if (first) {
            m_bmin[0] = m_bmax[0] = o.x;
            m_bmin[1] = m_bmax[1] = o.y;
            m_bmin[2] = m_bmax[2] = o.z;
            first = false;
        } else {
            if (o.x < m_bmin[0]) m_bmin[0] = o.x;
            if (o.x > m_bmax[0]) m_bmax[0] = o.x;
            if (o.y < m_bmin[1]) m_bmin[1] = o.y;
            if (o.y > m_bmax[1]) m_bmax[1] = o.y;
            if (o.z < m_bmin[2]) m_bmin[2] = o.z;
            if (o.z > m_bmax[2]) m_bmax[2] = o.z;
        }
    }

    Log("SelectServerView: %d objetos estaticos, %zu meshes na lista",
        (int)m_objects.size(), m_meshFiles.size());
    return !m_objects.empty();
}

GLMesh* SelectServerView::GetMesh(int index, GLTextureManager& textures) {
    auto it = m_meshes.find(index);
    if (it != m_meshes.end())
        return &it->second;

    auto fit = m_meshFiles.find(index);
    if (fit == m_meshFiles.end())
        return nullptr;

    FILE* f = OpenAsset(fit->second.c_str(), "rb");
    if (!f) {
        Log("mesh missing: %s", fit->second.c_str());
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes((size_t)sz);
    bool readOk = fread(bytes.data(), 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    if (!readOk)
        return nullptr;

    MsaData data;
    std::string err;
    if (!ParseMsa(bytes.data(), bytes.size(), data, &err)) {
        Log("msa parse failed %s: %s", fit->second.c_str(), err.c_str());
        return nullptr;
    }

    GLMesh& mesh = m_meshes[index];
    if (!mesh.Upload(data)) {
        Log("msa upload failed (FVF %u nao suportado na fase 1): %s",
            data.fvf, fit->second.c_str());
        m_meshes.erase(index);
        return nullptr;
    }

    // Resolve subset textures: .msa subset name -> "mesh\<name>.wys" -> list index -> GL texture
    for (int i = 0; i < mesh.subsetCount; ++i) {
        char rel[300];
        snprintf(rel, sizeof rel, "mesh\\%s.wys", mesh.textureNames[i].c_str());
        int texIndex = textures.FindModelTexture(rel);
        mesh.subsets[i].textureIndex = (int)textures.GetModelTexture(texIndex);
    }
    return &mesh;
}

void SelectServerView::Render(GLRenderDevice& device) {
    device.SetRenderStateBlock(1);

    for (const auto& o : m_objects) {
        GLMesh* mesh = GetMesh(o.meshIndex, *m_textures);
        if (!mesh)
            continue;

        // TMMesh::Render transform (TMMesh.cpp:159-178):
        //   world = YPR(angle, -90deg, 0) * Scale(1) * Translation(x, y, z)
        // (mesh scale defaults to 1; g_pDevice->m_matWorld is identity in this path)
        D3DXMATRIX rot, scale, trans, world;
        D3DXMatrixRotationYawPitchRoll(&rot, o.angle, -D3DXToRadian(90), 0.0f);
        D3DXMatrixScaling(&scale, 1.0f, 1.0f, 1.0f);
        D3DXMatrixTranslation(&trans, o.x, o.y, o.z);
        D3DXMatrixMultiply(&world, &rot, &scale);
        D3DXMatrixMultiply(&world, &world, &trans);

        device.SetWorldMatrix(world);
        device.DrawMesh(*mesh);
    }
}

void SelectServerView::Bounds(float* minXYZ, float* maxXYZ) const {
    memcpy(minXYZ, m_bmin, sizeof m_bmin);
    memcpy(maxXYZ, m_bmax, sizeof m_bmax);
}

}
