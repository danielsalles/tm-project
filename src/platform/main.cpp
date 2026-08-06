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
    const char* shotPath = nullptr;
    int shotFrames = 30;
    char mapName[32] = "Field2723";
    int weather = -1;   // -1 = like the original select-server: day-of-month % 4
    float startPitch = -0.7f, startYaw = 0.0f;
    float startCam[3] = { 0, 0, 0 };
    bool startCamSet = false;
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
        std::string meshList;
        if (ReadWholeFile("Mesh\\MeshTextureList.bin", texList))
            textures.LoadModelTextureList(texList.data(), texList.size());
        if (ReadWholeFile("env\\EnvTextureList3.bin", envTexList))
            textures.LoadEnvTextureList(envTexList.data(), envTexList.size());
        if (ReadWholeFile("Effect\\EffectTextureList.bin", fxTexList))
            textures.LoadEffectTextureList(fxTexList.data(), fxTexList.size());
        if (ReadWholeFileText("Mesh\\MeshList.txt", meshList))
            sceneLoaded = view.Load(mapName, textures, meshList);
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
        const float cp = cosf(camPitch);
        D3DXVECTOR3 eye(camX, camY, camZ);
        D3DXVECTOR3 dir(sinf(camYaw) * cp, sinf(camPitch), cosf(camYaw) * cp);
        D3DXVECTOR3 at = eye + dir;
        D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
        D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
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

    bool running = true;
    bool mouseLook = false;
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
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseLook) {
                    camYaw   += e.motion.xrel * 0.003f;
                    camPitch -= e.motion.yrel * 0.003f;
                    if (camPitch >  1.5f) camPitch =  1.5f;
                    if (camPitch < -1.5f) camPitch = -1.5f;
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

        // WASD/QE movement, dt-scaled; shift = 4x speed
        {
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

        if (sceneLoaded)
            view.FrameMove((float)SDL_GetTicks() / 1000.0f);

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
