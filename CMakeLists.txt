cmake_minimum_required(VERSION 3.20)

project(abys VERSION 0.1.0 LANGUAGES CXX)

option(ABYS_ENABLE_TESTS "Build abys tests" ON)
option(ABYS_ENABLE_COVERAGE "Enable coverage flags" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ABYS_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  else()
    message(WARNING "Coverage flags requested but compiler does not support gcov")
  endif()
endif()

set(ABYS_CORE_SOURCES
  src/version.cpp
  src/frontend_slang.cpp
  src/ir/tig_builder.cpp
)
find_package(slang CONFIG REQUIRED)

add_library(abys_core ${ABYS_CORE_SOURCES})

target_include_directories(abys_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(abys_core PUBLIC cxx_std_17)

target_compile_options(abys_core PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(abys_core PRIVATE slang::slang)
target_compile_definitions(abys_core PRIVATE ABYS_HAVE_SLANG=1)

add_executable(abys src/main.cpp)

target_link_libraries(abys PRIVATE abys_core)

if(ABYS_ENABLE_TESTS)
  enable_testing()
  add_executable(abys_smoke tests/smoke.cpp)
  target_link_libraries(abys_smoke PRIVATE abys_core)
  add_test(NAME abys_smoke COMMAND abys_smoke)
endif()
