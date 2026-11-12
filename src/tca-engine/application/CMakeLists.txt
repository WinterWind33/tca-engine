# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_APPLICATION_MODULE_HEADER_FILES
    application.hpp
    application-logger.hpp
    game-application-logic.hpp
    simple-console-application.hpp
)
set(TCA_ENGINE_APPLICATION_MODULE_SOURCE_FILES
    application.cpp
    application-logger.cpp
    game-application-logic.cpp
    simple-console-application.cpp
)

if(WIN32)
    list(APPEND TCA_ENGINE_APPLICATION_MODULE_SOURCE_FILES
        win32/win32-application.cpp
        win32/directx12-application.cpp
        win32/imgui-win32-utilities.cpp
        win32/imgui-dx12-application.cpp
        win32/opengl3-win32-application.cpp
        win32/imgui-opengl3-win32-application.cpp
    )

    list(APPEND TCA_ENGINE_APPLICATION_MODULE_HEADER_FILES
        win32/win32-application.hpp
        win32/directx12-application.hpp
        win32/imgui-win32-utilities.hpp
        win32/imgui-dx12-application.hpp
        win32/opengl3-win32-application.hpp
        win32/imgui-opengl3-win32-application.hpp
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND TCA_ENGINE_APPLICATION_MODULE_SOURCE_FILES
        linux/linux-application-controller.cpp
    )

    list(APPEND TCA_ENGINE_APPLICATION_MODULE_HEADER_FILES
        linux/linux-application-controller.hpp
    )
endif()

add_library(tca-engine-application STATIC ${TCA_ENGINE_APPLICATION_MODULE_HEADER_FILES} ${TCA_ENGINE_APPLICATION_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-application PUBLIC
    tca-engine-input
    tca-engine-platform
    tca-engine-rendering
    imgui
)

if(WIN32)
    target_link_libraries(tca-engine-application PUBLIC
        WIL::WIL
        d3d12.lib
    )
endif()

target_link_libraries(tca-engine-application PUBLIC TCAEngine::compile_options)
