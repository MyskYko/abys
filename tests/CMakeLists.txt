add_executable(abys_tests
  frontend/continuous_assignment_test.cpp
  frontend/flip_flop_test.cpp
  third_party/catch2/catch_amalgamated.cpp
  version_test.cpp
)

target_include_directories(abys_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} third_party/catch2)
target_link_libraries(abys_tests PRIVATE abys_core)

add_test(NAME abys_tests COMMAND abys_tests)
