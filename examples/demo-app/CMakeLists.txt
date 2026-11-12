# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

add_executable(tca-engine-demo main.cpp)

# Linking the aggregate target alone, on purpose: the demo stands in for a game consuming the
# engine, so if it ever needs a target (or an include directory) the aggregate does not carry,
# that is a hole in the engine's public surface to be fixed there rather than patched here.
target_link_libraries(tca-engine-demo PRIVATE
    tca-engine
    TCAEngine::compile_options
)

# The demo loads its sprite atlas from an assets folder next to the executable, as a shipped game
# would. Each file is a build output depending on its source, so editing an asset alone is enough
# for the next build to copy it again.
set(TCA_ENGINE_DEMO_ASSETS
    sprites.atlas
    sprites.png
)
set(TCA_ENGINE_DEMO_COPIED_ASSETS)
foreach(TCA_ENGINE_DEMO_ASSET IN LISTS TCA_ENGINE_DEMO_ASSETS)
    set(TCA_ENGINE_DEMO_ASSET_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/assets/${TCA_ENGINE_DEMO_ASSET})
    set(TCA_ENGINE_DEMO_ASSET_COPY
        $<TARGET_FILE_DIR:tca-engine-demo>/assets/${TCA_ENGINE_DEMO_ASSET})
    # A stamp stands in for the copy as the output, since an OUTPUT cannot be a generator
    # expression under every generator.
    set(TCA_ENGINE_DEMO_ASSET_STAMP
        ${CMAKE_CURRENT_BINARY_DIR}/assets-copied/${TCA_ENGINE_DEMO_ASSET}.stamp)
    add_custom_command(
        OUTPUT ${TCA_ENGINE_DEMO_ASSET_STAMP}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TCA_ENGINE_DEMO_ASSET_SOURCE} ${TCA_ENGINE_DEMO_ASSET_COPY}
        COMMAND ${CMAKE_COMMAND} -E touch ${TCA_ENGINE_DEMO_ASSET_STAMP}
        DEPENDS ${TCA_ENGINE_DEMO_ASSET_SOURCE}
        COMMENT "Copying the demo asset ${TCA_ENGINE_DEMO_ASSET}"
        VERBATIM
    )
    list(APPEND TCA_ENGINE_DEMO_COPIED_ASSETS ${TCA_ENGINE_DEMO_ASSET_STAMP})
endforeach()

add_custom_target(tca-engine-demo-assets DEPENDS ${TCA_ENGINE_DEMO_COPIED_ASSETS})
add_dependencies(tca-engine-demo tca-engine-demo-assets)
