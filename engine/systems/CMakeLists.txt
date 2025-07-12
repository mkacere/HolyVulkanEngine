file(GLOB_RECURSE SYSTEMS_SRC CONFIGURE_DEPENDS *.cpp *.h)

add_library(hvk_systems STATIC ${SYSTEMS_SRC})

target_include_directories(hvk_systems PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hvk_systems
    PUBLIC
        hvk_resources
        hvk_scene
        hvk_gfx
)