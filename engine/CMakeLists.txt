# Collect all engine source files
file(GLOB_RECURSE ENGINE_SOURCES
    *.cpp
    *.hpp
)

# Create a static library for the engine
add_library(HVKEngine STATIC ${ENGINE_SOURCES})

# Add external includes
target_include_directories(HVKEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # engine/
    ${CMAKE_SOURCE_DIR}/external/glfw/include
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

# Find Vulkan
find_package(Vulkan REQUIRED)
if (Vulkan_FOUND)
    target_include_directories(HVKEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(HVKEngine Vulkan::Vulkan)
endif()
