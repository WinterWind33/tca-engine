# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

option(TCA_ENGINE_BUILD_WEB_MODULE "If ON the engine will come with the WEB module compiled" ON)
option(TCA_ENGINE_BUILD_SETTINGS_MODULE "If ON the engine will come with the Settings module compiled" ON)

# Test-only helpers a downstream game can link from its own test targets. Defaults to following
# TCA_ENGINE_BUILD_TESTS, since that is what the engine's own suite needs, but the two are
# independent on purpose: a game consuming the engine as a subdirectory turns the engine's tests
# off and still wants this.
option(TCA_ENGINE_BUILD_TESTING_SUPPORT
    "If ON the engine will come with the testing support module compiled"
    ${TCA_ENGINE_BUILD_TESTS})

if(TCA_ENGINE_BUILD_TESTS AND NOT TCA_ENGINE_BUILD_TESTING_SUPPORT)
    message(FATAL_ERROR
        "TCA_ENGINE_BUILD_TESTS=ON requires TCA_ENGINE_BUILD_TESTING_SUPPORT=ON: the engine's own "
        "test suite is built on top of the testing support module.")
endif()

# Every sub folder below is its own CMake module (its own STATIC library target), so that each one
# can be reused, tested and (where relevant, e.g. the web module) opted out independently. This
# target only aggregates all of them behind the single tca-engine name consumers link against.
add_subdirectory(core)
add_subdirectory(input)
add_subdirectory(platform)
add_subdirectory(rendering)
add_subdirectory(application)
add_subdirectory(sounds)

add_library(tca-engine INTERFACE)
target_link_libraries(tca-engine INTERFACE
    tca-engine-core
    tca-engine-input
    tca-engine-platform
    tca-engine-rendering
    tca-engine-application
    tca-engine-sounds
    verso
)

if(TCA_ENGINE_BUILD_WEB_MODULE)
    add_subdirectory(web)

    target_link_libraries(tca-engine INTERFACE
        tca-engine-web
    )
endif()

if(TCA_ENGINE_BUILD_SETTINGS_MODULE)
    add_subdirectory(settings)
    # Lives with input, but needs the settings target to exist first.
    add_subdirectory(input/settings)

    target_link_libraries(tca-engine INTERFACE
        tca-engine-settings
        tca-engine-input-settings
    )
endif()

# Added after the aggregate target above because it links against it. Kept *out* of that target on
# purpose: test scaffolding has no business in a game's shipping binary, so consumers link
# TCAEngine::testing explicitly from their test targets instead.
if(TCA_ENGINE_BUILD_TESTING_SUPPORT)
    add_subdirectory(testing)
endif()

target_link_libraries(tca-engine INTERFACE TCAEngine::compile_options)

# CMAKE_CXX_STANDARD is a directory-scoped variable, so it configures the engine's own targets but
# not those of a project that add_subdirectory()s the engine. The public headers do not compile
# below this standard, so the requirement is carried by the target itself.
target_compile_features(tca-engine INTERFACE cxx_std_${TCA_ENGINE_CXX_STANDARD})
