add_executable(tm_tests
  test_main.cpp
  test_tmmath.cpp
  test_platform.cpp
  test_cpsock.cpp
  test_msa.cpp
  test_wys.cpp
  test_objectfile.cpp
  test_trn.cpp
  test_envtex.cpp
  test_msh.cpp
  test_picking.cpp
  test_anisound.cpp
  test_charani.cpp
  test_look.cpp
  test_route.cpp
  test_character.cpp
  test_fxbillboard.cpp
  test_fxlamp.cpp
  test_sunflare.cpp
  test_weatherfx.cpp
  test_critter.cpp
  test_charpick.cpp
  test_spawneffect.cpp
  test_skillmesh.cpp
  test_swing.cpp
  test_skills_golden.cpp
  test_ui.cpp
  test_audio.cpp
  test_config.cpp
  test_http.cpp
  test_input.cpp)
target_link_libraries(tm_tests PRIVATE tmcore tmgl)

# dados dourados acessíveis por path relativo ao diretório do teste
target_compile_definitions(tm_tests PRIVATE
  TM_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/data"
  TM_REPO_ROOT="${CMAKE_SOURCE_DIR}")

# GL smoke test: binário próprio (precisa de janela/contexto; skip gracioso sem GL 4.1)
add_executable(tm_glsmoke test_glsmoke.cpp)
target_link_libraries(tm_glsmoke PRIVATE tmcore tmgl glad)

add_test(NAME tmmath     COMMAND tm_tests tmmath)
add_test(NAME platform   COMMAND tm_tests platform)
add_test(NAME cpsock     COMMAND tm_tests cpsock)
add_test(NAME msa        COMMAND tm_tests msa)
add_test(NAME wys        COMMAND tm_tests wys)
add_test(NAME objectfile COMMAND tm_tests objectfile)
add_test(NAME trn        COMMAND tm_tests trn)
add_test(NAME envtex     COMMAND tm_tests envtex)
add_test(NAME msh        COMMAND tm_tests msh)
add_test(NAME picking    COMMAND tm_tests picking)
add_test(NAME anisound   COMMAND tm_tests anisound)
add_test(NAME charani    COMMAND tm_tests charani)
add_test(NAME look       COMMAND tm_tests look)
add_test(NAME route      COMMAND tm_tests route)
add_test(NAME character  COMMAND tm_tests character)
add_test(NAME fxbillboard COMMAND tm_tests fxbillboard)
add_test(NAME fxlamp     COMMAND tm_tests fxlamp)
add_test(NAME sunflare   COMMAND tm_tests sunflare)
add_test(NAME weatherfx  COMMAND tm_tests weatherfx)
add_test(NAME critter    COMMAND tm_tests critter)
add_test(NAME charpick   COMMAND tm_tests charpick)
add_test(NAME spawneffect COMMAND tm_tests spawneffect)
add_test(NAME skillmesh  COMMAND tm_tests skillmesh)
add_test(NAME swing     COMMAND tm_tests swing)
add_test(NAME skills    COMMAND tm_tests skills)
add_test(NAME ui        COMMAND tm_tests ui)
add_test(NAME audio     COMMAND tm_tests audio)
add_test(NAME config    COMMAND tm_tests config)
add_test(NAME http      COMMAND tm_tests http)
add_test(NAME input     COMMAND tm_tests input)
add_test(NAME glsmoke    COMMAND tm_glsmoke)
