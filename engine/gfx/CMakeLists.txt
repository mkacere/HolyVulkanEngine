file(GLOB_RECURSE GFX_SRC CONFIGURE_DEPENDS *.cpp *.h)

add_library(hvk_gfx STATIC ${GFX_SRC})

target_include_directories(hvk_gfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hvk_gfx
    PUBLIC hvk_core
)

# Vulkan support
find_package(Vulkan REQUIRED)
if (Vulkan_FOUND)
    target_include_directories(hvk_gfx PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(hvk_gfx PRIVATE Vulkan::Vulkan)
endif()

# Add GLM
find_package(glm REQUIRED)
target_link_libraries(hvk_gfx PUBLIC glm::glm)

# Add GLFW
find_package(glfw3 REQUIRED)
target_link_libraries(hvk_gfx PUBLIC glfw)