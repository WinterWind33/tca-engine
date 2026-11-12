# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_WEB_MODULE_HEADER_FILES
    web-logger.hpp
    diagnostics.hpp

    gui/web-view.hpp
)
set(TCA_ENGINE_WEB_MODULE_SOURCE_FILES
    web-logger.cpp

    gui/web-view.cpp
)

add_library(tca-engine-web STATIC ${TCA_ENGINE_WEB_MODULE_HEADER_FILES} ${TCA_ENGINE_WEB_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-web PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-web PUBLIC
    tca-engine-core
    tca-engine-rendering
    spdlog::spdlog
)

target_link_libraries(tca-engine-web PUBLIC TCAEngine::compile_options)
