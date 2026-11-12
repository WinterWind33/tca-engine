# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

set(TCA_ENGINE_PLATFORM_TERMINAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/terminal)

set(TCA_ENGINE_PLATFORM_MODULE_HEADER_FILES
    engine-type-traits.hpp
    application-window.hpp
    platform-logger.hpp
    folders.hpp
    atomic-file-write.hpp
    system-information.hpp
    images/image-decoder.hpp

    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/tokenizer.hpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/argument-parser.hpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/option-description.hpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/command-description.hpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/command-validator.hpp
)
set(TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES
    platform-logger.cpp
    atomic-file-write.cpp

    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/tokenizer.cpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/argument-parser.cpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/option-description.cpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/command-description.cpp
    ${TCA_ENGINE_PLATFORM_TERMINAL_DIR}/command-validator.cpp
)

if(WIN32)
    list(APPEND TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES
        win32/windows-type-traits.cpp
        win32/win32-application-window.cpp
        win32/folders.cpp
        win32/system-information.cpp
        win32/image-decoder.cpp
        win32/win32-keyboard.cpp
    )

    list(APPEND TCA_ENGINE_PLATFORM_MODULE_HEADER_FILES
        win32/win32-application-window.hpp
        win32/windows-messages.hpp
        win32/windows-common-macros.hpp
        win32/windows-type-traits.hpp
        win32/windows.hpp
        win32/win32-keyboard.hpp
    )
endif()

if(UNIX)
    # Every non-Windows platform decodes images with stb_image; Windows uses WIC instead.
    list(APPEND TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES
        stb/image-decoder.cpp
    )

    if(NOT APPLE)
        list(APPEND TCA_ENGINE_PLATFORM_MODULE_HEADER_FILES
          linux/linux-application-window.hpp
          linux/linux-keyboard.hpp
        )

        list(APPEND TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES
          linux/folders.cpp
          linux/system-information.cpp
          linux/linux-application-window.cpp
          linux/linux-keyboard.cpp
        )
    else()
        list(APPEND TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES
            macos/folders.cpp
            macos/system-information.cpp
        )
    endif()
endif()

add_library(tca-engine-platform STATIC ${TCA_ENGINE_PLATFORM_MODULE_HEADER_FILES} ${TCA_ENGINE_PLATFORM_MODULE_SOURCE_FILES})
target_include_directories(tca-engine-platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tca-engine-platform PUBLIC
    tca-engine-core
    tca-engine-input
    Microsoft.GSL::GSL
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(tca-engine-platform PUBLIC
        WIL::WIL
        windowscodecs.lib
        shell32.lib
    )

    if(TCA_ENGINE_USE_UNICODE)
        target_compile_definitions(tca-engine-platform PUBLIC UNICODE _UNICODE)
    endif()
endif()

if(UNIX)
    target_link_libraries(tca-engine-platform PRIVATE stb-image)
endif()

if(UNIX AND NOT APPLE)
    # PUBLIC: linux-application-window.hpp includes <GLFW/glfw3.h> directly, so consumers of
    # this target need GLFW's include directories too, not just this target's own sources.
    target_link_libraries(tca-engine-platform PUBLIC glfw)
    # PUBLIC too, so that no consumer's <GLFW/glfw3.h> drags in the system <GL/gl.h>, which would
    # clash with whatever OpenGL loader the consumer (or a rendering backend) includes.
    target_compile_definitions(tca-engine-platform PUBLIC GLFW_INCLUDE_NONE)
endif()

target_link_libraries(tca-engine-platform PUBLIC TCAEngine::compile_options)
