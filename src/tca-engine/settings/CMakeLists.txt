# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_SETTINGS_MODULE_HEADER_FILES
  settings-schema.hpp
  settings-value-traits.hpp
  settings-io.hpp
  settings-location.hpp
  settings-json-formatter.hpp
)
set(TCA_ENGINE_SETTINGS_MODULE_SOURCE_FILES
  settings-json-formatter.cpp
  settings-location.cpp
)

add_library(tca-engine-settings STATIC ${TCA_ENGINE_SETTINGS_MODULE_HEADER_FILES} ${TCA_ENGINE_SETTINGS_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-settings PUBLIC
    tca-engine-core
    # PUBLIC because settings-io.hpp writes through platform's atomic file primitive, and
    # settings-location.hpp asks platform where a game's user data belongs.
    tca-engine-platform
    Microsoft.GSL::GSL
)
# nlohmann is PRIVATE on purpose: it appears in no header of this module, so nothing downstream
# compiles against it or is pinned to the version the engine was built with.
target_link_libraries(tca-engine-settings PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(tca-engine-settings PRIVATE TCAEngine::compile_options)
