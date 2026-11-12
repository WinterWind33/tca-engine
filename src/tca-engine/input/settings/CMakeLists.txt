# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# The bridge between input and settings: lets an action map be a settings field. A target of its
# own, added only with the settings module, so that tca-engine-input never depends on settings:
# settings links platform, which links input, so input linking settings would be a cycle.
# Header-only.
set(TCA_ENGINE_INPUT_SETTINGS_MODULE_HEADER_FILES
    action-map-settings.hpp
)

add_library(tca-engine-input-settings INTERFACE ${TCA_ENGINE_INPUT_SETTINGS_MODULE_HEADER_FILES})
target_link_libraries(tca-engine-input-settings INTERFACE
    tca-engine-input
    tca-engine-settings
)

target_link_libraries(tca-engine-input-settings INTERFACE TCAEngine::compile_options)
