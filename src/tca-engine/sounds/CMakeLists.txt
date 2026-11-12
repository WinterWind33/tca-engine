# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_SOUNDS_MODULE_HEADER_FILES
    sounds.hpp
)
set(TCA_ENGINE_SOUNDS_MODULE_SOURCE_FILES
    sounds.cpp
)

add_library(tca-engine-sounds STATIC ${TCA_ENGINE_SOUNDS_MODULE_HEADER_FILES} ${TCA_ENGINE_SOUNDS_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-sounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-sounds PUBLIC
    tca-engine-core
    Microsoft.GSL::GSL
    spdlog::spdlog
)
target_link_libraries(tca-engine-sounds PRIVATE
    miniaudio
)

target_link_libraries(tca-engine-sounds PUBLIC TCAEngine::compile_options)
