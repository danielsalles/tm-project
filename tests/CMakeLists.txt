add_executable(tm_tests
  test_main.cpp
  test_tmmath.cpp
  test_platform.cpp
  test_cpsock.cpp)
target_link_libraries(tm_tests PRIVATE tmcore)

# dados dourados acessíveis por path relativo ao diretório do teste
target_compile_definitions(tm_tests PRIVATE
  TM_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/data")

add_test(NAME tmmath    COMMAND tm_tests tmmath)
add_test(NAME platform  COMMAND tm_tests platform)
add_test(NAME cpsock    COMMAND tm_tests cpsock)
