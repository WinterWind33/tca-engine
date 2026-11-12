# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.24)

project(TCAEngine
    VERSION 0.24.0 # x-release-please-version
    DESCRIPTION "A C++ game/application engine"
    LANGUAGES CXX C
)

# ---------------------------------------------------------------------------
# Options and cache variables (all prefixed with TCA_ENGINE_)
# ---------------------------------------------------------------------------

set(TCA_ENGINE_CXX_STANDARD 23 CACHE STRING "C++ standard used to build TCAEngine")
set_property(CACHE TCA_ENGINE_CXX_STANDARD PROPERTY STRINGS 17 20 23 26)

option(TCA_ENGINE_BUILD_TESTS "Build the TCAEngine test suite" ${PROJECT_IS_TOP_LEVEL})

# The example applications (examples/) are demos of the engine's public API, not part of the
# engine, so they stay off unless someone asks for them. Turned on in .vscode/settings.json and in
# CI, which is where they earn their keep: they are the cheapest check that the engine stays
# usable from outside its own sources.
option(TCA_ENGINE_BUILD_EXAMPLES "Build the TCAEngine example applications" OFF)
option(TCA_ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(TCA_ENGINE_USE_UNICODE "Build with Unicode support" ON)

# ---------------------------------------------------------------------------
# Language configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD ${TCA_ENGINE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---------------------------------------------------------------------------
# Versioning
# ---------------------------------------------------------------------------

set(TCA_ENGINE_VERSION       ${PROJECT_VERSION})
set(TCA_ENGINE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(TCA_ENGINE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(TCA_ENGINE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# ---------------------------------------------------------------------------
# Output directories: <root>/bin for executables, <root>/lib for libraries
# ---------------------------------------------------------------------------

set(TCA_ENGINE_BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Runtime output directory")
set(TCA_ENGINE_LIB_DIR "${CMAKE_SOURCE_DIR}/lib" CACHE PATH "Library and archive output directory")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TCA_ENGINE_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TCA_ENGINE_LIB_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${TCA_ENGINE_LIB_DIR}")

# The per-configuration CMAKE_<KIND>_OUTPUT_DIRECTORY_<CONFIG> variables are deliberately left unset:
# multi-config generators (Visual Studio, Ninja Multi-Config) then append a Debug/, Release/, ...
# subdirectory on their own. Pinning every configuration to the same path made them overwrite each
# other, so a Debug build could link the Release archives an earlier build left behind.

# ---------------------------------------------------------------------------
# Shared compile settings, consumed via target_link_libraries(<t> PRIVATE ...)
# ---------------------------------------------------------------------------

add_library(TCA_ENGINE_compile_options INTERFACE)
add_library(TCAEngine::compile_options ALIAS TCA_ENGINE_compile_options)

if(MSVC)
    target_compile_options(TCA_ENGINE_compile_options INTERFACE /W4 /permissive- /utf-8)
    if(TCA_ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(TCA_ENGINE_compile_options INTERFACE /WX)
    endif()
else()
    target_compile_options(TCA_ENGINE_compile_options INTERFACE -Wall -Wextra -Wpedantic)
    if(TCA_ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(TCA_ENGINE_compile_options INTERFACE -Werror)
    endif()
endif()

# Clang warns whenever a designated initializer list names some fields but not all of them. The
# Win32 DirectX 12 sources rely on exactly that: the large D3D12_* descriptor structs name the
# fields that matter and leave the rest value-initialized, which is what those APIs expect.
# Spelling out every remaining field just to write a zero would make the call sites longer and
# harder to read for no change in behaviour, so the diagnostic is turned off rather than obeyed.
#
# Detected by feature test rather than by compiler id: the warning is Clang-only today (GCC does
# not diagnose designated initializers this way and MSVC has no equivalent), but the negated form
# is the right thing to pass on any compiler that grows it. The *positive* spelling is what gets
# probed, because Clang silently accepts unknown -Wno-* options and would report support for
# anything asked of it.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-Wmissing-designated-field-initializers"
    TCA_ENGINE_HAS_WMISSING_DESIGNATED_FIELD_INITIALIZERS)
if(TCA_ENGINE_HAS_WMISSING_DESIGNATED_FIELD_INITIALIZERS)
    target_compile_options(TCA_ENGINE_compile_options
        INTERFACE -Wno-missing-designated-field-initializers)
endif()

# COM code cannot avoid __uuidof: the Windows SDK's IID_PPV_ARGS macro, used at every DirectX 12
# creation call, expands to it. Clang with its GNU-style driver (not clang-cl, which takes the MSVC
# branch above) flags that Microsoft keyword under -Wpedantic at each expansion site in our own
# sources, so the diagnostic is turned off on Windows. Probed the same way as above.
if(WIN32 AND NOT MSVC)
    check_cxx_compiler_flag("-Wlanguage-extension-token"
        TCA_ENGINE_HAS_WLANGUAGE_EXTENSION_TOKEN)
    if(TCA_ENGINE_HAS_WLANGUAGE_EXTENSION_TOKEN)
        target_compile_options(TCA_ENGINE_compile_options
            INTERFACE -Wno-language-extension-token)
    endif()
endif()

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------

find_package(Microsoft.GSL 4.2.2 CONFIG REQUIRED)
find_package(spdlog 1.17.0 CONFIG REQUIRED)
find_package(Eigen3 5.0.1 CONFIG REQUIRED)
find_package(nlohmann_json 3.12.0 CONFIG REQUIRED)

if(TCA_ENGINE_BUILD_TESTS)
    find_package(Catch2 3.15.1 CONFIG REQUIRED)
endif()

if(WIN32)
    find_package(wil CONFIG REQUIRED)
endif()

if(LINUX AND NOT APPLE)
  find_package(glfw3 3.5.1 CONFIG REQUIRED)
endif()


# A handful of libraries either lack a proper CONFIG package (imgui, miniaudio,
# imgui-markdown-ext, stb) or need patching around their own CMake warnings (verso); those are
# fetched directly from their git repositories at configure time instead.
add_subdirectory(third-party)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------

add_subdirectory(src/tca-engine)

if(TCA_ENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(TCA_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "TCAEngine ${TCA_ENGINE_VERSION} | C++${TCA_ENGINE_CXX_STANDARD} | tests: ${TCA_ENGINE_BUILD_TESTS} | examples: ${TCA_ENGINE_BUILD_EXAMPLES}")
