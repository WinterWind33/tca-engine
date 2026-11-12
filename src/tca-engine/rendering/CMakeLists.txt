# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_RENDERING_GUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gui)
set(TCA_ENGINE_RENDERING_QUEUES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rendering-queues)
set(TCA_ENGINE_RENDERING_THEMING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/theming)
set(TCA_ENGINE_RENDERING_SCENE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scene)

set(TCA_ENGINE_RENDERING_MODULE_HEADER_FILES
    colors.hpp
    color-conversions.hpp
    gpu-texture.hpp
    image.hpp
    image-factory.hpp
    image-color-sampling.hpp
    rendering-logger.hpp
    tickables.hpp

    ${TCA_ENGINE_RENDERING_GUI_DIR}/widget.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/label.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/fps-overlay.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/layout-widgets.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/callback-renderable.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/scroll-view.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-color-helpers.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-math-helpers.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-extensions-helpers.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-theme.hpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-blend-callbacks.hpp

    ${TCA_ENGINE_RENDERING_QUEUES_DIR}/image-render-queue.hpp
    ${TCA_ENGINE_RENDERING_QUEUES_DIR}/image-render-queue-base.hpp
    ${TCA_ENGINE_RENDERING_QUEUES_DIR}/null-image-render-queue.hpp

    # The theme resource database. Deliberately free of any ImGui (or other backend) include: it is
    # plain data, so it builds and is tested with no renderer at all, and the ImGui applier in
    # gui/imgui/imgui-theme.hpp is only one of its possible consumers.
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/theme-roles.hpp
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/theme.hpp
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/theme-factories.hpp
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/active-theme.hpp

    custom-tickables/fixed-step-tickables.hpp

    ${TCA_ENGINE_RENDERING_SCENE_DIR}/camera-2d.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-grid.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-renderable.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-surface.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/virtual-resolution.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/texture-atlas.hpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/sprite.hpp
)
set(TCA_ENGINE_RENDERING_MODULE_SOURCE_FILES
    rendering-logger.cpp
    tickables.cpp
    color-conversions.cpp
    image.cpp
    image-factory.cpp
    image-color-sampling.cpp

    ${TCA_ENGINE_RENDERING_GUI_DIR}/label.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/fps-overlay.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/layout-widgets.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/callback-renderable.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/scroll-view.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-color-helpers.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-math-helpers.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-extensions-helpers.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-theme.cpp
    ${TCA_ENGINE_RENDERING_GUI_DIR}/imgui/imgui-blend-callbacks.cpp

    ${TCA_ENGINE_RENDERING_QUEUES_DIR}/image-render-queue-base.cpp

    ${TCA_ENGINE_RENDERING_THEMING_DIR}/theme.cpp
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/theme-factories.cpp
    ${TCA_ENGINE_RENDERING_THEMING_DIR}/active-theme.cpp

    custom-tickables/fixed-step-tickables.cpp

    ${TCA_ENGINE_RENDERING_SCENE_DIR}/camera-2d.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-grid.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-renderable.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/scene-surface.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/virtual-resolution.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/texture-atlas.cpp
    ${TCA_ENGINE_RENDERING_SCENE_DIR}/sprite.cpp
)

if(WIN32)
    list(APPEND TCA_ENGINE_RENDERING_MODULE_SOURCE_FILES
        win32/directx-12/descriptor-heap-allocator.cpp
        win32/directx-12/directx12-rendering-manager.cpp
        win32/directx-12/dx12-image-render-queue.cpp
        win32/directx-12/dx12-additive-blending.cpp
        win32/opengl3/opengl3-rendering-manager.cpp
        win32/opengl3/opengl3-additive-blending.cpp
        win32/opengl3/opengl3-image-render-queue.cpp
        render-targets/win32/imgui-win32-window.cpp
    )

    list(APPEND TCA_ENGINE_RENDERING_MODULE_HEADER_FILES
        win32/directx-12/frame-context.hpp
        win32/directx-12/descriptor-heap-allocator.hpp
        win32/directx-12/directx12-rendering-manager.hpp
        win32/directx-12/dx12-image-render-queue.hpp
        win32/directx-12/dx12-additive-blending.hpp
        win32/opengl3/opengl3-rendering-manager.hpp
        win32/opengl3/opengl3-additive-blending.hpp
        win32/opengl3/opengl3-image-render-queue.hpp
        render-targets/win32/imgui-win32-window.hpp
    )
endif()

add_library(tca-engine-rendering STATIC ${TCA_ENGINE_RENDERING_MODULE_HEADER_FILES} ${TCA_ENGINE_RENDERING_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-rendering PUBLIC
    tca-engine-core
    tca-engine-input
    tca-engine-platform
    Microsoft.GSL::GSL
    spdlog::spdlog
    imgui
)
target_link_libraries(tca-engine-rendering PRIVATE
    imgui-markdown-ext
)

if(WIN32)
    target_link_libraries(tca-engine-rendering PUBLIC
        WIL::WIL
        d3d12.lib d3dcompiler.lib dxgi.lib Opengl32.lib
    )
endif()

target_link_libraries(tca-engine-rendering PUBLIC TCAEngine::compile_options)
