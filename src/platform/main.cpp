#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stb_image_write.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "platform/Platform.h"
#include "gl/GLRenderDevice.h"
#include "gl/GLMesh.h"
#include "gl/GLTexture.h"
#include "scene/SelectServerView.h"

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
    (void)argc; (void)argv;

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

    // --- phase 1 scene: select-server static objects (assets are NOT in the repo) ---
    tmx::GLTextureManager textures;
    tmx::SelectServerView view;
    bool sceneLoaded = false;
    {
        std::vector<uint8_t> texList, dat;
        std::string meshList;
        if (ReadWholeFile("Mesh\\MeshTextureList.bin", texList) &&
            ReadWholeFileText("Mesh\\MeshList.txt", meshList) &&
            ReadWholeFile("env\\Field2723.dat", dat)) {
            textures.LoadModelTextureList(texList.data(), texList.size());
            sceneLoaded = view.Load(meshList, dat.data(), dat.size(), textures);
        }
        if (!sceneLoaded)
            tmx::Log("assets do jogo nao encontrados — modo fallback (triangulo)");
    }

    // Fixed camera (doc 15 §7.3): placeholder framed on the scene bounds; the exact
    // original select-server camera is a day-7 capture from the D3D client.
    D3DXMATRIX matView, matProj;
    {
        float bmin[3], bmax[3];
        view.Bounds(bmin, bmax);
        D3DXVECTOR3 center((bmin[0] + bmax[0]) * 0.5f,
                           (bmin[1] + bmax[1]) * 0.5f,
                           (bmin[2] + bmax[2]) * 0.5f);
        if (!sceneLoaded)
            center = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        float radius = 10.0f;
        if (sceneLoaded) {
            float dx = bmax[0] - bmin[0], dz = bmax[2] - bmin[2];
            radius = (dx > dz ? dx : dz) * 0.5f;
            if (radius < 5.0f) radius = 5.0f;
            if (radius > 30.0f) radius = 30.0f; // keep everything inside far=70
        }
        D3DXVECTOR3 eye(center.x, center.y + radius * 0.9f, center.z - radius * 1.4f);
        D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
        D3DXMatrixLookAtLH(&matView, &eye, &center, &up);
        // game projection: fov 45deg, near 0.69*1.4, far 70 in select scenes
        D3DXMatrixPerspectiveFovLH(&matProj, 0.25f * D3DXToRadian(180),
                                   1024.0f / 768.0f, 0.69f * 1.4f, 70.0f);
    }
    device.SetViewProj(matView, matProj);

    // Scene lighting from RenderDevice's constructor defaults (RenderDevice.cpp:92-116)
    {
        D3DXVECTOR3 d0(-10.0f, 10.0f, -6.0f), d1(10.0f, -14.0f, 6.0f);
        D3DXVec3Normalize(&d0, &d0);
        D3DXVec3Normalize(&d1, &d1);
        device.SetDirectionalLight(0, d0, 1.0f, 1.0f, 1.0f);
        device.SetDirectionalLight(1, d1, 1.0f, 1.0f, 1.0f);
        // ~material: diffuse 0.7 x lights, emissive 0.3 as ambient floor (TMObject.cpp:106-124)
        device.SetAmbient(0.3f, 0.3f, 0.3f, 1.0f);
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
            case SDL_EVENT_WINDOW_RESIZED: {
                int w = 0, h = 0;
                SDL_GetWindowSizeInPixels(window, &w, &h);
                glViewport(0, 0, w, h);
                D3DXMatrixPerspectiveFovLH(&matProj, 0.25f * D3DXToRadian(180),
                                           (float)w / (float)h, 0.69f * 1.4f, 70.0f);
                device.SetViewProj(matView, matProj);
                break;
            }
            }
        }

        device.BeginFrame();
        device.Clear(0.08f, 0.10f, 0.16f, 1.0f); // placeholder sky until phase 2

        if (sceneLoaded) {
            view.Render(device);
        } else {
            device.SetRenderStateBlock(1);
            D3DXMATRIX world;
            D3DXMatrixIdentity(&world);
            device.SetWorldMatrix(world);
            device.DrawMesh(fallbackMesh);
        }

        device.EndFrame();
    }

    tmx::Log("shutdown limpo (%d meshes carregadas)", view.MeshesLoaded());
    fallbackMesh.Destroy();
    textures.DestroyAll();
    device.Shutdown();
    tmx::LogShutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
