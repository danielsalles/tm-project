cmake_minimum_required(VERSION 3.24)
project(TMProject LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TM_BUILD_TESTS "Build tests" ON)

include(FetchContent)

# --- SDL3 (estático, pinned) ---
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.16
  GIT_SHALLOW    TRUE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# --- glm (código novo apenas; legado usa TMMath) ---
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glm)

# --- glad (gerado offline e commitado) ---
add_library(glad STATIC extern/glad/src/gl.c)
target_include_directories(glad PUBLIC extern/glad/include)

# --- stb (header-only) ---
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE extern/stb)

# --- tmcore: platform + math + net (fase 0) ---
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

# --- executável ---
add_executable(TMProject src/platform/main.cpp)
target_link_libraries(TMProject PRIVATE tmcore glad glm::glm stb)

# --- testes ---
if(TM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
