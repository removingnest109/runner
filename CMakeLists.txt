cmake_minimum_required(VERSION 3.20)
project(runner CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Allow deps whose cmake_minimum_required predates CMake 4.x to configure,
# and silence their bundled-CMake deprecation notices (our own CMake is modern).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress upstream dependency CMake deprecation notices" FORCE)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

# Library of all non-main sources (sources appended by later tasks).
add_library(runner_lib STATIC
  src/placeholder.cpp
)
target_include_directories(runner_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(runner_lib PROPERTIES LINKER_LANGUAGE CXX)

# Test executable.
add_executable(runner_tests
  tests/test_main.cpp
  tests/test_smoke.cpp
)
target_link_libraries(runner_tests PRIVATE runner_lib doctest::doctest)

enable_testing()
add_test(NAME runner_tests COMMAND runner_tests)
