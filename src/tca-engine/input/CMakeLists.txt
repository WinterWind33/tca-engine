# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# Platform-neutral input: no Win32 and no ImGui in here. Platform layers feed it events; the
# application hands the resulting state to games.
set(TCA_ENGINE_INPUT_MODULE_HEADER_FILES
    actions/action-map.hpp
    actions/action-schema.hpp
    actions/standard-actions.hpp
    key.hpp
    input-collector.hpp
    input-frame.hpp
    keyboard-state.hpp
    mouse-button.hpp
    mouse-state.hpp
)
set(TCA_ENGINE_INPUT_MODULE_SOURCE_FILES
    key.cpp
    input-collector.cpp
    input-frame.cpp
    keyboard-state.cpp
    mouse-button.cpp
    mouse-state.cpp
)

add_library(tca-engine-input STATIC ${TCA_ENGINE_INPUT_MODULE_HEADER_FILES} ${TCA_ENGINE_INPUT_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-input PUBLIC
    tca-engine-core
)

target_link_libraries(tca-engine-input PUBLIC TCAEngine::compile_options)
