#define GL_SILENCE_DEPRECATION
#include <SDL3/SDL.h>
#include <glad/gl.h>

#include <cstdio>
#include <cmath>

#include "platform/Platform.h"

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

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("OpenGL 4.1 core indisponivel");
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    tmx::LogInit("tm.log");
    tmx::Log("OpenGL context up: %s", (const char*)glGetString(GL_VERSION));
    WriteBootReport("boot_report.txt");

    SDL_GL_SetSwapInterval(1);
    SDL_HideCursor();

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
                break;
            }
        }

        float t = tmx::GetTicks() / 1000.0f;
        float r = 0.2f + 0.2f * sinf(t);
        float g = 0.3f + 0.2f * sinf(t * 1.3f + 1.0f);
        float b = 0.4f + 0.2f * sinf(t * 0.7f + 2.0f);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        SDL_GL_SwapWindow(window);
    }

    tmx::Log("shutdown limpo");
    tmx::LogShutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
