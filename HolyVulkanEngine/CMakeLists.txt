# HolyVulkanEngine/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(HolyVulkanEngine LANGUAGES CXX)

# C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine library (subdirs will add sources with target_sources)
add_library(HVKEngine STATIC)

# Public & private include paths (superproject-friendly; no CMAKE_SOURCE_DIR)
target_include_directories(HVKEngine
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/external/glm
        ${CMAKE_CURRENT_LIST_DIR}/external/stb
        ${CMAKE_CURRENT_LIST_DIR}/external/tinygltf
        ${CMAKE_CURRENT_LIST_DIR}/external/glfw/include
        ${CMAKE_CURRENT_LIST_DIR}/external/VulkanMemoryAllocator/include
        ${CMAKE_CURRENT_LIST_DIR}/external/imgui
        ${CMAKE_CURRENT_LIST_DIR}/external/imgui/backends
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

# Dependencies
find_package(Vulkan REQUIRED)
target_link_libraries(HVKEngine PUBLIC Vulkan::Vulkan)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(HVKEngine PUBLIC winmm)
endif()

target_precompile_headers(HVKEngine
  PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_LIST_DIR}/src/pch.h>
)

# Prefer system GLFW; fall back to vendored external/glfw if present
find_package(glfw3 QUIET)  # provides target 'glfw' on most setups
if (glfw3_FOUND)
    target_link_libraries(HVKEngine PUBLIC glfw)
elseif (EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/glfw/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/glfw)
    target_link_libraries(HVKEngine PUBLIC glfw)
else()
    message(FATAL_ERROR "GLFW not found. Install glfw3 or add external/glfw.")
endif()

# ImGui source files
set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/external/imgui)
target_sources(HVKEngine PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# Submodules (each has a CMakeLists.txt that GLOB_RECURSEs *.cpp and calls target_sources)
add_subdirectory(src/hvk/core)
add_subdirectory(src/hvk/gfx)
add_subdirectory(src/hvk/resources)
add_subdirectory(src/hvk/scene)
add_subdirectory(src/hvk/systems)
add_subdirectory(src/hvk/ui)
