# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_CORE_UTILITIES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/utilities)

set(TCA_ENGINE_CORE_MODULE_HEADER_FILES
    events.hpp
    fsm.hpp
    logging/log-header.hpp
    logging/logging.hpp
    math/math.hpp
    math/size.hpp
    math/vector.hpp

    ${TCA_ENGINE_CORE_UTILITIES_DIR}/non-zero-values.hpp
    ${TCA_ENGINE_CORE_UTILITIES_DIR}/pointers.hpp
    ${TCA_ENGINE_CORE_UTILITIES_DIR}/ranged-value.hpp
)
set(TCA_ENGINE_CORE_MODULE_SOURCE_FILES
    logging/log-header.cpp
    logging/logging.cpp
)

add_library(tca-engine-core STATIC ${TCA_ENGINE_CORE_MODULE_HEADER_FILES} ${TCA_ENGINE_CORE_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-core PUBLIC
    Microsoft.GSL::GSL
    spdlog::spdlog
    Eigen3::Eigen
)

target_link_libraries(tca-engine-core PUBLIC TCAEngine::compile_options)
