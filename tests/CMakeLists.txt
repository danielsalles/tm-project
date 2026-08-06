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
  test_msh.cpp)
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
add_test(NAME glsmoke    COMMAND tm_glsmoke)
