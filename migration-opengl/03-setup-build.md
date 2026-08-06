# 03 — Setup, Build e Bootstrap

## Dependências

| Lib | Versão | Papel | Por quê |
|---|---|---|---|
| **SDL3** | ≥ 3.1 | janela, contexto GL, input, timer | Também cobre áudio depois (substitui DirectSound) e IME (substitui imm32). GLFW é a alternativa mínima se quiser só janela |
| **glad2** | core 4.1 | loader de funções GL | Gera loader só com 4.1 core, sem compat cruft |
| **glm** | ≥ 1.0 | math para código **novo** | Código legado usa o shim D3DX (04-convencoes.md) |
| **stb_image / stb_truetype / stb_image_write** | latest | TGA decode, fontes, screenshot | Header-only, zero build |
| (nenhuma lib DDS) | — | DXT1/3 | Loader próprio ~100 linhas (10-texturas.md); os arquivos são DXT cru sem header complexo |

Instalação recomendada: **FetchContent** (reprodutível, sem package manager). Pinned por tag.

## CMakeLists.txt (raiz)

```cmake
cmake_minimum_required(VERSION 3.24)
project(TMProject LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.0)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1)
FetchContent_MakeAvailable(glm)

# glad2: gerar com  python -m glad --api gl:core=4.1 --out-dir extern/glad
# ou vendored em extern/glad (recomendado: commitar o gerado)
add_library(glad STATIC extern/glad/src/gl.c)
target_include_directories(glad PUBLIC extern/glad/include)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE extern/stb)

file(GLOB_RECURSE TM_SOURCES CONFIGURE_DEPENDS
  src/*.cpp src/*.h)

add_executable(TMProject ${TM_SOURCES})
target_link_libraries(TMProject PRIVATE SDL3::SDL3-static glad glm::glm stb)

# macOS: framework necessários para GL via SDL (SDL cuida disso)
# Warnings úteis para caçar o código de 2003:
target_compile_options(TMProject PRIVATE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra
    -Wshorten-64-to-32 -Wdeprecated-declarations>)
```

### Notas por plataforma

- **macOS**: Apple expõe no máximo **4.1 core** (`NSOpenGLProfileVersion4_1Core` — SDL3
  seleciona automaticamente com os atributos abaixo). Sem `ARB_clip_control`, sem SSBO,
  sem persistent mapping. O design inteiro respeita isso.
- **Linux**: Mesa ≥ 22 cobre 4.1 em qualquer GPU dos últimos 12 anos (incl. llvmpipe).
- **Windows**: manter `.vcxproj` opcionalmente; CMake gera VS2022 solution.
- **Depreciação no macOS**: compila com warnings de deprecation do GL — suprimir com
  `-Wno-deprecated-declarations` apenas nos arquivos de backend GL, ou
  `#define GL_SILENCE_DEPRECATION` antes dos includes.

## Bootstrap mínimo (`src/platform/main.cpp`)

```cpp
#define GL_SILENCE_DEPRECATION
#include <SDL3/SDL.h>
#include <glad/gl.h>

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);   // DESTALPHA blend (lens flare) depende disso
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("WYD",
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(window);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("OpenGL 4.1 core indisponível");
        return 1;
    }

    // Sanity do contrato mínimo (uma vez, no boot):
    //  - GL 4.1 core garante: instancing, UBO, FBO, texture array, transform feedback
    //  - EXT_texture_compression_s3tc: necessária p/ .wys (DXT1/3). Universal em HW real.

    SDL_HideCursor();  // cursor é software (SCursor), ver 09-ui-fontes.md

    // ... init do jogo (TMGame::Init) e loop:
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            // alimentar input system (substitui DirectInput + message loop Win32)
        }
        g_pGame->Frame();              // = NewApp::Run(): FrameMove + RenderScene
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
```

## Substituições de plataforma fora do render (para o port compilar)

O foco desta pasta é o renderer, mas estes são os stubs mínimos para o jogo subir:

| Win32 | Substituição imediata |
|---|---|
| `CreateWindowEx`/message loop (`NewApp.cpp:294`) | SDL3 acima |
| DirectInput8 | SDL keyboard/mouse events |
| DirectSound (`dsutil.cpp`) | fase 2: miniaudio/SDL_audio — no MVP, stub silencioso |
| DirectShow (`TMVideoWnd`) | cortar vídeos de intro no MVP; fase 2: libmpv |
| WinInet (6 chamadas `InternetOpen`) | libcurl ou stub (download de guild mark / patch) |
| Winsock (`CPSock.cpp`) | quase drop-in: `closesocket`→`close`, `WSAStartup` no-op |
| imm32 (IME coreano) | SDL `SDL_StartTextInput` (fase 2) |
| `timeGetTime` | `SDL_GetTicks()` |
| `SetGammaRamp` (GDI) | uniform de brilho (12-modernizacoes.md) ou ignorar |
| `.cur` resources | modo software apenas |
| StackWalker | ignorar; usar `addr2line`/`atos` |

## Estrutura de diretórios sugerida

```
src/
├── platform/      main.cpp, SDL input, file system (fopen → std::filesystem ok)
├── gl/            GLRenderDevice, GLStateCache, GLShaderLibrary, batchers
├── render/        (código de gameplay inalterado, só com includes trocados)
├── game/          TM*, SControl*, ObjectManager... (idem)
├── math/          TMMath.h (shim D3DX)
└── net/           CPSock (BSD sockets)
shaders/
├── *.vert / *.frag   (GLSL 410, catálogo em 11-shaders.md)
extern/
├── glad/ stb/
```

## Verificação contínua

- Build no CI desde o dia 1: GitHub Actions com `macos-14` + `ubuntu-24.04` + `windows-2022`.
- O projeto já tem `.github/` — adicionar workflow matrix simples.
- Flag `-DGL_SILENCE_DEPRECATION` global no macOS.
- `RenderDoc` funciona nos 3 OS para captura de frame — essencial para comparar
  visualmente com o cliente D3D original (validação pixel-a-pixel, ver 13-roadmap.md).
