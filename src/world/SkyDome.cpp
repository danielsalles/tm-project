#include "world/SkyDome.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLStateCache.h"
#include "gl/GLTexture.h"
#include "platform/Platform.h"

#include "shaders_embedded.h"

#include <cstring>
#include <vector>

namespace tmx {

namespace {

// TMSky ctor tables (TMSky.cpp:63-78): clear color per weather (R,G,B 0..255).
const uint32_t kClearRGB[4][3] = {
    { 28, 120, 189 },
    { 51,  54,  42 },
    { 98,  47,   4 },
    { 19,  46,  51 },
};

// TMSky::m_LightVal (TMSky.cpp:13-19) — light 0 (sun) color per weather.
const float kLightVal[4][3] = {
    { 0.70f, 0.70f, 0.70f },
    { 0.30f, 0.30f, 0.30f },
    { 0.50f, 0.40f, 0.20f },
    { 0.26f, 0.34f, 0.34f },
};

// TMSky::FogList (TMSky.cpp:21-38), rows 0-3 = day weathers.
const float kFogList[4][2] = {
    { 14.0f, 34.0f },
    { 25.0f, 34.0f },
    { 55.0f, 70.0f },
    { 25.0f, 34.0f },
};

} // namespace

bool SkyDome::Init(const std::string& meshListTxt, GLTextureManager& textures,
                   std::string* err) {
    m_textures = &textures;

    if (!m_shader.Build(kCommonGlsl, kSkyVert, kSkyFrag, err))
        return false;
    m_locWorld = m_shader.UniformLoc("uWorld");
    m_locTex0  = m_shader.UniformLoc("uTex0");

    // MeshList.txt line 1 = mesh\sky001.msa (TMSky uses GetCommonMesh(1, ...)).
    std::string domePath;
    {
        const char* p = meshListTxt.c_str();
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
            if (index == 1 && n > 0) {
                domePath = path;
                break;
            }
        }
    }
    if (domePath.empty()) {
        *err = "sky: MeshList.txt has no index 1";
        return false;
    }

    FILE* f = OpenAsset(domePath.c_str(), "rb");
    if (!f) {
        *err = "sky: " + domePath + " missing";
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes((size_t)sz);
    const bool readOk = fread(bytes.data(), 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    if (!readOk) {
        *err = "sky: read failed";
        return false;
    }

    MsaData data;
    if (!ParseMsa(bytes.data(), bytes.size(), data, err))
        return false;
    if (!m_dome.Upload(data)) {
        *err = "sky: dome upload failed (unsupported FVF)";
        return false;
    }
    m_hasDome = true;
    SetWeather(0);
    Log("sky: dome '%s' subsets=%d fvf=%u verts=%u", domePath.c_str(),
        m_dome.subsetCount, data.fvf, data.NumVerts());
    return true;
}

void SkyDome::Destroy() {
    m_shader.Destroy();
    m_dome.Destroy();
    m_hasDome = false;
}

void SkyDome::SetWeather(int state) {
    m_weather = state & 3;   // day states only in phase 2 (night = +10, later)
    for (int i = 0; i < 3; ++i)
        m_clear[i] = kClearRGB[m_weather][i] / 255.0f;
}

void SkyDome::ApplyWeather(GLRenderDevice& device) const {
    // Fog: D3D formula adds the camera sight length (RenderDevice.cpp:1726-1730);
    // free-fly viewer has no fixed sight — use the gameplay default (~14).
    const float sightLen = 14.0f;
    const float start = kFogList[m_weather][0] + sightLen - 8.0f;
    const float end   = kFogList[m_weather][1] + sightLen - 15.0f;
    device.SetFog(m_clear[0], m_clear[1], m_clear[2], start, end);

    // Light colors: light0 = m_colorLight (weather), light1 = m_colorBackLight
    // (constant 0.69 gray — RenderDevice.cpp:130-135, 1632-1635).
    D3DXVECTOR3 d0(-10.0f, 10.0f, -6.0f), d1(10.0f, -14.0f, 6.0f);
    D3DXVec3Normalize(&d0, &d0);
    D3DXVec3Normalize(&d1, &d1);
    device.SetDirectionalLight(0, d0, kLightVal[m_weather][0],
                               kLightVal[m_weather][1], kLightVal[m_weather][2]);
    device.SetDirectionalLight(1, d1, 0.69f, 0.69f, 0.69f);
}

void SkyDome::Render(GLRenderDevice& device, float camX, float camZ) {
    if (!m_hasDome)
        return;

    GLStateCache& st = device.State();
    // Sky: no depth write, no cull (dome seen from inside), no blend (TMSky::Render
    // sets DESTBLEND but stage0 is effectively SELECTARG1 with full-alpha texture).
    st.depthTest = true;
    st.depthWrite = false;
    st.depthFunc = GL_LEQUAL;
    st.blend = false;
    st.cull = false;
    st.alphaTest = false;
    st.sampler[0] = GLSamplers::LinearMip();

    // TMSky::Render transform: YPR(180deg, -90deg, 0) * Scale(0.5, 0.25, 0.5) *
    // Translation(cam.x, -4, cam.z) — the dome follows the camera (m_fHeight=-5+1).
    D3DXMATRIX rot, scale, trans, world;
    D3DXMatrixRotationYawPitchRoll(&rot, D3DXToRadian(180), -D3DXToRadian(90), 0.0f);
    D3DXMatrixScaling(&scale, 0.5f, 0.25f, 0.5f);
    D3DXMatrixTranslation(&trans, camX, -4.0f, camZ);
    D3DXMatrixMultiply(&world, &rot, &scale);
    D3DXMatrixMultiply(&world, &world, &trans);

    m_shader.Bind();
    m_shader.SetMat4(m_locWorld, &world._11);
    glUniform1i(m_locTex0, 0);

    // Weather texture override. The leaked source indexes the model list numerically
    // (67+state) which only works with that build's list layout; resolve by NAME —
    // this build's MeshTextureList.txt maps weather 0-3 to sky01..sky04.wys.
    char skyRel[64];
    snprintf(skyRel, sizeof skyRel, "mesh\\sky0%d.wys", 1 + m_weather);
    const int texIndex = m_textures->FindModelTexture(skyRel);
    st.texture[0] = m_textures->GetModelTexture(texIndex);
    st.Apply();

    glBindVertexArray(m_dome.vao);
    for (int i = 0; i < m_dome.subsetCount; ++i) {
        glDrawElements(GL_TRIANGLES, (GLsizei)m_dome.subsets[i].indexCount,
                       GL_UNSIGNED_SHORT,
                       (void*)(uintptr_t)(m_dome.subsets[i].indexStart * 2));
    }
    glBindVertexArray(0);
}

}
