# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# Test-only helpers, built as a normal module rather than as part of the engine's own test
# executable so that a downstream game can link them with TCA_ENGINE_BUILD_TESTS=OFF. Deliberately
# framework-agnostic: nothing here pulls in Catch2 (or any other test framework), so consumers
# stay free to pick their own.
#
# This module is intentionally *not* linked into the aggregate tca-engine target: a game that
# links the engine should not end up with test scaffolding in its shipping binary, it opts into
# TCAEngine::testing from its test targets only.

set(TCA_ENGINE_TESTING_MODULE_HEADER_FILES
    scoped-temp-directory.hpp

    imgui/headless-imgui-context.hpp
)
set(TCA_ENGINE_TESTING_MODULE_SOURCE_FILES
    imgui/headless-imgui-context.cpp
)

if(WIN32)
    list(APPEND TCA_ENGINE_TESTING_MODULE_HEADER_FILES win32/windowing-utilities.hpp)
    list(APPEND TCA_ENGINE_TESTING_MODULE_SOURCE_FILES win32/windowing-utilities.cpp)
endif()

if(UNIX AND NOT APPLE)
    list(APPEND TCA_ENGINE_TESTING_MODULE_HEADER_FILES linux/windowing-utilities.hpp)
    list(APPEND TCA_ENGINE_TESTING_MODULE_SOURCE_FILES linux/windowing-utilities.cpp)
endif()

add_library(tca-engine-testing STATIC
    ${TCA_ENGINE_TESTING_MODULE_HEADER_FILES}
    ${TCA_ENGINE_TESTING_MODULE_SOURCE_FILES}
)
add_library(TCAEngine::testing ALIAS tca-engine-testing)

target_include_directories(tca-engine-testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-testing PUBLIC
    tca-engine
    imgui
)

target_link_libraries(tca-engine-testing PRIVATE TCAEngine::compile_options)

# The engine sets CMAKE_CXX_STANDARD for its own directory scope, which does not reach a consumer's
# targets. Its headers need that standard to compile, so the requirement travels with the target.
target_compile_features(tca-engine-testing PUBLIC cxx_std_${TCA_ENGINE_CXX_STANDARD})
