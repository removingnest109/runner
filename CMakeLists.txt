cmake_minimum_required(VERSION 3.20)
project(runner VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(GNUInstallDirs)

find_package(Threads REQUIRED)

option(RUNNER_BUILD_TESTS "Build the doctest suite" ON)

# Allow deps whose cmake_minimum_required predates CMake 4.x to configure,
# and silence their bundled-CMake deprecation notices (our own CMake is modern).
# Still needed: FTXUI is built from source even when packaged (see below).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress upstream dependency CMake deprecation notices" FORCE)

# Dependencies: prefer a system-provided package (distro builds, offline
# sandboxes), fall back to building from source via FetchContent (frictionless
# dev checkout with nothing installed).

find_package(tomlplusplus 3.4.0 QUIET)
if(NOT tomlplusplus_FOUND)
  FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0)
  FetchContent_MakeAvailable(tomlplusplus)
endif()

# FTXUI is not commonly packaged; when it is missing FetchContent builds it.
# In the offline Void build the template sets FETCHCONTENT_SOURCE_DIR_FTXUI +
# FETCHCONTENT_FULLY_DISCONNECTED so this uses a vendored local tree, no network.
# FTXUI is statically linked into runner, so suppress its own install rules
# (headers/libs/cmake config) to keep our install tree to just the runner binary.
# runner needs the FTXUI 6.0 selection API (screen.GetSelection()). It builds
# against both 6.x and 7.x, so we can't pin a single major with find_package's
# SameMajorVersion rule — query unversioned and enforce a 6.0 floor by hand.
find_package(ftxui QUIET)
if(ftxui_FOUND)
  if(ftxui_VERSION VERSION_LESS 6.0.0)
    message(FATAL_ERROR
      "runner requires FTXUI >= 6.0.0 (text selection API); found ${ftxui_VERSION}")
  endif()
else()
  set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9)
  FetchContent_MakeAvailable(ftxui)
endif()

# Generated version header (RUNNER_VERSION), consumed by src/cli.cpp.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp
  @ONLY)

# Library of all non-main sources.
add_library(runner_lib STATIC
  src/config.cpp
  src/plan.cpp
  src/ansi_parser.cpp
  src/process_runner.cpp
  src/cli.cpp
  src/clipboard.cpp
  src/app.cpp
)
target_include_directories(runner_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(runner_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(runner_lib PUBLIC
  tomlplusplus::tomlplusplus
  Threads::Threads
  ftxui::screen ftxui::dom ftxui::component)
set_target_properties(runner_lib PROPERTIES LINKER_LANGUAGE CXX)

if(RUNNER_BUILD_TESTS)
  find_package(doctest 2.4.11 QUIET)
  if(NOT doctest_FOUND)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)  # test-only; don't install it
    FetchContent_Declare(doctest
      GIT_REPOSITORY https://github.com/doctest/doctest.git
      GIT_TAG v2.4.11)
    FetchContent_MakeAvailable(doctest)
  endif()

  add_executable(runner_tests
    tests/test_main.cpp
    tests/test_smoke.cpp
    tests/test_config_parse.cpp
    tests/test_config_load.cpp
    tests/test_plan.cpp
    tests/test_ansi_parser.cpp
    tests/test_process_runner.cpp
    tests/test_clipboard.cpp
    tests/test_cli.cpp
  )
  target_link_libraries(runner_tests PRIVATE runner_lib doctest::doctest)

  enable_testing()
  add_test(NAME runner_tests COMMAND runner_tests)
endif()

add_executable(runner src/main.cpp)
target_link_libraries(runner PRIVATE runner_lib)

# Install rules (GNUInstallDirs). runner_lib / runner_tests / vendored FTXUI are
# not installed (static-linked / test-only). The LICENSE is installed by the
# distro package (Void: vlicense), not here, to respect distro conventions.
install(TARGETS runner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES docs/runner.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES runner.toml
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME runner.toml.example)
