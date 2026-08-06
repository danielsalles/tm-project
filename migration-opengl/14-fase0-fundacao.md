# 14 — Fase 0: Fundação (plano completo)

**Objetivo**: projeto compilando/linkando/abrindo janela nos 3 OS, com a base matemática
(shim D3DX) provadamente correta contra a implementação original, e stubs de plataforma
suficientes para o jogo linkar sem render.

**Critério de saída (definition of done)**:
- [ ] CI verde em macOS (arm64), Ubuntu (x86_64), Windows (MSVC)
- [ ] Janela GL 4.1 core abre, clear color pisca, fecha limpo
- [ ] Suite de testes dourados do TMMath passa (100% dos casos dentro da tolerância)
- [ ] `CPSock` compila contra BSD sockets e ecoa num teste local
- [ ] Nenhum header Win32/D3D incluído fora de `platform/win/`

**Duração estimada**: 1-2 semanas.

---

## 1. Estrutura de diretórios (criar nesta fase)

```
tm-project/
├── CMakeLists.txt
├── cmake/
│   └── Sanitizers.cmake            (opcional: ASAN/UBSAN em debug Linux/macOS)
├── .github/workflows/build.yml
├── extern/
│   ├── glad/                       (gerado, commitado — ver §3)
│   └── stb/                        (stb_image.h, stb_truetype.h, stb_image_write.h)
├── src/
│   ├── platform/
│   │   ├── main.cpp                (bootstrap SDL — único ponto com SDL_main)
│   │   ├── Platform.h              (fachada: tempo, arquivo, log, path)
│   │   ├── PlatformTime.cpp        (timeGetTime → SDL_GetTicks)
│   │   ├── PlatformFile.cpp        (fopen wrapper + paths relativos ao asset dir)
│   │   └── PlatformLog.cpp         (printf → SDL_Log + arquivo)
│   ├── math/
│   │   ├── TMMath.h                (shim D3DX — header-only)
│   │   └── TMMath.cpp              (funções maiores: Inverse, IntersectTri)
│   ├── net/
│   │   └── CPSock.cpp              (port Winsock→BSD, quase drop-in)
│   └── gl/
│       └── (vazio nesta fase — entra na fase 1)
├── tests/
│   ├── CMakeLists.txt
│   ├── golden/
│   │   ├── README.md               (como regenerar)
│   │   ├── golden_generator.cpp    (roda 1× no Windows com D3DX real)
│   │   └── data/
│   │       ├── matrix_ops.bin      (valores dourados)
│   │       ├── vector_ops.bin
│   │       ├── quaternion_ops.bin
│   │       └── intersect.bin
│   ├── test_main.cpp               (harness mínimo próprio — ver §5.4)
│   ├── test_tmmath.cpp             (compara shim vs. dourados)
│   └── test_cpsock.cpp             (echo local)
└── tools/
    └── (fase 2+)
```

O código legado permanece em `Projects/TMProject/` **intocado** nesta fase. A migração
dos 109 `.cpp` acontece nas fases 1+, arquivo a arquivo, copiando para `src/` conforme
forem portados. Isso mantém o cliente Windows original compilável como referência
durante todo o processo — ele é o oráculo dos testes dourados e das comparações visuais.

---

## 2. CMake completo (raiz)

```cmake
cmake_minimum_required(VERSION 3.24)
project(TMProject LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clangd/IDE

option(TM_BUILD_TESTS "Build tests" ON)

include(FetchContent)

# --- SDL3 (estático, pinned) ---
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.16
  GIT_SHALLOW    TRUE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# --- glm (só para código novo; legado usa TMMath) ---
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glm)

# --- glad (gerado offline e commitado, ver §3) ---
add_library(glad STATIC extern/glad/src/gl.c)
target_include_directories(glad PUBLIC extern/glad/include)

# --- stb (header-only) ---
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE extern/stb)

# --- platform + math + net (fase 0) ---
add_library(tmcore STATIC
  src/platform/PlatformTime.cpp
  src/platform/PlatformFile.cpp
  src/platform/PlatformLog.cpp
  src/math/TMMath.cpp
  src/net/CPSock.cpp)
target_include_directories(tmcore PUBLIC src)
target_link_libraries(tmcore PUBLIC SDL3::SDL3-static)

target_compile_definitions(tmcore PRIVATE
  $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>)

target_compile_options(tmcore PRIVATE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W3>)

# --- executável (fase 0: bootstrap mínimo) ---
add_executable(TMProject src/platform/main.cpp)
target_link_libraries(TMProject PRIVATE tmcore glad glm::glm stb)

# --- testes ---
if(TM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
```

**Por que FetchContent e não vcpkg/brew**: build reprodutível idêntico nos 3 OS e no CI,
sem "funciona na minha máquina". glad e stb são commitados porque a geração do glad exige
Python e o stb não muda nunca — remover passos do build importa mais que pureza.

---

## 3. Gerar o glad (uma vez, commitar)

```bash
pip install glad2
python -m glad --api gl:core=4.1 --out-dir extern/glad
# produz extern/glad/include/glad/gl.h + extern/glad/src/gl.c
git add extern/glad && git commit -m "chore: vendor glad2 gl:core=4.1"
```

Sem extensões por ora. Na fase 1 regenerar com `--extensions
GL_EXT_texture_compression_s3tc,GL_EXT_texture_filter_anisotropic` (ou checar em runtime
via `SDL_GL_ExtensionSupported` — mais simples; decisão: runtime check, glad fica limpo).

---

## 4. CI (`.github/workflows/build.yml`)

```yaml
name: build
on: [push, pull_request]
jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - { os: macos-14,        cc: clang, cxx: clang++ }
          - { os: ubuntu-24.04,    cc: gcc,   cxx: g++     }
          - { os: windows-2022,    cc: cl,    cxx: cl      }
    runs-on: ${{ matrix.os }}
    env: { CC: ${{ matrix.cc }}, CXX: ${{ matrix.cxx }} }
    steps:
      - uses: actions/checkout@v4
      - name: deps (linux)
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y libx11-dev libxext-dev
             libwayland-dev libxkbcommon-dev wayland-protocols libegl1-mesa-dev
      - name: configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DTM_BUILD_TESTS=ON
      - name: build
        run: cmake --build build -j
      - name: test
        run: ctest --test-dir build --output-on-failure
```

Headless: os testes da fase 0 não abrem janela GL (TMMath e sockets são CPU/rede local).
Testes com GL entram na fase 1 e precisam de `xvfb-run` no Linux; no macOS runner funciona
direto (WindowServer existe); Windows runner também. Decisão adiada — fase 0 não precisa.

---

## 5. TMMath + testes dourados

### 5.1 O que são "testes dourados" (golden tests)

> **Golden test** = teste cujo valor esperado não é calculado nem escrito à mão, mas
> **capturado da implementação original, confiável** (aqui: o D3DX9 real da Microsoft,
> rodando no Windows) e congelado em disco. O código novo é comparado contra esse oráculo.

Por que isso importa tanto aqui:

1. **As fórmulas do D3DX têm convenções específicas** (row-major, LH, ordem de YawPitchRoll,
   slerp com threshold de linearização, IntersectTri sem culling e com convenção de u/v).
   Escrever "a fórmula certa" de memória erra detalhes que só aparecem como picking
   deslocado ou animação torta em runtime.
2. **Bit-exactness não é garantido nem desejado** — D3DX usa float32 com otimizações de
   2003. Aceitamos erro pequeno, mas queremos *saber* quanto é, e congelar esse erro.
3. Qualquer regressão futura no shim (alguém "otimizar" a multiplicação de matriz) explode
   no CI, não no jogo.

### 5.2 Como capturar os dourados (uma vez, no Windows)

`tests/golden/golden_generator.cpp` — um console app MSVC compilado **contra o D3DX9 real**
(mesma `Dependencies/Directx` do repo). Ele executa cada operação sobre um conjunto fixo de
entradas (seeds determinísticas) e despeja os resultados em binário:

```cpp
// golden_generator.cpp — compilar 1× no Windows:
//   cl /EHsc golden_generator.cpp /I Dependencies\Directx\Include d3dx9.lib
#include <d3dx9.h>
#include <cstdio>

static FILE* out;
static void W(const void* p, size_t n) { fwrite(p, 1, n, out); }

// Entradas determinísticas: LCG próprio, NADA de rand() (varia por CRT)
static uint32_t s_seed = 0x12345678;
static float frand() {  // [-10, 10], 6 casas significativas
    s_seed = s_seed * 1664525u + 1013904223u;
    return (float)(s_seed >> 8) / 8388608.0f - 10.0f;
}

int main() {
    out = fopen("matrix_ops.bin", "wb");
    for (int i = 0; i < 256; i++) {                     // 256 casos por operação
        D3DXMATRIX A, B, R;
        FillRandom(&A, frand); FillRandom(&B, frand);   // 16 floats cada
        D3DXMatrixMultiply(&R, &A, &B);
        W(&A, 64); W(&B, 64); W(&R, 64);                // input + output congelados
    }
    // ... idem: Translation, Scaling, YPR, LookAtLH, PerspectiveFovLH,
    //     Inverse (com determinante), Transpose, Vec3Transform(Coord),
    //     Normalize/Dot/Cross/Lerp, QuaternionSlerp/RotationMatrix...
    fclose(out);
}
```

Formato: **binário float32 cru, sem header** — simples, sem ambiguidade de texto
(printf de float perde precisão a menos que `%a`/9 dígitos; binário elimina a questão).
Os arquivos `.bin` são commitados em `tests/golden/data/` (~256 casos × ~20 ops ×
≈128B ≈ 600KB total — ok).

Também capturar **valores especiais** além dos aleatórios:
- identidade × aleatória, matrizes com translação grande (posições de mundo ~±20.000,
  como o mapa do jogo), ângulos 0/π/2π, quaternions quase opostos (threshold do slerp!),
  raio paralelo ao triângulo, raio perpendicular, raio pegando aresta (IntersectTri),
  LookAt com eye==at (degenerado), fov extremos, near/far do jogo (0.966, 70).

### 5.3 O harness de teste (lado multiplataforma)

`tests/test_tmmath.cpp` lê os `.bin`, roda o **nosso shim** nas mesmas entradas e compara:

```cpp
TEST(MatrixMultiply_MatchesD3DX) {
    auto cases = LoadGolden("matrix_ops.bin");   // [A(64B) B(64B) R(64B)] × 256
    float maxErr = 0;
    for (auto& c : cases) {
        D3DXMATRIX r;
        D3DXMatrixMultiply(&r, &c.A, &c.B);      // NOSSA implementação
        maxErr = std::max(maxErr, MaxAbsDiff(r, c.R));  // erro absoluto por elemento
    }
    EXPECT_LE(maxErr, TOLERANCE_MULT);           // ver política abaixo
}
```

**Política de tolerância** (documentar no cabeçalho do teste):

| Operação | Tolerância | Justificativa |
|---|---|---|
| Multiply, Translation, Scaling, YPR, Transpose | **0 ou 1 ULP** | fórmula pura mul+add; deve ser bit-exata ou quase |
| TransformCoord/Transform | 1e-6 relativo | tem divisão (w) |
| LookAtLH, Normalize | 1e-6 | tem `sqrt`/`rsqrt` — D3DX pode usar SSE rsqrt com refinamento |
| PerspectiveFovLH | 0 (exata) | tan/cot via CRT; se divergir, usar mesma fórmula |
| Inverse | 1e-4 relativo | caminho numérico sensível; o jogo usa 2× só |
| QuaternionSlerp | 1e-5 **+ checagem do threshold** | D3DX lineariza abaixo de dot≈0.95? Congelar o comportamento exato nos casos especiais |
| IntersectTri | **bool exato + u/v/dist 1e-5** | o *sinal* (hit/miss) tem de ser idêntico — picking depende |

Se um caso bit-exato falhar por 1 ULP, **não alargar a tolerância cegamente**: investigar
por quê (ordem de avaliação? FMA?) e documentar. O objetivo é conhecer o erro, não escondê-lo.

### 5.4 Harness próprio vs. GoogleTest

Decisão: **harness mínimo próprio** (~80 linhas: macros `TEST`/`EXPECT_LE`, main com
contadores, saída TAP-like). GoogleTest via FetchContent é a alternativa óbvia, mas o
harness próprio tem zero dependência de rede no CI e é didaticamente transparente.
Se a suíte crescer (fase 2+), migrar para GTest — o formato dos `.bin` não muda.

### 5.5 Checklist completo de funções do shim (escopo da fase 0)

Tipos: `D3DXVECTOR2/3/4`, `D3DXMATRIX` (com `._11.._44`), `D3DXQUATERNION`, `D3DXCOLOR`,
`D3DXMATRIXA16` (alignas(16) — usado em `CMesh.cpp`).

Operações (contagem = usos no código — todas precisam existir; as com ✦ têm golden test):

| Grupo | Funções |
|---|---|
| Matriz criar | `Identity`, `Translation`✦, `Scaling`✦, `RotationX/Y/Z`, `RotationAxis`, `RotationYawPitchRoll`✦ (ordem! yaw→pitch→roll = Y·X·Z no D3D), `RotationQuaternion`✦, `LookAtLH`✦, `PerspectiveFovLH`✦ |
| Matriz operar | `Multiply`✦, `Inverse`✦, `Transpose`✦, `MultiplyTranspose` |
| Vetor | `Vec3Transform`✦, `Vec3TransformCoord`✦, `Vec3Normalize`✦, `Vec3Dot`, `Vec3Cross`✦, `Vec3Length`, `Vec3Lerp`, `Vec2Normalize/Length`, `Vec3Project`✦ (viewport Y-down!) |
| Quaternion | `QuaternionSlerp`✦, `QuaternionIdentity`, `QuaternionRotationMatrix`✦ |
| Cor | `ColorModulate`, `ColorLerp` |
| Geom | `IntersectTri`✦ (Möller–Trumbore, **sem culling**, convenção u/v/dist do D3D) |
| Macro | `D3DXToRadian/D3DXToDegree` |

Total: ~40 funções, ~600 linhas. Estimativa: 2-3 dias com os testes.

**Armadilhas documentadas no shim** (comentários obrigatórios):
- `D3DXMatrixRotationYawPitchRoll(yaw, pitch, roll)`: aplica roll(Z), depois pitch(X),
  depois yaw(Y) — `R = Rz * Rx * Ry` na convenção row-major do D3DX.
- `D3DXVec3Project`: viewport Y cresce para baixo (D3D) — usado por `TMSun`, `TMObject`.
- `D3DXIntersectTri`: retorna hit para ambos os lados do triângulo; `dist` é ao longo do
  raio não-normalizado? (verificar no gerador — congelar o comportamento real).
- Slerp do D3DX: usa `sin(θ)/θ` com fallback linear quando |dot| próximo de 1 — o
  threshold exato sai do golden generator com casos de fronteira.

---

## 6. Stubs de plataforma

### 6.1 `Platform.h` — superfície mínima

```cpp
namespace tm {
    uint32_t GetTicks();                 // ms desde boot (substitui timeGetTime/GetTickCount)
    void     Log(const char* fmt, ...);  // TMLog usa fprintf + arquivo; manter interface
    FILE*    OpenAsset(const char* relPath, const char* mode);  // relativo ao dir do jogo
    bool     FileExists(const char* relPath);
    int64_t  FileSize(const char* relPath);
    std::string ExeDir();                // macOS: bundle/Contents/Resources ou cwd?
}
```

Decisão de assets: o jogo lê paths relativos (`Mesh\...`, `UI\...`) com backslash.
`OpenAsset` traduz `\`→`/` e resolve contra o diretório de dados (cwd em dev; no macOS
`.app`, `Contents/Resources/data`). Nada de `std::filesystem` no código de gameplay —
só dentro de `PlatformFile.cpp`.

### 6.2 `CPSock` → BSD sockets (quase drop-in)

Diff real necessário (`CPSock.cpp:79-184`):

```cpp
// platform/compat incluído no topo:
#ifdef _WIN32
  // mantém original
#else
  #include <sys/socket.h> <netinet/in.h> <arpa/inet.h> <netdb.h> <unistd.h> <fcntl.h> <errno.h>
  #define closesocket close
  #define SOCKET_ERROR (-1)
  #define INVALID_SOCKET (-1)
  using SOCKET = int;
  static int WSAGetLastError() { return errno; }
  // WSAStartup/Cleanup viram no-op; ioctlsocket(FIONBIO) → fcntl(O_NONBLOCK)
#endif
```

`socket(2,1,0)` (AF_INET/SOCK_STREAM/IPPROTO_TCP numéricos) funciona igual nos dois.
Teste: echo server local em thread, conectar, enviar pacote `MSG_STANDARD` fake, comparar.

### 6.3 O que NÃO entra na fase 0

Áudio, IME, vídeo, curl, cursor, gamma. Cada um vira um stub de uma linha quando o código
que os chama for portado (fases 5-6). Não antecipar — stub cedo demais vira interface errada.

---

## 7. Bootstrap `main.cpp` (fase 0 — versão mínima)

Igual ao de `03-setup-build.md`, mas o loop da fase 0 só faz: clear com cor variando no
tempo (prova vsync+swap), ESC para sair, resize handling, e log das caps
(`GL_VERSION`, `GL_RENDERER`, extensões S3TC/aniso presentes?). Esse log vira o
`boot_report.txt` — artefato de CI para provar que o contexto 4.1 subiu de verdade.

Em CI headless não abrimos janela; o job de CI roda só build + ctest. O boot GL é validado
localmente e, opcionalmente, com `xvfb-run` num job futuro.

---

## 8. Cronograma detalhado (10 dias úteis)

| Dia | Entrega |
|---|---|
| 1 | Estrutura de dirs, CMake raiz configurando nos 3 OS (sem SDL ainda), CI rodando build vazio |
| 2 | SDL3 via FetchContent + glad vendored + `main.cpp` abrindo janela localmente (mac+linux) |
| 3 | CI matrix verde nos 3 OS com o executável; `boot_report.txt` como artefato |
| 4-5 | `TMMath.h`: tipos + todas as ~40 funções, sem otimizar, comentários de convenção |
| 6 | `golden_generator.cpp` no Windows (mesma Directx do repo) + geração dos `.bin` + casos especiais |
| 7 | Harness de teste + `test_tmmath.cpp` comparando tudo; investigar divergências >1 ULP |
| 8 | `Platform*` (tempo, arquivo, log) + `CPSock` port + teste de echo |
| 9 | Buffer para divergências do shim (sempre há 1-2: slerp threshold e IntersectTri são os suspeitos) |
| 10 | Definition of done revisado; merge; abrir issues da fase 1 |

## 9. Riscos da fase

| Risco | Mitigação |
|---|---|
| Shim com erro sutil que só aparece in-game | golden tests com casos especiais (slerp, intersect, world-scale grande) |
| SDL3 mudando API (é jovem) | pin de tag exata no FetchContent; upgrade é decisão explícita |
| Windows/MSVC divergindo de clang/gcc em warnings | warnings por compilador já no CMake desde o dia 1 |
| "Só mais um stub" virar porta de modernização prematura | fase 0 não toca em `Projects/` — regra de ouro |
