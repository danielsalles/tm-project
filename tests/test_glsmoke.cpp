// GL smoke test (doc 15 §8): hidden window, real GL 4.1 context, compile mesh_lit,
// one draw into the default framebuffer, glGetError must stay clean and at least
// one pixel must change. Skips (exit 0) when the platform has no GL 4.1 —
// notably the Windows CI runners, which only expose GL 1.1 via GDI.

#define GL_SILENCE_DEPRECATION
#include <SDL3/SDL.h>
#include <glad/gl.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "gl/GLRenderDevice.h"
#include "gl/GLMesh.h"
#include "platform/Platform.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SKIP glsmoke: SDL_Init falhou (%s)\n", SDL_GetError());
        return 0;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("glsmoke", 64, 64,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        printf("SKIP glsmoke: sem janela GL (%s)\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    // gladLoadGL returns non-zero even on a legacy GL 1.1 context (Windows runners
    // only expose GDI's 1.1) — must check the feature flag or 4.1 pointers stay NULL.
    if (!ctx || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) || !GLAD_GL_VERSION_4_1) {
        printf("SKIP glsmoke: GL 4.1 core indisponivel (%s)\n", SDL_GetError());
        if (ctx) SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

    printf("glsmoke: %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    glFrontFace(GL_CW);   // D3D winding convention (see main.cpp)

    // default GL viewport is 0x0 — without this nothing rasterizes
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    glViewport(0, 0, pw, ph);

    tmx::GLRenderDevice device;
    if (!device.Init(window)) {
        printf("FAIL glsmoke: GLRenderDevice::Init (shader compile?)\n");
        return 1;
    }

    // identity view/proj, triangle via the full facade
    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    device.SetViewProj(id, id);

    tmx::MsaData data;
    data.fvf = 530;
    data.fileStride = 32;
    data.memStride = 40;
    // triangle covering the center of the screen (NDC after identity matrices)
    const float verts[3][8] = {
        { -0.5f, -0.5f, 0.5f,  0, 0, -1,  0.0f, 0.0f },
        {  0.5f, -0.5f, 0.5f,  0, 0, -1,  1.0f, 0.0f },
        {  0.0f,  0.5f, 0.5f,  0, 0, -1,  0.5f, 1.0f },
    };
    data.vertices.resize(3 * 40, 0);
    for (int i = 0; i < 3; ++i)
        memcpy(data.vertices.data() + i * 40, verts[i], 32);
    const uint16_t idx[3] = { 0, 2, 1 };   // CW on screen = front with glFrontFace(GL_CW)
    data.indices.assign(idx, idx + 3);
    tmx::MsaData::AttrRange ar{};
    ar.faceStart = 0;
    ar.faceCount = 1;
    data.subsets.push_back(ar);
    data.textureNames.push_back("");

    tmx::GLMesh mesh;
    if (!mesh.Upload(data)) {
        printf("FAIL glsmoke: mesh upload\n");
        return 1;
    }

    // 1x1 white texture so MODULATE has something to sample
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    mesh.subsets[0].textureIndex = (int)tex;

    device.BeginFrame();
    device.Clear(0.0f, 0.0f, 0.0f, 1.0f);
    device.SetRenderStateBlock(1);
    device.SetWorldMatrix(id);
    device.DrawMesh(mesh);

    // read BEFORE swap: backbuffer contents are undefined after it (Metal)
    GLenum glErr = glGetError();
    uint8_t center[4] = { 0, 0, 0, 0 };
    glReadPixels(pw / 2, ph / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center);

    device.EndFrame();

    if (glErr != GL_NO_ERROR) {
        printf("FAIL glsmoke: glGetError = 0x%x\n", glErr);
        return 1;
    }
    if (device.DrawCallsThisFrame() != 1) {
        printf("FAIL glsmoke: expected 1 draw call, got %d\n", device.DrawCallsThisFrame());
        return 1;
    }

    if (center[0] == 0 && center[1] == 0 && center[2] == 0) {
        printf("FAIL glsmoke: centro da tela ficou preto — nada rasterizou\n");
        return 1;
    }

    printf("OK glsmoke: draw executou, pixel central = (%d,%d,%d)\n",
           center[0], center[1], center[2]);

    glDeleteTextures(1, &tex);
    mesh.Destroy();
    device.Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
