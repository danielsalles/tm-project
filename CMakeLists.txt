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

# --- shaders GLSL embedded (regenerado quando um .glsl muda) ---
set(TM_SHADER_FILES
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/common.glsl
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/mesh_lit.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/mesh_lit.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/terrain.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/terrain.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/sky.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/sky.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/sea.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/sea.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/skin.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/skin.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_quad.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_quad.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_skillmesh.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_skillmesh.frag
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_decal.vert
  ${CMAKE_SOURCE_DIR}/src/gl/shaders/fx_decal.frag)
set(TM_SHADERS_H ${CMAKE_BINARY_DIR}/generated/shaders_embedded.h)
add_custom_command(
  OUTPUT ${TM_SHADERS_H}
  COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${TM_SHADERS_H}"
          "-DFILES_DIR=${CMAKE_SOURCE_DIR}/src/gl/shaders"
          -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake
  DEPENDS ${TM_SHADER_FILES} ${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake
  COMMENT "Embedding GLSL shaders")

# --- tmgl: renderer + loaders + cena (fases 1-2) ---
add_library(tmgl STATIC
  ${TM_SHADERS_H}
  src/gl/GLShader.cpp
  src/gl/GLStateCache.cpp
  src/gl/GLRenderDevice.cpp
  src/gl/GLMesh.cpp
  src/gl/GLTexture.cpp
  src/gl/SkinPipeline.cpp
  src/gl/EffectRenderer.cpp
  src/gl/SkillMeshRenderer.cpp
  src/gl/GroundDecalRenderer.cpp
  src/gl/UIBatcher.cpp
  src/gl/UIRenderer.cpp
  src/gl/GLFont.cpp
  src/ui/SControl.cpp
  src/ui/SControls.cpp
  src/ui/SControlContainer.cpp
  src/ui/UILoader.cpp
  src/ui/TMFont3.cpp
  src/ui/RenderGeomControl.cpp
  src/world/TerrainData.cpp
  src/world/TerrainRenderer.cpp
  src/world/SkyDome.cpp
  src/world/SeaSurface.cpp
  src/world/SkinMesh.cpp
  src/world/AniSound.cpp
  src/world/CharacterAnimation.cpp
  src/world/LookResolver.cpp
  src/world/CharacterMesh.cpp
  src/world/Route.cpp
  src/world/Character.cpp
  src/world/Billboard.cpp
  src/world/LampFx.cpp
  src/world/SunFlare.cpp
  src/world/WeatherFx.cpp
  src/world/Critter.cpp
  src/world/BoneAnimation.cpp
  src/world/TreeRenderer.cpp
  src/world/SkillEffect.cpp
  src/world/SkillFx.cpp
  src/world/SwingTrail.cpp
  src/scene/ObjectFile.cpp
  src/scene/SelectServerView.cpp
  src/scene/FieldView.cpp)
target_include_directories(tmgl PUBLIC src src/ui ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(tmgl PUBLIC tmcore glad stb)
target_compile_options(tmgl PRIVATE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W3>)

# --- executável ---
add_executable(TMProject src/platform/main.cpp)
target_link_libraries(TMProject PRIVATE tmcore tmgl glad glm::glm stb)

# --- testes ---
if(TM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
