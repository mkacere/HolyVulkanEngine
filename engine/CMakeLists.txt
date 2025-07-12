# engine/CMakeLists.txt

# Collect all source files except foundation
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx/*.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/scene/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/*.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/systems/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp
)

# Create a static library for the engine
add_library(HVKEngine STATIC ${ENGINE_SOURCES})

# Add subdirectories (only if systems/ or others have their own CMakeLists)
add_subdirectory(systems)

target_include_directories(HVKEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # <gfx/...>, <core/...>, etc.
    ${CMAKE_SOURCE_DIR}/external/glfw/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${CMAKE_SOURCE_DIR}/external
)

# Link external libraries
target_link_libraries(HVKEngine
    glfw
)

# Vulkan setup
find_package(Vulkan REQUIRED)
if (Vulkan_FOUND)
    target_include_directories(HVKEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(HVKEngine Vulkan::Vulkan)
endif()
