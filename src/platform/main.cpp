#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "platform/Platform.h"
#include "gl/GLRenderDevice.h"
#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "scene/FieldView.h"
#include "world/SkyDome.h"
#include "world/SunFlare.h"
#include "world/TerrainData.h"
#include "world/SkillFx.h"
#include "world/SwingTrail.h"

#include <ctime>

static void WriteBootReport(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f)
        return;

    auto ext = [](const char* name) { return SDL_GL_ExtensionSupported(name) ? "yes" : "no"; };

    fprintf(f, "GL_VERSION:  %s\n", (const char*)glGetString(GL_VERSION));
    fprintf(f, "GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));
    fprintf(f, "GL_VENDOR:   %s\n", (const char*)glGetString(GL_VENDOR));
    fprintf(f, "GLSL:        %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    fprintf(f, "EXT_texture_compression_s3tc: %s\n", ext("GL_EXT_texture_compression_s3tc"));
    fprintf(f, "EXT_texture_filter_anisotropic: %s\n", ext("GL_EXT_texture_filter_anisotropic"));

    GLint maxTexSize = 0, maxUniforms = 0, maxUBOSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &maxUniforms);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
    fprintf(f, "MAX_TEXTURE_SIZE: %d\n", maxTexSize);
    fprintf(f, "MAX_VERTEX_UNIFORM_COMPONENTS: %d\n", maxUniforms);
    fprintf(f, "MAX_UNIFORM_BLOCK_SIZE: %d\n", maxUBOSize);

    fclose(f);
}

static bool ReadWholeFile(const char* relPath, std::vector<uint8_t>& out) {
    FILE* f = tmx::OpenAsset(relPath, "rb");
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

static bool ReadWholeFileText(const char* relPath, std::string& out) {
    std::vector<uint8_t> bytes;
    if (!ReadWholeFile(relPath, bytes))
        return false;
    out.assign((const char*)bytes.data(), bytes.size());
    return true;
}

static void SaveScreenshot(SDL_Window* window, const char* path) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    std::vector<uint8_t> pixels((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    stbi_flip_vertically_on_write(1); // GL reads bottom-up (10 §10.6)
    if (stbi_write_bmp(path, w, h, 3, pixels.data()))
        tmx::Log("screenshot: %s", path);
    else
        tmx::Log("screenshot falhou: %s", path);
}

int main(int argc, char** argv) {
    // --shot <path> [frames N]: render N frames, save screenshot, exit (automation/CI)
    // --map FieldXXYY: pick the ground/scene (default Field2723 = select-server)
    // --nochar: disable the playable character (phase 3)
    // --spawn type,x,z: extra character/monster (repeatable)
    // --follow/--nofollow: follow camera (default on when a character exists)
    const char* shotPath = nullptr;
    int shotFrames = 30;
    char mapName[32] = "Field2723";
    int weather = -1;   // -1 = like the original select-server: day-of-month % 4
    float startPitch = -0.7f, startYaw = 0.0f;
    float startCam[3] = { 0, 0, 0 };
    bool startCamSet = false;
    bool spawnChar = true;
    int followCam = -1;   // -1 = auto (on when char exists)
    struct SpawnReq { int type; float x, z; };
    std::vector<SpawnReq> spawnReqs;
    float walkTo[2] = { 0, 0 };
    bool walkToSet = false;
    float hoverPx = -1, hoverPy = -1;
    int startWeapon = 0;
    // Phase 5 combat VFX (doc 19 §13): --skill <name>,x,z[,level]. A small set
    // of demo effects is wired now (glow/burst/bash/meteor-lite); richer skills
    // arrive in later steps. Repeating the flag stacks multiple effects.
    struct SkillReq { char name[24]; float x, z; int level; };
    std::vector<SkillReq> skillReqs;
    bool swingLoop = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc)
            shotPath = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            shotFrames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--map") && i + 1 < argc) {
            snprintf(mapName, sizeof mapName, "%s", argv[++i]);
        }
        else if (!strcmp(argv[i], "--weather") && i + 1 < argc)
            weather = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pitch") && i + 1 < argc)
            startPitch = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--yaw") && i + 1 < argc)
            startYaw = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--cam") && i + 3 < argc) {
            startCam[0] = (float)atof(argv[++i]);
            startCam[1] = (float)atof(argv[++i]);
            startCam[2] = (float)atof(argv[++i]);
            startCamSet = true;
        }
        else if (!strcmp(argv[i], "--nochar"))
            spawnChar = false;
        else if (!strcmp(argv[i], "--follow"))
            followCam = 1;
        else if (!strcmp(argv[i], "--nofollow"))
            followCam = 0;
        else if (!strcmp(argv[i], "--spawn") && i + 1 < argc) {
            SpawnReq r{};
            if (sscanf(argv[++i], "%d,%f,%f", &r.type, &r.x, &r.z) == 3)
                spawnReqs.push_back(r);
        }
        else if (!strcmp(argv[i], "--walkto") && i + 2 < argc) {
            walkTo[0] = (float)atof(argv[++i]);
            walkTo[1] = (float)atof(argv[++i]);
            walkToSet = true;
        }
        else if (!strcmp(argv[i], "--weapon") && i + 1 < argc)
            startWeapon = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hoverpx") && i + 2 < argc) {
            hoverPx = (float)atof(argv[++i]);
            hoverPy = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--skill") && i + 1 < argc) {
            SkillReq r; r.level = 1; r.x = r.z = 0;
            char buf[96];
            snprintf(buf, sizeof buf, "%s", argv[++i]);
            char* tok = strtok(buf, ",");
            if (tok) { snprintf(r.name, sizeof r.name, "%s", tok); }
            tok = strtok(nullptr, ",");  if (tok) r.x = (float)atof(tok);
            tok = strtok(nullptr, ",");  if (tok) r.z = (float)atof(tok);
            tok = strtok(nullptr, ",");  if (tok) r.level = atoi(tok);
            skillReqs.push_back(r);
        }
        else if (!strcmp(argv[i], "--swing"))
            swingLoop = true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init falhou: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("TMProject (OpenGL port)",
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow falhou: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        SDL_Log("SDL_GL_CreateContext falhou: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) || !GLAD_GL_VERSION_4_1) {
        SDL_Log("OpenGL 4.1 core indisponivel");
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // D3D9 front faces are clockwise on screen (y-down window); GL's default is
    // CCW (y-up). Flipping the front-face definition makes D3DCULL_CCW == GL_BACK
    // with identical semantics to the original (validated on castle walls).
    glFrontFace(GL_CW);

    tmx::LogInit("tm.log");
    tmx::Log("OpenGL context up: %s", (const char*)glGetString(GL_VERSION));
    WriteBootReport("boot_report.txt");

    {   // default GL viewport is 0x0 — set it before anything draws
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        glViewport(0, 0, pw, ph);
    }

    SDL_GL_SetSwapInterval(1);

    tmx::GLRenderDevice device;
    if (!device.Init(window)) {
        tmx::Log("GLRenderDevice init falhou");
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- phase 2 scene: terrain + static objects of one map (assets NOT in the repo) ---
    tmx::GLTextureManager textures;
    tmx::FieldView view;
    bool sceneLoaded = false;
    {
        std::vector<uint8_t> texList, envTexList, fxTexList;
        std::string meshList, boneAniList;
        if (ReadWholeFile("Mesh\\MeshTextureList.bin", texList))
            textures.LoadModelTextureList(texList.data(), texList.size());
        if (ReadWholeFile("env\\EnvTextureList3.bin", envTexList))
            textures.LoadEnvTextureList(envTexList.data(), envTexList.size());
        if (ReadWholeFile("Effect\\EffectTextureList.bin", fxTexList))
            textures.LoadEffectTextureList(fxTexList.data(), fxTexList.size());
        ReadWholeFileText("Mesh\\BoneAni4.txt", boneAniList);
        if (ReadWholeFileText("Mesh\\MeshList.txt", meshList))
            sceneLoaded = view.Load(mapName, textures, meshList, boneAniList);
        if (sceneLoaded) {
            std::string glErr;
            if (!view.InitGL(&glErr)) {
                tmx::Log("FieldView InitGL falhou: %s", glErr.c_str());
                sceneLoaded = false;
            }
        }
        if (!sceneLoaded)
            tmx::Log("assets do jogo nao encontrados — modo fallback (triangulo)");
    }

    // --- phase 3: characters (own char + --spawn extras) ---
    tmx::Character* myChar = nullptr;
    if (sceneLoaded) {
        std::string aniSoundTxt;
        std::string boneAniTxt;
        ReadWholeFileText("Mesh\\BoneAni4.txt", boneAniTxt);
        bool charSys = false;
        {
            std::vector<uint8_t> raw;
            if (ReadWholeFile("AniSound4.txt", raw))
                aniSoundTxt.assign((const char*)raw.data(), raw.size());
        }
        std::string charErr;
        if (!aniSoundTxt.empty())
            charSys = view.InitCharacters(boneAniTxt, aniSoundTxt, textures, &charErr);
        if (charSys) {
            // Own character: ch01, TK look padrão, centro do mapa.
            const float cx = view.Terrain().OffsetX() + 64.0f;
            const float cz = view.Terrain().OffsetY() + 64.0f;
            if (spawnChar) {
                tmx::CharDesc d;
                d.weaponIndex = startWeapon;
                myChar = view.Spawn(d, cx, cz, &charErr);
                if (myChar)
                    tmx::Log("char spawn: (%.1f, %.1f)", cx, cz);
                else
                    tmx::Log("char spawn falhou: %s", charErr.c_str());
            }
            for (const auto& r : spawnReqs) {
                tmx::CharDesc d;
                d.boneAniIndex = r.type;
                if (!view.Spawn(d, r.x, r.z, &charErr))
                    tmx::Log("spawn %d falhou: %s", r.type, charErr.c_str());
            }
        } else if (!charErr.empty()) {
            tmx::Log("characters: %s", charErr.c_str());
        }
    }
    bool follow = followCam >= 0 ? (followCam != 0) : (myChar != nullptr);
    float followDist = 6.0f;

    // Sky dome + weather (phase 2 D4). Default weather = day-of-month % 4 like the
    // original select-server scene (TMSelectServerScene.cpp:323-325).
    tmx::SkyDome sky;
    bool skyLoaded = false;
    if (sceneLoaded) {
        std::string skyErr;
        std::string meshList;
        ReadWholeFileText("Mesh\\MeshList.txt", meshList);
        if (sky.Init(meshList, textures, &skyErr)) {
            skyLoaded = true;
            if (weather < 0) {
                time_t now = time(nullptr);
                struct tm* lt = localtime(&now);
                weather = (lt ? lt->tm_mday : 1) % 4;
            }
            sky.SetWeather(weather);
            tmx::Log("clima: %d (0=sol 1=nublado 2=chuva 3=neve)", weather);
        } else {
            tmx::Log("sky: %s", skyErr.c_str());
        }
    }

    // Free-fly camera (phase 2 validation): starts above the scene center looking
    // down; WASD moves, Q/E down/up, left-mouse drag looks, shift = fast.
    D3DXMATRIX matView, matProj;
    float camX, camY, camZ, camYaw = startYaw, camPitch = startPitch;
    {
        float bmin[3], bmax[3];
        view.Bounds(bmin, bmax);
        if (!sceneLoaded) {
            bmin[0] = bmin[1] = bmin[2] = -1.0f;
            bmax[0] = bmax[1] = bmax[2] = 1.0f;
        }
        camX = (bmin[0] + bmax[0]) * 0.5f;
        camZ = (bmin[2] + bmax[2]) * 0.5f - 14.0f;
        camY = (bmax[1] > bmin[1] ? bmax[1] : 5.0f) + 8.0f;
        if (view.HasTerrain()) {
            // Object heights can be huge (floating decor) — start relative to the
            // TERRAIN under the camera instead of the object bounds.
            float tMin = 1e9f, tMax = -1e9f;
            for (int i = 0; i < 4096; ++i) {
                const float h = view.Terrain().tiles[i].height * 0.1f;
                if (h < tMin) tMin = h;
                if (h > tMax) tMax = h;
            }
            camY = tMax + 8.0f;
        }
        if (startCamSet) {
            camX = startCam[0];
            camY = startCam[1];
            camZ = startCam[2];
        }
        tmx::Log("camera: (%.1f %.1f %.1f) bounds x[%.1f..%.1f] y[%.1f..%.1f] z[%.1f..%.1f]",
                 camX, camY, camZ, bmin[0], bmax[0], bmin[1], bmax[1], bmin[2], bmax[2]);
    }
    auto updateView = [&]() {
        if (follow && myChar) {
            // Orbit the character: eye = target - dir * dist (target at chest height).
            const float tx = myChar->X(), ty = myChar->Height() + 1.5f, tz = myChar->Z();
            const float cp = cosf(camPitch);
            D3DXVECTOR3 dir(sinf(camYaw) * cp, sinf(camPitch), cosf(camYaw) * cp);
            D3DXVECTOR3 eye(tx - dir.x * followDist, ty - dir.y * followDist,
                            tz - dir.z * followDist);
            D3DXVECTOR3 at(tx, ty, tz);
            D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
            camX = eye.x; camY = eye.y; camZ = eye.z;
            D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
        } else {
            const float cp = cosf(camPitch);
            D3DXVECTOR3 eye(camX, camY, camZ);
            D3DXVECTOR3 dir(sinf(camYaw) * cp, sinf(camPitch), cosf(camYaw) * cp);
            D3DXVECTOR3 at = eye + dir;
            D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
            D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
        }
        device.SetViewProj(matView, matProj);
    };
    {
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        // game projection: fov 45deg, near 0.69*1.4, far 540 (field value)
        D3DXMatrixPerspectiveFovLH(&matProj, 0.25f * D3DXToRadian(180),
                                   (float)pw / (float)ph, 0.69f * 1.4f, 540.0f);
    }
    updateView();

    // Scene lighting/weather: SkyDome applies fog + weather light colors (D4).
    // Directions are the RenderDevice ctor defaults (RenderDevice.cpp:92-116).
    {
        if (skyLoaded)
            sky.ApplyWeather(device);
        else {
            D3DXVECTOR3 d0(-10.0f, 10.0f, -6.0f), d1(10.0f, -14.0f, 6.0f);
            D3DXVec3Normalize(&d0, &d0);
            D3DXVec3Normalize(&d1, &d1);
            device.SetDirectionalLight(0, d0, 1.0f, 1.0f, 1.0f);
            device.SetDirectionalLight(1, d1, 1.0f, 1.0f, 1.0f);
        }
        device.SetAmbient(1.0f, 1.0f, 1.0f, 1.0f); // block-1 D3DRS_AMBIENT=0x33FFFFFF
    }

    // Fallback triangle when the game assets are absent (keeps the pipeline provable).
    tmx::GLMesh fallbackMesh;
    if (!sceneLoaded) {
        struct V { float x, y, z, nx, ny, nz; float u, v; };
        const V verts[3] = {
            { -0.5f, -0.5f, 0.0f,  0, 0, -1,  0.0f, 1.0f },
            {  0.5f, -0.5f, 0.0f,  0, 0, -1,  1.0f, 1.0f },
            {  0.0f,  0.5f, 0.0f,  0, 0, -1,  0.5f, 0.0f },
        };
        const uint16_t idx[3] = { 0, 1, 2 };

        tmx::MsaData data;
        data.fvf = 530;
        data.fileStride = 32;
        data.memStride = 40;
        data.vertices.resize(3 * 40, 0);
        for (int i = 0; i < 3; ++i)
            memcpy(data.vertices.data() + i * 40, &verts[i], 32);
        data.indices.assign(idx, idx + 3);
        tmx::MsaData::AttrRange ar{};
        ar.faceStart = 0; ar.faceCount = 1;
        data.subsets.push_back(ar);
        data.textureNames.push_back("");
        fallbackMesh.Upload(data);
    }

    // Unproject a window pixel to a world-space ray (LH, z 0..1).
    auto makeRay = [&](float px, float py, float ro[3], float rd[3]) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        const float ndcX = 2.0f * px / (float)w - 1.0f;
        const float ndcY = 1.0f - 2.0f * py / (float)h;
        D3DXMATRIX vp, inv;
        D3DXMatrixMultiply(&vp, &matView, &matProj);
        D3DXMatrixInverse(&inv, nullptr, &vp);
        D3DXVECTOR3 near3(ndcX, ndcY, 0.0f), far3(ndcX, ndcY, 1.0f);
        auto xform = [&](const D3DXVECTOR3& v, D3DXVECTOR3& out) {
            const float* m = &inv._11;
            const float w4 = m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15];
            out.x = (m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]) / w4;
            out.y = (m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]) / w4;
            out.z = (m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]) / w4;
        };
        D3DXVECTOR3 pn, pf;
        xform(near3, pn);
        xform(far3, pf);
        ro[0] = pn.x; ro[1] = pn.y; ro[2] = pn.z;
        rd[0] = pf.x - pn.x; rd[1] = pf.y - pn.y; rd[2] = pf.z - pn.z;
    };

    bool running = true;
    bool mouseLook = false;
    float mousePx = hoverPx, mousePy = hoverPy;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE)
                    running = false;
                else if (e.key.key == SDLK_F12)
                    SaveScreenshot(window, "screenshot.bmp");
                else if (e.key.key == SDLK_F) {
                    if (myChar) {
                        follow = !follow;
                        if (!follow) {   // keep the view where the follow cam left it
                            camPitch = -0.7f;
                        }
                    }
                }
                else if (myChar) {
                    const uint32_t nowMs = SDL_GetTicks();
                    switch (e.key.key) {
                    case SDLK_R: {   // walk/run toggle
                        const bool run = myChar->MaxSpeed() <= 2.0f;
                        myChar->SetMaxSpeed(run ? 4.0f : 2.0f);
                        break;
                    }
                    case SDLK_1: myChar->SetMotion(tmx::CharMotion::Attack01, nowMs); break;
                    case SDLK_2: myChar->SetMotion(tmx::CharMotion::Attack02, nowMs); break;
                    case SDLK_3: myChar->SetMotion(tmx::CharMotion::Attack03, nowMs); break;
                    case SDLK_4: myChar->SetMotion(tmx::CharMotion::Attack04, nowMs); break;
                    case SDLK_5: myChar->SetMotion(tmx::CharMotion::Attack05, nowMs); break;
                    case SDLK_6: myChar->SetMotion(tmx::CharMotion::Attack06, nowMs); break;
                    case SDLK_8: myChar->SetMotion(tmx::CharMotion::Stand01, nowMs); break;
                    case SDLK_9: myChar->SetMotion(tmx::CharMotion::LevelUp, nowMs); break;
                    case SDLK_0: myChar->SetMotion(tmx::CharMotion::Die, nowMs); break;
                    case SDLK_P: {   // cycle weapon (demo)
                        tmx::CharDesc d = myChar->Desc();
                        d.weaponIndex = (d.weaponIndex + 1) % 12;
                        const float px = myChar->X(), pz = myChar->Z();
                        view.RemoveCharacter(myChar);
                        myChar = view.Spawn(d, px, pz, nullptr);
                        break;
                    }
                    case SDLK_C: {   // cycle class (TK/Foema/BM/Hunter demo)
                        static const int kPairs[4][2] = {
                            { 0, 0 }, { 1, 1 }, { 0, 2 }, { 1, 3 } };  // (type, class)
                        tmx::CharDesc d = myChar->Desc();
                        int cur = 0;
                        for (int i = 0; i < 4; ++i)
                            if (kPairs[i][0] == d.boneAniIndex && kPairs[i][1] == d.classIndex)
                                cur = i;
                        cur = (cur + 1) % 4;
                        d.boneAniIndex = kPairs[cur][0];
                        d.classIndex = kPairs[cur][1];
                        const float px = myChar->X(), pz = myChar->Z();
                        view.RemoveCharacter(myChar);
                        myChar = view.Spawn(d, px, pz, nullptr);
                        break;
                    }
                    default: break;
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseLook = true;
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouseLook = false;
                    SDL_SetWindowRelativeMouseMode(window, false);
                } else if (e.button.button == SDL_BUTTON_RIGHT && view.HasTerrain()) {
                    // Pick: unproject the click point through inverse(view*proj).
                    int w = 0, h = 0;
                    SDL_GetWindowSizeInPixels(window, &w, &h);
                    const float ndcX = 2.0f * e.button.x / (float)w - 1.0f;
                    const float ndcY = 1.0f - 2.0f * e.button.y / (float)h;
                    D3DXMATRIX vp, inv;
                    D3DXMatrixMultiply(&vp, &matView, &matProj);
                    D3DXMatrixInverse(&inv, nullptr, &vp);
                    // LH perspective: near plane point = (ndc.x, ndc.y, 0), far = 1
                    D3DXVECTOR3 near3(ndcX, ndcY, 0.0f), far3(ndcX, ndcY, 1.0f);
                    auto xform = [&](const D3DXVECTOR3& v, D3DXVECTOR3& out) {
                        const float* m = &inv._11;
                        const float w4 = m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15];
                        out.x = (m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]) / w4;
                        out.y = (m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]) / w4;
                        out.z = (m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]) / w4;
                    };
                    D3DXVECTOR3 pn, pf;
                    xform(near3, pn);
                    xform(far3, pf);
                    D3DXVECTOR3 dir(pf.x - pn.x, pf.y - pn.y, pf.z - pn.z);
                    float hit[3];
                    const float ro[3] = { pn.x, pn.y, pn.z };
                    const float rd[3] = { dir.x, dir.y, dir.z };
                    if (tmx::TerrainPick(view.Terrain(), camX, camZ, ro, rd, hit)) {
                        if (myChar && !myChar->MoveTo(hit[0], hit[2], SDL_GetTicks()))
                            tmx::Log("move: sem rota para (%.1f, %.1f)", hit[0], hit[2]);
                    } else {
                        tmx::Log("pick: nada");
                    }
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (hoverPx >= 0.0f)   // --hoverpx automation: ignore real mouse
                    break;
                mousePx = e.motion.x;
                mousePy = e.motion.y;
                if (mouseLook) {
                    camYaw   += e.motion.xrel * 0.003f;
                    camPitch -= e.motion.yrel * 0.003f;
                    if (camPitch >  1.5f) camPitch =  1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (follow) {
                    followDist -= e.wheel.y * 0.8f;
                    if (followDist < 3.0f) followDist = 3.0f;
                    if (followDist > 14.0f) followDist = 14.0f;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                int w = 0, h = 0;
                SDL_GetWindowSizeInPixels(window, &w, &h);
                glViewport(0, 0, w, h);
                D3DXMatrixPerspectiveFovLH(&matProj, 0.25f * D3DXToRadian(180),
                                           (float)w / (float)h, 0.69f * 1.4f, 540.0f);
                break;
            }
            }
        }

        // WASD/QE movement, dt-scaled; shift = 4x speed (free-fly only)
        if (!follow) {
            static Uint64 lastTicks = SDL_GetTicks();
            const Uint64 now = SDL_GetTicks();
            const float dt = (float)(now - lastTicks) / 1000.0f;
            lastTicks = now;

            const bool* keys = SDL_GetKeyboardState(nullptr);
            float speed = (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) ? 60.0f : 15.0f;
            const float step = speed * dt;
            const float fwdX = sinf(camYaw), fwdZ = cosf(camYaw);
            const float rightX = cosf(camYaw), rightZ = -sinf(camYaw);
            if (keys[SDL_SCANCODE_W]) { camX += fwdX * step; camZ += fwdZ * step; }
            if (keys[SDL_SCANCODE_S]) { camX -= fwdX * step; camZ -= fwdZ * step; }
            if (keys[SDL_SCANCODE_D]) { camX += rightX * step; camZ += rightZ * step; }
            if (keys[SDL_SCANCODE_A]) { camX -= rightX * step; camZ -= rightZ * step; }
            if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE]) camY += step;
            if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LCTRL]) camY -= step;
        }
        updateView();

        {
            int fw = 0, fh = 0;
            SDL_GetWindowSizeInPixels(window, &fw, &fh);
            // Game horizon-angle convention: our camYaw with dir=(sin,cos) maps to
            // h = -yaw - pi/2 so that YPR(pi/2 - h, ...) faces the camera.
            tmx::FieldView::FxFrameInfo fxInfo;
            fxInfo.yawH = -camYaw - 1.5707963f;
            fxInfo.pitchV = camPitch;
            fxInfo.screenW = fw;
            fxInfo.screenH = fh;
            if (myChar) {
                fxInfo.focusX = myChar->X();
                fxInfo.focusH = myChar->Height();
                fxInfo.focusZ = myChar->Z();
            } else {
                fxInfo.focusX = camX + 2.5f;
                fxInfo.focusH = camY - 2.0f;
                fxInfo.focusZ = camZ + 2.5f;
            }
            fxInfo.right[0] = matView._11; fxInfo.right[1] = matView._21; fxInfo.right[2] = matView._31;
            fxInfo.up[0]    = matView._12; fxInfo.up[1]    = matView._22; fxInfo.up[2]    = matView._32;
            view.SetFxFrame(fxInfo);
            view.SetWeatherFx(weather);   // 2 = rain, 3 = snow, else off
        }
        // Mouse-over highlight (TMFieldScene green emissive).
        if (sceneLoaded && !mouseLook) {
            float ro[3], rd[3];
            makeRay(mousePx, mousePy, ro, rd);
            float bestT = 1e9f;
            tmx::Character* best = nullptr;
            for (size_t i = 0; i < view.CharacterCount(); ++i) {
                tmx::Character* c = view.GetCharacter(i);
                const float t = c->PickTest(ro, rd);
                if (t >= 0.0f && t < bestT) {
                    bestT = t;
                    best = c;
                }
                c->SetHighlight(false);
            }
            if (best)
                best->SetHighlight(true);
        }
        if (sceneLoaded)
            view.FrameMove((float)SDL_GetTicks() / 1000.0f);

        static int frameNo = 0;
        if (walkToSet && myChar && ++frameNo == 3)
            myChar->MoveTo(walkTo[0], walkTo[1], SDL_GetTicks());

        // Phase 5: spawn combat VFX on the 2nd frame (after the scene is live,
        // skills land in the already-ticked container). Height follows terrain.
        static bool skillsSpawned = false;
        if (!skillsSpawned && sceneLoaded) {
            skillsSpawned = true;
            const uint32_t nowMs = SDL_GetTicks();
            const tmx::TerrainData* terr = view.HasTerrain() ? &view.Terrain() : nullptr;
            for (const auto& r : skillReqs) {
                const float y = terr ? tmx::TerrainGetHeight(*terr, r.x, r.z) : 0.0f;
                if (!strcmp(r.name, "glow")) {
                    view.AddSkillEffect(std::make_unique<tmx::SkillGlow>(
                        r.x, y, r.z, 56, 700, 0.8f, 0xFFFFFFFF));
                } else if (!strcmp(r.name, "burst")) {
                    view.AddSkillEffect(std::make_unique<tmx::SkillBurst>(
                        r.x, y, r.z, 8, 700, 10, 1.2f, 0.6f, 0xFFFFAA00));
                } else if (!strcmp(r.name, "bash")) {
                    // TMSkillBash-lite: center glow + splash ring (fire tex 11,
                    // 0xFF111105 in the original) every ~250ms for the lifetime.
                    view.AddSkillEffect(std::make_unique<tmx::SkillGlow>(
                        r.x, y, r.z, 11, 700, 0.9f, 0xFF111105));
                    view.AddSkillEffect(std::make_unique<tmx::SkillBurst>(
                        r.x, y, r.z, 11, 700, 12, 1.0f, 0.5f, 0xFF332200));
                } else if (!strcmp(r.name, "heal")) {
                    view.AddSkillEffect(std::make_unique<tmx::SkillGlow>(
                        r.x, y, r.z, 56, 1200, 1.1f, 0xFF44FF44));
                    view.AddSkillEffect(std::make_unique<tmx::SkillBurst>(
                        r.x, y, r.z, 56, 1200, 14, 0.8f, 0.5f, 0xFF22AA22));
                } else if (!strcmp(r.name, "meteor")) {
                    // TMSkillMeteorStorm-lite: orange ground glow + wide burst
                    // (tex 11 fire / 71 ash in the original L4+ splash).
                    view.AddSkillEffect(std::make_unique<tmx::SkillGlow>(
                        r.x, y, r.z, 11, 900, 1.3f, 0xFFFF7711));
                    view.AddSkillEffect(std::make_unique<tmx::SkillBurst>(
                        r.x, y, r.z, 11, 900, 18, 2.0f * r.level, 0.7f, 0xFFAA3300));
                } else {
                    tmx::Log("--skill '%s' desconhecido (use glow/burst/bash/heal/meteor)", r.name);
                }
            }
        }

        // Phase 5 weapon trail demo (--swing): loop attacks on the focused char,
        // driving a SwingTrail ribbon from the right-hand bone.
        static tmx::SwingTrail* swingFx = nullptr;
        static uint32_t swingCycle = 0;
        if (swingLoop && myChar && sceneLoaded) {
            const uint32_t nowMs = SDL_GetTicks();
            if (!swingFx) {
                auto fx = std::make_unique<tmx::SwingTrail>();
                swingFx = fx.get();
                view.AddSkillEffect(std::move(fx));
            }
            if (nowMs - swingCycle > 1100) {
                swingCycle = nowMs;
                myChar->SetMotion(tmx::CharMotion::Attack01, nowMs);
                myChar->AttachSwing(swingFx, 0.9f, nowMs, 600);
            }
        }

        device.BeginFrame();
        {
            const float* cc = skyLoaded ? sky.ClearColor() : nullptr;
            if (cc)
                device.Clear(cc[0], cc[1], cc[2], 1.0f);
            else
                device.Clear(0.08f, 0.10f, 0.16f, 1.0f);
        }

        if (sceneLoaded) {
            if (skyLoaded)
                sky.Render(device, camX, camZ);
            // Sun + lens flare (weather 0 only — TMSky hides it for 1-9).
            if (skyLoaded && sky.Weather() == 0) {
                int fw = 0, fh = 0;
                SDL_GetWindowSizeInPixels(window, &fw, &fh);
                tmx::SunFlareEntry table[12];
                tmx::SunFlareBuildTable(fh > 0 ? (float)fw / 800.0f : 1.0f, table);
                tmx::FxQuad flares[12];
                if (tmx::SunFlareCompute(table, camX, camY, camZ, matView, matProj,
                                         fw, fh, 1.0f, flares))
                    for (const auto& q : flares)
                        view.EmitScreenFx(q);
            }
            view.Render(device);
        } else {
            device.SetRenderStateBlock(1);
            D3DXMATRIX world;
            D3DXMatrixIdentity(&world);
            device.SetWorldMatrix(world);
            device.DrawMesh(fallbackMesh);
        }

        device.EndFrame();

        if (shotPath && --shotFrames <= 0) {
            SaveScreenshot(window, shotPath);
            break;
        }
    }

    tmx::Log("shutdown limpo");
    sky.Destroy();
    view.Destroy();
    fallbackMesh.Destroy();
    textures.DestroyAll();
    device.Shutdown();
    tmx::LogShutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
