# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# Directories
set(TCA_ENGINE_TESTS_APPLICATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/application)
set(TCA_ENGINE_TESTS_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(TCA_ENGINE_TESTS_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/input)
set(TCA_ENGINE_TESTS_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform)
set(TCA_ENGINE_TESTS_TERMINAL_DIR ${TCA_ENGINE_TESTS_PLATFORM_DIR}/terminal)
set(TCA_ENGINE_TESTS_RENDERING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rendering)
set(TCA_ENGINE_TESTS_THEMING_DIR ${TCA_ENGINE_TESTS_RENDERING_DIR}/theming)
set(TCA_ENGINE_TESTS_SCENE_DIR ${TCA_ENGINE_TESTS_RENDERING_DIR}/scene)
set(TCA_ENGINE_TESTS_SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/settings)
set(TCA_ENGINE_TESTS_SOUNDS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sounds)
set(TCA_ENGINE_TESTS_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web)

set(TCA_ENGINE_TEST_DOUBLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test-doubles)
set(TCA_ENGINE_TESTS_TESTING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/testing)

set(TCA_ENGINE_TESTS_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/utilities/shared-log-sink.cpp

    ${TCA_ENGINE_TESTS_APPLICATION_DIR}/application.tests.cpp
    ${TCA_ENGINE_TESTS_APPLICATION_DIR}/application-logger.tests.cpp
    ${TCA_ENGINE_TESTS_APPLICATION_DIR}/game-application-logic.tests.cpp
    ${TCA_ENGINE_TESTS_APPLICATION_DIR}/simple-console-application.tests.cpp

    ${TCA_ENGINE_TESTS_CORE_DIR}/events.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/fsm.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/logging/log-header.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/logging/loggers.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/logging/logging.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/math/math.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/math/size.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/math/vector.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/utilities/non-zero-values.tests.cpp
    ${TCA_ENGINE_TESTS_CORE_DIR}/utilities/ranged-value.tests.cpp

    ${TCA_ENGINE_TESTS_PLATFORM_DIR}/atomic-file-write.tests.cpp
    ${TCA_ENGINE_TESTS_PLATFORM_DIR}/folders.tests.cpp
    ${TCA_ENGINE_TESTS_PLATFORM_DIR}/image-decoder.tests.cpp
    ${TCA_ENGINE_TESTS_PLATFORM_DIR}/platform-logger.tests.cpp
    ${TCA_ENGINE_TESTS_PLATFORM_DIR}/system-information.tests.cpp

    ${TCA_ENGINE_TESTS_TERMINAL_DIR}/tokenizer.tests.cpp
    ${TCA_ENGINE_TESTS_TERMINAL_DIR}/argument-parser.tests.cpp
    ${TCA_ENGINE_TESTS_TERMINAL_DIR}/option-description.tests.cpp
    ${TCA_ENGINE_TESTS_TERMINAL_DIR}/command-description.tests.cpp
    ${TCA_ENGINE_TESTS_TERMINAL_DIR}/command-validator.tests.cpp

    ${TCA_ENGINE_TESTS_RENDERING_DIR}/colors.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/color-conversions.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gpu-texture.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/image-factory.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/image-color-sampling.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/rendering-queues/image-render-queue-base.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/custom-tickables/fixed-step-tickables.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/callback-renderable.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/imgui/imgui-color-helpers.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/imgui/imgui-math-helpers.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/imgui/imgui-theme.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/label.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/layout-widgets.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/gui/scroll-view.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/rendering-logger.tests.cpp
    ${TCA_ENGINE_TESTS_RENDERING_DIR}/tickables.tests.cpp

    ${TCA_ENGINE_TESTS_THEMING_DIR}/theme.tests.cpp
    ${TCA_ENGINE_TESTS_THEMING_DIR}/theme-factories.tests.cpp
    ${TCA_ENGINE_TESTS_THEMING_DIR}/active-theme.tests.cpp

    ${TCA_ENGINE_TESTS_SCENE_DIR}/camera-2d.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/scene-blending.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/scene-grid.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/scene-surface.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/scene-sprites.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/scene-virtual-resolution.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/texture-atlas.tests.cpp
    ${TCA_ENGINE_TESTS_SCENE_DIR}/virtual-resolution.tests.cpp

    ${TCA_ENGINE_TESTS_SOUNDS_DIR}/sounds.tests.cpp

    ${TCA_ENGINE_TEST_DOUBLES_DIR}/application/application-logic.test-double.cpp
    ${TCA_ENGINE_TEST_DOUBLES_DIR}/rendering/rendering-queues/image-render-queue.test-double.cpp

    ${TCA_ENGINE_TESTS_TESTING_DIR}/headless-imgui-context.tests.cpp
    ${TCA_ENGINE_TESTS_TESTING_DIR}/scoped-temp-directory.tests.cpp
)

if(TCA_ENGINE_BUILD_SETTINGS_MODULE)
    list(APPEND TCA_ENGINE_TESTS_SOURCE_FILES
        ${TCA_ENGINE_TESTS_SETTINGS_DIR}/settings-schema.tests.cpp
        ${TCA_ENGINE_TESTS_SETTINGS_DIR}/settings-io.tests.cpp
        ${TCA_ENGINE_TESTS_SETTINGS_DIR}/settings-json-formatter.tests.cpp
        ${TCA_ENGINE_TESTS_SETTINGS_DIR}/settings-location.tests.cpp

        ${TCA_ENGINE_TESTS_INPUT_DIR}/action-map-settings.tests.cpp
    )
endif()

if(TCA_ENGINE_BUILD_WEB_MODULE)
    list(APPEND TCA_ENGINE_TESTS_SOURCE_FILES
        ${TCA_ENGINE_TESTS_WEB_DIR}/web-logger.tests.cpp
        ${TCA_ENGINE_TESTS_WEB_DIR}/gui/web-view.tests.cpp
    )
endif()

if(WIN32)
    list(APPEND TCA_ENGINE_TESTS_SOURCE_FILES
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/win32/directx12-application.tests.cpp
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/win32/imgui-dx12-application.tests.cpp
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/win32/imgui-opengl3-win32-application.tests.cpp
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/win32/win32-application.tests.cpp
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/win32/win32-application-controller.tests.cpp

        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/win32-application-window.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/win32-application-window-input.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/win32-application-window-mouse.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/win32-application-window-move.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/win32-keyboard.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/win32/windows-type-traits.tests.cpp

        ${TCA_ENGINE_TESTS_RENDERING_DIR}/render-targets/win32/imgui-win32-window.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/directx-12/descriptor-heap-allocator.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/directx-12/directx12-rendering-manager.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/directx-12/dx12-image-factory.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/directx-12/dx12-image-render-queue.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/opengl3/opengl3-image-render-queue.tests.cpp
        ${TCA_ENGINE_TESTS_RENDERING_DIR}/win32/opengl3/opengl3-rendering-manager.tests.cpp
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND TCA_ENGINE_TESTS_SOURCE_FILES
        ${TCA_ENGINE_TESTS_APPLICATION_DIR}/linux/linux-application-controller.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/linux/linux-application-window.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/linux/linux-application-window-input.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/linux/linux-application-window-events.tests.cpp
        ${TCA_ENGINE_TESTS_PLATFORM_DIR}/linux/linux-keyboard.tests.cpp
    )
endif()

add_executable(tca-engine_tests ${TCA_ENGINE_TESTS_SOURCE_FILES})

target_include_directories(tca-engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tca-engine_tests PRIVATE
    tca-engine
    # The reusable test helpers (headless ImGui context, scoped temp directory, Win32 windowing
    # utilities) live in the engine's testing support module rather than here, so that downstream
    # games can link them too. See src/tca-engine/testing/.
    TCAEngine::testing
    Catch2::Catch2WithMain
)

target_link_libraries(tca-engine_tests PRIVATE TCAEngine::compile_options)

# Binary test assets (e.g. sound files) are copied next to the test executable after each build,
# since tests load them via a path relative to the test executable's working directory (see
# tests/utilities/test-assets.hpp).
add_custom_command(TARGET tca-engine_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:tca-engine_tests>/res
)

include(CTest)
include(Catch)
catch_discover_tests(tca-engine_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:tca-engine_tests>
)

# The shared log sink is created once per process, and tca-engine_tests always creates it before a
# test could name a log folder. The tests that need a fresh process get their own binary.
add_executable(tca-engine_log-folder_tests
    ${TCA_ENGINE_TESTS_APPLICATION_DIR}/application-log-folder.tests.cpp
    ${TCA_ENGINE_TEST_DOUBLES_DIR}/application/application-logic.test-double.cpp
)

target_include_directories(tca-engine_log-folder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tca-engine_log-folder_tests PRIVATE
    tca-engine
    Catch2::Catch2WithMain
    TCAEngine::compile_options
)

catch_discover_tests(tca-engine_log-folder_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:tca-engine_log-folder_tests>
)

# The input module's tests get a binary of their own, linking that module alone, so working on
# input doesn't mean relinking the whole suite. Its settings bridge is tested in tca-engine_tests,
# since it needs the settings module and the testing support.
add_executable(tca-engine_input_tests
    ${TCA_ENGINE_TESTS_INPUT_DIR}/action-map.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/input-collector.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/input-frame.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/key.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/keyboard-state.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/mouse-button.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/mouse-state.tests.cpp
    ${TCA_ENGINE_TESTS_INPUT_DIR}/standard-actions.tests.cpp
)

target_include_directories(tca-engine_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tca-engine_input_tests PRIVATE
    tca-engine-input
    Catch2::Catch2WithMain
    TCAEngine::compile_options
)

catch_discover_tests(tca-engine_input_tests
    WORKING_DIRECTORY $<TARGET_FILE_DIR:tca-engine_input_tests>
)
