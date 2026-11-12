# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/tca-fetch-git-dependency.cmake)

# --- miniaudio ---
tca_fetch_git_dependency(
    NAME miniaudio
    REPOSITORY https://github.com/mackron/miniaudio.git
    TAG 0.11.25
)

# miniaudio does not ship its own CMake support. It requires exactly one translation unit to define
# MINIAUDIO_IMPLEMENTATION before including the header; the repository already ships miniaudio.c
# for that purpose, so it is built here as a STATIC library around that file, compiled as C++ since
# this project only enables the CXX language.
add_library(miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
target_include_directories(miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})

# --- imgui-markdown-ext (enkisoftware/imgui_markdown) ---
tca_fetch_git_dependency(
    NAME imgui-markdown-ext
    REPOSITORY https://github.com/enkisoftware/imgui_markdown.git
    # The repository has no tags, so a specific commit is pinned instead.
    COMMIT 4acbf80584753e15ea54eb271129995862daac8f
)

# imgui_markdown is a single-header library (depends on Dear ImGui, linked
# separately by consumers) and does not ship its own CMake support, so it is
# exposed here as an INTERFACE target.
add_library(imgui-markdown-ext INTERFACE)
target_include_directories(imgui-markdown-ext SYSTEM INTERFACE ${imgui-markdown-ext_SOURCE_DIR})

# --- imgui (ocornut/imgui) ---
tca_fetch_git_dependency(
    NAME imgui
    REPOSITORY https://github.com/ocornut/imgui.git
    TAG v1.92.8
)

# imgui does not ship its own CMake support, so it is built here as a STATIC
# library, including the Win32, DirectX 12 and OpenGL3 backends used by the engine.
set(TCA_ENGINE_IMGUI_SOURCE_FILES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

if(WIN32)
    list(APPEND TCA_ENGINE_IMGUI_SOURCE_FILES
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

if(LINUX AND NOT APPLE)
  list(APPEND TCA_ENGINE_IMGUI_SOURCE_FILES
      ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
endif()

add_library(imgui STATIC ${TCA_ENGINE_IMGUI_SOURCE_FILES})
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

if(LINUX AND NOT APPLE)
  target_link_libraries(imgui PRIVATE glfw)
endif()

if(NOT WIN32)
    # --- stb (nothings/stb) ---
    tca_fetch_git_dependency(
        NAME stb
        REPOSITORY https://github.com/nothings/stb.git
        # The repository has no tags, so a specific commit is pinned instead.
        COMMIT 31c1ad37456438565541f4919958214b6e762fb4
    )
endif()

# stb is a collection of single-header libraries and does not ship its own CMake support. Only
# stb_image.h is needed here (the image decoder backend everywhere but Windows), exposed as an
# INTERFACE target.
add_library(stb-image INTERFACE)
target_include_directories(stb-image SYSTEM INTERFACE ${stb_SOURCE_DIR})

# --- verso (WinterWind33/verso) ---
tca_fetch_git_dependency(
    NAME verso
    REPOSITORY https://github.com/WinterWind33/verso.git
    TAG v0.3.1
)

# verso ships its own CMake support and exposes the verso target directly. It does not mark its
# own include directories as SYSTEM, so consumers built with -Werror would fail on its warnings;
# promote them to SYSTEM here instead of patching the upstream project.
add_subdirectory(${verso_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/verso)
set_target_properties(verso PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "$<TARGET_PROPERTY:verso,INTERFACE_INCLUDE_DIRECTORIES>"
)
